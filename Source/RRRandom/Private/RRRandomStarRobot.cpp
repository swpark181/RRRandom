#include "RRRandomStarRobot.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomGameState.h"
#include "RRRandomProjectile.h"
#include "RRRandomVehicle.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The robot's parts (char-pipeline/scripts/blender_robot_parts.py: 640 tall, front on +X, pivot the middle of its
	// bottom, cut at the hips 315, the neck 465 and the shoulders 100 out), each with the whole robot's pivot
	const TCHAR* PartPaths[] =
	{
		TEXT("/Game/Vehicles/SM_StarRobotLegs.SM_StarRobotLegs"),
		TEXT("/Game/Vehicles/SM_StarRobotTorso.SM_StarRobotTorso"),
		TEXT("/Game/Vehicles/SM_StarRobotHead.SM_StarRobotHead"),
		TEXT("/Game/Vehicles/SM_StarRobotArmL.SM_StarRobotArmL"),
		TEXT("/Game/Vehicles/SM_StarRobotArmR.SM_StarRobotArmR"),
	};
	constexpr float RobotHeight = 640.f;
	// Where the parts turn: the neck, and the shoulders (the left arm on -Y, Unreal's left with the front on +X)
	const FVector NeckPivot(0.f, 0.f, 465.f);
	const FVector ShoulderPivotL(0.f, -190.f, 520.f);
	const FVector ShoulderPivotR(0.f, 190.f, 520.f);
	// A fist, from its shoulder with the arm hanging down
	const FVector FistFromShoulder(10.f, 0.f, -290.f);
	// The jointed parts (char-pipeline/scripts/blender_robot_joints.py, cut at the knees 160 and elbows 390, the pelvis the
	// legs' middle block), each with the whole robot's pivot, and where they turn: the hips, knees and elbows. The forearm
	// hangs forward and down from its elbow to the tip of its fist
	const TCHAR* JointPaths[] =
	{
		TEXT("/Game/Vehicles/SM_StarRobotPelvis.SM_StarRobotPelvis"),
		TEXT("/Game/Vehicles/SM_StarRobotThighL.SM_StarRobotThighL"),
		TEXT("/Game/Vehicles/SM_StarRobotThighR.SM_StarRobotThighR"),
		TEXT("/Game/Vehicles/SM_StarRobotShinL.SM_StarRobotShinL"),
		TEXT("/Game/Vehicles/SM_StarRobotShinR.SM_StarRobotShinR"),
		TEXT("/Game/Vehicles/SM_StarRobotUpperArmL.SM_StarRobotUpperArmL"),
		TEXT("/Game/Vehicles/SM_StarRobotUpperArmR.SM_StarRobotUpperArmR"),
		TEXT("/Game/Vehicles/SM_StarRobotForearmL.SM_StarRobotForearmL"),
		TEXT("/Game/Vehicles/SM_StarRobotForearmR.SM_StarRobotForearmR"),
	};
	const FVector HipPivotL(35.f, -86.f, 300.f);
	const FVector HipPivotR(35.f, 86.f, 300.f);
	const FVector KneePivotL(36.f, -100.f, 160.f);
	const FVector KneePivotR(36.f, 100.f, 160.f);
	const FVector ElbowPivotL(-5.f, -178.f, 392.f);
	const FVector ElbowPivotR(-5.f, 178.f, 392.f);
	const FVector FistTipL(67.f, -232.f, 212.f);
	const FVector FistTipR(67.f, 232.f, 212.f);
	// The feet's soles, under each shin (for the dust of a step)
	const FVector SoleL(36.f, -112.f, 5.f);
	const FVector SoleR(36.f, 112.f, 5.f);
	// What shots hit
	constexpr float HullRadius = 190.f;
	constexpr float HullHalfHeight = 320.f;

	// The rocket fists: one every this many seconds (each arm in turn), this much more damage than a pistol shot, this
	// big, this far, this fast; and the upper body turns to the aim this fast (degrees a second), back to the front after
	// this long without a shot
	constexpr float FistInterval = 0.5f;
	constexpr float FistDamage = 4.5f;
	// The forearm flies off as the rocket (ARRRandomProjectile::MakeRocketFist), this wide for hits, and grows back on
	// this long after
	constexpr float FistRadius = 55.f;
	constexpr float FistRange = 2600.f;
	constexpr float FistSpeed = 0.55f;
	constexpr float ForearmRegrow = 0.45f;
	constexpr float ForearmPopTime = 0.25f;
	// A fist that lands on a structure smashes a ball this big out of it (ARRRandomWreckage)
	constexpr float FistWreckRadius = 250.f;
	constexpr float AimTurnRate = 300.f;
	constexpr float AimHoldTime = 2.f;
	// Arms up in the aim this long after a fist flew
	constexpr float ArmsUpTime = 1.6f;

	// The shield: takes all but this share of the damage; one held press keeps it up this long; its energy lasts this
	// many seconds and refills in this many; emptied, it can't go up again until this full
	constexpr float ShieldDamageShare = 0.15f;
	constexpr float ShieldHold = 0.35f;
	constexpr float ShieldSeconds = 5.f;
	constexpr float ShieldRefillSeconds = 9.f;
	constexpr float ShieldRearm = 0.3f;
	constexpr float ShieldRadius = 400.f;

	// Walking, a step at a time: each leg swings this far either way from the hip (a step this long, so the planted foot
	// doesn't slide: 2 x leg length x sin(swing)), the knee of the swinging leg bends this much to lift its foot, and the
	// hips sink as the legs open (the planted leg stays straight on the floor); the hips lean over the planted foot and
	// turn with the swinging leg, the chest turns back the other way and leans into the walk, the arms swing against the
	// legs with the elbows bending as they come forward, each foot comes down with a jolt and a puff of dust
	constexpr float LegLength = 300.f;
	constexpr float StepSwing = 24.f;
	constexpr float StrideLength = 2.f * LegLength * 0.4067f;
	constexpr float KneeLift = 42.f;
	constexpr float SwingHipLift = 0.3f;
	constexpr float WalkRoll = 3.5f;
	constexpr float WalkHipYaw = 6.f;
	constexpr float ChestCounterYaw = 1.8f;
	constexpr float WalkLean = 4.f;
	constexpr float WalkArmSwing = 20.f;
	constexpr float WalkElbow = 14.f;
	constexpr float RestElbow = 6.f;
	constexpr float StepJoltSink = 12.f;
	// Raised into the aim, the forearms straighten out level with it
	constexpr float AimElbow = -28.f;
	// Turning on the spot, each degree a second steps like this much walking speed
	constexpr float TurnStepShare = 2.5f;

	// Where the cars come back out when it breaks: thrown this far round it (past GatherRadius, so it takes gathering
	// them again), onto floor no more than this far above or below its feet; failing that, close round it
	constexpr float BreakSpread = 1050.f;
	constexpr float BreakNearSpread = 340.f;
	constexpr float BreakFloorStep = 150.f;

	// The combining, seconds from the start: the cars hop, fly up into a ring and whirl, turn into the parts, the legs
	// land, the chest drops on, the arms swoop in, the head pops up, a last pose
	constexpr float HopEnd = 1.f;
	constexpr float LaunchEnd = 3.f;
	constexpr float WhirlEnd = 4.2f;
	constexpr float LegsLand = 5.6f;
	constexpr float ChestLand = 6.8f;
	constexpr float ArmsStart = 6.8f;
	constexpr float ArmsLock = 7.6f;
	constexpr float HeadStart = 8.f;
	constexpr float HeadUp = 8.5f;
	constexpr float PoseStart = 8.9f;
	// The beats: a pop of light at each (PlayBeat)
	const float BeatTimes[] = { 0.f, LaunchEnd, WhirlEnd, LegsLand, ChestLand, ArmsLock, HeadUp, 9.3f };
	// The ring the cars whirl round, and how high
	constexpr float WhirlRadius = 280.f;
	constexpr float WhirlHeight = 560.f;

	const FLinearColor StarCyan(0.35f, 0.95f, 1.f);
	const FLinearColor StarGold(1.f, 0.78f, 0.25f);
	const FLinearColor StarViolet(0.62f, 0.45f, 1.f);

	float EaseOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return 1.f - (1.f - T) * (1.f - T); }
	float EaseIn(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T; }
	float EaseInOut(float T) { T = FMath::Clamp(T, 0.f, 1.f); return T * T * (3.f - 2.f * T); }
	// Past the end and back: a cartoon snap
	float BackOut(float T, float Overshoot = 1.8f)
	{
		T = FMath::Clamp(T, 0.f, 1.f) - 1.f;
		return 1.f + T * T * ((Overshoot + 1.f) * T + Overshoot);
	}
	float Phase(float Time, float Start, float End) { return FMath::Clamp((Time - Start) / FMath::Max(End - Start, KINDA_SMALL_NUMBER), 0.f, 1.f); }
	// A pop: from small up past full size and settling, over a quarter second from Start
	float Pop(float Time, float Start) { return Time < Start ? 0.f : FMath::Lerp(0.35f, 1.f, BackOut(Phase(Time, Start, Start + 0.25f), 3.f)); }
	// A squash on landing: 0 to 1 and back over a quarter second from the landing
	float Squash(float Time, float Landed) { const float T = Phase(Time, Landed, Landed + 0.3f); return Time < Landed ? 0.f : FMath::Sin(T * PI) * (1.f - T * 0.5f); }

	int32 SeatIndex(ERRRobotSeat Seat) { return Seat == ERRRobotSeat::Move ? 0 : Seat == ERRRobotSeat::Attack ? 1 : Seat == ERRRobotSeat::Defend ? 2 : -1; }
	ERRRobotSeat IndexSeat(int32 Index) { return Index == 0 ? ERRRobotSeat::Move : Index == 1 ? ERRRobotSeat::Attack : ERRRobotSeat::Defend; }

	float ServerNow(const UWorld* World)
	{
		const AGameStateBase* State = World ? World->GetGameState() : nullptr;
		return State ? State->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.f);
	}
}

