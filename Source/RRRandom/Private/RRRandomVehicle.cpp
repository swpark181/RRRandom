#include "RRRandomVehicle.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "RRRandomPlaneStation.h"
#include "RRRandomProjectile.h"
#include "RRRandomSkidMarks.h"
#include "RRRandomUIStyle.h"
#include "RRRandomWreckage.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// By ERRVehicleKind. Each clearly faster than the last, and all of them much faster than walking (speed is a share of
	// the walk); the motorbike and the car trade grip for drifts.
	// Seats (round 71): where the hips go and how high the deck or cushion is, measured on the meshes (the top surface's
	// height every 5 units along each seat's line, from char-pipeline/output/vehicles: a cushion is the low flat stretch
	// in front of a backrest).
	const FRRVehicleHandling Handlings[] =
	{
		// Kickboard: twice the walk, otherwise like walking; standing on the deck (53 high, -45 .. 30), the bar's stem at 35
		{ 2.f, 2600.f, 8.f, 720.f, 2200.f, false, 53.f, 0.f, 0.f, TEXT("KICKBOARD"), -8.f },
		// Motorbike: faster, slides out in tight turns and leans into them; one rear wheel's mark. The saddle runs from
		// -75 to -5, sloping from 93 down to 86 toward the tank (0 .. 40, 100 .. 109 high). Round 90: four times the walk,
		// winding up to it like a real bike over about 2.6 seconds (SpeedUpTime)
		{ 4.f, 2400.f, 1.6f, 560.f, 1100.f, true, 86.f, 0.7f, 28.f, TEXT("MOTORBIKE"), -22.f, false, 0.f, 16.f, 0.65f, 0.f,
			0.f, 0.f, false, 0.f, false, 0.f, ERRPlaneGun::Pilot, 2.6f },
		// Car: fastest, slides the most, rolls a little; a tandem two-seater, the rear seat right behind the driver's;
		// two rear wheels' marks. Driver's cushion -20 .. 20 at 52 (backrest at -40 .. -30), the rear one -115 .. -55 at 62
		// (backrest -135 .. -120)
		{ 3.5f, 2200.f, 1.1f, 460.f, 1000.f, true, 52.f, 0.15f, 6.f, TEXT("CAR"), -12.f, true, -95.f, 22.f, 0.55f, 0.72f, 0.f, 0.f, true, 62.f },
		// The star team (the haetae robot's cars, round 79): one seat on the middle line of the chest car (an open SUV
		// carrying the robot's head under its floor: cushion -5 .. 30 at 82, backrest -45 .. -10) and of the arms car (a
		// low hot rod with the fists folded inside: cushion -35 .. 0 at 58, backrest -55 .. -45), two one behind the other
		// in the long legs car (cushions -15 .. 20 at 88 and -130 .. -90 at 112, backrests -45 .. -30 and -150 .. -135;
		// riders go 6 lower, into the seats rather than on top of them). The chest car is the sturdy one, the arms car the
		// nimble one, the legs car (rocket nozzles at its tail) the fastest that turns the slowest; all ram.
		{ 3.f, 2000.f, 1.4f, 440.f, 1100.f, true, 82.f, 0.12f, 5.f, TEXT("STAR CHEST"), 2.f, false, 2.f, 24.f, 0.55f, 0.75f, 0.f, 0.f, true, 82.f },
		{ 3.2f, 2200.f, 1.2f, 500.f, 1000.f, true, 58.f, 0.15f, 6.f, TEXT("STAR ARMS"), -25.f, false, -25.f, 22.f, 0.55f, 0.7f, 0.f, 0.f, true, 58.f },
		{ 3.8f, 2400.f, 1.f, 380.f, 900.f, true, 82.f, 0.1f, 4.f, TEXT("STAR LEGS"), -5.f, true, -118.f, 18.f, 0.7f, 0.5f, 0.f, 0.f, true, 106.f },
		// The planes (round 88), each tougher, faster and better armed than the last: one seat (the cushion's height and
		// place from the models' height profiles: the wooden plane's 142 over X -10 (its propeller is part of the model),
		// the fighter's 85 over X 10, the spaceship's 108 over X 35), no drift or ramming; the hull's health and the gun at the end.
		// Wooden plane: twice the walk, turns gently; the pilot shoots its own gun
		{ 2.f, 1600.f, 8.f, 260.f, 900.f, false, 142.f, 0.f, 0.f, TEXT("WOODEN PLANE"), -10.f, false, 0.f, 0.f, 0.6f, 0.f, 0.f, 0.f, false, 0.f, true, 200.f, ERRPlaneGun::Pilot },
		// Fighter: faster, twin machine guns in the nose
		{ 2.6f, 2000.f, 8.f, 300.f, 1100.f, false, 85.f, 0.f, 0.f, TEXT("FIGHTER"), 10.f, false, 0.f, 0.f, 0.6f, 0.f, 0.f, 0.f, false, 0.f, true, 300.f, ERRPlaneGun::Guns },
		// Spaceship: fastest and toughest, heavy laser bolts from its wing tips
		{ 3.f, 2400.f, 8.f, 340.f, 1300.f, false, 108.f, 0.f, 0.f, TEXT("SPACESHIP"), 35.f, false, 0.f, 0.f, 0.6f, 0.f, 0.f, 0.f, false, 0.f, true, 400.f, ERRPlaneGun::Laser },
	};
	// A plane's own guns: seconds between shots, damage (a pistol shot's share), shot size and speed (shares), look (tier)
	struct FPlaneGunStats
	{
		float Interval;
		float Damage;
		float Scale;
		float Speed;
		ERRWeaponTier Tier;
	};
	const FPlaneGunStats PlaneGunStats[] =
	{
		// The pilot's own gun: unused (the gun's own numbers go)
		{ 0.15f, 1.f, 1.f, 1.f, ERRWeaponTier::Common },
		// The fighter's machine guns, the two in turn: quick small tracers, half a pistol shot each
		{ 0.09f, 0.45f, 0.7f, 1.5f, ERRWeaponTier::Rare },
		// The spaceship's lasers, the two in turn: slow, big and heavy
		{ 0.5f, 2.2f, 1.5f, 1.3f, ERRWeaponTier::Legendary },
	};
	// Where a plane's own guns are on its model (the left one; the right one mirrored): the fighter's in its nose, the
	// spaceship's laser cannons at the front of its sides
	const FVector GunMuzzle(190.f, 60.f, 80.f);
	const FVector LaserMuzzle(180.f, 150.f, 90.f);
	// Without a plane's model: an engine cube stretched to about its size (100 a side, pivot in its middle)
	const FVector FallbackPlaneScale(3.8f, 3.4f, 0.7f);
	// The wooden plane's propeller: a blade this big (engine cube scale) just in front of the nose, turning this fast
	// flying and idling
	const FVector PropellerScale(0.04f, 1.5f, 0.14f);
	constexpr float PropellerSpin = 2400.f;
	constexpr float PropellerIdleSpin = 400.f;
	// A plane banks into turns (degrees per degree a second of turn, at most) and noses up or down with the climb (at
	// most, at full climbing speed); easing at this rate
	constexpr float PlaneBankPerTurn = 0.12f;
	constexpr float PlaneMaxBank = 28.f;
	constexpr float PlaneMaxPitch = 18.f;
	constexpr float PlaneTiltRate = 6.f;
	// Shot down: it falls faster and faster (to at most CrashMaxFall), spinning; where it lands it blows up, smashing the
	// blocks within CrashWreckRadius and hurting the pilot's side within CrashBlastRadius by CrashBlastDamage
	constexpr float CrashGravity = 2200.f;
	constexpr float CrashMaxFall = 3200.f;
	constexpr float CrashSpinRate = 540.f;
	constexpr float CrashWreckRadius = 220.f;
	constexpr float CrashBlastRadius = 300.f;
	constexpr float CrashBlastDamage = 40.f;
	// Left a moment after it blows up, so every machine hears about it first
	constexpr float CrashLinger = 1.5f;
	// The star legs car's rocket nozzles (its mesh: front on +X, 420 long), where the flames come out, and how big each
	// flame is; they burn above this speed
	const FVector4f LegsRocketNozzles[] =
	{
		FVector4f(-206.f, -52.f, 120.f, 1.f),
		FVector4f(-206.f, 52.f, 120.f, 1.f),
		FVector4f(-203.f, -48.f, 64.f, 0.6f),
		FVector4f(-203.f, 48.f, 64.f, 0.6f),
	};
	constexpr float LegsRocketMinSpeed = 80.f;
	// Only a few smoke puffs: at speed the full rocket fist's smoke trails behind as a wall
	constexpr float LegsRocketSmoke = 0.12f;
	const TCHAR* MeshPaths[] =
	{
		TEXT("/Game/Vehicles/SM_Kickboard.SM_Kickboard"),
		TEXT("/Game/Vehicles/SM_Motorbike.SM_Motorbike"),
		TEXT("/Game/Vehicles/SM_Car.SM_Car"),
		TEXT("/Game/Vehicles/SM_StarChest.SM_StarChest"),
		TEXT("/Game/Vehicles/SM_StarArms.SM_StarArms"),
		TEXT("/Game/Vehicles/SM_StarLegs.SM_StarLegs"),
		TEXT("/Game/Vehicles/SM_PlaneWood.SM_PlaneWood"),
		TEXT("/Game/Vehicles/SM_PlaneFighter.SM_PlaneFighter"),
		TEXT("/Game/Vehicles/SM_PlaneSpaceship.SM_PlaneSpaceship"),
	};
	// Drifting counts from this many degrees between where the vehicle faces and where it goes, at this speed or more
	constexpr float DriftAngleStart = 8.f;
	constexpr float DriftMinSpeed = 150.f;
	// How fast the lean follows, degrees a second
	constexpr float LeanRate = 120.f;
	// Tyre marks: one strip every this many units a wheel slides, at full strength and width from this speed, and
	// this faint at the slowest drift
	constexpr float SkidSegment = 24.f;
	constexpr float SkidFullSpeed = 520.f;
	constexpr float SkidMinStrength = 0.3f;
	// The car running into an enemy: faster than walking knocks them out at once; slower, it hurts by its speed (all of
	// a brawler's health at walking speed), from this speed up, the same brawler at most once in this many seconds; the
	// car's body reaches this much past its mesh, and the hit throws them along and up
	constexpr float RamMinSpeed = 60.f;
	constexpr float RamRepeatTime = 0.8f;
	constexpr float RamReach = 15.f;
	const FVector2D RamKnock(500.f, 250.f);

	/**
	 * Round 91, the lights (PlaceLights): by ERRVehicleKind, where its front lamps and back lamps sit on its model (forward,
	 * right, up; a Y over 0 is a pair, mirrored across), how big a lamp bead is, and the neon under it. On the meshes' boxes
	 * (Kickboard 140 x 96 x 115, Motorbike 220 x 120 x 140, Car 360 x 162 x 114, star cars 300-420 long, planes 360-420 long and
	 * 380-460 across). A plane's "front" pair is its wing tips (red left, green right) and its "back" its white tail light.
	 */
	struct FRRVehicleLamps
	{
		FVector Front;
		FVector Back;
		float Size;
		FLinearColor Neon;
	};
	const FRRVehicleLamps VehicleLamps[] =
	{
		// Kickboard: a lamp on the handlebar stem, a tail light at the back of the deck; mint neon
		{ FVector(52.f, 0.f, 70.f), FVector(-70.f, 0.f, 16.f), 9.f, FLinearColor(0.25f, 1.f, 0.75f) },
		// Motorbike: the headlamp at the front of the tank, the tail light behind the saddle; hot pink
		{ FVector(104.f, 0.f, 88.f), FVector(-104.f, 0.f, 78.f), 12.f, FLinearColor(1.f, 0.3f, 0.75f) },
		// Car: two headlights in the bumper, two tail lights; sky blue
		{ FVector(176.f, 52.f, 52.f), FVector(-177.f, 56.f, 58.f), 14.f, FLinearColor(0.3f, 0.75f, 1.f) },
		// The star cars: gold, like the star team
		{ FVector(146.f, 62.f, 62.f), FVector(-147.f, 66.f, 72.f), 11.f, FLinearColor(1.f, 0.78f, 0.3f) },
		{ FVector(158.f, 44.f, 48.f), FVector(-158.f, 48.f, 60.f), 11.f, FLinearColor(1.f, 0.78f, 0.3f) },
		{ FVector(204.f, 58.f, 62.f), FVector(-202.f, 80.f, 98.f), 12.f, FLinearColor(1.f, 0.78f, 0.3f) },
		// The planes: wing tips and tail, from the models' vertices (the farthest point out to the side and the tail's top, the bead half out of it; no neon: they fly)
		{ FVector(36.f, 188.f, 95.f), FVector(-154.f, 0.f, 203.f), 22.f, FLinearColor::Black },
		{ FVector(-22.f, 228.f, 63.f), FVector(-179.f, 0.f, 122.f), 22.f, FLinearColor::Black },
		{ FVector(30.f, 192.f, 79.f), FVector(-174.f, 0.f, 177.f), 22.f, FLinearColor::Black },
	};
	const FLinearColor HeadLampColor(1.f, 0.93f, 0.78f);
	const FLinearColor TailLampColor(1.f, 0.12f, 0.18f);
	const FLinearColor WingLeftColor(1.f, 0.1f, 0.12f);
	const FLinearColor WingRightColor(0.15f, 1.f, 0.3f);
	// Lamp beads' glow (M_FXCloak "Glow") parked and ridden; the neon under it (candelas) parked and ridden, reaching this
	// far past the model's half length; the headlight beam (candelas), its reach and cones, tipped down this much
	constexpr float LampGlowParked = 4.f;
	constexpr float LampGlowRidden = 10.f;
	constexpr float ColoredLampShare = 0.3f;
	// In the air faster than this (a jump ramp's leap): the rush trail (ARRRandomEffects::RushTrail)
	constexpr float RushMinSpeed = 900.f;
	constexpr float NeonParked = 6.f;
	constexpr float NeonRidden = 14.f;
	constexpr float NeonReach = 1.5f;
	constexpr float HeadLightIntensity = 220.f;
	constexpr float HeadLightReach = 1400.f;
	constexpr float HeadLightInnerCone = 14.f;
	constexpr float HeadLightOuterCone = 30.f;
	constexpr float HeadLightDip = 7.f;
	// A plane's lights blink this many times a second, on for this share of each
	constexpr float BlinkRate = 1.2f;
	constexpr float BlinkOn = 0.35f;
}

