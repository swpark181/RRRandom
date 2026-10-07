#include "RRRandomCharacter.h"
#include "RRRandomAIController.h"
#include "RRRandomCover.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomEventComponent.h"
#include "RRRandomGameMode.h"
#include "RRRandomProjectile.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Below this ground speed the character counts as standing still
	constexpr float WalkAnimationThreshold = 10.f;

	const FName ColorParameter(TEXT("DiffuseColor"));
	const FName RagdollProfile(TEXT("Ragdoll"));

	// Shots between brawlers less than this far apart in height fly level
	constexpr float SameLevelHeight = 50.f;
	// Below the starting spot by this much means the brawler fell off the arena
	constexpr float FallOutDepth = 1500.f;

	// Ledges lower than this above the feet aren't worth climbing; walking or jumping handles them
	constexpr float MinClimbHeight = 50.f;
	// Surfaces steeper than this count as a wall to climb
	constexpr float MaxWallNormalZ = 0.3f;
	// How far past the wall face the brawler ends up, beyond its own radius
	constexpr float ClimbLedgeInset = 10.f;
	// Share of the climb spent going up; the rest goes over the edge
	constexpr float ClimbUpShare = 0.6f;
	// Seconds after pressing jump during which reaching a wall still climbs it
	constexpr float ClimbJumpWindow = 0.5f;
}

ARRRandomCharacter::ARRRandomCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Face the direction we walk, not the camera
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Brawlers placed in a level, and bots the game mode spawns, are driven by the bot controller
	AIControllerClass = ARRRandomAIController::StaticClass();

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 640.f, 0.f);
	Movement->bConstrainToPlane = true;
	Movement->bSnapToPlaneAtStart = true;
	Movement->MaxWalkSpeed = 300.f;
	// Nobody walks off the edge of the arena (the top of cover is the exception, see UpdateLedgeWalking)
	Movement->bCanWalkOffLedges = false;
	// Enough steering in the air to aim a jump at cover
	Movement->AirControl = 0.35f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
	CameraBoom->TargetArmLength = 1400.f;
	CameraBoom->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMesh(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAnim(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Walk_Fwd.Tutorial_Walk_Fwd"));
	// Engine material that works on skeletal meshes and has a "DiffuseColor" parameter
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintableMaterial(TEXT("/Engine/TemplateResources/M_Template_Master.M_Template_Master"));

	// The mannequin's feet sit at the bottom of the capsule and it faces +X
	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));
	if (MannequinMesh.Succeeded())
	{
		BodyMesh->SetSkeletalMeshAsset(MannequinMesh.Object);
	}
	if (TintableMaterial.Succeeded())
	{
		BodyBaseMaterial = TintableMaterial.Object;
	}
	if (IdleAnim.Succeeded())
	{
		IdleAnimation = IdleAnim.Object;
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		BodyMesh->AnimationData.AnimToPlay = IdleAnimation;
		BodyMesh->AnimationData.bSavedLooping = true;
		BodyMesh->AnimationData.bSavedPlaying = true;
	}
	if (WalkAnim.Succeeded())
	{
		WalkAnimation = WalkAnim.Object;
	}

	ProjectileClass = ARRRandomProjectile::StaticClass();
	RandomEvents = CreateDefaultSubobject<URRRandomEventComponent>(TEXT("RandomEvents"));
	RandomEvents->ColorParameterName = TEXT("DiffuseColor");
	DiceBuffs = CreateDefaultSubobject<URRRandomDiceBuffComponent>(TEXT("DiceBuffs"));
}

void ARRRandomCharacter::BeginPlay()
{
	// Before Super, so the random event component picks up the tintable material when it begins play
	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (BodyBaseMaterial)
	{
		for (int32 Slot = 0; Slot < FMath::Max(1, BodyMesh->GetNumMaterials()); ++Slot)
		{
			BodyMesh->SetMaterial(Slot, BodyBaseMaterial);
		}
	}

	Super::BeginPlay();

	// The random event component has made per-slot dynamic materials by now
	for (int32 Slot = 0; Slot < BodyMesh->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(BodyMesh->GetMaterial(Slot)))
		{
			Material->SetVectorParameterValue(ColorParameter, GetTeamColor(Team));
		}
	}

	WeaponStream.GenerateNewSeed();
	DiceBuffs->OnWeaponDie.AddUObject(this, &ARRRandomCharacter::OnWeaponDie);

	BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	Health = MaxHealth;
	Ammo = GetMaxAmmo();
	HomeLocation = GetActorLocation();
	HomeRotation = GetActorRotation();
	MeshRelativeLocation = BodyMesh->GetRelativeLocation();
	MeshRelativeRotation = BodyMesh->GetRelativeRotation();
	MeshCollisionProfile = BodyMesh->GetCollisionProfileName();
}

