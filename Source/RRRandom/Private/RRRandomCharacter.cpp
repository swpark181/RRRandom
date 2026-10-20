#include "RRRandomCharacter.h"
#include "RRRandomAIController.h"
#include "RRRandomBodyAnimInstance.h"
#include "RRRandomCover.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomDiceTown.h"
#include "RRRandomEffects.h"
#include "RRRandomGridPath.h"
#include "Components/PoseableMeshComponent.h"
#include "RRRandomEventComponent.h"
#include "RRRandomFlightEffectsComponent.h"
#include "RRRandomGunComponent.h"
#include "RRRandomGameMode.h"
#include "RRRandomGameState.h"
#include "RRRandomKit.h"
#include "RRRandomMovementComponent.h"
#include "RRRandomOre.h"
#include "RRRandomOverheadDieComponent.h"
#include "RRRandomProjectile.h"
#include "RRRandomTigerSpirit.h"
#include "RRRandomFoxClones.h"
#include "RRRandomMoonHop.h"
#include "RRRandomAcornRain.h"
#include "RRRandomDragonFlight.h"
#include "RRRandomVehicle.h"
#include "RRRandomWingsComponent.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Below this ground speed the character counts as standing still
	constexpr float WalkAnimationThreshold = 10.f;

	// The body mesh's place on the capsule: feet at its bottom, turned to face +X
	const FVector MeshFeet(0.f, 0.f, -96.f);
	const FRotator MeshTurn(0.f, -90.f, 0.f);

	// The cartoon post process (cel bands, ink lines; /Game/PostProcess/MI_PP_Toon). Off: plain lighting.
	constexpr bool bCartoonLook = false;

	// A narrowish, far-away camera keeps the look mostly flat (2.5D: cover walls nearly parallel), with just enough
	// perspective that a flyer grows as it rises toward the camera
	constexpr float CameraFieldOfView = 30.f;
	// Looking down this steeply, a flyer's shadow (URRRandomFlightEffectsComponent) shows right under it
	constexpr float CameraPitch = -70.f;
	// In flight the body leans forward up to FlightLeanMax degrees at FlightLeanSpeed (ground speed), easing at
	// FlightLeanRate; the player's camera pulls back by up to FlightCameraPullShare of its length, easing at FlightCameraPullRate
	constexpr float FlightLeanMax = 18.f;
	constexpr float FlightLeanSpeed = 600.f;
	constexpr float FlightLeanRate = 6.f;
	constexpr float FlightCameraPullShare = 0.12f;
	constexpr float FlightCameraPullRate = 2.f;
	// Third person (a cheat): the over-the-shoulder camera's distance, its offset from the brawler (forward, right, up), and its
	// field of view; the same into the sights, and how fast the camera goes into and out of them
	constexpr float ShoulderArmLength = 340.f;
	const FVector ShoulderOffset(0.f, 75.f, 95.f);
	constexpr float ShoulderFieldOfView = 80.f;
	// (the fox's head is big: in the sights the camera stays back and well off to the right, so the brawler sits at the
	// screen's lower left edge instead of filling the middle; the narrower view still zooms)
	constexpr float SightsArmLength = 290.f;
	const FVector SightsOffset(0.f, 125.f, 95.f);
	constexpr float SightsFieldOfView = 52.f;
	// Third person in flight: the camera pulls back by this share, rises this much and widens this many degrees, so the
	// ground below and around stays in view; all of it, distance and offset, also grows with a giant's size
	constexpr float ShoulderFlightPull = 0.55f;
	constexpr float ShoulderFlightRise = 70.f;
	constexpr float ShoulderFlightFieldOfView = 8.f;
	// The glowing effects (ARRRandomEffects): the body's flash when hit (its brightness, how long it fades) and how strong
	// each gun's hit burst is and its color, in ERRHitEffect order
	// The dance line after the match: close enough to count as there, the longest the way may take (teleported after),
	// the time the dash is planned for (to be there within 1.5 s; a little under, for setting off and the corners), and
	// its fastest
	constexpr float FormationArriveRadius = 45.f;
	constexpr float VictoryGatherTime = 3.f;
	constexpr float FormationDashTime = 1.3f;
	constexpr float FormationDashMaxSpeed = 6000.f;
	// Even a short way is run like a speedster, at least this many times the walk
	constexpr float FormationDashMinShare = 2.5f;
	// Dashing: how hard it speeds up, stops and turns
	constexpr float DashAcceleration = 30000.f;
	constexpr float DashTurnRate = 3000.f;
	// Afterimages: how many at once, one every this many seconds, how long each lasts, how bright and see-through
	constexpr int32 AfterimageCount = 8;
	constexpr float AfterimageInterval = 0.035f;
	constexpr float AfterimageLife = 0.3f;
	constexpr float AfterimageOpacity = 0.55f;
	// The rabbit's super hop: launched up at this share of a normal jump's speed (about 2.5 times as high)
	constexpr float SuperHopJumpShare = 1.6f;
	const FLinearColor AfterimageColor(1.f, 0.8f, 0.25f);
	constexpr float HitFlashBrightness = 2.5f;
	constexpr float HitFlashTime = 0.18f;
	constexpr float HitBurstStrengths[] = { 1.f, 0.75f, 0.65f, 1.3f, 1.5f, 2.5f, 1.f, 2.5f };
	const FLinearColor HitBurstColors[] = { FLinearColor(1.f, 0.85f, 0.3f), FLinearColor(1.f, 0.6f, 0.25f), FLinearColor(1.f, 0.75f, 0.2f),
		FLinearColor(0.4f, 0.8f, 1.f), FLinearColor(0.7f, 0.45f, 1.f), FLinearColor(1.f, 0.45f, 0.2f), FLinearColor(0.35f, 0.6f, 1.f), FLinearColor(0.8f, 0.6f, 1.f) };
	constexpr float SightsSpeed = 14.f;
	// The 90 degree camera 1400 away that this replaces; the arm grows so the same stretch of arena fits on screen
	constexpr float FramingFieldOfView = 90.f;
	constexpr float FramingArmLength = 1400.f;

	const FName ColorParameter(TEXT("DiffuseColor"));
	// The fox material's share of DiffuseColor; the mannequin's material has no such parameter and ignores it
	const FName TintStrengthParameter(TEXT("TintStrength"));
	// The fox material's glow when hit (char-pipeline/scripts/ue_fox_hitflash.py)
	const FName HitFlashParameter(TEXT("HitFlash"));
	// Any nonzero custom stencil marks a character for the toon post process's silhouette line
	constexpr int32 CharacterStencil = 1;
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
	// Extra room the server allows a client's climb request, for movement during the request's trip
	constexpr float ClimbRequestTolerance = 150.f;
}