ARRRandomVehicle::ARRRandomVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = true;
	SetReplicateMovement(true);

	// The root stands on the floor; the body under it leans into drifts
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	// Scenery: brawlers, shots and sight pass through
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	// Inside the toon silhouette line, like the brawlers
	Body->SetRenderCustomDepth(true);
	Body->SetCustomDepthStencilValue(1);
	// Tyre marks are for the ground
	Body->SetReceivesDecals(false);

	for (const TCHAR* Path : MeshPaths)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(Path);
		KindMeshes.Add(Mesh.Succeeded() ? Mesh.Object : nullptr);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	Propeller = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Propeller"));
	Propeller->SetupAttachment(Body);
	Propeller->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Propeller->SetCanEverAffectNavigation(false);
	Propeller->SetStaticMesh(Cube);
	Propeller->SetRelativeScale3D(PropellerScale);
	Propeller->SetVisibility(false);

	// Round 91: its lights (PlaceLights puts them on the kind's model)
	UnderGlow = CreateDefaultSubobject<UPointLightComponent>(TEXT("UnderGlow"));
	UnderGlow->SetupAttachment(Body);
	UnderGlow->SetIntensityUnits(ELightUnits::Candelas);
	UnderGlow->SetCastShadows(false);
	HeadLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadLight"));
	HeadLight->SetupAttachment(Body);
	HeadLight->SetIntensityUnits(ELightUnits::Candelas);
	HeadLight->SetLightColor(HeadLampColor);
	HeadLight->SetAttenuationRadius(HeadLightReach);
	HeadLight->SetInnerConeAngle(HeadLightInnerCone);
	HeadLight->SetOuterConeAngle(HeadLightOuterCone);
	HeadLight->SetCastShadows(false);
	HeadLight->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glow(TEXT("/Game/Effects/DiceFX/M_FXCloak.M_FXCloak"));
	for (int32 i = 0; i < 4; ++i)
	{
		UStaticMeshComponent* Lamp = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Lamp%d"), i));
		Lamp->SetupAttachment(Body);
		Lamp->SetStaticMesh(SphereMesh.Succeeded() ? SphereMesh.Object : nullptr);
		if (Glow.Succeeded())
		{
			Lamp->SetMaterial(0, Glow.Object);
		}
		Lamp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Lamp->SetCanEverAffectNavigation(false);
		Lamp->SetCastShadow(false);
		Lamp->SetReceivesDecals(false);
		Lamps.Add(Lamp);
	}
}