FLinearColor ARRRandomCharacter::GetTeamColor(int32 InTeam)
{
	return InTeam == 0 ? FLinearColor(0.15f, 0.45f, 1.f) : FLinearColor(1.f, 0.2f, 0.15f);
}

float ARRRandomCharacter::GetProjectileSpeed() const
{
	return ProjectileClass ? ProjectileClass->GetDefaultObject<ARRRandomProjectile>()->GetSpeed() * Weapon.ProjectileSpeedMultiplier : 0.f;
}

void ARRRandomCharacter::OnWeaponDie(int32 Face)
{
	if (bAlive)
	{
		EquipWeapon(FRRWeapon::Roll(WeaponStream, Face, MaxAmmo));
	}
}

void ARRRandomCharacter::EquipWeapon(const FRRWeapon& NewWeapon)
{
	Weapon = NewWeapon;
	Ammo = GetMaxAmmo();
	bReloading = false;
	ReloadRemaining = 0.f;
}

void ARRRandomCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateDamagePopups(DeltaSeconds);
	if (!bAlive)
	{
		RespawnRemaining -= DeltaSeconds;
		if (RespawnRemaining <= 0.f)
		{
			Respawn();
		}
		return;
	}

	// Fell off the arena (jumping past its edge): counts as a knockout
	if (GetActorLocation().Z < HomeLocation.Z - FallOutDepth)
	{
		KnockOut(FVector::UpVector, nullptr);
		return;
	}

	UpdateAmmoAndHealth(DeltaSeconds);
	if (bClimbing)
	{
		UpdateClimb(DeltaSeconds);
	}
	else if (GetCharacterMovement()->IsFalling() && GetWorld()->GetTimeSeconds() - JumpPressedTime < ClimbJumpWindow)
	{
		// Jumped toward a wall: grab its top once in reach
		TryClimb();
	}
	UpdateLedgeWalking();
	UpdateWalkSpeed();
	UpdateLocomotionAnimation();
}

void ARRRandomCharacter::Jump()
{
	if (!bAlive || bClimbing)
	{
		return;
	}
	JumpPressedTime = GetWorld()->GetTimeSeconds();
	if (!TryClimb())
	{
		Super::Jump();
	}
}

bool ARRRandomCharacter::TryClimb()
{
	UWorld* World = GetWorld();
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector Location = GetActorLocation();
	const float FeetZ = Location.Z - HalfHeight;

	// Toward where the brawler is trying to go, or where it faces when standing still
	FVector Forward = GetLastMovementInputVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = GetActorForwardVector().GetSafeNormal2D();
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomClimb), false, this);
	const FCollisionObjectQueryParams StaticOnly(ECC_WorldStatic);

	// A wall right in front, at chest height
	FHitResult Wall;
	if (!World->LineTraceSingleByObjectType(Wall, Location, Location + Forward * (Radius + ClimbReach), StaticOnly, Params)
		|| FMath::Abs(Wall.ImpactNormal.Z) > MaxWallNormalZ)
	{
		return false;
	}

	// Its top, a capsule's width past the wall face, high enough to be worth climbing and low enough to reach
	const FVector Over = Wall.ImpactPoint - Wall.ImpactNormal.GetSafeNormal2D() * (Radius + ClimbLedgeInset);
	FHitResult Top;
	if (!World->LineTraceSingleByObjectType(Top, FVector(Over.X, Over.Y, FeetZ + MaxClimbHeight + 1.f), FVector(Over.X, Over.Y, FeetZ + MinClimbHeight), StaticOnly, Params)
		|| Top.bStartPenetrating || Top.ImpactNormal.Z < GetCharacterMovement()->GetWalkableFloorZ())
	{
		return false;
	}

	// Room to stand up there
	const FVector Destination(Over.X, Over.Y, Top.ImpactPoint.Z + HalfHeight + 2.f);
	if (World->OverlapBlockingTestByChannel(Destination, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(Radius, HalfHeight), Params))
	{
		return false;
	}

	bClimbing = true;
	ClimbElapsed = 0.f;
	ClimbStart = Location;
	ClimbTarget = Destination;
	SetActorRotation((-Wall.ImpactNormal).GetSafeNormal2D().Rotation());
	// Movement off while the climb moves the capsule; input does nothing meanwhile
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	return true;
}