ARRRandomCharacter::ARRRandomCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<URRRandomMovementComponent>(ACharacter::CharacterMovementComponentName))
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
	// The view stays put when the brawler turns giant
	CameraBoom->SetUsingAbsoluteScale(true);
	CameraBoom->SetRelativeRotation(FRotator(CameraPitch, 0.f, 0.f));
	CameraBoom->TargetArmLength = FramingArmLength * FMath::Tan(FMath::DegreesToRadians(FramingFieldOfView * 0.5f))
		/ FMath::Tan(FMath::DegreesToRadians(CameraFieldOfView * 0.5f));
	CameraBoom->bDoCollisionTest = false;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;
	TopDownCamera->SetFieldOfView(CameraFieldOfView);

	FPostProcessSettings& Post = TopDownCamera->PostProcessSettings;
	// Motion blur smears the small, quick brawlers seen from far away
	Post.bOverride_MotionBlurAmount = true;
	Post.MotionBlurAmount = 0.f;
	// Cartoon look (off for now, bCartoonLook): the post process cuts lighting into flat bands and inks the edges.
	// The instance is where its values are tuned in the editor (live while playing in the editor).
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ToonPostProcess(TEXT("/Game/PostProcess/MI_PP_Toon.MI_PP_Toon"));
	if (bCartoonLook && ToonPostProcess.Succeeded())
	{
		Post.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, ToonPostProcess.Object));
		// Ambient occlusion darkens creases in a realistic way that muddies the flat colors
		Post.bOverride_AmbientOcclusionIntensity = true;
		Post.AmbientOcclusionIntensity = 0.f;
		// A little more color, for the bright anime (Genshin-like) look
		Post.bOverride_ColorSaturation = true;
		Post.ColorSaturation = FVector4(1.15f, 1.15f, 1.15f, 1.f);
	}
	else
	{
		// Plain lighting: soft contact shadows in the creases, and a touch more color for the bright toy-town look
		Post.bOverride_AmbientOcclusionIntensity = true;
		Post.AmbientOcclusionIntensity = 0.6f;
		Post.bOverride_ColorSaturation = true;
		Post.ColorSaturation = FVector4(1.08f, 1.08f, 1.08f, 1.f);
	}

	// The four brawlers (Tripo models, Mixamo rigs and animations; RRRandomSpecies.h); everyone starts as the fox, and the
	// species set before begin play (SetSpecies) swaps the body in
	SpeciesLooks = FRRSpeciesLook::GetAll();
	// Fallback when the project's fox assets are missing
	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMesh(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAnim(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> WalkAnim(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Walk_Fwd.Tutorial_Walk_Fwd"));
	// Engine material that works on skeletal meshes and has a "DiffuseColor" parameter
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintableMaterial(TEXT("/Engine/TemplateResources/M_Template_Master.M_Template_Master"));

	// Both bodies have their feet at the bottom of the capsule and face +X after this turn
	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetRelativeLocationAndRotation(MeshFeet, MeshTurn);
	const bool bFox = SpeciesLooks.IsValidIndex(RRSpecies::Fox) && SpeciesLooks[RRSpecies::Fox].IsUsable();
	if (bFox)
	{
		UseLook(SpeciesLooks[RRSpecies::Fox]);
	}
	else
	{
		if (MannequinMesh.Succeeded())
		{
			BodyMesh->SetSkeletalMeshAsset(MannequinMesh.Object);
		}
		if (TintableMaterial.Succeeded())
		{
			BodyBaseMaterial = TintableMaterial.Object;
		}
		IdleAnimation = IdleAnim.Succeeded() ? IdleAnim.Object : nullptr;
		WalkAnimation = WalkAnim.Succeeded() ? WalkAnim.Object : nullptr;
	}
	if (IdleAnimation)
	{
		// One animation at a time (PlayBodyAnimation), with the tail bent on top by code
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		BodyMesh->SetAnimInstanceClass(URRRandomBodyAnimInstance::StaticClass());
	}

	ProjectileClass = ARRRandomProjectile::StaticClass();
	RandomEvents = CreateDefaultSubobject<URRRandomEventComponent>(TEXT("RandomEvents"));
	RandomEvents->ColorParameterName = TEXT("DiffuseColor");
	DiceBuffs = CreateDefaultSubobject<URRRandomDiceBuffComponent>(TEXT("DiceBuffs"));
	OreCarry = CreateDefaultSubobject<URRRandomOreCarryComponent>(TEXT("OreCarry"));
	OreCarry->SetupAttachment(RootComponent);
	Kit = CreateDefaultSubobject<URRRandomKitComponent>(TEXT("Kit"));
	OverheadDie = CreateDefaultSubobject<URRRandomOverheadDieComponent>(TEXT("OverheadDie"));
	OverheadDie->SetupAttachment(RootComponent);
	Wings = CreateDefaultSubobject<URRRandomWingsComponent>(TEXT("Wings"));
	Wings->SetupAttachment(RootComponent);
	// Between the shoulder blades of a body without the fox's spine bone; on the fox the wings follow that bone
	Wings->SetRelativeLocation(FVector(-20.f, 0.f, 10.f));
	FlightEffects = CreateDefaultSubobject<URRRandomFlightEffectsComponent>(TEXT("FlightEffects"));
	FlightEffects->SetupAttachment(RootComponent);
	// Held between the hands every frame (hidden on the mannequin, which lacks the fox's hand bones)
	Gun = CreateDefaultSubobject<URRRandomGunComponent>(TEXT("Gun"));
	Gun->SetupAttachment(BodyMesh);
}

void ARRRandomCharacter::UseLook(const FRRSpeciesLook& Look)
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetSkeletalMeshAsset(Look.Mesh);
	BodyMesh->SetRelativeScale3D(FVector(Look.MeshScale));
	BodyBaseMaterial = Look.Material;
	IdleAnimation = Look.Idle;
	WalkAnimation = Look.Walk;
	WalkAnimationSpeed = Look.WalkSpeed;
	FireAnimation = Look.Fire;
	DeathAnimation = Look.Death;
	FlyBackAnimation = Look.FlyBack;
	VictoryAnimations = Look.Dances;
	VictoryAnimation = Look.Dances.Num() > 0 ? Look.Dances[0] : nullptr;
	DefeatAnimations = Look.Slumps;
	AimAnimations = Look.Aim;
}

void ARRRandomCharacter::SetSpecies(int32 NewSpecies)
{
	if (!HasAuthority() || !SpeciesLooks.IsValidIndex(NewSpecies) || NewSpecies == Species)
	{
		return;
	}
	Species = static_cast<uint8>(NewSpecies);
	if (HasActorBegunPlay())
	{
		ApplySpecies();
	}
}

void ARRRandomCharacter::OnRep_Species()
{
	// Before begin play, BeginPlay applies it
	if (HasActorBegunPlay())
	{
		ApplySpecies();
	}
}

UTexture2D* ARRRandomCharacter::GetPortrait() const
{
	return SpeciesLooks.IsValidIndex(ShownSpecies) ? SpeciesLooks[ShownSpecies].Portrait.Get() : nullptr;
}

void ARRRandomCharacter::ApplySpecies()
{
	if (Species == ShownSpecies || !SpeciesLooks.IsValidIndex(Species) || !SpeciesLooks[Species].IsUsable())
	{
		return;
	}
	ShownSpecies = Species;
	UseLook(SpeciesLooks[Species]);
	// Every species has its ultimate (OnUltimateDie); a clone rolls nothing
	DiceBuffs->UltimateDieChance = bDecoy ? 0.f : UltimateDieChance;
	ApplyKit();
	CurrentAnimation = nullptr;
	if (!HasActorBegunPlay())
	{
		return;
	}
	// Already playing: new tintable materials (and the random events' hold on them), the team color, a fresh pose
	USkeletalMeshComponent* BodyMesh = GetMesh();
	for (int32 Slot = 0; Slot < FMath::Max(1, BodyMesh->GetNumMaterials()); ++Slot)
	{
		BodyMesh->SetMaterial(Slot, BodyBaseMaterial);
	}
	RandomEvents->RefreshMesh(BodyMesh);
	ApplyTeamColor();
	for (UPoseableMeshComponent* Copy : Afterimages)
	{
		if (Copy)
		{
			Copy->DestroyComponent();
		}
	}
	Afterimages.Reset();
	AfterimageAges.Reset();
	if (UAnimSequence* Pose = bAlive ? IdleAnimation.Get() : DeathAnimation.Get())
	{
		PlayBodyAnimation(Pose, bAlive);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s is now the %s."), *GetName(), RRSpecies::GetAssetName(Species));
}

void ARRRandomCharacter::BeginPlay()
{
	// The species picked before begin play (on a joiner's machine: replicated with the brawler), then (before Super, so the
	// random event component picks up the tintable material when it begins play) the body's materials
	ApplySpecies();
	DiceBuffs->UltimateDieChance = bDecoy ? 0.f : UltimateDieChance;
	ApplyKit();
	UE_CLOG(!HasAuthority(), LogTemp, Log, TEXT("RRRandom: %s arrives as the %s."), *GetName(), RRSpecies::GetAssetName(ShownSpecies));
	USkeletalMeshComponent* BodyMesh = GetMesh();
	if (BodyBaseMaterial)
	{
		for (int32 Slot = 0; Slot < FMath::Max(1, BodyMesh->GetNumMaterials()); ++Slot)
		{
			BodyMesh->SetMaterial(Slot, BodyBaseMaterial);
		}
	}
	// The toon post process finds the body in custom depth to draw its silhouette line
	BodyMesh->SetRenderCustomDepth(true);
	BodyMesh->SetCustomDepthStencilValue(CharacterStencil);
	// Tyre marks (ARRRandomSkidMarks) are for the ground, not feet standing in them
	BodyMesh->SetReceivesDecals(false);
	// Shots leave from the gun in the hands (URRRandomGunComponent), so the server poses brawlers it doesn't draw too
	if (HasAuthority())
	{
		BodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
	// The gun in its hand belongs to the silhouette too
	if (Gun)
	{
		Gun->SetRenderCustomDepth(true);
		Gun->SetCustomDepthStencilValue(CharacterStencil);
	}

	Super::BeginPlay();

	// The random event component has made per-slot dynamic materials by now
	ApplyTeamColor();

	// Health, ammo, guns and dice are the server's; clients get them replicated (already, by the time they begin play)
	if (HasAuthority())
	{
		WeaponStream.GenerateNewSeed();
		DiceBuffs->OnWeaponDie.AddUObject(this, &ARRRandomCharacter::OnWeaponDie);
		DiceBuffs->OnGiantDie.AddUObject(this, &ARRRandomCharacter::OnGiantDie);
		DiceBuffs->OnUltimateDie.AddUObject(this, &ARRRandomCharacter::OnUltimateDie);
		DiceBuffs->OnFlightDie.AddUObject(this, &ARRRandomCharacter::OnFlightDie);
		DiceBuffs->OnOwnDie.AddUObject(this, &ARRRandomCharacter::OnOwnDie);
		// (a fox's clone starts with its fox's health)
		if (!bDecoy)
		{
			Health = MaxHealth;
		}
		Ammo = GetMaxAmmo();
	}

	BaseWalkSpeed = GetCharacterMovement()->MaxWalkSpeed;
	BaseCameraArmLength = CameraBoom->TargetArmLength;
	BaseMaxAcceleration = GetCharacterMovement()->MaxAcceleration;
	BaseGroundFriction = GetCharacterMovement()->GroundFriction;
	BaseBrakingDeceleration = GetCharacterMovement()->BrakingDecelerationWalking;
	BaseTurnRate = GetCharacterMovement()->RotationRate.Yaw;
	BaseJumpZVelocity = GetCharacterMovement()->JumpZVelocity;
	BaseJumpMaxHoldTime = JumpMaxHoldTime;
	BaseAirControl = GetCharacterMovement()->AirControl;
	HomeLocation = GetActorLocation();
	HomeRotation = GetActorRotation();
	MeshRelativeLocation = BodyMesh->GetRelativeLocation();
	MeshRelativeRotation = BodyMesh->GetRelativeRotation();
	MeshCollisionProfile = BodyMesh->GetCollisionProfileName();
	CapsulePawnResponse = GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn);

	// Joined a game where this brawler is lying knocked out
	if (!bAlive)
	{
		ApplyKnockedOutBody();
	}
}

void ARRRandomCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// A brawler leaving the game (a bot making room for a player) parks its vehicle for others
	GetOffVehicle();
	Super::EndPlay(EndPlayReason);
}

void ARRRandomCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARRRandomCharacter, Team);
	DOREPLIFETIME(ARRRandomCharacter, Species);
	DOREPLIFETIME_CONDITION(ARRRandomCharacter, bThirdPerson, COND_SkipOwner);
	DOREPLIFETIME(ARRRandomCharacter, Health);
	DOREPLIFETIME(ARRRandomCharacter, Weapon);
	DOREPLIFETIME(ARRRandomCharacter, Ammo);
	DOREPLIFETIME(ARRRandomCharacter, bReloading);
	DOREPLIFETIME_CONDITION(ARRRandomCharacter, ReloadRemaining, COND_OwnerOnly);
	DOREPLIFETIME(ARRRandomCharacter, bAlive);
	DOREPLIFETIME(ARRRandomCharacter, Kills);
	DOREPLIFETIME(ARRRandomCharacter, KnockdownDirection);
	DOREPLIFETIME(ARRRandomCharacter, FlingVelocity);
	DOREPLIFETIME(ARRRandomCharacter, Ultimate);
	DOREPLIFETIME(ARRRandomCharacter, bDecoy);
	DOREPLIFETIME_CONDITION(ARRRandomCharacter, RespawnRemaining, COND_OwnerOnly);
	DOREPLIFETIME(ARRRandomCharacter, bGiant);
	DOREPLIFETIME(ARRRandomCharacter, GiantRemaining);
	DOREPLIFETIME(ARRRandomCharacter, bFlying);
	DOREPLIFETIME(ARRRandomCharacter, FlightRemaining);
	DOREPLIFETIME(ARRRandomCharacter, Vehicle);
	DOREPLIFETIME(ARRRandomCharacter, bInvincible);
	DOREPLIFETIME(ARRRandomCharacter, bPassenger);
	DOREPLIFETIME(ARRRandomCharacter, ShotCount);
	DOREPLIFETIME(ARRRandomCharacter, CelebrationPick);
	DOREPLIFETIME(ARRRandomCharacter, bHasFormation);
	DOREPLIFETIME(ARRRandomCharacter, bInFormation);
	DOREPLIFETIME(ARRRandomCharacter, FormationSpot);
	DOREPLIFETIME(ARRRandomCharacter, FormationPath);
	DOREPLIFETIME(ARRRandomCharacter, FormationDashSpeed);
}

FLinearColor ARRRandomCharacter::GetTeamColor(int32 InTeam)
{
	return InTeam == 0 ? FLinearColor(0.15f, 0.45f, 1.f) : FLinearColor(1.f, 0.2f, 0.15f);
}

void ARRRandomCharacter::ApplyTeamColor()
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	for (int32 Slot = 0; Slot < BodyMesh->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(BodyMesh->GetMaterial(Slot)))
		{
			Material->SetVectorParameterValue(ColorParameter, GetTeamColor(Team));
			Material->SetScalarParameterValue(TintStrengthParameter, BodyTeamTint);
		}
	}
}