void ARRRandomVehicle::PlaceLights()
{
	const FRRVehicleLamps& Spots = VehicleLamps[FMath::Clamp(static_cast<int32>(Kind), 0, UE_ARRAY_COUNT(VehicleLamps) - 1)];
	const bool bPlane = IsPlane();
	const bool bModel = Body->GetStaticMesh() && Body->GetStaticMesh() != Cube;
	if (LampMaterials.Num() != Lamps.Num())
	{
		LampMaterials.Reset();
		for (UStaticMeshComponent* Lamp : Lamps)
		{
			LampMaterials.Add(Lamp->CreateDynamicMaterialInstance(0, Lamp->GetMaterial(0)));
		}
	}
	for (int32 i = 0; i < Lamps.Num(); ++i)
	{
		const bool bBack = i >= 2;
		const bool bMirror = (i % 2) == 1;
		FVector Spot = bBack ? Spots.Back : Spots.Front;
		// A single lamp (on the middle line) has no mirror
		const bool bShown = bModel && (!bMirror || Spot.Y > 0.f);
		Spot.Y *= bMirror ? -1.f : 1.f;
		Lamps[i]->SetVisibility(bShown);
		Lamps[i]->SetRelativeLocation(Spot);
		Lamps[i]->SetRelativeScale3D(FVector(Spots.Size / 100.f));
		if (UMaterialInstanceDynamic* Material = LampMaterials[i])
		{
			// A plane's wing tips: red on the left (-Y), green on the right; its tail white. A car's front white, back red
			const FLinearColor Color = bPlane ? (bBack ? FLinearColor::White : (bMirror ? WingLeftColor : WingRightColor))
				: (bBack ? TailLampColor : HeadLampColor);
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Material->SetScalarParameterValue(TEXT("Glow"), LampGlowParked);
			Material->SetScalarParameterValue(TEXT("Fill"), 1.f);
			Material->SetScalarParameterValue(TEXT("Opacity"), 1.f);
		}
	}
	// The neon under it, a little above the floor, reaching just past its sides
	const FBox Box = bModel ? Body->GetStaticMesh()->GetBoundingBox() : FBox(FVector(-100.f), FVector(100.f));
	UnderGlow->SetVisibility(bModel && !bPlane);
	UnderGlow->SetRelativeLocation(FVector(Box.GetCenter().X, 0.f, 12.f));
	UnderGlow->SetLightColor(Spots.Neon);
	UnderGlow->SetAttenuationRadius(Box.GetExtent().X * NeonReach + 60.f);
	UnderGlow->SetIntensity(NeonParked);
	HeadLight->SetRelativeLocation(FVector(Spots.Front.X, 0.f, Spots.Front.Z));
	HeadLight->SetRelativeRotation(FRotator(-HeadLightDip, 0.f, 0.f));
}