ARRRandomStarRobot::ARRRandomStarRobot()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = true;
	SetReplicateMovement(true);
	bAlwaysRelevant = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Shots hit it (they are world-dynamic and block everything); animal friends, sight and the camera go through
	Hull = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Hull"));
	Hull->SetupAttachment(Root);
	Hull->InitCapsuleSize(HullRadius, HullHalfHeight);
	Hull->SetRelativeLocation(FVector(0.f, 0.f, HullHalfHeight));
	Hull->SetCollisionObjectType(ECC_WorldDynamic);
	Hull->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Hull->SetCollisionResponseToAllChannels(ECR_Ignore);
	Hull->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Hull->SetCanEverAffectNavigation(false);

	Hips = CreateDefaultSubobject<USceneComponent>(TEXT("Hips"));
	Hips->SetupAttachment(Root);
	Upper = CreateDefaultSubobject<USceneComponent>(TEXT("Upper"));
	Upper->SetupAttachment(Hips);
	Neck = CreateDefaultSubobject<USceneComponent>(TEXT("Neck"));
	Neck->SetupAttachment(Upper);
	Neck->SetRelativeLocation(NeckPivot);
	ShoulderL = CreateDefaultSubobject<USceneComponent>(TEXT("ShoulderL"));
	ShoulderL->SetupAttachment(Upper);
	ShoulderL->SetRelativeLocation(ShoulderPivotL);
	ShoulderR = CreateDefaultSubobject<USceneComponent>(TEXT("ShoulderR"));
	ShoulderR->SetupAttachment(Upper);
	ShoulderR->SetRelativeLocation(ShoulderPivotR);

	for (const TCHAR* Path : PartPaths)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(Path);
		PartMeshes.Add(Mesh.Succeeded() ? Mesh.Object : nullptr);
	}
	auto MakePart = [this](const TCHAR* Name, USceneComponent* Parent, int32 Index, const FVector& Offset)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetStaticMesh(PartMeshes[Index]);
		Part->SetRelativeLocation(Offset);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		// Inside the toon silhouette line, like the animal friends and the vehicles
		Part->SetRenderCustomDepth(true);
		Part->SetCustomDepthStencilValue(1);
		Part->SetReceivesDecals(false);
		return Part;
	};
	Legs = MakePart(TEXT("Legs"), Hips, 0, FVector::ZeroVector);
	Torso = MakePart(TEXT("Torso"), Upper, 1, FVector::ZeroVector);
	Head = MakePart(TEXT("Head"), Neck, 2, -NeckPivot);
	ArmL = MakePart(TEXT("ArmL"), ShoulderL, 3, -ShoulderPivotL);
	ArmR = MakePart(TEXT("ArmR"), ShoulderR, 4, -ShoulderPivotR);

	// The jointed legs and arms, shown once it's combined
	TArray<UStaticMesh*> Joints;
	for (const TCHAR* Path : JointPaths)
	{
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(Path);
		Joints.Add(Mesh.Succeeded() ? Mesh.Object : nullptr);
	}
	bHasJoints = !Joints.Contains(nullptr);
	auto MakeJoint = [this](const TCHAR* Name, USceneComponent* Parent, const FVector& At)
	{
		USceneComponent* Joint = CreateDefaultSubobject<USceneComponent>(Name);
		Joint->SetupAttachment(Parent);
		Joint->SetRelativeLocation(At);
		return Joint;
	};
	auto MakeJointPart = [&](const TCHAR* Name, USceneComponent* Parent, int32 Index, const FVector& Offset)
	{
		UStaticMeshComponent* Part = MakePart(Name, Parent, 0, Offset);
		Part->SetStaticMesh(Joints[Index]);
		Part->SetVisibility(false);
		return Part;
	};
	HipL = MakeJoint(TEXT("HipL"), Hips, HipPivotL);
	HipR = MakeJoint(TEXT("HipR"), Hips, HipPivotR);
	KneeL = MakeJoint(TEXT("KneeL"), HipL, KneePivotL - HipPivotL);
	KneeR = MakeJoint(TEXT("KneeR"), HipR, KneePivotR - HipPivotR);
	ElbowL = MakeJoint(TEXT("ElbowL"), ShoulderL, ElbowPivotL - ShoulderPivotL);
	ElbowR = MakeJoint(TEXT("ElbowR"), ShoulderR, ElbowPivotR - ShoulderPivotR);
	Pelvis = MakeJointPart(TEXT("Pelvis"), Hips, 0, FVector::ZeroVector);
	ThighL = MakeJointPart(TEXT("ThighL"), HipL, 1, -HipPivotL);
	ThighR = MakeJointPart(TEXT("ThighR"), HipR, 2, -HipPivotR);
	ShinL = MakeJointPart(TEXT("ShinL"), KneeL, 3, -KneePivotL);
	ShinR = MakeJointPart(TEXT("ShinR"), KneeR, 4, -KneePivotR);
	UpperArmL = MakeJointPart(TEXT("UpperArmL"), ShoulderL, 5, -ShoulderPivotL);
	UpperArmR = MakeJointPart(TEXT("UpperArmR"), ShoulderR, 6, -ShoulderPivotR);
	ForearmL = MakeJointPart(TEXT("ForearmL"), ElbowL, 7, -ElbowPivotL);
	ForearmR = MakeJointPart(TEXT("ForearmR"), ElbowR, 8, -ElbowPivotR);

	for (const TCHAR* Name : { TEXT("ChestCar"), TEXT("ArmsCar"), TEXT("LegsCar") })
	{
		UStaticMeshComponent* Car = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Car->SetupAttachment(Root);
		Car->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Car->SetCanEverAffectNavigation(false);
		Car->SetRenderCustomDepth(true);
		Car->SetCustomDepthStencilValue(1);
		Car->SetReceivesDecals(false);
		Car->SetUsingAbsoluteLocation(true);
		Car->SetUsingAbsoluteRotation(true);
		Car->SetUsingAbsoluteScale(true);
		CarMeshes.Add(Car);
	}

	// A glassy bubble round it (the cloak material: a fresnel rim that glows)
	Shield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shield"));
	Shield->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Shield->SetStaticMesh(Sphere.Object);
	}
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cloak(TEXT("/Game/Effects/DiceFX/M_FXCloak.M_FXCloak"));
	if (Cloak.Succeeded())
	{
		Shield->SetMaterial(0, Cloak.Object);
	}
	Shield->SetRelativeLocation(FVector(0.f, 0.f, RobotHeight * 0.48f));
	Shield->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shield->SetCastShadow(false);
	Shield->SetVisibility(false);
}

