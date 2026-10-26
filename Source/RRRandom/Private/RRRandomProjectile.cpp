#include "RRRandomProjectile.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomStarRobot.h"
#include "RRRandomVehicle.h"
#include "RRRandomWreckage.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName ColorParameter(TEXT("Color"));
	// A foam finger's poke in the back does this many times the target's whole health
	constexpr float BackstabDamageShare = 10.f;

	enum class EImpact : uint8 { None, Spark, Burst, Explosion };

	/** Effects per weapon tier: the stronger the gun's damage, the flashier its shots. */
	struct FTierLook
	{
		float GlowIntensity;
		float GlowRadius;
		EImpact Muzzle;
		float MuzzleScale;
		bool bTrail;
		float TrailScale;
		EImpact Impact;
		float ImpactScale;
	};

	const FTierLook TierLooks[] =
	{
		// Common: just the ball
		{ 0.f,   0.f,   EImpact::None,  0.f,  false, 0.f,  EImpact::None,      0.f },
		// Rare: glow and a spark where it lands
		{ 20.f,  200.f, EImpact::None,  0.f,  false, 0.f,  EImpact::Spark,     0.6f },
		// Epic: brighter, muzzle flash, trail, bigger burst
		{ 40.f,  280.f, EImpact::Spark, 0.5f, true,  0.4f, EImpact::Burst,     0.7f },
		// Legendary: everything, ending in an explosion
		{ 70.f,  380.f, EImpact::Burst, 0.6f, true,  0.6f, EImpact::Explosion, 0.6f },
	};

	// The star robot's missiles: this long and thick (the ball stretched into a dart); turning this fast toward their aim at
	// first (degrees a second), this much faster for each second flown; bursting this close to it, in a fireball this big
	const FVector MissileLook(0.5f, 0.16f, 0.16f);
	constexpr float MissileTurnRate = 90.f;
	constexpr float MissileTurnGrowth = 500.f;
	constexpr float MissileArrive = 60.f;
	constexpr float MissileBlast = 0.45f;

	// Templates may loop for previewing, so one-shot effects stop spawning after this and let their particles finish
	constexpr float OneShotSpawnTime = 0.25f;

	void SpawnOneShot(const UObject* Context, UNiagaraSystem* System, const FVector& Location, const FRotator& Rotation, float Scale)
	{
		if (!System || Scale <= 0.f)
		{
			return;
		}
		UNiagaraComponent* Effect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Context, System, Location, Rotation, FVector(Scale));
		if (Effect)
		{
			TWeakObjectPtr<UNiagaraComponent> WeakEffect(Effect);
			FTimerHandle Handle;
			Context->GetWorld()->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakEffect]()
			{
				if (WeakEffect.IsValid())
				{
					WeakEffect->Deactivate();
				}
			}), OneShotSpawnTime, false);
		}
	}
}