void ARRRandomCharacter::UpdateClimb(float DeltaSeconds)
{
	ClimbElapsed += DeltaSeconds;
	const float Alpha = ClimbDuration > 0.f ? FMath::Clamp(ClimbElapsed / ClimbDuration, 0.f, 1.f) : 1.f;

	// Straight up beside the wall first, then over the edge, so the capsule never cuts through the corner
	const float UpAlpha = FMath::Clamp(Alpha / ClimbUpShare, 0.f, 1.f);
	const float OverAlpha = FMath::Clamp((Alpha - ClimbUpShare) / (1.f - ClimbUpShare), 0.f, 1.f);
	const FVector Flat = FMath::Lerp(FVector(ClimbStart.X, ClimbStart.Y, 0.f), FVector(ClimbTarget.X, ClimbTarget.Y, 0.f), FMath::InterpEaseInOut(0.f, 1.f, OverAlpha, 2.f));
	const float Z = FMath::InterpEaseOut(ClimbStart.Z, ClimbTarget.Z, UpAlpha, 2.f);
	SetActorLocation(FVector(Flat.X, Flat.Y, Z));

	if (Alpha >= 1.f)
	{
		bClimbing = false;
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
}

void ARRRandomCharacter::UpdateLedgeWalking()
{
	// Standing on cover: walking off its edge drops to the floor. On the floor itself: the arena edge stops everyone.
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const AActor* Floor = Movement->CurrentFloor.HitResult.GetActor();
	GetCharacterMovement()->bCanWalkOffLedges = Movement->IsMovingOnGround() && Floor && Floor->IsA<ARRRandomCover>();
}

void ARRRandomCharacter::UpdateAmmoAndHealth(float DeltaSeconds)
{
	if (bReloading)
	{
		ReloadRemaining -= DeltaSeconds * DiceBuffs->GetMultiplier(ERRDiceBuffStat::AttackSpeed);
		if (ReloadRemaining <= 0.f)
		{
			Ammo = GetMaxAmmo();
			bReloading = false;
			ReloadRemaining = 0.f;
		}
	}

	if (Health < MaxHealth && GetWorld()->GetTimeSeconds() - LastCombatTime >= RegenDelay)
	{
		Health = FMath::Min(MaxHealth, Health + MaxHealth * RegenPerSecond * DeltaSeconds);
	}
}

void ARRRandomCharacter::UpdateDamagePopups(float DeltaSeconds)
{
	for (FRRDamagePopup& Popup : DamagePopups)
	{
		Popup.Age += DeltaSeconds;
	}
	DamagePopups.RemoveAll([](const FRRDamagePopup& Popup) { return Popup.Age >= DamagePopupLifetime; });
}

void ARRRandomCharacter::UpdateWalkSpeed()
{
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * RandomEvents->GetSpeedMultiplier() * DiceBuffs->GetMultiplier(ERRDiceBuffStat::MoveSpeed);
}

void ARRRandomCharacter::UpdateLocomotionAnimation()
{
	const float Speed = GetVelocity().Size2D();
	UAnimSequence* Wanted = Speed > WalkAnimationThreshold ? WalkAnimation : IdleAnimation;
	if (Wanted && Wanted != CurrentAnimation)
	{
		GetMesh()->PlayAnimation(Wanted, true);
		CurrentAnimation = Wanted;
	}
	if (CurrentAnimation == WalkAnimation)
	{
		GetMesh()->SetPlayRate(FMath::Clamp(Speed / WalkAnimationSpeed, 0.5f, 3.f));
	}
}

bool ARRRandomCharacter::FireAt(const FVector& TargetLocation)
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	if (!bAlive || !ProjectileClass || bReloading || bClimbing || Now < NextFireTime)
	{
		return false;
	}
	if (Ammo <= 0)
	{
		StartReload();
		return false;
	}
	NextFireTime = Now + FireInterval * Weapon.FireIntervalMultiplier / DiceBuffs->GetMultiplier(ERRDiceBuffStat::AttackSpeed);
	--Ammo;
	LastCombatTime = Now;
	// Like an FPS, an empty magazine reloads by itself
	if (Ammo <= 0)
	{
		StartReload();
	}

	// The body only turns; the shot itself may tilt toward a target on another level
	FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector().GetSafeNormal2D();
	}
	const FRotator Aim = Direction.Rotation();
	SetActorRotation(Aim);

	const FVector Muzzle = GetActorLocation() + Aim.RotateVector(MuzzleOffset);
	FVector ShotDirection = TargetLocation + FVector(0.f, 0.f, MuzzleOffset.Z) - Muzzle;
	// Small height differences (same floor, mid-jump) keep shots level; a target closer than the muzzle can't tilt them
	if (FMath::Abs(ShotDirection.Z) < SameLevelHeight || FVector::DotProduct(ShotDirection, Direction) <= 0.f)
	{
		ShotDirection = Direction;
	}
	FRotator ShotRotation = ShotDirection.Rotation();
	ShotRotation.Pitch = FMath::Clamp(ShotRotation.Pitch, -MaxShotPitch, MaxShotPitch);

	// Deferred so damage, range, speed and look are in place before the shot begins play. Heavy guns fire bigger shots.
	const FTransform SpawnTransform(ShotRotation, Muzzle, FVector(Weapon.ProjectileScale));
	if (ARRRandomProjectile* Projectile = World->SpawnActorDeferred<ARRRandomProjectile>(ProjectileClass, SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Projectile->DamageMultiplier = DiceBuffs->GetMultiplier(ERRDiceBuffStat::AttackPower) * Weapon.DamageMultiplier;
		Projectile->MaxRange = AttackRange;
		Projectile->SetSpeedMultiplier(Weapon.ProjectileSpeedMultiplier);
		Projectile->Tier = Weapon.Tier;
		// The player's own shots keep their default color so they stand out
		if (!IsPlayerControlled())
		{
			Projectile->ShotColor = GetTeamColor(Team);
		}
		Projectile->FinishSpawning(SpawnTransform);
	}
	return true;
}