void ARRRandomStarRobot::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomStarRobot, Cars);
	DOREPLIFETIME(ARRRandomStarRobot, CarStarts);
	DOREPLIFETIME(ARRRandomStarRobot, CarYaws);
	DOREPLIFETIME(ARRRandomStarRobot, Combiner);
	DOREPLIFETIME(ARRRandomStarRobot, CombineStartTime);
	DOREPLIFETIME(ARRRandomStarRobot, bAssembled);
	DOREPLIFETIME(ARRRandomStarRobot, Team);
	DOREPLIFETIME(ARRRandomStarRobot, Crew);
	DOREPLIFETIME(ARRRandomStarRobot, Health);
	DOREPLIFETIME(ARRRandomStarRobot, bShieldUp);
	DOREPLIFETIME(ARRRandomStarRobot, ShieldEnergy);
	DOREPLIFETIME(ARRRandomStarRobot, AimYaw);
	DOREPLIFETIME(ARRRandomStarRobot, ShotCount);
	DOREPLIFETIME(ARRRandomStarRobot, bIntroTurn);
	DOREPLIFETIME(ARRRandomStarRobot, IntroTurnYaw);
}

void ARRRandomStarRobot::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Material = Shield->GetMaterial(0))
	{
		ShieldMaterial = Shield->CreateDynamicMaterialInstance(0, Material);
		if (ShieldMaterial)
		{
			ShieldMaterial->SetVectorParameterValue(TEXT("Color"), StarCyan);
		}
	}
	Shield->SetWorldScale3D(FVector(ShieldRadius / 50.f));
	// A client joining after it was done: no combining to play
	BeatsPlayed = bAssembled ? UE_ARRAY_COUNT(BeatTimes) : 0;
	OnRep_Assembled();
}

void ARRRandomStarRobot::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Gone without breaking apart (the level closing): nobody left inside
	if (HasAuthority())
	{
		for (ARRRandomCharacter* Member : TArray<ARRRandomCharacter*>(Crew))
		{
			if (Member && Member->GetRobot() == this)
			{
				Member->SetRobotSeat(nullptr, ERRRobotSeat::None);
			}
		}
		if (Combiner && Combiner->GetRobot() == this)
		{
			Combiner->SetRobotSeat(nullptr, ERRRobotSeat::None);
		}
	}
	Super::EndPlay(EndPlayReason);
}

ARRRandomStarRobot* ARRRandomStarRobot::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ARRRandomStarRobot> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

bool ARRRandomStarRobot::FindGathered(const UObject* WorldContext, ARRRandomVehicle* OutCars[3], FVector& OutCenter)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || Get(World))
	{
		return false;
	}
	OutCars[0] = OutCars[1] = OutCars[2] = nullptr;
	for (TActorIterator<ARRRandomVehicle> It(World); It; ++It)
	{
		const int32 Part = It->GetStarPart();
		if (Part >= 0 && Part < 3 && !It->IsCombined() && !OutCars[Part])
		{
			OutCars[Part] = *It;
		}
	}
	if (!OutCars[0] || !OutCars[1] || !OutCars[2])
	{
		return false;
	}
	OutCenter = (OutCars[0]->GetActorLocation() + OutCars[1]->GetActorLocation() + OutCars[2]->GetActorLocation()) / 3.f;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (FVector::Dist2D(OutCars[Index]->GetActorLocation(), OutCenter) > GatherRadius)
		{
			return false;
		}
	}
	// On the floor: the lowest of the three (a car on cover or stairs shouldn't lift the robot off the ground)
	OutCenter.Z = FMath::Min3(OutCars[0]->GetActorLocation().Z, OutCars[1]->GetActorLocation().Z, OutCars[2]->GetActorLocation().Z);
	return true;
}

bool ARRRandomStarRobot::CanStartCombine(const ARRRandomCharacter* Who)
{
	if (!Who || !Who->IsAlive() || Who->IsDecoy() || Who->GetRobot() || Who->IsGiant() || Who->CanFly() || ARRRandomGameState::IsPlayLocked(Who))
	{
		return false;
	}
	ARRRandomVehicle* Found[3];
	FVector Center;
	return FindGathered(Who, Found, Center) && FVector::Dist2D(Who->GetActorLocation(), Center) <= CombineReach;
}

ARRRandomStarRobot* ARRRandomStarRobot::StartCombine(ARRRandomCharacter* Who)
{
	UWorld* World = Who ? Who->GetWorld() : nullptr;
	ARRRandomVehicle* Found[3];
	FVector Center;
	if (!World || !Who->HasAuthority() || !CanStartCombine(Who) || !FindGathered(Who, Found, Center))
	{
		return nullptr;
	}
	// Facing the one combining it, so it comes together (and strikes its pose) toward that one's camera
	const FVector Toward = (Who->GetActorLocation() - Center) * FVector(1.f, 1.f, 0.f);
	const FTransform SpawnTransform(FRotator(0.f, Toward.IsNearlyZero() ? Who->GetActorRotation().Yaw + 180.f : Toward.Rotation().Yaw, 0.f), Center);
	ARRRandomStarRobot* Robot = World->SpawnActorDeferred<ARRRandomStarRobot>(ARRRandomStarRobot::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Robot)
	{
		return nullptr;
	}
	Robot->Team = Who->GetTeam();
	Robot->Combiner = Who;
	Robot->CombineStartTime = ServerNow(World);
	Robot->Health = Robot->MaxHealth;
	Robot->Crew.Init(nullptr, 3);
	for (ARRRandomVehicle* Car : Found)
	{
		// Everyone off: the cars are about to fly
		for (ARRRandomCharacter* Rider : { Car->GetPassenger(), Car->GetRider() })
		{
			if (Rider)
			{
				Rider->GetOffVehicle();
			}
		}
		Robot->Cars.Add(Car);
		Robot->CarStarts.Add(Car->GetActorLocation());
		Robot->CarYaws.Add(Car->GetActorRotation().Yaw);
		Car->SetCombined(true);
	}
	Who->GetOffVehicle();
	Who->SetRobotSeat(Robot, ERRRobotSeat::Combiner);
	Robot->FinishSpawning(SpawnTransform);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) starts combining the star robot, facing it."), *Who->GetName(), Who->GetTeam());
	return Robot;
}

float ARRRandomStarRobot::GetCombineProgress() const
{
	return bAssembled ? 1.f : FMath::Clamp((ServerNow(GetWorld()) - CombineStartTime) / CombineSeconds, 0.f, 1.f);
}

ARRRandomCharacter* ARRRandomStarRobot::GetCrew(ERRRobotSeat Seat) const
{
	const int32 Index = SeatIndex(Seat);
	return Crew.IsValidIndex(Index) ? Crew[Index].Get() : nullptr;
}

int32 ARRRandomStarRobot::CountCrew() const
{
	int32 Count = 0;
	for (const ARRRandomCharacter* Member : Crew)
	{
		Count += Member ? 1 : 0;
	}
	return Count;
}

ERRRobotSeat ARRRandomStarRobot::GetFreeSeat() const
{
	for (int32 Index = 0; Index < 3; ++Index)
	{
		if (!Crew.IsValidIndex(Index) || !Crew[Index])
		{
			return IndexSeat(Index);
		}
	}
	return ERRRobotSeat::None;
}