void ARRRandomVehicle::UpdateLights(float DeltaSeconds, float Speed)
{
	LightTime += DeltaSeconds;
	const bool bRidden = Rider != nullptr;
	const bool bPlane = IsPlane();
	// A plane blinks; a ridden car's lamps shine brighter, and its headlight comes on
	const bool bBlinkOn = FMath::Fmod(LightTime * BlinkRate, 1.f) < BlinkOn;
	for (int32 i = 0; i < LampMaterials.Num(); ++i)
	{
		if (UMaterialInstanceDynamic* Material = LampMaterials[i])
		{
			// A plane's wing tips steady, its tail blinking; colored lamps (red, green) less bright, or they bloom to white
			const bool bColored = bPlane ? i < 2 : i >= 2;
			const float Glow = bPlane ? (bBlinkOn || i < 2 ? LampGlowRidden : 0.6f) : (bRidden ? LampGlowRidden : LampGlowParked);
			Material->SetScalarParameterValue(TEXT("Glow"), Glow * (bColored ? ColoredLampShare : 1.f));
		}
	}
	if (!bPlane)
	{
		UnderGlow->SetIntensity(bRidden ? NeonRidden : NeonParked);
		const bool bBeam = bRidden && Body->GetStaticMesh() && Body->GetStaticMesh() != Cube;
		if (HeadLight->IsVisible() != bBeam)
		{
			HeadLight->SetVisibility(bBeam);
		}
		HeadLight->SetIntensity(HeadLightIntensity);
	}
}

void ARRRandomVehicle::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && IsPlane())
	{
		PlaneHealth = GetPlaneMaxHealth();
	}
	ApplyKind();
}

void ARRRandomVehicle::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsPlane() && EndPlayReason == EEndPlayReason::Destroyed)
	{
		// Put down: gone in a puff (one blown up has had its blast)
		if (!bExploded)
		{
			const FVector Middle = GetActorLocation() + FVector(0.f, 0.f, GetHeight() * 0.5f);
			if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
			{
				Effects->FoxPoof(Middle, 2.f);
			}
			ARRRandomHUD::PopComicWord(this, TEXT("POOF!"), Middle + FVector(0.f, 0.f, 80.f), RRUI::LavenderDeep, 1.f);
		}
		if (HasAuthority())
		{
			if (ARRRandomPlaneStation* Home = Station.Get())
			{
				Home->OnPlaneGone();
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ARRRandomVehicle::GetGunStats(ERRPlaneGun Gun, float& OutInterval, float& OutDamage, float& OutScale, float& OutSpeed, uint8& OutTier)
{
	const FPlaneGunStats& Stats = PlaneGunStats[FMath::Clamp(static_cast<int32>(Gun), 0, UE_ARRAY_COUNT(PlaneGunStats) - 1)];
	OutInterval = Stats.Interval;
	OutDamage = Stats.Damage;
	OutScale = Stats.Scale;
	OutSpeed = Stats.Speed;
	OutTier = static_cast<uint8>(Stats.Tier);
}

void ARRRandomVehicle::GetGunMuzzles(TArray<FVector>& OutMuzzles) const
{
	OutMuzzles.Reset();
	const UStaticMesh* Mesh = Body->GetStaticMesh();
	if (!Mesh)
	{
		return;
	}
	// The fighter's two guns either side of its nose, the spaceship's laser cannons out at the front of its sides (measured
	// on the models; on a stand-in cube, from its box)
	const FTransform Frame = Body->GetComponentTransform();
	const bool bLaser = GetHandling().Gun == ERRPlaneGun::Laser;
	const bool bStandIn = Mesh == Cube;
	const FBox Box = Mesh->GetBoundingBox();
	for (const float Side : { 1.f, -1.f })
	{
		const FVector Local = bStandIn ? FVector(Box.Max.X, Side * Box.GetExtent().Y * (bLaser ? 0.85f : 0.15f), Box.GetCenter().Z)
			: (bLaser ? FVector(LaserMuzzle.X, Side * LaserMuzzle.Y, LaserMuzzle.Z) : FVector(GunMuzzle.X, Side * GunMuzzle.Y, GunMuzzle.Z));
		OutMuzzles.Add(Frame.TransformPosition(Local));
	}
}

const FRRVehicleHandling& ARRRandomVehicle::GetHandling(ERRVehicleKind InKind)
{
	return Handlings[FMath::Clamp(static_cast<int32>(InKind), 0, UE_ARRAY_COUNT(Handlings) - 1)];
}

void ARRRandomVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomVehicle, Kind);
	DOREPLIFETIME(ARRRandomVehicle, StarTeam);
	DOREPLIFETIME(ARRRandomVehicle, Rider);
	DOREPLIFETIME(ARRRandomVehicle, Passenger);
	DOREPLIFETIME(ARRRandomVehicle, bCombined);
	DOREPLIFETIME(ARRRandomVehicle, PlaneHealth);
	DOREPLIFETIME(ARRRandomVehicle, bCrashing);
}

void ARRRandomVehicle::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyKind();
}