ARRRandomProjectile::ARRRandomProjectile()
{
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	// Keep shots out of the mouse cursor trace and the camera boom. Shots still hit WorldDynamic, since the level's
	// floor is movable and shots angled down from cover must land on it; other shots are skipped in BeginPlay.
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->OnComponentHit.AddDynamic(this, &ARRRandomProjectile::OnHit);
	RootComponent = Collision;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(RootComponent);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetRelativeScale3D(FVector(0.25f));
	if (SphereMesh.Succeeded())
	{
		Visual->SetStaticMesh(SphereMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Visual->SetMaterial(0, ShapeMaterial.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 5000.f;
	Movement->MaxSpeed = 5000.f;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;

	InitialLifeSpan = 2.f;

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetCastShadows(false);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetVisibility(false);

	// Engine Niagara templates, so effects need no project assets
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> SparkSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurstLightweight.DirectionalBurstLightweight"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> BurstSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> ExplosionSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion.SimpleExplosion"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> RingSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/RadialBurst.RadialBurst"));
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> TrailSystem(TEXT("/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight"));
	SparkEffect = SparkSystem.Object;
	BurstEffect = BurstSystem.Object;
	ExplosionEffect = ExplosionSystem.Object;
	RingEffect = RingSystem.Object;
	TrailEffect = TrailSystem.Object;

	// Ticks only to drag a trail along
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// The server's shots show up on every client; their spawn carries position, size and velocity
	bReplicates = true;
	SetReplicatingMovement(true);
}

void ARRRandomProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARRRandomProjectile, Tier);
	DOREPLIFETIME(ARRRandomProjectile, bPoke);
	DOREPLIFETIME(ARRRandomProjectile, RocketArm);
	DOREPLIFETIME(ARRRandomProjectile, bMissile);
	DOREPLIFETIME(ARRRandomProjectile, MissileTarget);
	DOREPLIFETIME(ARRRandomProjectile, bImpacted);
	DOREPLIFETIME(ARRRandomProjectile, bOutOfRange);
	DOREPLIFETIME(ARRRandomProjectile, bHitBrawler);
	DOREPLIFETIME(ARRRandomProjectile, ImpactLocation);
	DOREPLIFETIME(ARRRandomProjectile, ImpactNormal);
}

void ARRRandomProjectile::PostNetReceiveVelocity(const FVector& NewVelocity)
{
	Super::PostNetReceiveVelocity(NewVelocity);

	// The client copy starts at the default speed; take the gun's real speed, which may be above the default cap
	Movement->MaxSpeed = FMath::Max(Movement->MaxSpeed, NewVelocity.Size());
	Movement->Velocity = NewVelocity;
}

void ARRRandomProjectile::BeginPlay()
{
	Super::BeginPlay();

	// The viewer's own shots keep their default color so they stand out; everyone else's take their team color
	if (const ARRRandomCharacter* Shooter = Cast<ARRRandomCharacter>(GetInstigator()))
	{
		if (!Shooter->IsLocallyControlled() || !Shooter->IsPlayerControlled())
		{
			ShotColor = ARRRandomCharacter::GetTeamColor(Shooter->GetTeam());
		}
	}

	// A poke is the finger itself reaching out: nothing flies
	if (bPoke)
	{
		Visual->SetVisibility(false);
	}
	const FTierLook& Look = TierLooks[static_cast<int32>(Tier)];
	if (Look.GlowIntensity > 0.f && !bPoke)
	{
		Glow->SetLightColor(FRRWeapon::GetTierColor(Tier));
		Glow->SetIntensity(Look.GlowIntensity);
		Glow->SetAttenuationRadius(Look.GlowRadius);
		Glow->SetVisibility(true);
	}
	// A rocket fist: the robot's forearm flies, its own exhaust behind it (the robot plays the launch)
	const bool bRocket = RocketArm != 0;
	if (bRocket)
	{
		if (UStaticMesh* Forearm = ARRRandomStarRobot::LoadForearm(RocketArm == 1))
		{
			Visual->SetStaticMesh(Forearm);
			Visual->EmptyOverrideMaterials();
			RocketPose = ARRRandomStarRobot::GetForearmFlightPose(RocketArm == 1);
			Visual->SetRelativeTransform(RocketPose);
		}
		Glow->SetLightColor(FLinearColor(1.f, 0.55f, 0.15f));
		SetActorTickEnabled(true);
	}
	// A missile: a little dart in the shot's color, steering and trailing flame as it goes (Tick)
	if (bMissile)
	{
		Visual->SetRelativeScale3D(MissileLook);
		Glow->SetLightColor(FLinearColor(1.f, 0.55f, 0.15f));
		SetActorTickEnabled(true);
	}
	if (!bPoke && !bRocket && !bMissile)
	{
		SpawnOneShot(this, Look.Muzzle == EImpact::Spark ? SparkEffect : (Look.Muzzle == EImpact::Burst ? BurstEffect : nullptr), GetActorLocation(), GetActorRotation(), Look.MuzzleScale);
	}
	if (Look.bTrail && TrailEffect && !bPoke && !bRocket && !bMissile)
	{
		Trail = UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, TrailEffect, GetActorLocation(), GetActorRotation(), FVector(Look.TrailScale));
		SetActorTickEnabled(Trail != nullptr);
	}

	if (HasAuthority())
	{
		// Don't hit the shooter on the way out, nor anyone on the shooter's team
		if (APawn* Shooter = GetInstigator())
		{
			PassThrough(Shooter);
			if (const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(Shooter))
			{
				for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
				{
					if (It->GetTeam() == Brawler->GetTeam())
					{
						PassThrough(*It);
					}
				}
				// The team's star robot too (its own fists start next to it)
				for (TActorIterator<ARRRandomStarRobot> It(GetWorld()); It; ++It)
				{
					if (It->GetTeam() == Brawler->GetTeam())
					{
						PassThrough(*It);
					}
				}
				// And the team's planes (a pilot's own shots start inside its plane)
				for (TActorIterator<ARRRandomVehicle> It(GetWorld()); It; ++It)
				{
					if (It->IsPlane() && It->GetRider() && It->GetRider()->GetTeam() == Brawler->GetTeam())
					{
						PassThrough(*It);
					}
				}
			}
		}

		// Shots pass through each other: both ways, since shots already in the air don't know about this one
		for (TActorIterator<ARRRandomProjectile> It(GetWorld()); It; ++It)
		{
			if (*It != this)
			{
				Collision->IgnoreActorWhenMoving(*It, true);
				It->Collision->IgnoreActorWhenMoving(this, true);
			}
		}

		if (MaxRange > 0.f && GetSpeed() > 0.f)
		{
			// One that reaches its range without hitting anything ends in the air: a shot fizzles out, a rocket fist blows up
			SetLifeSpan(0.f);
			FTimerHandle Handle;
			GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				RunOutOfRange();
			}), MaxRange / GetSpeed(), false);
		}
	}
	else
	{
		// Hits and lifetime are the server's call; this copy only shows where the shot is
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (bRocket)
	{
		return;
	}
	if (UMaterialInstanceDynamic* Material = Visual->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(ColorParameter, ShotColor);
	}
}