bool ARRRandomStarRobot::CanBoard(const ARRRandomCharacter* Who, bool bAnyDistance) const
{
	return bAssembled && Who && Who->IsAlive() && !Who->IsDecoy() && !Who->GetRobot() && !Who->IsGiant() && !Who->CanFly() && Who->GetTeam() == Team
		&& GetFreeSeat() != ERRRobotSeat::None && (bAnyDistance || FVector::Dist2D(Who->GetActorLocation(), GetActorLocation()) <= BoardReach * GetActorScale3D().Z);
}

void ARRRandomStarRobot::Board(ARRRandomCharacter* Who, bool bAnyDistance)
{
	if (!HasAuthority() || !CanBoard(Who, bAnyDistance))
	{
		return;
	}
	const ERRRobotSeat Seat = GetFreeSeat();
	Who->GetOffVehicle();
	Crew[SeatIndex(Seat)] = Who;
	if (Seat == ERRRobotSeat::Move)
	{
		// The driver's capsule goes to the robot's feet and carries it from now on (like a vehicle under it)
		UCapsuleComponent* Capsule = Who->GetCapsuleComponent();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		Who->TeleportTo(GetActorLocation() + FVector(0.f, 0.f, HalfHeight + 2.f), FRotator(0.f, GetActorRotation().Yaw, 0.f), false, true);
		Who->SetRobotSeat(this, Seat);
		AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(FVector(0.f, 0.f, -Capsule->GetUnscaledCapsuleHalfHeight()));
		SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	else
	{
		Who->SetRobotSeat(this, Seat);
	}
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->TeleportPoof(Who->GetActorLocation(), StarCyan);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s gets into the star robot (seat %d, crew %d/3)."), *Who->GetName(), SeatIndex(Seat), CountCrew());
}

void ARRRandomStarRobot::Leave(ARRRandomCharacter* Who)
{
	if (!HasAuthority() || !Who)
	{
		return;
	}
	if (Who == Combiner && !bAssembled)
	{
		BreakApart(true);
		return;
	}
	ClearSeat(Who);
}

void ARRRandomStarRobot::ClearSeat(ARRRandomCharacter* Who)
{
	const int32 Index = Crew.IndexOfByKey(Who);
	if (Index == INDEX_NONE || !Who)
	{
		return;
	}
	if (Index == 0)
	{
		// Without its driver it stands where it is, level
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		SetActorRotation(FRotator(0.f, GetActorRotation().Yaw, 0.f));
		SetActorScale3D(FVector::OneVector);
	}
	Crew[Index] = nullptr;
	if (Index == 2)
	{
		bShieldUp = false;
	}
	Who->SetRobotSeat(nullptr, ERRRobotSeat::None);
	// Out beside it, right or left, whichever is clear
	if (Who->IsAlive())
	{
		const float Step = HullRadius + Who->GetCapsuleComponent()->GetScaledCapsuleRadius() + 30.f;
		const FVector Feet = GetActorLocation() + FVector(0.f, 0.f, Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.f);
		for (const float Side : { 1.f, -1.f, 0.f })
		{
			const FVector Out = Side != 0.f ? GetActorRightVector() * Step * Side : -GetActorForwardVector() * Step;
			if (Who->TeleportTo(Feet + Out, FRotator(0.f, GetActorRotation().Yaw, 0.f)))
			{
				break;
			}
		}
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->TeleportPoof(Who->GetActorLocation(), StarCyan);
		}
	}
}

bool ARRRandomStarRobot::IsDamageProxy(const ARRRandomCharacter* Who) const
{
	for (const ARRRandomCharacter* Member : Crew)
	{
		if (Member)
		{
			return Member == Who;
		}
	}
	return false;
}

void ARRRandomStarRobot::CrewFire(ARRRandomCharacter* Who, const FVector& Target)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !bAssembled || !World || ARRRandomGameState::IsPlayLocked(this))
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Who == GetCrew(ERRRobotSeat::Defend))
	{
		if (!bShieldSpent && ShieldEnergy > 0.f)
		{
			bShieldUp = true;
			ShieldHoldUntil = Now + ShieldHold;
		}
		return;
	}
	if (Who != GetCrew(ERRRobotSeat::Attack))
	{
		return;
	}
	// The upper body turns to the aim (relative to the legs)
	const FVector Toward = (Target - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (!Toward.IsNearlyZero())
	{
		AimYaw = FRotator::NormalizeAxis(Toward.Rotation().Yaw - GetActorRotation().Yaw);
	}
	if (Now < NextFistTime)
	{
		return;
	}
	NextFistTime = Now + FistInterval;
	++ShotCount;
	OnRep_Shot();
	// The forearm of the arm whose turn it is, held out level toward the aim, flies off from where it is (its middle:
	// past the upper arm, half the forearm out)
	const bool bLeft = (ShotCount & 1) != 0;
	const FRotator Facing(0.f, GetActorRotation().Yaw + AimYaw, 0.f);
	const FVector Shoulder = GetActorLocation() + Facing.RotateVector(bLeft ? ShoulderPivotL : ShoulderPivotR);
	const float UpperArm = ShoulderPivotL.Z - ElbowPivotL.Z;
	const FVector Fist = Shoulder + Facing.Vector() * (UpperArm + GetForearmLength() * 0.5f);
	FVector Direction = Target - Fist;
	FRotator ShotRotation = Direction.IsNearlyZero() ? Facing : Direction.Rotation();
	// Down at someone close by (the fists start up at its shoulders), never up into the sky
	ShotRotation.Pitch = FMath::Clamp(ShotRotation.Pitch, -65.f, 10.f);
	if (FMath::Abs(FRotator::NormalizeAxis(ShotRotation.Yaw - Facing.Yaw)) > 30.f)
	{
		ShotRotation.Yaw = Facing.Yaw;
	}
	const FTransform SpawnTransform(ShotRotation, Fist);
	if (ARRRandomProjectile* Projectile = World->SpawnActorDeferred<ARRRandomProjectile>(ARRRandomProjectile::StaticClass(), SpawnTransform, this, Who, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Projectile->MakeRocketFist(bLeft, FistRadius);
		Projectile->DamageMultiplier = FistDamage;
		Projectile->WreckRadius = FistWreckRadius;
		Projectile->MaxRange = FistRange;
		Projectile->SetSpeedMultiplier(FistSpeed);
		Projectile->HitEffect = ERRHitEffect::Cannon;
		Projectile->Tier = ERRWeaponTier::Legendary;
		Projectile->FinishSpawning(SpawnTransform);
	}
}

float ARRRandomStarRobot::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const ARRRandomCharacter* Attacker = DamageCauser ? Cast<ARRRandomCharacter>(DamageCauser->GetInstigator()) : nullptr;
	if (!Attacker && EventInstigator)
	{
		Attacker = Cast<ARRRandomCharacter>(EventInstigator->GetPawn());
	}
	if (!HasAuthority() || !bAssembled || IsActorBeingDestroyed() || DamageAmount <= 0.f || (Attacker && Attacker->GetTeam() == Team) || (Attacker && Attacker->IsDecoy())
		|| ARRRandomGameState::IsPlayLocked(this))
	{
		return 0.f;
	}
	const float Taken = DamageAmount * (bShieldUp ? ShieldDamageShare : 1.f);
	Health = FMath::Max(0.f, Health - Taken);
	if (Health <= 0.f)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: the star robot (team %d) breaks apart (last hit by %s)."), Team, *GetNameSafe(Attacker));
		BreakApart(false);
	}
	return Taken;
}

void ARRRandomStarRobot::FinishCombine()
{
	bAssembled = true;
	ARRRandomCharacter* Who = Combiner;
	Combiner = nullptr;
	if (Who && Who->GetRobot() == this)
	{
		Who->SetRobotSeat(nullptr, ERRRobotSeat::None);
	}
	OnRep_Assembled();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the star robot (team %d) is combined."), Team);
	// The one who combined it gets the first seat, wherever it stood; having struck its pose toward that one, it turns
	// round to face the way that one was looking, ready to go (the driver turns it: ARRRandomCharacter)
	IntroTurnYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw + 180.f);
	bIntroTurn = true;
	if (Who && Who->IsAlive())
	{
		Board(Who, true);
	}
}