void ARRRandomVehicle::SetKind(ERRVehicleKind NewKind)
{
	Kind = NewKind;
	ApplyKind();
}

void ARRRandomVehicle::OnRep_Kind()
{
	ApplyKind();
}

void ARRRandomVehicle::ApplyKind()
{
	const int32 Index = static_cast<int32>(Kind);
	UStaticMesh* Mesh = KindMeshes.IsValidIndex(Index) ? KindMeshes[Index].Get() : nullptr;
	const bool bPlane = IsPlane();
	// A plane without its model: a stretched engine cube standing on the floor, so it still shows
	const bool bStandIn = bPlane && !Mesh && Cube;
	Body->SetStaticMesh(bStandIn ? Cube.Get() : Mesh);
	Body->SetRelativeScale3D(bStandIn ? FallbackPlaneScale : FVector::OneVector);
	Body->SetRelativeLocation(bStandIn ? FVector(0.f, 0.f, 50.f * FallbackPlaneScale.Z) : FVector::ZeroVector);
	// A plane's hull stops shots (only shots: animal friends, sight and the camera pass through), which hurt it (TakeDamage)
	if (bPlane)
	{
		Body->SetCollisionObjectType(ECC_WorldDynamic);
		Body->SetCollisionResponseToAllChannels(ECR_Ignore);
		Body->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	// The wooden plane's propeller, just in front of its nose (in the body's own space, whatever its scale)
	const bool bPropeller = Kind == ERRVehicleKind::PlaneWood && bStandIn;
	Propeller->SetVisibility(bPropeller);
	if (bPropeller)
	{
		const FBox Box = Body->GetStaticMesh()->GetBoundingBox();
		const FVector BodyScale = Body->GetRelativeScale3D();
		Propeller->SetRelativeLocation(FVector(Box.Max.X + 3.f / BodyScale.X, 0.f, Box.GetCenter().Z));
		Propeller->SetRelativeScale3D(PropellerScale / BodyScale);
	}
	PlaceLights();
}

UStaticMesh* ARRRandomVehicle::GetKindMesh(ERRVehicleKind InKind) const
{
	const int32 Index = static_cast<int32>(InKind);
	return KindMeshes.IsValidIndex(Index) ? KindMeshes[Index] : nullptr;
}

void ARRRandomVehicle::SetCombined(bool bNewCombined, const FVector& Where, float Yaw)
{
	if (!HasAuthority() || bCombined == bNewCombined || Rider || Passenger)
	{
		return;
	}
	bCombined = bNewCombined;
	if (!bCombined)
	{
		// Back on the floor below the spot (its pivot is the middle of its bottom)
		FVector Spot = Where;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RRVehicleFloor), false, this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Where + FVector(0.f, 0.f, 400.f), Where - FVector(0.f, 0.f, 2000.f), ECC_Visibility, Params))
		{
			Spot = Hit.ImpactPoint;
		}
		SetActorLocationAndRotation(Spot, FRotator(0.f, Yaw, 0.f));
	}
	OnRep_Combined();
}

void ARRRandomVehicle::OnRep_Combined()
{
	// Part of the robot: out of sight and out of the way (the robot stands where the cars were gathered, often right on
	// one of them)
	SetActorHiddenInGame(bCombined);
	SetActorEnableCollision(!bCombined);
	SkidSpots.Reset();
}

float ARRRandomVehicle::GetHeight() const
{
	const UStaticMesh* Mesh = Body->GetStaticMesh();
	return Mesh ? Mesh->GetBoundingBox().Max.Z : 100.f;
}

float ARRRandomVehicle::GetHalfWidth() const
{
	const UStaticMesh* Mesh = Body->GetStaticMesh();
	return Mesh ? Mesh->GetBounds().BoxExtent.Y : 50.f;
}

int32 ARRRandomVehicle::Mount(ARRRandomCharacter* NewRider)
{
	// A plane has room only for the one it pops in under (never offered to anyone walking by: HasRoom)
	const bool bPlaneFree = IsPlane() && !Rider && !bCrashing;
	if (!HasAuthority() || !NewRider || (!HasRoom() && !bPlaneFree))
	{
		return -1;
	}
	if (Rider)
	{
		// The rear seat: the character attaches itself to the vehicle
		Passenger = NewRider;
		return 1;
	}
	Rider = NewRider;
	// What the car runs into is the driver's doing (kills go to them)
	SetInstigator(NewRider);
	// Under the rider's feet with the driver's seat there, turned with it (the vehicle's pivot is the middle of its bottom)
	UCapsuleComponent* Capsule = NewRider->GetCapsuleComponent();
	AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(-GetHandling().DriverSeatX, -GetHandling().DriverSeatY, -Capsule->GetUnscaledCapsuleHalfHeight()));
	SetActorRelativeRotation(FRotator::ZeroRotator);
	SetActorRelativeScale3D(FVector::OneVector);
	return 0;
}