void ARRRandomProjectile::MakeRocketFist(bool bLeft, float Radius)
{
	RocketArm = bLeft ? 1 : 2;
	Collision->SetSphereRadius(Radius);
}

void ARRRandomProjectile::MakeMissile(const FVector& Target, float Radius)
{
	bMissile = true;
	MissileTarget = Target;
	Collision->SetSphereRadius(Radius);
}

void ARRRandomProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Let the trail's particles die out where they are
	if (Trail)
	{
		Trail->Deactivate();
		Trail = nullptr;
	}
	ForgetFriends();
	Super::EndPlay(EndPlayReason);
}

void ARRRandomProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (Trail)
	{
		Trail->SetWorldLocation(GetActorLocation());
	}
	// A rocket fist spins round the way it flies, its exhaust streaming out of the elbow end
	if (RocketArm != 0 && !bImpacted)
	{
		RocketSpin = FMath::Fmod(RocketSpin + RocketSpinRate * DeltaSeconds, 360.f);
		Visual->SetRelativeTransform(RocketPose * FTransform(FQuat(FVector::ForwardVector, FMath::DegreesToRadians(RocketSpin))));
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			const FVector Forward = GetActorForwardVector();
			Effects->RocketExhaust(GetActorLocation() - Forward * ARRRandomStarRobot::GetForearmLength() * 0.5f, -Forward, 1.f, DeltaSeconds);
		}
	}
	// A missile turns toward its aim, harder the longer it has flown (every machine steers its own copy the same way; the
	// server's place corrects it), and bursts once it gets there
	if (bMissile && !bImpacted)
	{
		MissileAge += DeltaSeconds;
		const FVector ToTarget = FVector(MissileTarget) - GetActorLocation();
		if (HasAuthority() && ToTarget.Size() < MissileArrive)
		{
			RunOutOfRange();
			return;
		}
		const float Speed = Movement->Velocity.Size();
		if (Speed > 0.f && !ToTarget.IsNearlyZero())
		{
			const FVector Heading = FMath::VInterpNormalRotationTo(Movement->Velocity / Speed, ToTarget.GetSafeNormal(), DeltaSeconds, MissileTurnRate + MissileTurnGrowth * MissileAge);
			Movement->Velocity = Heading * Speed;
		}
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			const FVector Forward = GetActorForwardVector();
			Effects->RocketExhaust(GetActorLocation() - Forward * 25.f, -Forward, 0.35f, DeltaSeconds, 0.3f);
		}
	}
}