void ARRRandomStarRobot::BreakApart(bool bBackToStart)
{
	if (IsActorBeingDestroyed())
	{
		return;
	}
	for (ARRRandomCharacter* Member : TArray<ARRRandomCharacter*>(Crew))
	{
		if (Member)
		{
			ClearSeat(Member);
		}
	}
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (Combiner && Combiner->GetRobot() == this)
	{
		Combiner->SetRobotSeat(nullptr, ERRRobotSeat::None);
	}
	Combiner = nullptr;
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	const float Yaw = GetActorRotation().Yaw;
	for (int32 Index = 0; Index < Cars.Num(); ++Index)
	{
		ARRRandomVehicle* Car = Cars[Index];
		if (!Car)
		{
			continue;
		}
		FVector Spot = bBackToStart && CarStarts.IsValidIndex(Index) ? CarStarts[Index]
			: GetActorLocation() + FRotator(0.f, Yaw + 120.f * Index, 0.f).Vector() * BreakNearSpread;
		if (!bBackToStart)
		{
			// Thrown far, a little to either side if that lands on a roof or off the edge
			for (const float Turn : { 0.f, 25.f, -25.f, 50.f, -50.f })
			{
				const FVector Far = GetActorLocation() + FRotator(0.f, Yaw + 120.f * Index + Turn, 0.f).Vector() * BreakSpread;
				FHitResult Hit;
				FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRobotBreak), false, this);
				if (GetWorld()->LineTraceSingleByChannel(Hit, Far + FVector(0.f, 0.f, 500.f), Far - FVector(0.f, 0.f, 1500.f), ECC_Visibility, Params)
					&& FMath::Abs(Hit.ImpactPoint.Z - GetActorLocation().Z) <= BreakFloorStep)
				{
					Spot = Hit.ImpactPoint;
					break;
				}
			}
		}
		const float CarYaw = bBackToStart && CarYaws.IsValidIndex(Index) ? CarYaws[Index] : Yaw + 120.f * Index + 90.f;
		Car->SetCombined(false, Spot, CarYaw);
		if (Effects)
		{
			Effects->TeleportPoof(Car->GetActorLocation() + FVector(0.f, 0.f, 80.f), StarViolet);
		}
	}
	// Everyone sees it go (the cars' poofs above only show on the server's screen)
	MulticastBreak(bBackToStart);
	Destroy();
}

void ARRRandomStarRobot::MulticastBreak_Implementation(bool bFizzle)
{
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	if (!Effects)
	{
		return;
	}
	const FVector Middle = GetActorLocation() + FVector(0.f, 0.f, RobotHeight * 0.5f);
	if (bFizzle)
	{
		// Called off: a puff of smoke where it was coming together
		Effects->FoxPoof(Middle, 2.f);
		for (const FVector& Start : CarStarts)
		{
			Effects->TeleportPoof(Start + FVector(0.f, 0.f, 80.f), StarViolet);
		}
		return;
	}
	// Broken: a burst of stars, smoke and gold
	Effects->FireBurst(Middle, 2.f);
	Effects->GiantBurst(GetActorLocation(), 3.f, StarGold);
	Effects->FoxPoof(Middle, 1.6f);
}

void ARRRandomStarRobot::OnRep_Assembled()
{
	// Done: the parts make the robot and shots can hit it; combining: the cars fly about instead
	Hull->SetCollisionEnabled(bAssembled ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	for (UStaticMeshComponent* Car : CarMeshes)
	{
		Car->SetVisibility(!bAssembled);
	}
	if (bAssembled)
	{
		for (UStaticMeshComponent* Part : { Legs.Get(), Torso.Get(), Head.Get(), ArmL.Get(), ArmR.Get() })
		{
			Part->SetVisibility(true);
			Part->SetWorldScale3D(FVector::OneVector);
		}
		ShowJointed(bHasJoints);
		Hips->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Upper->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Neck->SetRelativeLocationAndRotation(NeckPivot, FRotator::ZeroRotator);
		ShoulderL->SetRelativeLocationAndRotation(ShoulderPivotL, FRotator::ZeroRotator);
		ShoulderR->SetRelativeLocationAndRotation(ShoulderPivotR, FRotator::ZeroRotator);
		Legs->SetRelativeLocation(FVector::ZeroVector);
		Torso->SetRelativeLocation(FVector::ZeroVector);
		LastPoseYaw = GetActorRotation().Yaw;
		if (AssembledSeenTime < 0.f && GetWorld())
		{
			AssembledSeenTime = GetWorld()->GetTimeSeconds();
		}
	}
}

void ARRRandomStarRobot::OnRep_Shot()
{
	// Punch with the arm whose turn it was (more than one fist since last seen: the latest)
	if (ShotCount == LastShotSeen)
	{
		return;
	}
	LastShotSeen = ShotCount;
	const bool bLeft = (ShotCount & 1) != 0;
	(bLeft ? PunchL : PunchR) = 1.f;
	const FVector Facing = FRotator(0.f, GetActorRotation().Yaw + AimYaw, 0.f).Vector();
	// The forearm is the rocket: it leaves the arm (the shot carries it from here) and grows back on a moment later
	FVector Launch = (bLeft ? ShoulderL : ShoulderR)->GetComponentLocation() + Facing * -FistFromShoulder.Z;
	if (bHasJoints && bAssembled)
	{
		UStaticMeshComponent* Forearm = bLeft ? ForearmL.Get() : ForearmR.Get();
		Launch = Forearm->Bounds.Origin;
		Forearm->SetVisibility(false);
		(bLeft ? ForearmBackL : ForearmBackR) = GetWorld()->GetTimeSeconds() + ForearmRegrow;
	}
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->RocketLaunch(Launch, Facing, StarGold);
	}
}

UStaticMesh* ARRRandomStarRobot::LoadForearm(bool bLeft)
{
	const TCHAR* Path = JointPaths[bLeft ? 7 : 8];
	return LoadObject<UStaticMesh>(nullptr, Path);
}

FTransform ARRRandomStarRobot::GetForearmFlightPose(bool bLeft)
{
	// Turned so the elbow-to-fist line runs along the shot, its middle on the shot
	const FVector Elbow = bLeft ? ElbowPivotL : ElbowPivotR;
	const FVector Tip = bLeft ? FistTipL : FistTipR;
	const FQuat Turn = FQuat::FindBetweenNormals((Tip - Elbow).GetSafeNormal(), FVector::ForwardVector);
	return FTransform(Turn, -Turn.RotateVector((Elbow + Tip) * 0.5f));
}

float ARRRandomStarRobot::GetForearmLength()
{
	return FVector::Dist(ElbowPivotL, FistTipL);
}

void ARRRandomStarRobot::ShowJointed(bool bJointedParts)
{
	for (UStaticMeshComponent* Part : { Pelvis.Get(), ThighL.Get(), ThighR.Get(), ShinL.Get(), ShinR.Get(), UpperArmL.Get(), UpperArmR.Get(), ForearmL.Get(), ForearmR.Get() })
	{
		Part->SetVisibility(bJointedParts);
	}
	if (!bJointedParts)
	{
		return;
	}
	for (UStaticMeshComponent* Part : { Legs.Get(), ArmL.Get(), ArmR.Get() })
	{
		Part->SetVisibility(false);
	}
	for (USceneComponent* Joint : { HipL.Get(), HipR.Get(), KneeL.Get(), KneeR.Get(), ElbowL.Get(), ElbowR.Get() })
	{
		Joint->SetRelativeRotation(FRotator::ZeroRotator);
		Joint->SetRelativeScale3D(FVector::OneVector);
	}
	ForearmBackL = ForearmBackR = -1.f;
}