void ARRRandomVehicle::Dismount()
{
	if (!HasAuthority() || !Rider)
	{
		return;
	}
	// Where it is, level and facing the way it went
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
	SetActorScale3D(FVector::OneVector);
	Rider = nullptr;
	SetInstigator(nullptr);
	// A plane is never left parked: put down, it goes (in a puff, EndPlay); shot down, it falls on (UpdatePlane)
	if (IsPlane() && !bCrashing)
	{
		SetLifeSpan(0.05f);
		SetActorHiddenInGame(true);
	}
}

void ARRRandomVehicle::RemovePassenger(ARRRandomCharacter* Who)
{
	if (HasAuthority() && Passenger == Who)
	{
		Passenger = nullptr;
	}
}

void ARRRandomVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCombined)
	{
		return;
	}
	UpdateLights(DeltaSeconds, Rider ? Rider->GetVelocity().Size2D() : 0.f);
	if (IsPlane())
	{
		UpdatePlane(DeltaSeconds);
		return;
	}

	// Leaning into a drift: the angle between where it faces and where it is going, while going fast enough
	const FRRVehicleHandling& Handling = GetHandling();
	float Wanted = 0.f;
	bool bDrifting = false;
	float Speed = 0.f;
	if (Rider && Handling.bDrifts)
	{
		const FVector Velocity = Rider->GetVelocity() * FVector(1.f, 1.f, 0.f);
		Speed = Velocity.Size();
		if (Speed >= DriftMinSpeed)
		{
			const float Drift = FRotator::NormalizeAxis(Velocity.Rotation().Yaw - Rider->GetActorRotation().Yaw);
			if (FMath::Abs(Drift) >= DriftAngleStart)
			{
				// Sliding out to one side leans the body into the turn, the other way
				Wanted = FMath::Clamp(-Drift * Handling.LeanPerDrift, -Handling.MaxLean, Handling.MaxLean);
				bDrifting = Rider->GetCharacterMovement()->IsMovingOnGround();
			}
		}
	}
	Lean = FMath::FInterpConstantTo(Lean, Wanted, DeltaSeconds, LeanRate);
	Body->SetRelativeRotation(FRotator(0.f, 0.f, Lean));
	UpdateSkidMarks(bDrifting, Speed);
	// Round 91: leaping off a jump ramp (in the air and fast), a contrail and rushing wind, like a human cannon shot
	const bool bRushing = Rider && Rider->GetCharacterMovement()->IsFalling() && Rider->GetVelocity().Size2D() > RushMinSpeed;
	if (bRushing && bWasRushing)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->RushTrail(LastRushSpot, GetActorLocation() + FVector(0.f, 0.f, 60.f), Rider->GetVelocity(), DeltaSeconds);
		}
	}
	bWasRushing = bRushing;
	LastRushSpot = GetActorLocation() + FVector(0.f, 0.f, 60.f);
	// An engine that pulls (round 90): wound up near its top speed, streaks of light stream off it
	if (Rider && Handling.SpeedUpTime > 0.f && Speed > Rider->GetCharacterMovement()->MaxWalkSpeed * 0.8f)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->SpeedTrail(GetActorLocation() + FVector(0.f, 0.f, 70.f), GetActorLocation(), Rider->GetVelocity(), FLinearColor(1.f, 0.75f, 0.35f), DeltaSeconds);
		}
	}
	// The star legs car's four rocket nozzles at its tail (two big over two small) burn while it's going
	if (Kind == ERRVehicleKind::StarLegs && Rider && Speed > LegsRocketMinSpeed)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			const FTransform Frame = Body->GetComponentTransform();
			const FVector Back = -Frame.GetUnitAxis(EAxis::X);
			const float Size = FMath::GetMappedRangeValueClamped(FVector2D(LegsRocketMinSpeed, 900.f), FVector2D(0.45f, 0.85f), Speed);
			for (const FVector4f& Nozzle : LegsRocketNozzles)
			{
				Effects->RocketExhaust(Frame.TransformPosition(FVector(Nozzle.X, Nozzle.Y, Nozzle.Z)), Back, Size * Nozzle.W, DeltaSeconds, LegsRocketSmoke);
			}
		}
	}
	if (HasAuthority() && Handling.bRams)
	{
		UpdateRam();
	}
}