float ARRRandomProjectile::GetSpeed() const
{
	return Movement->InitialSpeed;
}

float ARRRandomProjectile::GetRadius() const
{
	return Collision->GetUnscaledSphereRadius();
}

void ARRRandomProjectile::PassThroughIfTeammate(ARRRandomCharacter* Newcomer)
{
	const ARRRandomCharacter* Shooter = Cast<ARRRandomCharacter>(GetInstigator());
	if (HasAuthority() && !bImpacted && Shooter && Newcomer && Newcomer->GetTeam() == Shooter->GetTeam())
	{
		PassThrough(Newcomer);
	}
}

void ARRRandomProjectile::PassThrough(AActor* Friend)
{
	Collision->IgnoreActorWhenMoving(Friend, true);
	// Both ways: a brawler walking into its own shot just ahead of the muzzle would otherwise stop it (and bump into it)
	if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Friend->GetRootComponent()))
	{
		Body->IgnoreActorWhenMoving(this, true);
		FriendBodies.Add(Body);
	}
}

void ARRRandomProjectile::ForgetFriends()
{
	for (const TWeakObjectPtr<UPrimitiveComponent>& Body : FriendBodies)
	{
		if (Body.IsValid())
		{
			Body->IgnoreActorWhenMoving(this, false);
		}
	}
	FriendBodies.Reset();
}

void ARRRandomProjectile::RunOutOfRange()
{
	if (bImpacted)
	{
		return;
	}
	bImpacted = true;
	bOutOfRange = true;
	ImpactLocation = GetActorLocation();
	ImpactNormal = -GetActorForwardVector();
	ForceNetUpdate();
	SpawnImpactEffects(ImpactLocation, ImpactNormal);
	HideAfterImpact();
	SetLifeSpan(ImpactLingerTime);
}

void ARRRandomProjectile::SetSpeedMultiplier(float Multiplier)
{
	// Before the movement component initializes, which turns InitialSpeed into the starting velocity
	Movement->InitialSpeed *= Multiplier;
	Movement->MaxSpeed *= Multiplier;
}

void ARRRandomProjectile::SpawnImpactEffects(const FVector& Location, const FVector& Normal)
{
	if (bMissile)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->RocketBlast(Location, Normal, MissileBlast);
		}
		return;
	}
	if (RocketArm != 0)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->RocketBlast(Location, Normal, 1.5f);
		}
		return;
	}
	// Out of range in the air: the ball puffs away where it got to (a poke is never seen)
	if (bOutOfRange)
	{
		ARRRandomEffects* Effects = bPoke ? nullptr : ARRRandomEffects::Get(this);
		if (Effects)
		{
			Effects->ShotFizzle(Location, -Normal, GetActorScale3D().X, ShotColor);
		}
		return;
	}
	// Landing on anything but a brawler (which shows its own hit, ARRRandomCharacter::MulticastDamagePopup): a splash on
	// the surface, a little bigger for each tier
	if (!bHitBrawler && !bPoke)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->ShotSplat(Location, Normal, GetActorScale3D().X * (1.f + 0.2f * static_cast<int32>(Tier)), ShotColor);
		}
	}
	const FTierLook& Look = TierLooks[static_cast<int32>(Tier)];
	const FRotator AwayFromSurface = Normal.Rotation();
	switch (Look.Impact)
	{
	case EImpact::Spark:
		SpawnOneShot(this, SparkEffect, Location, AwayFromSurface, Look.ImpactScale);
		break;
	case EImpact::Burst:
		SpawnOneShot(this, BurstEffect, Location, AwayFromSurface, Look.ImpactScale);
		break;
	case EImpact::Explosion:
		SpawnOneShot(this, ExplosionEffect, Location, AwayFromSurface, Look.ImpactScale);
		SpawnOneShot(this, RingEffect, Location, FRotator::ZeroRotator, Look.ImpactScale);
		break;
	default:
		break;
	}
}