float ARRRandomStarRobot::GetSecondsSinceAssembled() const
{
	return AssembledSeenTime >= 0.f && GetWorld() ? GetWorld()->GetTimeSeconds() - AssembledSeenTime : UE_BIG_NUMBER;
}

FVector ARRRandomStarRobot::GetTopLocation() const
{
	return GetActorLocation() + FVector(0.f, 0.f, RobotHeight * GetActorScale3D().Z);
}

void ARRRandomStarRobot::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	const float Elapsed = ServerNow(World) - CombineStartTime;

	if (HasAuthority())
	{
		if (!bAssembled)
		{
			// The one combining knocked out (or gone): it's off, the cars drop back where they were
			if (!Combiner || !Combiner->IsAlive() || Combiner->GetRobot() != this)
			{
				UE_LOG(LogTemp, Log, TEXT("RRRandom: the star robot's combining is called off (%s)."), Combiner ? TEXT("its combiner is down") : TEXT("its combiner left"));
				BreakApart(true);
				return;
			}
			if (Elapsed >= CombineSeconds)
			{
				FinishCombine();
			}
		}
		else
		{
			// Anyone inside knocked out or gone (a hit can't do it, but falling off the world or leaving the game can)
			for (int32 Index = 0; Index < Crew.Num(); ++Index)
			{
				ARRRandomCharacter* Member = Crew[Index];
				if (Crew[Index] && (!IsValid(Member) || !Member->IsAlive() || Member->GetRobot() != this))
				{
					if (IsValid(Member))
					{
						ClearSeat(Member);
					}
					else
					{
						Crew[Index] = nullptr;
						if (Index == 0)
						{
							DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
						}
					}
				}
			}
			// The shield: held up by the DEFEND seat's fire, draining; emptied it needs to refill a bit first
			const float Now = World->GetTimeSeconds();
			if (bShieldUp)
			{
				ShieldEnergy = FMath::Max(0.f, ShieldEnergy - DeltaSeconds / ShieldSeconds);
				if (ShieldEnergy <= 0.f)
				{
					bShieldSpent = true;
				}
				if (ShieldEnergy <= 0.f || Now > ShieldHoldUntil || !GetCrew(ERRRobotSeat::Defend))
				{
					bShieldUp = false;
				}
			}
			else
			{
				ShieldEnergy = FMath::Min(1.f, ShieldEnergy + DeltaSeconds / ShieldRefillSeconds);
				bShieldSpent &= ShieldEnergy < ShieldRearm;
			}
			// Facing front again a while after the last fist
			if (Now - (NextFistTime - FistInterval) > AimHoldTime)
			{
				AimYaw = 0.f;
			}
		}
	}

	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (bAssembled)
	{
		PoseAssembled(DeltaSeconds);
	}
	else
	{
		PoseCombining(FMath::Clamp(Elapsed, 0.f, CombineSeconds), DeltaSeconds);
	}
}

void ARRRandomStarRobot::PlayBeat(int32 Beat)
{
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	if (!Effects)
	{
		return;
	}
	const FVector Feet = GetActorLocation();
	const FVector Up = FVector::UpVector;
	switch (Beat)
	{
	case 0:
		// The cars light up
		for (const UStaticMeshComponent* Car : CarMeshes)
		{
			Effects->TeleportPoof(Car->GetComponentLocation() + Up * 80.f, StarCyan);
		}
		break;
	case 1:
		// Gathered in the air: a ring of light round them
		Effects->SonicBoom(Feet + Up * WhirlHeight, Up, StarCyan);
		Effects->WingsBurst(Feet + Up * WhirlHeight, StarViolet);
		break;
	case 2:
		// Each car turns into its parts: a shooting star's burst at each
		for (const UStaticMeshComponent* Car : CarMeshes)
		{
			Effects->StarImpact(Car->GetComponentLocation(), Up, 3.f, StarGold);
		}
		break;
	case 3:
		// The legs land: a thump that shakes the ground
		Effects->TigerStrike(Feet, 420.f, StarCyan);
		break;
	case 4:
		// The chest locks onto the hips
		Effects->StarImpact(Feet + Up * 330.f, Up, 3.5f, StarGold);
		break;
	case 5:
		// The arms clamp on
		Effects->HitBurst(GetActorTransform().TransformPosition(ShoulderPivotL), GetActorRightVector(), 4.f, StarGold);
		Effects->HitBurst(GetActorTransform().TransformPosition(ShoulderPivotR), -GetActorRightVector(), 4.f, StarGold);
		break;
	case 6:
		// The head pops up, its eyes lighting
		Effects->MoonFlash(GetActorTransform().TransformPosition(FVector(60.f, 0.f, 560.f)), 2.f);
		Effects->WingsBurst(GetActorTransform().TransformPosition(FVector(40.f, 0.f, 560.f)), StarCyan);
		break;
	default:
		// The pose: gold rings, a pillar of light, a fountain of stars
		Effects->UltimateDieBurst(Feet + Up * (RobotHeight + 60.f), 80.f, Feet);
		Effects->GiantBurst(Feet, 3.f, StarGold);
		break;
	}
}