void ARRRandomCharacter::OnRep_Team()
{
	// Before BeginPlay the body has no tintable materials yet; BeginPlay colors it
	if (HasActorBegunPlay())
	{
		ApplyTeamColor();
	}
}

void ARRRandomCharacter::OnRep_Alive()
{
	if (!HasActorBegunPlay())
	{
		return;
	}
	if (bAlive)
	{
		ApplyStandingBody();
	}
	else
	{
		ApplyKnockedOutBody();
	}
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

void ARRRandomCharacter::OnGiantDie(int32 Face)
{
	StartGiant(GiantBaseDuration + Face * GiantSecondsPerPip);
}

void ARRRandomCharacter::StartGiant(float Seconds)
{
	if (!bAlive)
	{
		return;
	}
	// Another giant die while giant tops the time up rather than adding to it
	bGiant = true;
	GiantRemaining = FMath::Max(GiantRemaining, Seconds);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) turns giant for %.0f s."), *GetName(), Team, GiantRemaining);
}

void ARRRandomCharacter::OnUltimateDie(int32 Face)
{
	UWorld* World = GetWorld();
	if (!bAlive || bDecoy || Ultimate || !World)
	{
		return;
	}
	// Each species' own: the tiger's giant, the fox's clones, the rabbit's moon hops, the squirrel's acorn rain, the
	// dragon's flight
	UClass* Kind = ShownSpecies == RRSpecies::Fox ? ARRRandomFoxClones::StaticClass()
		: ShownSpecies == RRSpecies::Rabbit ? ARRRandomMoonHop::StaticClass()
		: ShownSpecies == RRSpecies::Squirrel ? ARRRandomAcornRain::StaticClass()
		: ShownSpecies == RRSpecies::Dragon ? ARRRandomDragonFlight::StaticClass()
		: ARRRandomTigerSpirit::StaticClass();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.bDeferConstruction = true;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARRRandomUltimate* NewUltimate = World->SpawnActor<ARRRandomUltimate>(Kind, GetActorTransform(), Params);
	if (!NewUltimate)
	{
		return;
	}
	NewUltimate->Setup(this, Face);
	Ultimate = NewUltimate;
	// Off any vehicle, standing still, if it holds the brawler
	if (NewUltimate->HoldsBrawler())
	{
		GetOffVehicle();
		GetCharacterMovement()->StopMovementImmediately();
	}
	NewUltimate->FinishSpawning(GetActorTransform());
}

bool ARRRandomCharacter::IsUsingUltimate() const
{
	return Ultimate && Ultimate->HoldsBrawler();
}

void ARRRandomCharacter::EndUltimate(ARRRandomUltimate* Which)
{
	if (Ultimate == Which)
	{
		Ultimate = nullptr;
	}
}

void ARRRandomCharacter::MakeDecoyOf(const ARRRandomCharacter* Source)
{
	bDecoy = true;
	Team = Source->Team;
	SetSpecies(Source->Species);
	Weapon = Source->Weapon;
	Health = Source->Health;
	bThirdPerson = false;
}

void ARRRandomCharacter::VanishDecoy()
{
	if (!HasAuthority() || !bDecoy || IsActorBeingDestroyed() || !bAlive)
	{
		return;
	}
	bAlive = false;
	MulticastDecoyPoof();
	SetLifeSpan(0.2f);
}

void ARRRandomCharacter::MulticastDecoyPoof_Implementation()
{
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->FoxPoof(GetActorLocation());
	}
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void ARRRandomCharacter::OnFlightDie(int32 Face)
{
	StartFlight(FlightBaseDuration + Face * FlightSecondsPerPip);
}

void ARRRandomCharacter::OnOwnDie(int32 Face)
{
	Kit->ApplyOwnDie(Face);
}

void ARRRandomCharacter::CheatOwnDie(int32 Face)
{
	const FRRKit& Traits = RRKit::Get(Species);
	if (Traits.GiantDieChance > 0.f)
	{
		OnGiantDie(Face);
	}
	else if (Traits.FlightDieChance > 0.f)
	{
		OnFlightDie(Face);
	}
	else
	{
		OnOwnDie(Face);
	}
}

void ARRRandomCharacter::ApplyKit()
{
	const FRRKit& Traits = RRKit::Get(Species);
	if (BaseMaxHealth <= 0.f)
	{
		BaseMaxHealth = MaxHealth;
	}
	// Health keeps its share of the new maximum (a joiner's pick arriving after its brawler began play)
	const float OldMax = MaxHealth;
	MaxHealth = BaseMaxHealth * Traits.HealthScale;
	if (HasAuthority() && HasActorBegunPlay() && OldMax > 0.f)
	{
		Health = FMath::Min(MaxHealth, Health * MaxHealth / OldMax);
	}
	JumpMaxCount = Traits.JumpCount;
	// The giant and flight dice are the tiger's and the dragon's; the others roll their own die instead. A clone rolls nothing.
	DiceBuffs->GiantDieChance = bDecoy ? 0.f : Traits.GiantDieChance;
	DiceBuffs->FlightDieChance = bDecoy ? 0.f : Traits.FlightDieChance;
	DiceBuffs->OwnDieChance = bDecoy ? 0.f : Traits.OwnDieChance;
}

bool ARRRandomCharacter::TrySuperHop()
{
	// In mid-air with a super hop stored: the second jump goes super high instead (the rabbit's own die)
	if (Kit->GetSuperHops() <= 0 || bFlying || !GetCharacterMovement()->IsFalling())
	{
		return false;
	}
	if (!HasAuthority())
	{
		ServerSuperHop();
	}
	DoSuperHop();
	return true;
}

void ARRRandomCharacter::ServerSuperHop_Implementation()
{
	if (bAlive && Kit->GetSuperHops() > 0 && GetCharacterMovement()->IsFalling())
	{
		DoSuperHop();
	}
}

void ARRRandomCharacter::DoSuperHop()
{
	if (HasAuthority() && !Kit->SpendSuperHop())
	{
		return;
	}
	// Keeps going the way it was, launched up at SuperHopJumpShare of a normal jump's speed (about 2.5 times as high); no
	// more jumps until landing
	const FVector Velocity = GetCharacterMovement()->Velocity;
	LaunchCharacter(FVector(Velocity.X, Velocity.Y, BaseJumpZVelocity * SuperHopJumpShare), true, true);
	JumpCurrentCount = JumpMaxCount;
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		Effects->SonicBoom(Feet, FVector::UpVector, RRKit::Get(Species).DieColor);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s super hops (%d left)."), *GetName(), Kit->GetSuperHops());
}

void ARRRandomCharacter::LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride)
{
	Super::LaunchCharacter(LaunchVelocity * RRKit::Get(Species).KnockbackScale, bXYOverride, bZOverride);
}

bool ARRRandomCharacter::IsNoticedByTeam(int32 InTeam) const
{
	return IsSeenByTeam(GetWorld(), InTeam, GetActorLocation()) && !Kit->IsHiddenFrom(InTeam);
}

void ARRRandomCharacter::StartFlight(float Seconds)
{
	if (!bAlive)
	{
		return;
	}
	bFlying = true;
	FlightRemaining = FMath::Max(FlightRemaining, Seconds);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) can fly for %.0f s."), *GetName(), Team, FlightRemaining);
}

void ARRRandomCharacter::UpdateFlight()
{
	// Flying: a held jump keeps rising at FlightRiseSpeed for up to FlightMaxHoldTime (the engine's variable-height jump)
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->JumpZVelocity = bFlying ? FlightRiseSpeed : BaseJumpZVelocity;
	JumpMaxHoldTime = bFlying ? FlightMaxHoldTime : BaseJumpMaxHoldTime;

	// The glide lasts until landing even if the power runs out mid-air, but not once falling off the arena. The dragon's
	// kit glides too while jump is held on the way down (not on the way up: the glide's braking would cut its jump short).
	if (URRRandomMovementComponent* RandomMovement = Cast<URRRandomMovementComponent>(Movement))
	{
		const bool bAboveArena = Movement->IsFalling() && GetActorLocation().Z > HomeLocation.Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		bFlightGlide = bFlying || (bFlightGlide && bAboveArena);
		const bool bHeldGlide = RRKit::Get(Species).bGlide && bAlive && bJumpHeld && bAboveArena && Movement->Velocity.Z <= 0.f;
		RandomMovement->bGliding = bFlightGlide || bHeldGlide;
		Movement->AirControl = RandomMovement->bGliding ? FlightAirControl : BaseAirControl;

		// Feet at most FlightMaxHeightInBodies normal body heights above the arena floor (where the brawler started)
		const UCapsuleComponent* Capsule = GetCapsuleComponent();
		const float FloorZ = HomeLocation.Z - Capsule->GetUnscaledCapsuleHalfHeight();
		RandomMovement->FlightCeilingZ = FloorZ + FlightMaxHeightInBodies * Capsule->GetUnscaledCapsuleHalfHeight() * 2.f + Capsule->GetScaledCapsuleHalfHeight();
	}
}

void ARRRandomCharacter::UpdateGiantSize(float DeltaSeconds)
{
	const float Current = GetActorScale3D().Z;
	const float Target = bGiant ? GiantScale : 1.f;
	if (FMath::IsNearlyEqual(Current, Target))
	{
		return;
	}
	const float GrowSpeed = GiantGrowTime > 0.f ? FMath::Abs(GiantScale - 1.f) / GiantGrowTime : UE_BIG_NUMBER;
	SetBodyScale(FMath::FInterpConstantTo(Current, Target, DeltaSeconds, GrowSpeed));
}