void ARRRandomCharacter::StartReload()
{
	if (!bAlive || bReloading || Ammo >= GetMaxAmmo() || GetReloadTime() <= 0.f)
	{
		return;
	}
	bReloading = true;
	ReloadRemaining = GetReloadTime();
}

float ARRRandomCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	ARRRandomCharacter* Attacker = DamageCauser ? Cast<ARRRandomCharacter>(DamageCauser->GetInstigator()) : nullptr;
	if (!bAlive || (Attacker && Attacker != this && Attacker->GetTeam() == Team))
	{
		return 0.f;
	}

	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Damage <= 0.f)
	{
		return 0.f;
	}

	Health = FMath::Max(0.f, Health - Damage);
	LastCombatTime = GetWorld()->GetTimeSeconds();

	FRRDamagePopup& Popup = DamagePopups.AddDefaulted_GetRef();
	Popup.Amount = Damage;
	Popup.OffsetX = FMath::FRandRange(-25.f, 25.f);

	if (Health <= 0.f)
	{
		FVector ShotDirection = DamageCauser ? DamageCauser->GetActorForwardVector() : -GetActorForwardVector();
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			ShotDirection = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection;
		}
		KnockOut(ShotDirection, Attacker);
	}
	return Damage;
}

void ARRRandomCharacter::KnockOut(const FVector& ShotDirection, ARRRandomCharacter* Attacker)
{
	bAlive = false;
	bClimbing = false;
	Health = 0.f;
	RespawnRemaining = RespawnDelay;
	DiceBuffs->ClearBuffs();
	// Back to the starting pistol
	Weapon = FRRWeapon();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetCollisionProfileName(RagdollProfile);
	// Shots fly over the fallen body instead of being soaked up by it
	BodyMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	BodyMesh->SetSimulatePhysics(true);
	BodyMesh->SetAllPhysicsLinearVelocity(ShotDirection.GetSafeNormal2D() * KnockdownSpeed + FVector(0.f, 0.f, 200.f));

	if (ARRRandomGameMode* GameMode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>())
	{
		GameMode->OnBrawlerKnockedOut(this, Attacker);
	}
}

void ARRRandomCharacter::Respawn()
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetSimulatePhysics(false);
	BodyMesh->SetCollisionProfileName(MeshCollisionProfile);
	BodyMesh->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	BodyMesh->SetRelativeLocationAndRotation(MeshRelativeLocation, MeshRelativeRotation);
	if (IdleAnimation)
	{
		BodyMesh->PlayAnimation(IdleAnimation, true);
		CurrentAnimation = IdleAnimation;
	}

	SetActorLocationAndRotation(HomeLocation, HomeRotation, false, nullptr, ETeleportType::TeleportPhysics);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	Health = MaxHealth;
	Ammo = GetMaxAmmo();
	bReloading = false;
	ReloadRemaining = 0.f;
	LastCombatTime = -UE_BIG_NUMBER;
	NextFireTime = 0.f;
	bAlive = true;
}