void ARRRandomStarRobot::PoseCombining(float Time, float DeltaSeconds)
{
	// Beats as they come (a late joiner skips the ones already past)
	if (BeatsPlayed == 0 && Time > 0.5f)
	{
		while (BeatsPlayed < UE_ARRAY_COUNT(BeatTimes) && BeatTimes[BeatsPlayed] < Time - 0.5f)
		{
			++BeatsPlayed;
		}
	}
	const FTransform Frame = GetActorTransform();
	const FVector Center = GetActorLocation();
	const float BaseYaw = GetActorRotation().Yaw;
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);

	// The cars: hop in place, fly up into a ring above the middle, whirl round it; then gone into the parts
	for (int32 Index = 0; Index < CarMeshes.Num(); ++Index)
	{
		UStaticMeshComponent* Car = CarMeshes[Index];
		const ARRRandomVehicle* Real = Cars.IsValidIndex(Index) ? Cars[Index].Get() : nullptr;
		if (!Car->GetStaticMesh() && Real)
		{
			Car->SetStaticMesh(Real->GetKindMesh(Real->GetKind()));
		}
		const bool bShown = Time < WhirlEnd;
		Car->SetVisibility(bShown);
		if (!bShown || !CarStarts.IsValidIndex(Index))
		{
			continue;
		}
		const FVector Start = CarStarts[Index];
		const float StartYaw = CarYaws.IsValidIndex(Index) ? CarYaws[Index] : BaseYaw;
		FVector Where = Start;
		FRotator Turn(0.f, StartYaw, 0.f);
		FVector Scale(1.f);
		const FVector Before = Car->GetComponentLocation();
		if (Time < HopEnd)
		{
			// Two hops, squashed on the ground, stretched in the air
			const float Hop = FMath::Abs(FMath::Sin(Time / HopEnd * 2.f * PI));
			Where.Z += Hop * 70.f;
			Scale = FVector(1.f + (0.5f - Hop) * 0.25f, 1.f + (0.5f - Hop) * 0.25f, 1.f - (0.5f - Hop) * 0.35f);
			Turn.Yaw = FMath::Lerp(StartYaw, BaseYaw, EaseInOut(Time / HopEnd));
		}
		else
		{
			// Round the ring: each a third of the way round, the ring turning faster and faster
			const float Spin = 120.f * Index + 260.f * FMath::Square(Time - HopEnd);
			const float Rise = EaseOut(Phase(Time, HopEnd, LaunchEnd));
			const FVector Ring = Center + FRotator(0.f, BaseYaw + Spin, 0.f).Vector() * WhirlRadius + FVector(0.f, 0.f, WhirlHeight);
			// Up in an arc from where it was: up first, then over
			const FVector Lifted = FMath::Lerp(Start, FVector(Start.X, Start.Y, Ring.Z), EaseOut(Rise * 1.4f));
			Where = FMath::Lerp(Lifted, Ring, EaseInOut(Rise));
			Turn = FRotator(FMath::Sin(Time * 6.f + Index) * 15.f, BaseYaw + Spin + 90.f + Time * 220.f, FMath::Cos(Time * 5.f + Index) * 20.f);
			// Stretched along the way it flies, squashing into a ball as it is about to change
			const float Change = Phase(Time, WhirlEnd - 0.35f, WhirlEnd);
			Scale = FVector(1.f + 0.2f * Rise - 0.5f * Change, 1.f - 0.1f * Rise - 0.3f * Change, 1.f - 0.1f * Rise - 0.3f * Change);
			if (Effects && Time > HopEnd + 0.1f)
			{
				Effects->StarTail(Before + FVector(0.f, 0.f, 70.f), Where + FVector(0.f, 0.f, 70.f), 90.f, 900.f, Index == 0 ? StarGold : Index == 1 ? StarViolet : StarCyan, DeltaSeconds);
			}
		}
		Car->SetWorldLocationAndRotation(Where, Turn);
		Car->SetWorldScale3D(Scale);
	}

	// The parts: out of the cars where they changed, then into place one by one
	const auto RingSpot = [&](int32 Index, float Height)
	{
		const float Spin = 120.f * Index + 260.f * FMath::Square(WhirlEnd - HopEnd);
		return Center + FRotator(0.f, BaseYaw + Spin, 0.f).Vector() * WhirlRadius + FVector(0.f, 0.f, Height);
	};
	const bool bParts = Time >= WhirlEnd;
	for (UStaticMeshComponent* Part : { Legs.Get(), Torso.Get(), Head.Get(), ArmL.Get(), ArmR.Get() })
	{
		Part->SetVisibility(bParts);
	}
	ShowJointed(false);
	if (!bParts)
	{
		return;
	}

	// Legs: from the legs car's spot down to the ground, landing with a squash
	{
		const float T = Phase(Time, WhirlEnd, LegsLand);
		const FVector From = Frame.InverseTransformPosition(RingSpot(2, WhirlHeight - 200.f));
		const FVector Local = FMath::Lerp(From, FVector::ZeroVector, EaseIn(T)) + FVector(0.f, 0.f, FMath::Sin(T * PI) * 180.f);
		const float Squish = Squash(Time, LegsLand) * 0.18f;
		Hips->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Legs->SetRelativeLocationAndRotation(Local, FRotator(0.f, (1.f - T) * 540.f, 0.f));
		Legs->SetRelativeScale3D(FVector(1.f + Squish, 1.f + Squish, 1.f - Squish) * Pop(Time, WhirlEnd));
	}

	// Chest (with the head tucked inside): floats high over the middle, then drops onto the hips
	{
		const float Float = Phase(Time, WhirlEnd, LegsLand);
		const FVector High = FVector(0.f, 0.f, 520.f);
		const FVector From = Frame.InverseTransformPosition(RingSpot(0, WhirlHeight)) - FVector(0.f, 0.f, 380.f);
		FVector Local = FMath::Lerp(From, High, EaseOut(Float));
		float Spin = (1.f - EaseOut(Float)) * 360.f;
		if (Time > LegsLand)
		{
			Local = FMath::Lerp(High, FVector::ZeroVector, EaseIn(Phase(Time, LegsLand, ChestLand)));
			Spin = 0.f;
		}
		const float Squish = Squash(Time, ChestLand) * 0.12f;
		Upper->SetRelativeLocationAndRotation(Local, FRotator(0.f, Spin, 0.f));
		Torso->SetRelativeScale3D(FVector(1.f + Squish, 1.f + Squish, 1.f - Squish) * Pop(Time, WhirlEnd));
	}

	// Arms: off to the sides, then swooping in and clamping on with a snap past and back (they ride on the chest, so
	// they are given relative to it)
	{
		const float Side = Time < ArmsStart ? 1.f : 1.f - BackOut(Phase(Time, ArmsStart, ArmsLock), 2.2f);
		const float Out = 420.f * Side;
		const float Swing = Time < ArmsStart ? FMath::Sin(Time * 5.f) * 25.f : (1.f - Phase(Time, ArmsStart, ArmsLock)) * -60.f;
		// Before the chest lands they hang in the air level with where its shoulders will be
		const FVector Lift = Time < ChestLand ? -Upper->GetRelativeLocation() : FVector::ZeroVector;
		const FVector ArmsCar = Frame.InverseTransformPosition(RingSpot(1, WhirlHeight));
		const float Gather = EaseOut(Phase(Time, WhirlEnd, LegsLand));
		const FVector Early = FMath::Lerp(ArmsCar - FVector(0.f, 0.f, 300.f), FVector::ZeroVector, Gather);
		const FVector Drift = Time < ArmsStart ? Early * (1.f - Phase(Time, LegsLand, ArmsStart)) : FVector::ZeroVector;
		ShoulderL->SetRelativeLocationAndRotation(ShoulderPivotL + Lift + Drift + FVector(0.f, -Out, 0.f), FRotator(Swing, 0.f, Side * 40.f));
		ShoulderR->SetRelativeLocationAndRotation(ShoulderPivotR + Lift + Drift + FVector(0.f, Out, 0.f), FRotator(Swing, 0.f, -Side * 40.f));
		const float Scale = Pop(Time, WhirlEnd + 0.1f);
		ArmL->SetRelativeScale3D(FVector(Scale));
		ArmR->SetRelativeScale3D(FVector(Scale));
	}

	// Head: tucked down inside the chest, then springing up with a wobble
	{
		const float T = Phase(Time, HeadStart, HeadUp);
		const float Rise = Time < HeadStart ? 0.f : BackOut(T, 3.f);
		const float Wobble = Time > HeadUp ? FMath::Sin((Time - HeadUp) * 22.f) * 8.f * FMath::Max(0.f, 1.f - (Time - HeadUp) * 2.f) : 0.f;
		Neck->SetRelativeLocationAndRotation(NeckPivot - FVector(0.f, 0.f, 150.f * (1.f - Rise)), FRotator(0.f, 0.f, Wobble));
		Head->SetRelativeScale3D(FVector(Time < HeadStart ? 0.01f : FMath::Lerp(0.4f, 1.f, FMath::Min(1.f, Rise))));
	}

	// The pose: arms thrown up, then a bounce down onto its feet ready to go
	if (Time >= PoseStart)
	{
		const float T = Phase(Time, PoseStart, CombineSeconds);
		const float Raise = FMath::Sin(T * PI);
		ShoulderL->SetRelativeRotation(FRotator(0.f, 0.f, 70.f * Raise));
		ShoulderR->SetRelativeRotation(FRotator(0.f, 0.f, -70.f * Raise));
		const float Bounce = FMath::Sin(T * PI * 2.f) * (1.f - T);
		Hips->SetRelativeLocation(FVector(0.f, 0.f, Bounce * 40.f));
		Legs->SetRelativeScale3D(FVector(1.f - Bounce * 0.06f, 1.f - Bounce * 0.06f, 1.f + Bounce * 0.08f));
	}

	while (BeatsPlayed < UE_ARRAY_COUNT(BeatTimes) && Time >= BeatTimes[BeatsPlayed])
	{
		PlayBeat(BeatsPlayed++);
	}
}