void ARRRandomCharacter::SetBodyScale(float NewScale)
{
	// The capsule grows around its center; lift it by the change so the feet stay on the floor.
	// Only where the movement is decided (server, or the controlling client); others get the position replicated.
	if (HasAuthority() || IsLocallyControlled())
	{
		const float HalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		AddActorWorldOffset(FVector(0.f, 0.f, HalfHeight * (NewScale - GetActorScale3D().Z)));
	}
	SetActorScale3D(FVector(NewScale));
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
	UpdateHitFlash(DeltaSeconds);
	// Out of sight while its ultimate stands in for it (the tiger's giant, the rabbit's hops)
	const bool bStandIn = Ultimate && Ultimate->HidesBrawler();
	if (GetMesh()->IsVisible() == bStandIn)
	{
		GetMesh()->SetVisibility(!bStandIn);
	}
	if (!bAlive)
	{
		UpdateFling(DeltaSeconds);
		// Powers lost on a knockout end without a show
		bWasGiant = bGiant;
		bWasFlying = bFlying;
		// Nobody gets back up once the match is over
		if (HasAuthority() && !ARRRandomGameState::IsMatchOver(this))
		{
			RespawnRemaining -= DeltaSeconds;
			if (RespawnRemaining <= 0.f)
			{
				Respawn();
			}
		}
		return;
	}
	UpdatePowerEffects(DeltaSeconds);
	UpdateFormation(DeltaSeconds);
	UpdateDashEffects(DeltaSeconds);

	if (HasAuthority())
	{
		// Fell off the arena (jumping past its edge): counts as a knockout
		if (GetActorLocation().Z < HomeLocation.Z - FallOutDepth)
		{
			// Invincible: back to the start instead
			if (bInvincible)
			{
				GetOffVehicle();
				TeleportTo(HomeLocation, HomeRotation);
				GetCharacterMovement()->StopMovementImmediately();
				return;
			}
			KnockOut(FVector::UpVector, nullptr, true);
			return;
		}

		UpdateAmmoAndHealth(DeltaSeconds);
		if (bGiant)
		{
			GiantRemaining -= DeltaSeconds;
			if (GiantRemaining <= 0.f)
			{
				bGiant = false;
				GiantRemaining = 0.f;
			}
		}
		if (bFlying)
		{
			FlightRemaining -= DeltaSeconds;
			if (FlightRemaining <= 0.f)
			{
				bFlying = false;
				FlightRemaining = 0.f;
			}
		}
		if (ClientTrustRemaining > 0.f)
		{
			ClientTrustRemaining -= DeltaSeconds;
			if (ClientTrustRemaining <= 0.f)
			{
				SetTrustClientMovement(false);
			}
		}
	}

	if (bDashing)
	{
		UpdateDash();
	}
	if (bClimbing)
	{
		UpdateClimb(DeltaSeconds);
	}
	else if (!bFlying && IsLocallyControlled() && GetCharacterMovement()->IsFalling() && GetWorld()->GetTimeSeconds() - JumpPressedTime < ClimbJumpWindow)
	{
		// Jumped toward a wall: grab its top once in reach (a flyer just flies over it)
		TryClimb();
	}
	UpdateFlight();
	UpdateGiantSize(DeltaSeconds);
	UpdateLedgeWalking();
	// Taking off with the flight power, or turning giant (too big for any seat), leaves the vehicle behind
	if (HasAuthority() && Vehicle && (bFlying || bGiant))
	{
		GetOffVehicle();
	}
	UpdateVehicleHandling();
	UpdateWalkSpeed();
	UpdateLocomotionAnimation();
	UpdateCamera(DeltaSeconds);
}

void ARRRandomCharacter::SetJumpHeld(bool bHeld)
{
	if (bJumpHeld == bHeld)
	{
		return;
	}
	bJumpHeld = bHeld;
	// Only a gliding kit needs the server to know
	if (!HasAuthority() && IsLocallyControlled() && RRKit::Get(Species).bGlide)
	{
		ServerSetJumpHeld(bHeld);
	}
}

void ARRRandomCharacter::ServerSetJumpHeld_Implementation(bool bHeld)
{
	bJumpHeld = bHeld;
}

void ARRRandomCharacter::Jump()
{
	SetJumpHeld(true);
	// No jumping (or climbing) on a vehicle, or while the tiger's spirit is out
	if (!bAlive || bClimbing || Vehicle || IsUsingUltimate())
	{
		return;
	}
	// Flying, a quick second press dashes for as long as it is held (StopJumping ends it)
	const float Now = GetWorld()->GetTimeSeconds();
	const bool bDoubleTap = bFlying && Now - JumpPressedTime <= DashDoubleTapTime;
	JumpPressedTime = Now;
	if (bDoubleTap && TryDash())
	{
		JumpPressedTime = -UE_BIG_NUMBER;
		return;
	}
	// With the flight power the jump is the lift-off; holding it keeps rising. In mid-air a stored super hop (the rabbit's)
	// takes the place of the second jump.
	if (bFlying || !TryClimb())
	{
		if (!TrySuperHop())
		{
			Super::Jump();
		}
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

	// Only the controlling machine decides to climb; a remote player's server hears about it through ServerStartClimb
	if (!IsLocallyControlled())
	{
		return false;
	}

	// Toward where the brawler is trying to go, or where it faces when standing still
	FVector Forward = GetLastMovementInputVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		Forward = GetActorForwardVector().GetSafeNormal2D();
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomClimb), false, this);
	const FCollisionObjectQueryParams StaticOnly(ECC_WorldStatic);

	// A wall right in front, at chest height, or else a low one at the knees (a window sill, which a giant's chest is over)
	FHitResult Wall;
	const FVector Knees(Location.X, Location.Y, FeetZ + MinClimbHeight + 25.f);
	const bool bChest = World->LineTraceSingleByObjectType(Wall, Location, Location + Forward * (Radius + ClimbReach), StaticOnly, Params);
	if ((!bChest && !World->LineTraceSingleByObjectType(Wall, Knees, Knees + Forward * (Radius + ClimbReach), StaticOnly, Params))
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

	const FVector WallFacing = (-Wall.ImpactNormal).GetSafeNormal2D();
	StartClimb(Destination, WallFacing);
	if (!HasAuthority())
	{
		ServerStartClimb(Destination, WallFacing);
	}
	return true;
}

void ARRRandomCharacter::StartClimb(const FVector& Destination, const FVector& WallFacing)
{
	bClimbing = true;
	ClimbElapsed = 0.f;
	ClimbStart = GetActorLocation();
	ClimbTarget = Destination;
	SetActorRotation(WallFacing.Rotation());
	// Movement off while the climb moves the capsule; input does nothing meanwhile.
	// The server leaves a remote climber's movement alone: that client moves itself and the server follows its position.
	if (IsLocallyControlled())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}
}

void ARRRandomCharacter::ServerStartClimb_Implementation(FVector_NetQuantize Destination, FVector_NetQuantizeNormal WallFacing)
{
	// Only somewhere a climb from here could end; anything else is ignored
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	const float MaxReach = Capsule->GetScaledCapsuleRadius() * 2.f + ClimbReach + ClimbLedgeInset + ClimbRequestTolerance;
	const FVector Location = GetActorLocation();
	if (!bAlive || bClimbing || FVector::Dist2D(Location, Destination) > MaxReach || Destination.Z - Location.Z > MaxClimbHeight + ClimbRequestTolerance)
	{
		return;
	}
	StartClimb(Destination, WallFacing);
	SetTrustClientMovement(true);
}

bool ARRRandomCharacter::TryDash()
{
	// Like climbing, only the controlling machine decides; a remote player's server hears about it through ServerStartDash
	if (!IsLocallyControlled() || !bAlive || !bFlying || bClimbing || bDashing || bDashUsed)
	{
		return false;
	}

	// Toward where the brawler is trying to go, or where it faces when not steering
	FVector Direction = GetLastMovementInputVector().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector().GetSafeNormal2D();
	}
	StartDash(Direction);
	if (!HasAuthority())
	{
		ServerStartDash(Direction);
	}
	return true;
}

void ARRRandomCharacter::ServerStartDash_Implementation(FVector_NetQuantizeNormal Direction)
{
	if (!bAlive || !bFlying || bClimbing || bDashing || bDashUsed)
	{
		return;
	}
	StartDash(FVector(Direction).GetSafeNormal2D());
	SetTrustClientMovement(true);
}

void ARRRandomCharacter::ServerEndDash_Implementation()
{
	EndDash();
}

void ARRRandomCharacter::StartDash(const FVector& Direction)
{
	bDashing = true;
	bDashUsed = true;
	DashDirection = Direction.IsNearlyZero() ? GetActorForwardVector().GetSafeNormal2D() : Direction;
	SetActorRotation(DashDirection.Rotation());
	// The server leaves a remote dasher's movement alone: that client moves itself and the server follows its position
	if (IsLocallyControlled())
	{
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		if (Movement->IsMovingOnGround())
		{
			Movement->SetMovementMode(MOVE_Falling);
		}
		Movement->Velocity = DashDirection * DashSpeed;
	}
}

void ARRRandomCharacter::UpdateDash()
{
	// The flight power running out ends the dash too
	if (!bFlying)
	{
		EndDash();
		return;
	}
	// Level and at full speed while the key is held, turning toward the movement keys
	if (IsLocallyControlled())
	{
		const FVector Steer = GetLastMovementInputVector().GetSafeNormal2D();
		if (!Steer.IsNearlyZero())
		{
			DashDirection = Steer;
		}
		GetCharacterMovement()->Velocity = DashDirection * DashSpeed;
	}
}

void ARRRandomCharacter::EndDash()
{
	if (!bDashing)
	{
		return;
	}
	bDashing = false;
	if (IsLocallyControlled())
	{
		// Back to air speed, then gliding as before
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		Movement->Velocity = DashDirection * FMath::Min<float>(DashSpeed, Movement->MaxWalkSpeed);
		if (!HasAuthority())
		{
			ServerEndDash();
		}
	}
	else if (HasAuthority())
	{
		// Keep trusting the client a little longer, until its first moves after the dash have arrived
		ClientTrustRemaining = FMath::Max(ClimbTrustMargin, KINDA_SMALL_NUMBER);
	}
}

void ARRRandomCharacter::StopJumping()
{
	Super::StopJumping();
	SetJumpHeld(false);
	// Letting go of the second press ends the dash
	if (bDashing && IsLocallyControlled())
	{
		EndDash();
	}
}

void ARRRandomCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	// Back on the ground: the next flight gets its dash
	bDashUsed = false;
}

void ARRRandomCharacter::SetTrustClientMovement(bool bTrust)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bIgnoreClientMovementErrorChecksAndCorrection = bTrust;
	Movement->bServerAcceptClientAuthoritativePosition = bTrust;
	if (!bTrust)
	{
		ClientTrustRemaining = 0.f;
		// The client's climbing mode (none) may have been copied over; never leave a standing brawler stuck in it
		if (bAlive && Movement->MovementMode == MOVE_None)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
}