void ARRRandomProjectile::HideAfterImpact()
{
	Movement->StopMovementImmediately();
	Movement->Deactivate();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForgetFriends();
	Visual->SetVisibility(false);
	Glow->SetVisibility(false);
	if (Trail)
	{
		Trail->Deactivate();
		Trail = nullptr;
	}
	SetActorTickEnabled(false);
}

void ARRRandomProjectile::OnRep_Impact()
{
	if (bImpacted)
	{
		SpawnImpactEffects(ImpactLocation, ImpactNormal);
		HideAfterImpact();
	}
}

void ARRRandomProjectile::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Also called when something else moves into the shot: the shooter and its team (PassThrough) never stop it either way
	if (bImpacted || !HasAuthority() || (OtherActor && Collision->GetMoveIgnoreActors().Contains(OtherActor)))
	{
		return;
	}

	if (OtherActor && OtherActor != GetInstigator())
	{
		const FVector ShotDirection = GetActorForwardVector();
		float Damage = FMath::RandRange(MinDamage, MaxDamage) * DamageMultiplier;
		if (bPoke)
		{
			Damage = PokeDamage;
			// From behind: the poke flies the way the target faces. Far more than anyone's health, giants included.
			const ARRRandomCharacter* Victim = Cast<ARRRandomCharacter>(OtherActor);
			if (Victim && FVector::DotProduct(ShotDirection.GetSafeNormal2D(), Victim->GetActorForwardVector().GetSafeNormal2D()) >= FMath::Cos(FMath::DegreesToRadians(BackstabAngle)))
			{
				Damage = Victim->GetMaxHealth() * BackstabDamageShare;
				UE_LOG(LogTemp, Log, TEXT("RRRandom: %s pokes %s in the back with the foam finger."), *GetNameSafe(GetInstigator()), *Victim->GetName());
			}
		}
		UGameplayStatics::ApplyPointDamage(OtherActor, Damage, ShotDirection, Hit, GetInstigatorController(), this, UDamageType::StaticClass());

		if (OtherComp && OtherComp->IsSimulatingPhysics())
		{
			OtherComp->AddImpulseAtLocation(ShotDirection * ImpactImpulse, Hit.ImpactPoint, Hit.BoneName);
		}
	}

	if (WreckRadius > 0.f && !Cast<APawn>(OtherActor))
	{
		ARRRandomWreckage::Wreck(this, Hit.ImpactPoint, Hit.ImpactPoint, WreckRadius, false, WreckDig);
	}

	// Tell the clients where it landed, then go once they've had time to hear it
	bImpacted = true;
	bHitBrawler = Cast<ARRRandomCharacter>(OtherActor) != nullptr;
	ImpactLocation = Hit.ImpactPoint;
	ImpactNormal = Hit.ImpactNormal;
	ForceNetUpdate();
	SpawnImpactEffects(Hit.ImpactPoint, Hit.ImpactNormal);
	HideAfterImpact();
	SetLifeSpan(ImpactLingerTime);
}