void ARRRandomStarRobot::PoseAssembled(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	// Walking with its driver's steps
	const ARRRandomCharacter* Driver = GetCrew(ERRRobotSeat::Move);
	// Turning on the spot steps too (degrees a second counted as this much walking)
	const float Yaw = GetActorRotation().Yaw;
	const float TurnSpeed = DeltaSeconds > 0.f ? FMath::Abs(FRotator::NormalizeAxis(Yaw - LastPoseYaw)) / DeltaSeconds * TurnStepShare : 0.f;
	LastPoseYaw = Yaw;
	const float Speed = FMath::Max(Driver ? Driver->GetVelocity().Size2D() : 0.f, TurnSpeed);
	WalkAmount = FMath::FInterpTo(WalkAmount, FMath::Clamp(Speed / 250.f, 0.f, 1.f), DeltaSeconds, 5.f);
	// A whole cycle (2 pi) is two steps, left then right
	const float LastPhase = WalkPhase;
	WalkPhase = FMath::Fmod(WalkPhase + DeltaSeconds * Speed / StrideLength * PI, 2.f * PI);
	const float S = FMath::Sin(WalkPhase);
	const float C = FMath::Cos(WalkPhase);

	// The legs: the left one's swing is +Swing sin(phase) (forward when positive), the right's the opposite; a leg swings
	// forward while cos is positive for it, its knee bending most halfway and straight again as its foot comes down
	const float Swing = StepSwing * WalkAmount;
	const float HipAngleL = Swing * S;
	const float KneeBendL = KneeLift * WalkAmount * FMath::Square(FMath::Max(0.f, C));
	const float KneeBendR = KneeLift * WalkAmount * FMath::Square(FMath::Max(0.f, -C));
	// A foot comes down when its leg is furthest forward: the left at a quarter of the cycle, the right at three quarters
	const auto Crossed = [&](float Mark) { return LastPhase < Mark ? (WalkPhase >= Mark || WalkPhase < LastPhase) : (WalkPhase >= Mark && WalkPhase < LastPhase); };
	const bool bPlantL = WalkAmount > 0.25f && Crossed(HALF_PI);
	const bool bPlantR = WalkAmount > 0.25f && Crossed(1.5f * PI);
	if (bPlantL || bPlantR)
	{
		StepJolt = WalkAmount;
	}
	StepJolt = FMath::FInterpConstantTo(StepJolt, 0.f, DeltaSeconds, 5.f);

	// The hips: sinking as the legs open (the planted leg straight: its foot stays on the floor), a jolt as a foot lands,
	// leaning over the planted foot, turning with the swinging leg; idle, breathing
	const float Breath = FMath::Sin(Now * 2.2f) * 4.f * (1.f - WalkAmount);
	const float Sink = LegLength * (1.f - FMath::Cos(FMath::DegreesToRadians(HipAngleL))) + StepJoltSink * FMath::Sin(StepJolt * PI);
	const float HipYaw = WalkHipYaw * S * WalkAmount;
	Hips->SetRelativeLocationAndRotation(FVector(0.f, 0.f, Breath - Sink), FRotator(0.f, HipYaw, -WalkRoll * C * WalkAmount));
	Legs->SetRelativeScale3D(FVector::OneVector);
	if (bHasJoints)
	{
		HipL->SetRelativeRotation(FRotator(HipAngleL + KneeBendL * SwingHipLift, 0.f, 0.f));
		HipR->SetRelativeRotation(FRotator(-HipAngleL + KneeBendR * SwingHipLift, 0.f, 0.f));
		KneeL->SetRelativeRotation(FRotator(-KneeBendL, 0.f, 0.f));
		KneeR->SetRelativeRotation(FRotator(-KneeBendR, 0.f, 0.f));
	}

	// The upper body turns to the aim, twists back against the hips and leans into the walk (about the hips' height)
	ShownAimYaw = FMath::FixedTurn(ShownAimYaw, AimYaw, AimTurnRate * DeltaSeconds);
	const FRotator Chest(-WalkLean * WalkAmount, ShownAimYaw - HipYaw * ChestCounterYaw, 0.f);
	const FVector Waist(0.f, 0.f, LegLength);
	Upper->SetRelativeLocationAndRotation(Waist - Chest.RotateVector(Waist), Chest);
	// The head nods a little with each step
	Neck->SetRelativeLocationAndRotation(NeckPivot, FRotator(2.5f * WalkAmount * FMath::Cos(2.f * WalkPhase) + 2.f * StepJolt, 0.f, Breath * 0.5f));

	// Arms: swinging against the legs as it walks (the elbow bending as the arm comes forward), or held out level toward
	// the aim after a fist, each punching back when it fires
	PunchL = FMath::FInterpConstantTo(PunchL, 0.f, DeltaSeconds, 3.f);
	PunchR = FMath::FInterpConstantTo(PunchR, 0.f, DeltaSeconds, 3.f);
	const bool bAiming = ShotCount > 0 && LastShotSeen == ShotCount && (PunchL > 0.f || PunchR > 0.f || Now - AimSeenTime < ArmsUpTime);
	if (PunchL > 0.f || PunchR > 0.f)
	{
		AimSeenTime = Now;
	}
	ArmsUp = FMath::FInterpTo(ArmsUp, bAiming ? 1.f : 0.f, DeltaSeconds, 8.f);
	const float SwingL = -WalkArmSwing * S * WalkAmount * (1.f - ArmsUp);
	const float SwingR = -SwingL;
	const float Up = 85.f * ArmsUp;
	ShoulderL->SetRelativeLocationAndRotation(ShoulderPivotL - FVector(PunchL * 60.f, 0.f, 0.f), FRotator(Up + SwingL - PunchL * 25.f, 0.f, 0.f));
	ShoulderR->SetRelativeLocationAndRotation(ShoulderPivotR - FVector(PunchR * 60.f, 0.f, 0.f), FRotator(Up + SwingR - PunchR * 25.f, 0.f, 0.f));
	if (bHasJoints)
	{
		const auto Elbow = [&](float Arm) { return FMath::Lerp(RestElbow + WalkElbow * FMath::Max(0.f, Arm / WalkArmSwing), AimElbow, ArmsUp); };
		ElbowL->SetRelativeRotation(FRotator(Elbow(SwingL), 0.f, 0.f));
		ElbowR->SetRelativeRotation(FRotator(Elbow(SwingR), 0.f, 0.f));
		// A forearm that flew off grows back on with a pop and a sparkle
		for (int32 Side = 0; Side < 2; ++Side)
		{
			float& Back = Side == 0 ? ForearmBackL : ForearmBackR;
			UStaticMeshComponent* Forearm = Side == 0 ? ForearmL.Get() : ForearmR.Get();
			USceneComponent* Joint = Side == 0 ? ElbowL.Get() : ElbowR.Get();
			if (Back < 0.f || Now < Back)
			{
				continue;
			}
			if (!Forearm->IsVisible())
			{
				Forearm->SetVisibility(true);
				if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
				{
					Effects->DiceLandBurst(Forearm->Bounds.Origin, 30.f);
				}
			}
			const float Grown = (Now - Back) / ForearmPopTime;
			Joint->SetRelativeScale3D(FVector(Grown >= 1.f ? 1.f : FMath::Lerp(0.2f, 1.f, BackOut(Grown, 2.5f))));
			if (Grown >= 1.f)
			{
				Back = -1.f;
			}
		}
	}

	// Each foot coming down puffs dust round it
	if ((bPlantL || bPlantR) && bHasJoints)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			const UStaticMeshComponent* Shin = bPlantL ? ShinL.Get() : ShinR.Get();
			FVector Sole = Shin->GetComponentTransform().TransformPosition(bPlantL ? SoleL : SoleR);
			Sole.Z = GetActorLocation().Z;
			Effects->Footstep(Sole, 0.9f + 0.3f * WalkAmount);
		}
	}

	// The shield bubble, popping up and fading
	ShieldShown = FMath::FInterpTo(ShieldShown, bShieldUp ? 1.f : 0.f, DeltaSeconds, 10.f);
	Shield->SetVisibility(ShieldShown > 0.02f);
	if (ShieldShown > 0.02f)
	{
		const float Pulse = 1.f + FMath::Sin(Now * 9.f) * 0.015f;
		Shield->SetWorldScale3D(FVector(ShieldRadius / 50.f * FMath::Lerp(0.6f, 1.f, ShieldShown) * Pulse));
		if (ShieldMaterial)
		{
			ShieldMaterial->SetScalarParameterValue(TEXT("Opacity"), ShieldShown);
			ShieldMaterial->SetScalarParameterValue(TEXT("Fill"), 0.12f * ShieldShown);
		}
	}
}