void ARRRandomCharacter::UpdateClimb(float DeltaSeconds)
{
	ClimbElapsed += DeltaSeconds;
	const float Alpha = ClimbDuration > 0.f ? FMath::Clamp(ClimbElapsed / ClimbDuration, 0.f, 1.f) : 1.f;

	// Only where the brawler is controlled; the server just times a remote client's climb
	if (IsLocallyControlled())
	{
		// Straight up beside the wall first, then over the edge, so the capsule never cuts through the corner
		const float UpAlpha = FMath::Clamp(Alpha / ClimbUpShare, 0.f, 1.f);
		const float OverAlpha = FMath::Clamp((Alpha - ClimbUpShare) / (1.f - ClimbUpShare), 0.f, 1.f);
		const FVector Flat = FMath::Lerp(FVector(ClimbStart.X, ClimbStart.Y, 0.f), FVector(ClimbTarget.X, ClimbTarget.Y, 0.f), FMath::InterpEaseInOut(0.f, 1.f, OverAlpha, 2.f));
		const float Z = FMath::InterpEaseOut(ClimbStart.Z, ClimbTarget.Z, UpAlpha, 2.f);
		SetActorLocation(FVector(Flat.X, Flat.Y, Z));
	}

	if (Alpha >= 1.f)
	{
		bClimbing = false;
		if (IsLocallyControlled())
		{
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		else if (HasAuthority())
		{
			// Keep trusting the client a little longer, until its first moves on top have arrived
			ClientTrustRemaining = FMath::Max(ClimbTrustMargin, KINDA_SMALL_NUMBER);
		}
	}
}

void ARRRandomCharacter::UpdateLedgeWalking()
{
	// Standing on cover: walking off its edge drops to the floor. On the floor itself: the arena edge stops everyone. In
	// Dice Town anywhere: off stairs, landings, roofs and window sills (a hidden wall round its rim stops the fall off it)
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const AActor* Floor = Movement->CurrentFloor.HitResult.GetActor();
	GetCharacterMovement()->bCanWalkOffLedges = Movement->IsMovingOnGround() && Floor && (Floor->IsA<ARRRandomCover>() || Floor->IsA<ARRRandomDiceTown>());
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
	DamagePopups.RemoveAll([](const FRRDamagePopup& Popup) { return Popup.Age >= Popup.Lifetime; });
}

void ARRRandomCharacter::UpdateWalkSpeed()
{
	const float VehicleSpeed = Vehicle ? Vehicle->GetHandling().SpeedMultiplier : 1.f;
	// On foot the species' kit counts too (the rabbit quicker, the tiger slower); a vehicle goes at its own speed
	const float KitSpeed = Vehicle ? 1.f : RRKit::Get(Species).MoveSpeedScale;
	GetCharacterMovement()->MaxWalkSpeed = IsUsingUltimate() ? 0.f
		: BaseWalkSpeed * VehicleSpeed * KitSpeed * RandomEvents->GetSpeedMultiplier() * DiceBuffs->GetMultiplier(ERRDiceBuffStat::MoveSpeed);
	if (IsDashingToFormation())
	{
		GetCharacterMovement()->MaxWalkSpeed = FormationDashSpeed;
	}
}

void ARRRandomCharacter::UpdateVehicleHandling()
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Vehicle)
	{
		const FRRVehicleHandling& Handling = Vehicle->GetHandling();
		Movement->MaxAcceleration = Handling.Acceleration;
		Movement->GroundFriction = Handling.Grip;
		Movement->BrakingDecelerationWalking = Handling.Braking;
		Movement->RotationRate.Yaw = Handling.TurnRate;
	}
	else if (IsDashingToFormation())
	{
		Movement->MaxAcceleration = DashAcceleration;
		Movement->GroundFriction = BaseGroundFriction * 4.f;
		Movement->BrakingDecelerationWalking = DashAcceleration;
		Movement->RotationRate.Yaw = DashTurnRate;
	}
	else
	{
		Movement->MaxAcceleration = BaseMaxAcceleration;
		Movement->GroundFriction = BaseGroundFriction;
		Movement->BrakingDecelerationWalking = BaseBrakingDeceleration;
		Movement->RotationRate.Yaw = BaseTurnRate;
	}
	// Shooting on foot, the brawler keeps facing where it shoots and strafes (the aiming runs go every way);
	// otherwise it turns to where it goes
	Movement->bOrientRotationToMovement = Vehicle || IsDashingToFormation() || (!bThirdPerson && !WantsAimPose());
	ApplyThirdPersonRotation();
	// Up on the deck or in the seat (the body's own offset otherwise; knocked out it is a ragdoll and left alone)
	if (bAlive && !bClimbing)
	{
		const float Seat = Vehicle ? Vehicle->GetHandling().SeatHeight : 0.f;
		GetMesh()->SetRelativeLocation(MeshRelativeLocation + FVector(0.f, 0.f, Seat));
	}
}

ARRRandomVehicle* ARRRandomCharacter::FindVehicleInReach() const
{
	ARRRandomVehicle* Nearest = nullptr;
	// A clone stays on its feet
	if (bDecoy)
	{
		return nullptr;
	}
	float NearestDistance = ARRRandomVehicle::MountRange * GetActorScale3D().Z;
	for (TActorIterator<ARRRandomVehicle> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
		if (It->HasRoom() && Distance <= NearestDistance && FMath::Abs(It->GetActorLocation().Z - GetActorLocation().Z) < GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f)
		{
			Nearest = *It;
			NearestDistance = Distance;
		}
	}
	return Nearest;
}

void ARRRandomCharacter::ToggleRide()
{
	if (!HasAuthority())
	{
		ServerToggleRide();
		return;
	}
	if (Vehicle)
	{
		GetOffVehicle();
		return;
	}
	if (!bAlive || bClimbing || bFlying || bGiant || ARRRandomGameState::IsPlayLocked(this))
	{
		return;
	}
	if (ARRRandomVehicle* Ride = FindVehicleInReach())
	{
		const int32 Seat = Ride->Mount(this);
		if (Seat < 0)
		{
			return;
		}
		Vehicle = Ride;
		bPassenger = Seat == 1;
		ApplyRide();
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s gets on a %s%s."), *GetName(), Ride->GetHandling().Name, bPassenger ? TEXT(" (rear seat)") : TEXT(""));
	}
}

void ARRRandomCharacter::OnRep_Ride()
{
	ApplyRide();
}

void ARRRandomCharacter::ApplyRide()
{
	// A passenger is carried: no movement of its own, attached to the vehicle's rear seat; every machine does this
	const bool bShouldSit = Vehicle && bPassenger;
	if (bShouldSit == bSeatedAsPassenger)
	{
		return;
	}
	bSeatedAsPassenger = bShouldSit;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (bShouldSit)
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		// The driver's capsule sweeps right past the one behind it
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		AttachToComponent(Vehicle->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(Vehicle->GetRearSeatLocation(Capsule->GetUnscaledCapsuleHalfHeight()));
		SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	else
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		if (bAlive)
		{
			Movement->SetMovementMode(MOVE_Walking);
		}
	}
}

void ARRRandomCharacter::ServerToggleRide_Implementation()
{
	ToggleRide();
}

void ARRRandomCharacter::GetOffVehicle()
{
	if (!HasAuthority() || !Vehicle)
	{
		return;
	}
	ARRRandomVehicle* Parked = Vehicle;
	if (bPassenger)
	{
		Parked->RemovePassenger(this);
		Vehicle = nullptr;
		bPassenger = false;
		ApplyRide();
	}
	else
	{
		// The driver getting off lets the one behind out first
		if (ARRRandomCharacter* Behind = Parked->GetPassenger())
		{
			Behind->GetOffVehicle();
		}
		Parked->Dismount();
		Vehicle = nullptr;
	}
	// Step off beside it (right, or left if that is blocked); a knocked-out rider falls where it is
	if (bAlive)
	{
		const float Step = Parked->GetHalfWidth() + GetCapsuleComponent()->GetScaledCapsuleRadius() + 10.f;
		for (const float Side : { 1.f, -1.f })
		{
			if (TeleportTo(GetActorLocation() + GetActorRightVector() * Step * Side, GetActorRotation()))
			{
				break;
			}
		}
	}
}

void ARRRandomCharacter::UpdateLocomotionAnimation()
{
	// On a vehicle the legs stand still; it does the moving
	const float Speed = Vehicle ? 0.f : GetVelocity().Size2D();
	// Shooting, the anim instance lays the aiming set over this; without it the standing fire animation stands in
	const bool bShooting = FireAnimation && IsAiming() && !AimAnimations.IsComplete();
	UAnimSequence* Wanted = Speed > WalkAnimationThreshold ? WalkAnimation : (bShooting ? FireAnimation : IdleAnimation);

	if (IsCelebrating())
	{
		Wanted = VictoryAnimations.Num() > 0 ? VictoryAnimations[CelebrationPick % VictoryAnimations.Num()].Get() : VictoryAnimation.Get();
	}
	else if (IsDespairing())
	{
		Wanted = DefeatAnimations[CelebrationPick % DefeatAnimations.Num()];
	}
	if (Wanted && Wanted != CurrentAnimation)
	{
		PlayBodyAnimation(Wanted, true);
	}
	if (CurrentAnimation == WalkAnimation)
	{
		GetMesh()->SetPlayRate(FMath::Clamp(Speed / WalkAnimationSpeed, 0.5f, 3.f));
	}

	// Standing and shooting: turn the body into the bladed stance so the rifle in its hands points down the line of fire.
	// The aiming set does the same standing, and on the move keeps the legs on the way the brawler goes (the anim
	// instance twists the spine for the rest).
	const URRRandomBodyAnimInstance* Body = Cast<URRRandomBodyAnimInstance>(GetMesh()->GetAnimInstance());
	const float Stance = Body && AimAnimations.IsComplete() ? Body->GetAimWeight() * (1.f - Body->GetAimMoveWeight())
		: (CurrentAnimation == FireAnimation ? 1.f : 0.f);
	// Dancing after the match: turn the body to face the results camera, whichever way the brawler was facing
	const bool bAfterMatch = CurrentAnimation && (VictoryAnimations.Contains(CurrentAnimation) || DefeatAnimations.Contains(CurrentAnimation));
	const float Turn = bAfterMatch && CelebrationViewPoint.IsSet()
		? FRotator::NormalizeAxis((CelebrationViewPoint.GetValue() - GetActorLocation()).Rotation().Yaw - GetActorRotation().Yaw) : 0.f;
	// Flying (or gliding down after it): the body leans forward into the way it goes, more the faster
	const float DeltaSeconds = GetWorld()->GetDeltaSeconds();
	const URRRandomMovementComponent* RandomMovement = Cast<URRRandomMovementComponent>(GetCharacterMovement());
	const bool bInFlight = !Vehicle && GetCharacterMovement()->IsFalling() && (bFlying || (RandomMovement && RandomMovement->bGliding));
	const float Lean = bInFlight ? FlightLeanMax * FMath::Clamp(Speed / FlightLeanSpeed, 0.f, 1.f) : 0.f;
	if (FireStanceAlpha != Stance || CelebrationTurn != Turn || FlightLean != Lean)
	{
		FireStanceAlpha = FMath::FInterpConstantTo(FireStanceAlpha, Stance, DeltaSeconds, FireStanceSpeed);
		CelebrationTurn = FMath::FInterpConstantTo(CelebrationTurn, Turn, DeltaSeconds, CelebrationTurnRate);
		FlightLean = FMath::Abs(FlightLean - Lean) < 0.05f ? Lean : FMath::FInterpTo(FlightLean, Lean, DeltaSeconds, FlightLeanRate);
		const FQuat Facing = (MeshRelativeRotation + FRotator(0.f, FireStanceYaw * FireStanceAlpha + CelebrationTurn, 0.f)).Quaternion();
		// About the brawler's right (+Y), which tips its front (+X) down and its head forward
		GetMesh()->SetRelativeRotation(FQuat(FVector::RightVector, FMath::DegreesToRadians(FlightLean)) * Facing);
	}
}