void ARRRandomVehicle::UpdateRam()
{
	if (!Rider || !Rider->IsAlive() || !Body->GetStaticMesh())
	{
		return;
	}
	const FVector Velocity = Rider->GetVelocity() * FVector(1.f, 1.f, 0.f);
	const float Speed = Velocity.Size();
	if (Speed < RamMinSpeed)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	RamTimes.RemoveAll([Now](const TPair<TWeakObjectPtr<ARRRandomCharacter>, float>& Entry) { return !Entry.Key.IsValid() || Now - Entry.Value > RamRepeatTime; });
	// The body's box, in the vehicle's own space (its pivot is the middle of its bottom)
	const FBox Box = Body->GetStaticMesh()->GetBoundingBox();
	const FTransform Frame = GetActorTransform();
	const float WalkSpeed = Rider->GetBaseWalkSpeed();
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Victim = *It;
		if (Victim == Rider || Victim == Passenger || !Victim->IsAlive() || Victim->GetTeam() == Rider->GetTeam()
			|| RamTimes.ContainsByPredicate([Victim](const TPair<TWeakObjectPtr<ARRRandomCharacter>, float>& Entry) { return Entry.Key.Get() == Victim; }))
		{
			continue;
		}
		const float Radius = Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float HalfHeight = Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Local = Frame.InverseTransformPosition(Victim->GetActorLocation());
		const bool bTouching = FMath::Abs(Local.X - Box.GetCenter().X) <= Box.GetExtent().X + Radius + RamReach
			&& FMath::Abs(Local.Y - Box.GetCenter().Y) <= Box.GetExtent().Y + Radius + RamReach
			&& Local.Z + HalfHeight >= Box.Min.Z - RamReach && Local.Z - HalfHeight <= Box.Max.Z + RamReach;
		// Only what the car drives into, not someone walking into its side or back as it goes by
		const FVector Toward = (Victim->GetActorLocation() - Rider->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (!bTouching || FVector::DotProduct(Toward, Velocity) <= 0.f)
		{
			continue;
		}
		RamTimes.Emplace(Victim, Now);
		const FVector Direction = Velocity.GetSafeNormal();
		const bool bDeadly = Speed > WalkSpeed;
		const float Damage = bDeadly ? Victim->GetMaxHealth() * 100.f : Victim->GetMaxHealth() * Speed / FMath::Max(WalkSpeed, 1.f);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s rams %s at %.0f (walk %.0f): %s."), *Rider->GetName(), *Victim->GetName(), Speed, WalkSpeed,
			bDeadly ? TEXT("knockout") : *FString::Printf(TEXT("%.0f damage"), Damage));
		FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Victim->GetActorLocation(), -Direction);
		// A knockout from this throws the body off through the air
		Victim->SetRamImpact(Velocity);
		UGameplayStatics::ApplyPointDamage(Victim, Damage, Direction, Hit, Rider->GetController(), this, UDamageType::StaticClass());
		if (Victim->IsAlive())
		{
			Victim->LaunchCharacter(Direction * RamKnock.X * Speed / FMath::Max(WalkSpeed, 1.f) + FVector(0.f, 0.f, RamKnock.Y), true, true);
		}
	}
}

void ARRRandomVehicle::UpdateSkidMarks(bool bDrifting, float Speed)
{
	const FRRVehicleHandling& Handling = GetHandling();
	const UStaticMesh* Mesh = Body->GetStaticMesh();
	if (!bDrifting || Handling.SkidWidth <= 0.f || !Mesh || GetNetMode() == NM_DedicatedServer)
	{
		SkidSpots.Reset();
		return;
	}
	// The rear wheels' ground contact, turned with the vehicle (its pivot is the middle of its bottom)
	const FVector Extent = Mesh->GetBounds().BoxExtent;
	TArray<FVector, TInlineAllocator<2>> Wheels;
	const FVector Rear(-Extent.X * Handling.RearAxleShare, 0.f, 0.f);
	if (Handling.TrackShare > 0.f)
	{
		Wheels.Add(GetActorTransform().TransformPosition(Rear + FVector(0.f, Extent.Y * Handling.TrackShare, 0.f)));
		Wheels.Add(GetActorTransform().TransformPosition(Rear - FVector(0.f, Extent.Y * Handling.TrackShare, 0.f)));
	}
	else
	{
		Wheels.Add(GetActorTransform().TransformPosition(Rear));
	}
	if (SkidSpots.Num() != Wheels.Num())
	{
		SkidSpots = Wheels;
		return;
	}
	// Darker and wider the faster it slides
	const float Fast = FMath::Clamp((Speed - DriftMinSpeed) / FMath::Max(SkidFullSpeed - DriftMinSpeed, 1.f), 0.f, 1.f);
	const float Strength = FMath::Lerp(SkidMinStrength, 1.f, Fast);
	const float Width = Handling.SkidWidth * FMath::Lerp(0.6f, 1.f, Fast);
	ARRRandomSkidMarks* Marks = nullptr;
	for (int32 Index = 0; Index < Wheels.Num(); ++Index)
	{
		if (FVector::Dist2D(SkidSpots[Index], Wheels[Index]) < SkidSegment)
		{
			continue;
		}
		Marks = Marks ? Marks : ARRRandomSkidMarks::Get(GetWorld());
		if (Marks)
		{
			Marks->AddMark(SkidSpots[Index], Wheels[Index], Width, Strength);
		}
		SkidSpots[Index] = Wheels[Index];
	}
}