void ARRRandomCharacter::UpdateCamera(float DeltaSeconds)
{
	if (!IsLocallyControlled() || BaseCameraArmLength <= 0.f)
	{
		return;
	}
	ThirdPersonBlend = bThirdPerson ? 1.f : 0.f;
	SightsBlend = FMath::FInterpTo(SightsBlend, IsAimingDownSights() ? 1.f : 0.f, DeltaSeconds, SightsSpeed);
	// In flight the player's own camera pulls back, so the climb shows and there is more to see from up there
	const URRRandomMovementComponent* RandomMovement = Cast<URRRandomMovementComponent>(GetCharacterMovement());
	const bool bInFlight = !Vehicle && GetCharacterMovement()->IsFalling() && (bFlying || (RandomMovement && RandomMovement->bGliding));
	FlightCameraPull = FMath::FInterpTo(FlightCameraPull, bInFlight ? 1.f : 0.f, DeltaSeconds, FlightCameraPullRate);
	if (bThirdPerson)
	{
		// Over the right shoulder, turned by the mouse (the controller's rotation), kept out of walls; into the sights:
		// further off to the side, a narrower view. Distance and offset grow with the body (a giant is twice the size), and
		// in flight the camera pulls back and up
		const float Body = GetActorScale3D().Z;
		CameraBoom->bUsePawnControlRotation = true;
		CameraBoom->bDoCollisionTest = true;
		CameraBoom->TargetArmLength = FMath::Lerp(ShoulderArmLength, SightsArmLength, SightsBlend) * Body * (1.f + ShoulderFlightPull * FlightCameraPull);
		CameraBoom->SocketOffset = FMath::Lerp(ShoulderOffset, SightsOffset, SightsBlend) * Body + FVector(0.f, 0.f, ShoulderFlightRise * FlightCameraPull);
		TopDownCamera->SetFieldOfView(FMath::Lerp(ShoulderFieldOfView, SightsFieldOfView, SightsBlend) + ShoulderFlightFieldOfView * FlightCameraPull);
		return;
	}

	// Top-down: straight on from above
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->SetWorldRotation(FRotator(CameraPitch, 0.f, 0.f));
	CameraBoom->SocketOffset = FVector::ZeroVector;
	CameraBoom->TargetArmLength = BaseCameraArmLength * (1.f + FlightCameraPullShare * FlightCameraPull);
	TopDownCamera->SetFieldOfView(CameraFieldOfView);
}

void ARRRandomCharacter::SetThirdPerson(bool bOn)
{
	bThirdPerson = bOn;
	bAimingDownSights = false;
	ApplyThirdPersonRotation();
	if (!HasAuthority())
	{
		ServerSetThirdPerson(bOn);
	}
}

void ARRRandomCharacter::ServerSetThirdPerson_Implementation(bool bOn)
{
	bThirdPerson = bOn;
	ApplyThirdPersonRotation();
}

void ARRRandomCharacter::ApplyThirdPersonRotation()
{
	// In third person the brawler faces where the camera looks (strafing); on a vehicle it still turns the way it drives
	bUseControllerRotationYaw = bThirdPerson && !Vehicle && !IsDashingToFormation();
}

bool ARRRandomCharacter::IsDespairing() const
{
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	return DefeatAnimations.Num() > 0 && bAlive && State && State->IsMatchOver() && Team != State->GetWinningTeam() && State->GetMVP() != this;
}

bool ARRRandomCharacter::IsCelebrating() const
{
	// The match is over: the winners (and the MVP, whichever team) dance
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	// The winners in the dance line once there; without one, the winners and the MVP where they stand
	return VictoryAnimation && bAlive && State && State->IsMatchOver()
		&& (bHasFormation ? bInFormation : (Team == State->GetWinningTeam() || State->GetMVP() == this));
}

void ARRRandomCharacter::PlayBodyAnimation(UAnimSequence* Animation, bool bLoop, float PlayRate)
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	// The body instance is a single-node one; the component's PlayAnimation would swap it for a plain one (no tail)
	if (UAnimSingleNodeInstance* Instance = BodyMesh->GetSingleNodeInstance())
	{
		Instance->SetAnimationAsset(Animation, bLoop, PlayRate);
		Instance->SetPlaying(true);
	}
	else
	{
		BodyMesh->PlayAnimation(Animation, bLoop);
	}
	// The walk's speed-matched rate would otherwise carry over to the next animation
	BodyMesh->SetPlayRate(PlayRate);
	CurrentAnimation = Animation;
}

FVector ARRRandomCharacter::GetEyeLocation() const
{
	// Half the brawler's height (the capsule's middle): what stands taller than that blocks its view. A giant, or a brawler
	// standing on cover or flying, sees over more.
	return GetActorLocation();
}

float ARRRandomCharacter::GetSightDistance(const UWorld* World, const FVector& Eye, const FVector& Direction, float MaxDistance)
{
	// Level at eye height: anything in the way there is taller than half the one looking (cover is, low hills aren't)
	FHitResult Hit;
	const FVector Flat = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
	if (World->LineTraceSingleByObjectType(Hit, Eye, Eye + Flat * MaxDistance, FCollisionObjectQueryParams(ECC_WorldStatic),
		FCollisionQueryParams(SCENE_QUERY_STAT(RRRandomSight), false)))
	{
		return Hit.Distance;
	}
	return MaxDistance;
}

bool ARRRandomCharacter::IsSeenByTeam(const UWorld* World, int32 InTeam, const FVector& Location)
{
	const bool bThirdPersonGame = ARRRandomGameState::IsThirdPersonMode(World);
	for (TActorIterator<ARRRandomCharacter> It(World); It; ++It)
	{
		if (It->GetTeam() != InTeam || !It->IsAlive())
		{
			continue;
		}
		const FVector Eye = It->GetEyeLocation();
		const float Distance = FVector::Dist2D(Eye, Location);
		const float Radius = bThirdPersonGame ? It->ThirdPersonSightRange : It->GetVisionRadius();
		if (Distance <= Radius && GetSightDistance(World, Eye, Location - Eye, Distance) >= Distance)
		{
			return true;
		}
	}
	return false;
}

bool ARRRandomCharacter::IsAiming() const
{
	return GetWorld()->GetTimeSeconds() - LastShotTime < FireAnimationHold;
}

bool ARRRandomCharacter::WantsAimPose() const
{
	// The foam finger has its own pose (URRRandomGunComponent); a climber's hands are on the wall
	return AimAnimations.IsComplete() && bAlive && !bClimbing && !Weapon.bFoamFinger && IsAiming() && !IsCelebrating();
}

float ARRRandomCharacter::GetAimPoseWeight() const
{
	const URRRandomBodyAnimInstance* Body = Cast<URRRandomBodyAnimInstance>(GetMesh()->GetAnimInstance());
	return Body ? Body->GetAimWeight() : 0.f;
}

void ARRRandomCharacter::OnRep_ShotCount()
{
	LastShotTime = GetWorld()->GetTimeSeconds();
}

float ARRRandomCharacter::GetFireInterval() const
{
	return FireInterval * Weapon.FireIntervalMultiplier / DiceBuffs->GetMultiplier(ERRDiceBuffStat::AttackSpeed);
}

FVector ARRRandomCharacter::FaceToward(const FVector& TargetLocation)
{
	// The body only turns; the shot itself may tilt toward a target on another level
	FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = GetActorForwardVector().GetSafeNormal2D();
	}
	SetActorRotation(Direction.Rotation());
	return Direction;
}

bool ARRRandomCharacter::FireAt(const FVector& TargetLocation)
{
	if (HasAuthority())
	{
		return ShootAt(TargetLocation, 0.f);
	}

	// Client: run the cooldown here so holding the button sends one request per shot, and let the server shoot.
	// Ammo comes back replicated; the server reloads an empty magazine by itself.
	const float Now = GetWorld()->GetTimeSeconds();
	if (!bAlive || !ProjectileClass || bReloading || bClimbing || Ammo <= 0 || Now < NextFireTime)
	{
		return false;
	}
	NextFireTime = Now + GetFireInterval();
	LastShotTime = Now;
	FaceToward(TargetLocation);
	ServerFireAt(TargetLocation);
	return true;
}

void ARRRandomCharacter::ServerFireAt_Implementation(FVector_NetQuantize TargetLocation)
{
	ShootAt(TargetLocation, FireTimeSlack);
}

bool ARRRandomCharacter::ShootAt(const FVector& TargetLocation, float CooldownSlack)
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	if (!bAlive || !ProjectileClass || bReloading || bClimbing || IsUsingUltimate() || Now + CooldownSlack < NextFireTime || ARRRandomGameState::IsPlayLocked(this))
	{
		return false;
	}
	if (Ammo <= 0)
	{
		StartReload();
		return false;
	}
	NextFireTime = Now + GetFireInterval();
	--Ammo;
	LastCombatTime = Now;
	// Shooting gives an invisible brawler away (and that shot gets the fox's ambush bonus)
	if (HasAuthority())
	{
		Kit->OnShot();
	}
	LastShotTime = Now;
	// Other machines play the firing animation when this changes
	++ShotCount;
	// Like an FPS, an empty magazine reloads by itself
	if (Ammo <= 0)
	{
		StartReload();
	}

	const FVector Direction = FaceToward(TargetLocation);
	const FRotator Aim = Direction.Rotation();

	// Out of the gun's muzzle as it sits in the aiming pose (URRRandomGunComponent keeps track of it)
	const FVector Muzzle = GetActorLocation() + Aim.RotateVector(Gun ? Gun->GetAimMuzzleOffset() : MuzzleOffset);
	FVector ShotDirection;
	float PitchLimit = MaxShotPitch;
	if (bThirdPerson && IsPlayerControlled())
	{
		// Third person: the target is where the crosshair points (ARRRandomPlayerController::GetCrosshairTarget), so the shot
		// flies from the muzzle straight to it, up or down, as aimed
		ShotDirection = TargetLocation - Muzzle;
		PitchLimit = ThirdPersonMaxShotPitch;
		if (FVector::DotProduct(ShotDirection, Direction) <= 0.f || ShotDirection.IsNearlyZero())
		{
			ShotDirection = Direction;
		}
	}
	else
	{
		// From above the target is a brawler's middle or a spot on the ground: aim at the muzzle's height over it
		ShotDirection = TargetLocation + FVector(0.f, 0.f, Muzzle.Z - GetActorLocation().Z) - Muzzle;
		// Small height differences (same floor, mid-jump) keep shots level; a target closer than the muzzle can't tilt them
		if (FMath::Abs(ShotDirection.Z) < SameLevelHeight || FVector::DotProduct(ShotDirection, Direction) <= 0.f)
		{
			ShotDirection = Direction;
		}
	}
	FRotator ShotRotation = ShotDirection.Rotation();
	ShotRotation.Pitch = FMath::Clamp(ShotRotation.Pitch, -PitchLimit, PitchLimit);

	// Deferred so damage, range, speed and look are in place before the shot begins play. Heavy guns fire bigger shots.
	const FTransform SpawnTransform(ShotRotation, Muzzle, FVector(Weapon.ProjectileScale));
	if (ARRRandomProjectile* Projectile = World->SpawnActorDeferred<ARRRandomProjectile>(ProjectileClass, SpawnTransform, this, this, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Projectile->DamageMultiplier = DiceBuffs->GetMultiplier(ERRDiceBuffStat::AttackPower) * Weapon.DamageMultiplier * (bGiant ? GiantDamageMultiplier : 1.f);
		Projectile->MaxRange = GetAttackRange();
		Projectile->SetSpeedMultiplier(Weapon.ProjectileSpeedMultiplier);
		// A foam finger's poke: unseen, a flat point of damage, a knockout from behind
		Projectile->bPoke = Weapon.bFoamFinger;
		Projectile->HitEffect = Weapon.GetHitEffect();
		Projectile->Tier = Weapon.bFoamFinger ? ERRWeaponTier::Common : Weapon.Tier;
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
	if (!HasAuthority())
	{
		ServerStartReload();
		return;
	}
	bReloading = true;
	ReloadRemaining = GetReloadTime();
}

void ARRRandomCharacter::ServerStartReload_Implementation()
{
	StartReload();
}

void ARRRandomCharacter::RollDice()
{
	if (!bAlive || bDecoy || IsUsingUltimate() || ARRRandomGameState::IsPlayLocked(this))
	{
		return;
	}
	if (!HasAuthority())
	{
		ServerRollDice();
		return;
	}
	DiceBuffs->RollDice();
}

void ARRRandomCharacter::ServerRollDice_Implementation()
{
	RollDice();
}

void ARRRandomCharacter::AddDamagePopup(float Amount, ERRHitEffect Effect, bool bBackstab)
{
	FRRDamagePopup& Popup = DamagePopups.AddDefaulted_GetRef();
	Popup.Amount = Amount;
	Popup.bBackstab = bBackstab;
	Popup.OffsetX = FMath::FRandRange(-25.f, 25.f);
	Popup.Effect = Effect;
	Popup.Spin = FMath::FRandRange(-25.f, 25.f);
	Popup.Nudge = FVector2D(FMath::FRandRange(-14.f, 14.f), FMath::FRandRange(-14.f, 14.f));
	Popup.Lifetime = Effect == ERRHitEffect::Skull ? 1.6f : DamagePopupLifetime;
}

void ARRRandomCharacter::MulticastDamagePopup_Implementation(float Amount, ERRHitEffect Effect, bool bBackstab)
{
	AddDamagePopup(Amount, Effect, bBackstab);
	// Every machine: the body flashes, and a burst of sparks flies off it toward the camera, the gun's color and size
	HitFlash = 1.f;
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		const int32 Kind = FMath::Clamp(static_cast<int32>(Effect), 0, static_cast<int32>(UE_ARRAY_COUNT(HitBurstStrengths)) - 1);
		const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
		const float Body = GetActorScale3D().Z;
		const FVector Where = GetActorLocation() + FVector(FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(-15.f, 15.f), FMath::FRandRange(0.f, 40.f)) * Body;
		const FVector Toward = Camera ? (Camera->GetCameraLocation() - Where).GetSafeNormal() : FVector::UpVector;
		Effects->HitBurst(Where + Toward * 30.f * Body, Toward, HitBurstStrengths[Kind], HitBurstColors[Kind]);
	}
}