void ARRRandomVehicle::UpdatePlane(float DeltaSeconds)
{
	// The propeller: a blur flying, idling slowly otherwise
	if (Propeller->IsVisible())
	{
		PropellerTurn = FMath::Fmod(PropellerTurn + (Rider || bCrashing ? PropellerSpin : PropellerIdleSpin) * DeltaSeconds, 360.f);
		Propeller->SetRelativeRotation(FRotator(0.f, 0.f, PropellerTurn));
	}
	if (bExploded)
	{
		return;
	}

	if (bCrashing)
	{
		// Nose down, spinning round and round, trailing fire and smoke
		CrashSpin = FMath::Fmod(CrashSpin + CrashSpinRate * DeltaSeconds, 360.f);
		Body->SetRelativeRotation(FRotator(-35.f, CrashSpin, 25.f));
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->RocketExhaust(GetActorLocation() + FVector(0.f, 0.f, GetHeight() * 0.6f), FVector::UpVector, 0.7f, DeltaSeconds, 1.5f);
		}
		if (!HasAuthority())
		{
			return;
		}
		// Straight down, faster and faster, till it meets the ground (or anything else in the way)
		CrashFallSpeed = FMath::Min(CrashFallSpeed + CrashGravity * DeltaSeconds, CrashMaxFall);
		const FVector From = GetActorLocation();
		const FVector To = From - FVector(0.f, 0.f, CrashFallSpeed * DeltaSeconds);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RRPlaneCrash), false, this);
		for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
		{
			Params.AddIgnoredActor(*It);
		}
		FBox Area(ForceInit);
		const ARRRandomCharacter* Anyone = TActorIterator<ARRRandomCharacter>(GetWorld()) ? *TActorIterator<ARRRandomCharacter>(GetWorld()) : nullptr;
		const bool bHasArea = Anyone && Anyone->GetFlightArea(Area);
		if (GetWorld()->LineTraceSingleByChannel(Hit, From + FVector(0.f, 0.f, 20.f), To, ECC_Visibility, Params))
		{
			Explode(Hit.ImpactPoint);
		}
		else if (bHasArea && To.Z < Area.Min.Z)
		{
			// Off the island's edge: it goes up in the air where the island ends
			Explode(To);
		}
		else
		{
			SetActorLocation(To);
		}
		return;
	}

	// Flying: banking into turns (by how fast the way it goes turns) and nosing up or down with the climb
	float WantedBank = 0.f;
	float WantedPitch = 0.f;
	if (Rider)
	{
		const FVector Velocity = Rider->GetVelocity();
		const float Yaw = Velocity.Rotation().Yaw;
		const float Speed = Velocity.Size2D();
		if (Speed > 80.f && DeltaSeconds > 0.f)
		{
			WantedBank = FMath::Clamp(FMath::FindDeltaAngleDegrees(LastPlaneYaw, Yaw) / DeltaSeconds * PlaneBankPerTurn, -PlaneMaxBank, PlaneMaxBank);
		}
		LastPlaneYaw = Yaw;
		const float TopSpeed = FMath::Max(Rider->GetCharacterMovement()->MaxFlySpeed, 1.f);
		WantedPitch = FMath::Clamp(Velocity.Z / TopSpeed, -1.f, 1.f) * PlaneMaxPitch;
	}
	PlaneBank = FMath::FInterpTo(PlaneBank, WantedBank, DeltaSeconds, PlaneTiltRate);
	PlanePitch = FMath::FInterpTo(PlanePitch, WantedPitch, DeltaSeconds, PlaneTiltRate);
	Body->SetRelativeRotation(FRotator(PlanePitch, 0.f, PlaneBank));
}

float ARRRandomVehicle::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || !IsPlane() || bCrashing || !Rider || PlaneHealth <= 0.f || Rider->IsInvincible() || ARRRandomGameState::IsPlayLocked(this))
	{
		return 0.f;
	}
	// The pilot's own side's shots (theirs, and its own) do nothing
	const ARRRandomCharacter* Attacker = DamageCauser ? Cast<ARRRandomCharacter>(DamageCauser->GetInstigator()) : nullptr;
	if (Attacker && Attacker->GetTeam() == Rider->GetTeam())
	{
		return 0.f;
	}
	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Damage <= 0.f)
	{
		return 0.f;
	}
	PlaneHealth = FMath::Max(0.f, PlaneHealth - Damage);
	// The number floats up over the pilot, with the sticker of the gun that hit
	const ARRRandomProjectile* Shot = Cast<ARRRandomProjectile>(DamageCauser);
	Rider->MulticastDamagePopup(Damage, Shot ? Shot->HitEffect : ERRHitEffect::Pistol, false);
	if (PlaneHealth <= 0.f)
	{
		ShootDown(EventInstigator);
	}
	return Damage;
}

void ARRRandomVehicle::ShootDown(AController* Killer)
{
	if (!HasAuthority() || !IsPlane() || bCrashing)
	{
		return;
	}
	bCrashing = true;
	PlaneHealth = 0.f;
	CrashKiller = Killer;
	CrashFallSpeed = 0.f;
	// The pilot is thrown out and glides down (which lets go of the plane; crashing, it stays and falls)
	if (ARRRandomCharacter* Pilot = Rider)
	{
		CrashTeam = Pilot->GetTeam();
		Pilot->BailOut();
	}
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the %s is shot down by %s."), GetHandling().Name, *GetNameSafe(Killer ? Killer->GetPawn() : nullptr));
}

void ARRRandomVehicle::Explode(const FVector& Where)
{
	if (bExploded)
	{
		return;
	}
	bExploded = true;
	SetActorLocation(Where);
	MulticastExplode(Where);
	// The blocks where it lands are smashed (mended later like any wreck)
	ARRRandomWreckage::Wreck(this, Where, Where, CrashWreckRadius, false, 2);
	// The blast hurts the pilot's side round it, for whoever shot it down
	APawn* KillerPawn = CrashKiller.IsValid() ? CrashKiller->GetPawn() : nullptr;
	SetInstigator(KillerPawn);
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Victim = *It;
		const FVector Offset = Victim->GetActorLocation() - Where;
		if (!Victim->IsAlive() || Victim->GetTeam() != CrashTeam || Offset.Size() > CrashBlastRadius + Victim->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			continue;
		}
		const FVector Direction = Offset.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Offset.GetSafeNormal2D();
		const FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Victim->GetActorLocation(), -Direction);
		UGameplayStatics::ApplyPointDamage(Victim, CrashBlastDamage, Direction, Hit, CrashKiller.Get(), this, UDamageType::StaticClass());
	}
	SetLifeSpan(CrashLinger);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the %s blows up at %s."), GetHandling().Name, *Where.ToString());
}

void ARRRandomVehicle::MulticastExplode_Implementation(const FVector_NetQuantize& Where)
{
	bExploded = true;
	Body->SetVisibility(false, true);
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->RocketBlast(Where, FVector::UpVector, 3.f);
		Effects->FoxPoof(Where + FVector(0.f, 0.f, 80.f), 2.5f);
	}
	ARRRandomHUD::PopComicWord(this, TEXT("KA-BOOM!"), Where + FVector(0.f, 0.f, 220.f), RRUI::PinkDeep, 1.6f);
}

void ARRRandomVehicle::SetStation(ARRRandomPlaneStation* NewStation)
{
	Station = NewStation;
}