void ARRRandomCharacter::UpdatePowerEffects(float DeltaSeconds)
{
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	if (!Effects)
	{
		return;
	}
	const float Body = GetActorScale3D().Z;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, HalfHeight);
	const FLinearColor Tint = FMath::Lerp(GetTeamColor(Team), FLinearColor::White, 0.45f);
	// Giant: a burst on growing, a puff on shrinking back, an aura between
	if (bGiant != bWasGiant)
	{
		bWasGiant = bGiant;
		if (bGiant)
		{
			Effects->GiantBurst(Feet, GiantScale, Tint);
		}
		else
		{
			Effects->GiantShrink(Feet, Body);
		}
	}
	if (bGiant)
	{
		Effects->GiantAura(Feet, Body, HalfHeight * 2.f, Tint, DeltaSeconds);
	}
	// Wings: a burst of light as they open, a trail of stars while flying or gliding
	const FVector Back = Wings ? Wings->GetComponentLocation() : GetActorLocation() + FVector(0.f, 0.f, HalfHeight * 0.4f);
	if (bFlying != bWasFlying)
	{
		bWasFlying = bFlying;
		if (bFlying)
		{
			Effects->WingsBurst(Back, Tint);
		}
	}
	const URRRandomMovementComponent* RandomMovement = Cast<URRRandomMovementComponent>(GetCharacterMovement());
	if (!Vehicle && GetCharacterMovement()->IsFalling() && (bFlying || (RandomMovement && RandomMovement->bGliding)))
	{
		Effects->FlightTrail(Back, GetVelocity(), Tint, DeltaSeconds);
	}
}

void ARRRandomCharacter::EndPowers()
{
	bGiant = false;
	GiantRemaining = 0.f;
	bFlying = false;
	FlightRemaining = 0.f;
}

void ARRRandomCharacter::SetFormationSpot(const FVector& Spot)
{
	if (!HasAuthority())
	{
		return;
	}
	GetOffVehicle();
	EndPowers();
	FormationSpot = Spot;
	bHasFormation = true;
	bInFormation = false;
	GatherTime = 0.f;
	FormationPathIndex = 0;

	// The way there on foot, round everyone else (they move), and the speed that covers it in time
	TArray<const AActor*> Ignore;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		Ignore.Add(*It);
	}
	TArray<FVector> Route;
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	FormationPath.Reset();
	FormationDashSpeed = 0.f;
	if (FRRGridPath::Find(GetWorld(), GetActorLocation(), Spot, Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(), Ignore, Route))
	{
		const float Length = FRRGridPath::Length(GetActorLocation(), Route);
		for (const FVector& Point : Route)
		{
			FormationPath.Add(Point);
		}
		FormationDashSpeed = FMath::Clamp(Length / FormationDashTime, BaseWalkSpeed * FormationDashMinShare, FormationDashMaxSpeed);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s dashes to the dance line: %d corners, %.0f long, at %.0f."), *GetName(), Route.Num(), Length, FormationDashSpeed);
	}
	else
	{
		// No way there on foot: straight there
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s has no way to the dance line on foot."), *GetName());
		MulticastFormationTeleport(GetActorLocation(), Spot);
		TeleportTo(Spot, FRotator(0.f, GetActorRotation().Yaw, 0.f), false, true);
		GetCharacterMovement()->StopMovementImmediately();
		bInFormation = true;
	}
}

void ARRRandomCharacter::SetFormationLeader()
{
	if (!HasAuthority())
	{
		return;
	}
	GetOffVehicle();
	EndPowers();
	FormationSpot = GetActorLocation();
	bHasFormation = true;
	bInFormation = true;
}

void ARRRandomCharacter::UpdateFormation(float DeltaSeconds)
{
	if (!bHasFormation || bInFormation)
	{
		return;
	}
	GatherTime += DeltaSeconds;
	const float Distance = FVector::Dist2D(FormationSpot, GetActorLocation());
	// Close enough to stop: a little more at full speed, as one frame covers a lot
	const float Arrive = FMath::Max(FormationArriveRadius, FormationDashSpeed * DeltaSeconds * 1.2f);
	// Run along the route by whichever machine moves this brawler (the server for bots, the player's own machine for
	// players): to the next corner, the next once this one is passed
	if (IsLocallyControlled() && FormationPath.Num() > 0 && Distance > Arrive)
	{
		const float Reach = FMath::Max(60.f, FormationDashSpeed * DeltaSeconds * 2.f);
		while (FormationPathIndex < FormationPath.Num() - 1 && FVector::Dist2D(FormationPath[FormationPathIndex], GetActorLocation()) < Reach)
		{
			++FormationPathIndex;
		}
		const FVector Toward = (FVector(FormationPath[FMath::Min(FormationPathIndex, FormationPath.Num() - 1)]) - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		AddMovementInput(Toward.GetSafeNormal(), 1.f);
	}
	if (!HasAuthority())
	{
		return;
	}
	if (Distance <= Arrive)
	{
		GetCharacterMovement()->StopMovementImmediately();
		bInFormation = true;
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s arrives in the dance line after %.1f s."), *GetName(), GatherTime);
		return;
	}
	// Time's up (stuck on the way, or too far even at full tilt): teleported the rest of the way
	if (GatherTime >= VictoryGatherTime)
	{
		MulticastFormationTeleport(GetActorLocation(), FormationSpot);
		TeleportTo(FormationSpot, FRotator(0.f, GetActorRotation().Yaw, 0.f), false, true);
		GetCharacterMovement()->StopMovementImmediately();
		bInFormation = true;
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s teleports into the dance line (%.0f away after %.1f s)."), *GetName(), Distance, GatherTime);
	}
}

void ARRRandomCharacter::UpdateDashEffects(float DeltaSeconds)
{
	const bool bDashNow = IsDashingToFormation();
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	const FLinearColor Tint = FMath::Lerp(GetTeamColor(Team), FLinearColor::White, 0.45f);
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	if (bDashNow != bDashShown && Effects)
	{
		// A boom as it sets off (behind it, toward the spot) and as it stops
		const FVector Way = bDashNow && FormationPath.Num() > 0 ? (FVector(FormationPath[0]) - GetActorLocation()).GetSafeNormal2D() : GetActorForwardVector();
		Effects->SonicBoom(GetActorLocation(), Way, Tint);
	}
	bDashShown = bDashNow;

	// Afterimages: copies of the body's pose left behind as it goes, fading
	if (bDashNow)
	{
		if (Effects)
		{
			Effects->SpeedTrail(GetActorLocation(), Feet, GetVelocity(), Tint, DeltaSeconds);
		}
		AfterimageTimer -= DeltaSeconds;
		if (AfterimageTimer <= 0.f && GetVelocity().Size2D() > BaseWalkSpeed)
		{
			AfterimageTimer = AfterimageInterval;
			if (Afterimages.Num() < AfterimageCount)
			{
				UMaterialInterface* Ghost = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/DiceFX/M_FXGhost.M_FXGhost"));
				UPoseableMeshComponent* Copy = NewObject<UPoseableMeshComponent>(this);
				Copy->SetSkinnedAssetAndUpdate(GetMesh()->GetSkeletalMeshAsset());
				Copy->SetUsingAbsoluteLocation(true);
				Copy->SetUsingAbsoluteRotation(true);
				Copy->SetUsingAbsoluteScale(true);
				Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Copy->SetCastShadow(false);
				Copy->SetupAttachment(GetRootComponent());
				Copy->RegisterComponent();
				for (int32 Slot = 0; Ghost && Slot < Copy->GetNumMaterials(); ++Slot)
				{
					Copy->SetMaterial(Slot, UMaterialInstanceDynamic::Create(Ghost, Copy));
				}
				Afterimages.Add(Copy);
				AfterimageAges.Add(AfterimageLife);
			}
			UPoseableMeshComponent* Copy = Afterimages[NextAfterimage % Afterimages.Num()];
			AfterimageAges[NextAfterimage % Afterimages.Num()] = 0.f;
			++NextAfterimage;
			Copy->SetWorldTransform(GetMesh()->GetComponentTransform());
			Copy->CopyPoseFromSkeletalComponent(GetMesh());
			Copy->SetVisibility(true);
		}
	}
	for (int32 Index = 0; Index < Afterimages.Num(); ++Index)
	{
		if (AfterimageAges[Index] >= AfterimageLife)
		{
			if (Afterimages[Index]->IsVisible())
			{
				Afterimages[Index]->SetVisibility(false);
			}
			continue;
		}
		AfterimageAges[Index] += DeltaSeconds;
		const float Fade = 1.f - FMath::Clamp(AfterimageAges[Index] / AfterimageLife, 0.f, 1.f);
		for (int32 Slot = 0; Slot < Afterimages[Index]->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Afterimages[Index]->GetMaterial(Slot)))
			{
				Material->SetScalarParameterValue(TEXT("Opacity"), AfterimageOpacity * Fade * Fade);
				Material->SetVectorParameterValue(TEXT("Color"), FMath::Lerp(Tint, AfterimageColor, 0.6f));
			}
		}
	}
}

void ARRRandomCharacter::MulticastFormationTeleport_Implementation(const FVector_NetQuantize& From, const FVector_NetQuantize& To)
{
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		const FLinearColor Tint = FMath::Lerp(GetTeamColor(Team), FLinearColor::White, 0.45f);
		Effects->TeleportPoof(From, Tint);
		Effects->TeleportPoof(To, Tint);
	}
}

void ARRRandomCharacter::UpdateHitFlash(float DeltaSeconds)
{
	if (HitFlash <= 0.f && !bHitFlashShown)
	{
		return;
	}
	HitFlash = FMath::Max(0.f, HitFlash - DeltaSeconds / HitFlashTime);
	bHitFlashShown = HitFlash > 0.f;
	USkeletalMeshComponent* BodyMesh = GetMesh();
	for (int32 Slot = 0; Slot < BodyMesh->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(BodyMesh->GetMaterial(Slot)))
		{
			Material->SetScalarParameterValue(HitFlashParameter, FMath::Square(HitFlash) * HitFlashBrightness);
		}
	}
}

float ARRRandomCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Shots only hit on the server
	ARRRandomCharacter* Attacker = DamageCauser ? Cast<ARRRandomCharacter>(DamageCauser->GetInstigator()) : nullptr;
	// Shots still in the air when the match ends do nothing
	if (!HasAuthority() || !bAlive || bInvincible || IsUsingUltimate() || ARRRandomGameState::IsPlayLocked(this) || (Attacker && Attacker != this && Attacker->GetTeam() == Team))
	{
		return 0.f;
	}

	// A fox's clone's shots are only for show; the clone itself goes up in smoke at the first real hit
	if (Attacker && Attacker->IsDecoy())
	{
		return 0.f;
	}
	if (bDecoy)
	{
		if (DamageAmount > 0.f)
		{
			VanishDecoy();
		}
		return 0.f;
	}

	FVector ShotDirection = DamageCauser ? DamageCauser->GetActorForwardVector() : -GetActorForwardVector();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		ShotDirection = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection;
	}
	const ARRRandomProjectile* Shot = Cast<ARRRandomProjectile>(DamageCauser);

	// A fox's shot in the back (within BackstabHalfAngle of straight behind: going the way this brawler faces), or its first
	// shot out of invisibility, hits harder
	bool bBackstab = false;
	if (Shot && Attacker && Attacker != this && RRKit::Get(Attacker->GetSpecies()).bBackstab)
	{
		const float Behind = FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), ShotDirection.GetSafeNormal2D());
		bBackstab = Behind >= FMath::Cos(FMath::DegreesToRadians(RRKit::BackstabHalfAngle)) || Attacker->GetKit()->ConsumeAmbush();
		if (bBackstab)
		{
			DamageAmount *= RRKit::BackstabMultiplier;
		}
	}

	// Giants shrug off part of every hit
	const float Damage = Super::TakeDamage(DamageAmount * (bGiant ? GiantDamageTakenMultiplier : 1.f), DamageEvent, EventInstigator, DamageCauser);
	if (Damage <= 0.f)
	{
		return 0.f;
	}

	// A shield (the squirrel's own die) takes what it can first; an invisible brawler flickers into view
	Health = FMath::Max(0.f, Health - Kit->AbsorbDamage(Damage));
	Kit->OnHit();
	LastCombatTime = GetWorld()->GetTimeSeconds();
	// The number floats up on every machine, the host included, with the sticker of the gun that hit; a foam finger's
	// knockout shows a skull
	ERRHitEffect Effect = Shot ? Shot->HitEffect : ERRHitEffect::Pistol;
	if (Effect == ERRHitEffect::Poke && Health <= 0.f)
	{
		Effect = ERRHitEffect::Skull;
	}
	MulticastDamagePopup(Damage, Effect, bBackstab);
	UE_CLOG(bBackstab, LogTemp, Log, TEXT("RRRandom: %s backstabs %s for %.0f."), *Attacker->GetName(), *GetName(), Damage);

	if (Health <= 0.f)
	{
		KnockOut(ShotDirection, Attacker);
	}
	return Damage;
}

void ARRRandomCharacter::KnockOut(const FVector& ShotDirection, ARRRandomCharacter* Attacker, bool bFell)
{
	bAlive = false;
	Health = 0.f;
	RespawnRemaining = RespawnDelay;
	DiceBuffs->ClearBuffs();
	Kit->ClearPowers();
	// Back to the starting pistol (the squirrel keeps its gun in its cheek pouch). A giant falls as one and gets back up
	// normal size (see ApplyStandingBody).
	if (!RRKit::Get(Species).bCheekPouch)
	{
		Weapon = FRRWeapon();
	}
	bGiant = false;
	GiantRemaining = 0.f;
	bFlying = false;
	FlightRemaining = 0.f;
	KnockdownDirection = ShotDirection.GetSafeNormal2D();
	// Run over this very moment: thrown along the vehicle's way, faster the faster it went
	FlingVelocity = FVector::ZeroVector;
	if (FlyBackAnimation && RamImpactTime == GetWorld()->GetTimeSeconds() && !RamImpact.IsNearlyZero())
	{
		const float Speed = FMath::Clamp(RamImpact.Size2D() * FlingSpeedShare, FlingSpeedRange.X, FlingSpeedRange.Y) * RRKit::Get(Species).KnockbackScale;
		KnockdownDirection = RamImpact.GetSafeNormal2D();
		FlingVelocity = RamImpact.GetSafeNormal2D() * Speed + FVector(0.f, 0.f, FlingLift);
	}
	SetTrustClientMovement(false);
	// Thrown off: the vehicle stays where the rider fell
	GetOffVehicle();
	ApplyKnockedOutBody();

	if (ARRRandomGameMode* GameMode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>())
	{
		GameMode->OnBrawlerKnockedOut(this, Attacker, bFell);
	}
}

void ARRRandomCharacter::SetRamImpact(const FVector& Velocity)
{
	RamImpact = Velocity * FVector(1.f, 1.f, 0.f);
	RamImpactTime = GetWorld()->GetTimeSeconds();
}

void ARRRandomCharacter::ApplyKnockedOutBody()
{
	bClimbing = false;
	bDashing = false;
	if (!FVector(FlingVelocity).IsNearlyZero() && FlyBackAnimation)
	{
		// Run over: the capsule flies off (falling, through other brawlers and the vehicle's driver, stopped by the world)
		// facing back the way the vehicle came, with the body playing its flight backward; EndFling lays it down on landing
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		Movement->bOrientRotationToMovement = false;
		bUseControllerRotationYaw = false;
		FireStanceAlpha = 0.f;
		CelebrationTurn = 0.f;
		USkeletalMeshComponent* BodyMesh = GetMesh();
		BodyMesh->SetRelativeRotation(MeshRelativeRotation);
		SetActorRotation((-FVector(FlingVelocity).GetSafeNormal2D()).Rotation());
		GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		Movement->SetMovementMode(MOVE_Falling);
		Movement->Velocity = FlingVelocity;
		BodyMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		PlayBodyAnimation(FlyBackAnimation, false);
		bFlinging = true;
		FlingTime = 0.f;
		FlingStart = GetActorLocation();
		return;
	}
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	USkeletalMeshComponent* BodyMesh = GetMesh();
	// Out of the rifle stance, so the fall lines up with the shot that dropped it
	FireStanceAlpha = 0.f;
	CelebrationTurn = 0.f;
	BodyMesh->SetRelativeRotation(MeshRelativeRotation);
	if (DeathAnimation)
	{
		// The death animation falls backward, so face against the final shot to fall the way it pushed
		const FVector Facing = -FVector(KnockdownDirection);
		if (!Facing.IsNearlyZero())
		{
			SetActorRotation(Facing.Rotation());
		}
		// Shots fly over the fallen body instead of being soaked up by it
		BodyMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
		PlayBodyAnimation(DeathAnimation, false);
		return;
	}

	BodyMesh->SetCollisionProfileName(RagdollProfile);
	// Shots fly over the fallen body instead of being soaked up by it
	BodyMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	BodyMesh->SetSimulatePhysics(true);
	BodyMesh->SetAllPhysicsLinearVelocity(FVector(KnockdownDirection) * KnockdownSpeed + FVector(0.f, 0.f, 200.f));
}

void ARRRandomCharacter::UpdateFling(float DeltaSeconds)
{
	if (!bFlinging)
	{
		return;
	}
	FlingTime += DeltaSeconds;
	// Down on the ground (or laid down by the server already); off the edge it just keeps falling until getting back up
	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (FlingTime > 0.2f && (Movement->IsMovingOnGround() || Movement->MovementMode == MOVE_None))
	{
		EndFling();
	}
}

void ARRRandomCharacter::EndFling()
{
	UE_CLOG(HasAuthority(), LogTemp, Log, TEXT("RRRandom: %s, run over, lands %.0f away (%+.0f up) after %.2f s."), *GetName(),
		FVector::Dist2D(GetActorLocation(), FlingStart), GetActorLocation().Z - FlingStart.Z, FlingTime);
	bFlinging = false;
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, CapsulePawnResponse);
}

void ARRRandomCharacter::ApplyStandingBody()
{
	if (bFlinging)
	{
		EndFling();
	}
	// Size is left alone while knocked out, so the ragdoll isn't rescaled mid-fall; back to normal here
	SetActorScale3D(FVector(bGiant ? GiantScale : 1.f));

	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetSimulatePhysics(false);
	BodyMesh->SetCollisionProfileName(MeshCollisionProfile);
	BodyMesh->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	BodyMesh->SetRelativeLocationAndRotation(MeshRelativeLocation, MeshRelativeRotation);
	if (IdleAnimation)
	{
		PlayBodyAnimation(IdleAnimation, true);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ARRRandomCharacter::ReviveNow()
{
	if (HasAuthority() && !bAlive)
	{
		RespawnRemaining = 0.f;
		Respawn();
	}
}

void ARRRandomCharacter::Respawn()
{
	// Clients follow through replicated movement and OnRep_Alive
	SetActorLocationAndRotation(HomeLocation, HomeRotation, false, nullptr, ETeleportType::TeleportPhysics);
	FlingVelocity = FVector::ZeroVector;
	ApplyStandingBody();

	Health = MaxHealth;
	Ammo = GetMaxAmmo();
	bReloading = false;
	ReloadRemaining = 0.f;
	LastCombatTime = -UE_BIG_NUMBER;
	NextFireTime = 0.f;
	bAlive = true;
}
