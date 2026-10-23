#include "RRRandomStarRobot.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomGameState.h"
#include "RRRandomProjectile.h"
#include "RRRandomVehicle.h"
#include "RRRandomWreckage.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The robot's parts (char-pipeline/scripts/blender_robot_parts.py: the haetae robot of round 79, 640 tall, front on
	// +X, pivot the middle of its bottom, cut at the hips 285, the neck 462 and the shoulders 135 out), each with the
	// whole robot's pivot
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
	const FVector NeckPivot(0.f, 0.f, 462.f);
	const FVector ShoulderPivotL(-10.f, -180.f, 440.f);
	const FVector ShoulderPivotR(-10.f, 180.f, 440.f);
	// The jointed parts (char-pipeline/scripts/blender_robot_joints.py, cut at the knees 165 and elbows 320, the pelvis the
	// legs' middle block), each with the whole robot's pivot, and where they turn: the hips, knees and elbows. The forearm
	// hangs down from its elbow to the tip of its fist
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
	const FVector HipPivotL(10.f, -75.f, 282.f);
	const FVector HipPivotR(10.f, 75.f, 282.f);
	const FVector KneePivotL(14.f, -97.f, 166.f);
	const FVector KneePivotR(14.f, 97.f, 166.f);
	const FVector ElbowPivotL(-2.f, -200.f, 325.f);
	const FVector ElbowPivotR(-2.f, 200.f, 325.f);
	const FVector FistTipL(20.f, -212.f, 150.f);
	const FVector FistTipR(20.f, 212.f, 150.f);
	// The feet's soles, under each shin (for the dust of a step)
	const FVector SoleL(15.f, -100.f, 5.f);
	const FVector SoleR(15.f, 100.f, 5.f);
	// What shots hit
	constexpr float HullRadius = 190.f;
	constexpr float HullHalfHeight = 320.f;

	// The upper body turns to the aim this fast (degrees a second), back to the front after this long without an attack
	constexpr float AimTurnRate = 300.f;
	constexpr float AimHoldTime = 2.f;

	// The light sword, in the right fist: the grip at the fist's tip (on the forearm), the hilt and the blade along the
	// grip's +X, the blade this long and this thick (its glow; the white core inside it half as thick)
	constexpr float HiltLength = 80.f;
	constexpr float HiltRadius = 15.f;
	constexpr float BladeStart = 30.f;
	constexpr float BladeLength = 400.f;
	constexpr float BladeRadius = 17.f;
	// A combo of up to three swings: each this long; its blow lands this far into it; pressed again from this far into a
	// swing the next follows, starting this far in (cutting the follow-through short); not pressed again by the end, the
	// combo is over and the cooldown starts
	const float SwingSeconds[] = { 0.45f, 0.45f, 0.7f };
	constexpr float SwingHitAt = 0.45f;
	constexpr float ComboFrom = 0.25f;
	constexpr float ComboChainAt = 0.75f;
	// Who the blow hits: within this reach of the robot's middle (flat), this far either side of where the upper body faces
	// (the third, an overhead slam, straight down in front: narrower but it lands harder), for this much damage; and the
	// structures in the blade's way are smashed with a ball this big round it (the slam's a bigger crater where it lands)
	constexpr float SwordReach = 720.f;
	const float SwingArc[] = { 75.f, 75.f, 35.f };
	const float SwingDamage[] = { 45.f, 45.f, 80.f };
	constexpr float SwordWreckRadius = 150.f;
	// The sweeps cut this high over its feet (a low cover's height)
	constexpr float SwordCutHeight = 90.f;
	constexpr float SlamWreckRadius = 280.f;
	// A bot in the ATTACK seat uses the sword this close and the missiles further off
	constexpr float BotSwordRange = SwordReach * 1.1f;

	// The missiles: this many a volley from launchers on its shoulders, fanned out up and to the sides, then curving in on
	// the aim (turning this fast at first, faster the longer they fly); this fast, this wide for hits, each this many times
	// a pistol shot's damage; one that gets to the aim without hitting anything bursts there
	constexpr int32 MissileCount = 8;
	const FVector MissilePodL(-40.f, -150.f, 540.f);
	const FVector MissilePodR(-40.f, 150.f, 540.f);
	constexpr float MissileFanYaw = 70.f;
	constexpr float MissileFanPitch = 40.f;
	constexpr float MissileSpeed = 0.4f;
	constexpr float MissileRadius = 22.f;
	constexpr float MissileDamage = 1.3f;
	constexpr float MissileRange = 4500.f;

	// The shield: held up in front of the left fist, this big across; it stops what comes at it within this far either side
	// of where the upper body faces, its health taking the damage (what's left over when it breaks goes through); one
	// held press keeps it up this long; lowered, it mends this much a second after this long; broken, it can't go up again
	// until it has this much back. A bot in the DEFEND seat only raises it this soon after a hit.
	constexpr float ShieldRadius = 230.f;
	constexpr float ShieldArc = 80.f;
	constexpr float ShieldHold = 0.35f;
	constexpr float ShieldMendRate = 15.f;
	constexpr float ShieldMendDelay = 1.f;
	constexpr float ShieldRearm = 90.f;
	constexpr float BotShieldAfterHit = 1.2f;

	// Robot against robot (the other team's). A sword blow on a robot does this many times its damage and makes it reel
	// this long. A shield raised this soon before a blow lands parries it: the swinger reels this long. The slam breaks a
	// raised shield (its health takes this much, it drops) and the defender reels. A missile landing mid-swing stops the
	// swing and makes it reel. Two blows this close together (the rival's still coming, or just landed) lock blades.
	constexpr float SwordOnRobot = 2.5f;
	constexpr float HitReel = 0.3f;
	constexpr float HitReelGuard = 1.5f;
	constexpr float ParryWindow = 0.2f;
	constexpr float ParryReel = 1.2f;
	constexpr float GuardBreakDamage = 200.f;
	constexpr float GuardBreakReel = 1.f;
	constexpr float MissileReel = 0.4f;
	constexpr float ClashWindow = 0.15f;
	// After a clash, these two don't lock blades again for this long (or they would do little else)
	constexpr float ClashCooldown = 8.f;
	// A clash: a wrong key locks that seat out this long; a bot presses a key every this many seconds, now and then the
	// wrong one; the loser reels this long (wide open), an even one pushes both back a little; the outcome shows this long
	constexpr float ClashWrongLock = 0.3f;
	const FVector2D BotClashPace(0.3f, 0.5f);
	constexpr float BotClashMiss = 0.08f;
	constexpr float ClashLoseReel = 2.f;
	constexpr float ClashEvenReel = 0.4f;
	constexpr float ClashOutcomeShown = 1.f;
	// The blades count as crossed this close; the light where they bite (candelas, reach)
	constexpr float ClashTouch = 160.f;
	// The blades cross this high over the floor between the two (over their shoulders)
	constexpr float ClashBindHeight = 480.f;
	// The sword arm aims this far below it, the blade tipping up from the fist (the steeper the X)
	constexpr float ClashBindDrop = 110.f;
	// Each of a clash's exchanges: winding up to this far into it, striking the other blade at this, pressed against it
	// till this, then drawn back for the next
	constexpr float ClashWindEnd = 0.2f;
	constexpr float ClashStrikeAt = 0.28f;
	constexpr float ClashPushEnd = 0.88f;
	constexpr float ClashLightIntensity = 120.f;
	constexpr float ClashLightRadius = 1400.f;
	// Brought down with the rival this close: kneels this long (only the rival's slam finishes it off), then gets up with
	// this share of its health
	constexpr float DownNear = 4000.f;
	constexpr float DownSeconds = 3.f;
	constexpr float RiseHealthShare = 0.2f;
	// A bot in the DEFEND seat tries to parry a rival blow this often, raising the shield this long before it lands
	constexpr float BotParryChance = 0.5f;
	constexpr float BotParryLead = 0.12f;
	// A bot in the ATTACK seat goes for the rival robot when it's this close
	constexpr float BotRivalRange = 3500.f;
	// A robot made by a cheat (CheatAssemble): its bots don't start attacking for this long
	constexpr float CheatBotHold = 3.f;

	// Walking, a step at a time: each leg swings this far either way from the hip (a step this long, so the planted foot
	// doesn't slide: 2 x leg length x sin(swing)), the knee of the swinging leg bends this much to lift its foot, and the
	// hips sink as the legs open (the planted leg stays straight on the floor); the hips lean over the planted foot and
	// turn with the swinging leg, the chest turns back the other way and leans into the walk, the arms swing against the
	// legs with the elbows bending as they come forward, each foot comes down with a jolt and a puff of dust
	constexpr float LegLength = 282.f;
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
	// Holding the sword and swinging it (the right arm) and holding up the shield (the left): how far the chest turns and
	// leans (negative: forward), the shoulder (pitch: the arm forward from hanging down; yaw: then out to its right), the
	// elbow (bending the forearm forward), the wrist (the blade's pitch off the fist: -90 runs it on along the forearm) and
	// how deep it crouches (0 to 1, a landing's crouch). A swing goes from its wind-up to its follow-through.
	struct FArmPose
	{
		float ChestYaw;
		float Lean;
		FRotator Shoulder;
		float Elbow;
		float Wrist;
		float Crouch;
	};
	const FArmPose SwordRest = { 0.f, 0.f, FRotator(20.f, 0.f, 0.f), 35.f, -25.f, 0.f };
	// A slash from its right across to its left, low enough for animal friends; back across from the left, rising; up over
	// its head and down into the ground in front
	const FArmPose SwingWindUp[] =
	{
		{ 40.f, 0.f, FRotator(60.f, 70.f, 0.f), 20.f, -90.f, 0.f },
		{ -45.f, 0.f, FRotator(50.f, -75.f, 0.f), 10.f, -90.f, 0.f },
		{ 0.f, 12.f, FRotator(175.f, 0.f, 0.f), -50.f, -90.f, 0.f },
	};
	const FArmPose SwingFollow[] =
	{
		{ -45.f, -8.f, FRotator(55.f, -65.f, 0.f), 0.f, -90.f, 0.15f },
		{ 45.f, -5.f, FRotator(80.f, 70.f, 0.f), 0.f, -90.f, 0.15f },
		{ 0.f, -18.f, FRotator(50.f, 0.f, 0.f), 0.f, -90.f, 0.7f },
	};
	// A swing: winding up over the first part, the blow fast up to just past SwingHitAt, held, then back to rest
	constexpr float SwingWindEnd = 0.3f;
	constexpr float SwingStrikeEnd = 0.52f;
	constexpr float SwingRecoverFrom = 0.8f;
	const FRotator ShieldShoulder(70.f, 30.f, 0.f);
	constexpr float ShieldElbow = 45.f;
	// Jumping: in the air the thighs come up and the knees bend this much, the arms go up this far; a landing crouches this
	// deep (thighs, knees, the chest leaning in) and stands up again over this long; the shock ring's size. The thigh is this
	// long (hip 282 to knee 166)
	constexpr float JumpTuckHip = 32.f;
	constexpr float JumpTuckKnee = 62.f;
	constexpr float JumpArmsUp = 50.f;
	constexpr float JumpCrouchHip = 38.f;
	constexpr float JumpCrouchKnee = 76.f;
	constexpr float JumpCrouchLean = 10.f;
	constexpr float JumpLandTime = 0.45f;
	constexpr float JumpLandRing = 0.7f;
	constexpr float ThighLength = 116.f;
	// Turning on the spot, each degree a second steps like this much walking speed
	constexpr float TurnStepShare = 2.5f;

	// Where the cars come back out when it breaks: thrown this far round it (past GatherRadius, so it takes gathering
	// them again), onto floor no more than this far above or below its feet; failing that, close round it
	constexpr float BreakSpread = 1050.f;
	constexpr float BreakNearSpread = 340.f;
	constexpr float BreakFloorStep = 150.f;

	// The combining, seconds from the start: the cars hop, fly up into a ring and whirl, turn into the parts, the legs
	// land, the chest drops on, the arms swoop in with their fists still hidden in them (the arms car keeps its fists
	// inside), the fists shoot out, the chest's floor opens and the head (hidden under it in the chest car) peeks out and
	// springs up, a last pose
	constexpr float HopEnd = 1.f;
	constexpr float LaunchEnd = 3.f;
	constexpr float WhirlEnd = 4.2f;
	constexpr float LegsLand = 5.6f;
	constexpr float ChestLand = 6.8f;
	constexpr float ArmsStart = 6.8f;
	constexpr float ArmsLock = 7.4f;
	constexpr float FistsStart = 7.6f;
	constexpr float FistsOut = 7.95f;
	constexpr float HeadStart = 8.2f;
	constexpr float HeadPeek = 8.6f;
	constexpr float HeadUp = 8.9f;
	constexpr float PoseStart = 9.1f;
	// The beats: a pop of light at each (PlayBeat)
	const float BeatTimes[] = { 0.f, LaunchEnd, WhirlEnd, LegsLand, ChestLand, ArmsLock, FistsStart, HeadStart, HeadUp, 9.45f };
	// Folded away the fists are this small at the elbows; peeking, the head is this far down in the chest and this big
	constexpr float FistHidden = 0.06f;
	constexpr float HeadSunk = 150.f;
	constexpr float HeadPeekSink = 70.f;
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
	// Two poses of the arms, Alpha of the way from A to B
	FArmPose BlendPose(const FArmPose& A, const FArmPose& B, float Alpha)
	{
		return { FMath::Lerp(A.ChestYaw, B.ChestYaw, Alpha), FMath::Lerp(A.Lean, B.Lean, Alpha), FMath::Lerp(A.Shoulder, B.Shoulder, Alpha),
			FMath::Lerp(A.Elbow, B.Elbow, Alpha), FMath::Lerp(A.Wrist, B.Wrist, Alpha), FMath::Lerp(A.Crouch, B.Crouch, Alpha) };
	}

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

	// The light sword in the right fist: a gold hilt, then a cyan glow round a white-hot core (the cloak material: a
	// fresnel rim that glows, filled in), all along the grip's +X (the engine cylinder stands on Z, so it's tipped over)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Cloak(TEXT("/Game/Effects/DiceFX/M_FXCloak.M_FXCloak"));
	SwordGrip = CreateDefaultSubobject<USceneComponent>(TEXT("SwordGrip"));
	SwordGrip->SetupAttachment(ElbowR);
	SwordGrip->SetRelativeLocation(FistTipR - ElbowPivotR);
	auto MakeSwordPart = [&](const TCHAR* Name, float From, float Length, float Radius, bool bGlow)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(SwordGrip);
		if (Cylinder.Succeeded())
		{
			Part->SetStaticMesh(Cylinder.Object);
		}
		UMaterialInterface* Material = bGlow ? Cloak.Object : ShapeMaterial.Object;
		if (Material)
		{
			Part->SetMaterial(0, Material);
		}
		Part->SetRelativeLocationAndRotation(FVector(From + Length * 0.5f, 0.f, 0.f), FRotator(-90.f, 0.f, 0.f));
		Part->SetRelativeScale3D(FVector(Radius / 50.f, Radius / 50.f, Length / 100.f));
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(!bGlow);
		Part->SetRenderCustomDepth(!bGlow);
		Part->SetCustomDepthStencilValue(1);
		Part->SetReceivesDecals(false);
		Part->SetVisibility(false);
		return Part;
	};
	SwordHilt = MakeSwordPart(TEXT("SwordHilt"), -HiltLength * 0.5f, HiltLength, HiltRadius, false);
	SwordBlade = MakeSwordPart(TEXT("SwordBlade"), BladeStart, BladeLength, BladeRadius, true);
	SwordCore = MakeSwordPart(TEXT("SwordCore"), BladeStart, BladeLength, BladeRadius * 0.5f, true);

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

	// A light where the blades bite in a clash (ARRRandomStarRobot::PoseAssembled)
	ClashLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ClashLight"));
	ClashLight->SetupAttachment(Root);
	ClashLight->SetUsingAbsoluteLocation(true);
	ClashLight->SetIntensityUnits(ELightUnits::Candelas);
	ClashLight->SetAttenuationRadius(ClashLightRadius);
	ClashLight->SetCastShadows(false);
	ClashLight->SetVisibility(false);

	// A glowing disc (a flattened ball in the cloak material), placed at the left fist every frame it's up
	Shield = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shield"));
	Shield->SetupAttachment(Root);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Shield->SetStaticMesh(Sphere.Object);
	}
	if (Cloak.Succeeded())
	{
		Shield->SetMaterial(0, Cloak.Object);
	}
	Shield->SetUsingAbsoluteLocation(true);
	Shield->SetUsingAbsoluteRotation(true);
	Shield->SetUsingAbsoluteScale(true);
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
	DOREPLIFETIME(ARRRandomStarRobot, ShieldHealth);
	DOREPLIFETIME(ARRRandomStarRobot, bShieldBroken);
	DOREPLIFETIME(ARRRandomStarRobot, AimYaw);
	DOREPLIFETIME(ARRRandomStarRobot, MissileVolleys);
	DOREPLIFETIME(ARRRandomStarRobot, SwingCount);
	DOREPLIFETIME(ARRRandomStarRobot, SwingStage);
	DOREPLIFETIME(ARRRandomStarRobot, SwordReadyTime);
	DOREPLIFETIME(ARRRandomStarRobot, MissileReadyTime);
	DOREPLIFETIME(ARRRandomStarRobot, StaggerUntil);
	DOREPLIFETIME(ARRRandomStarRobot, bDowned);
	DOREPLIFETIME(ARRRandomStarRobot, ClashRival);
	DOREPLIFETIME(ARRRandomStarRobot, ClashStartTime);
	DOREPLIFETIME(ARRRandomStarRobot, ClashKeys);
	DOREPLIFETIME(ARRRandomStarRobot, ClashDone);
	DOREPLIFETIME(ARRRandomStarRobot, ClashMisses);
	DOREPLIFETIME(ARRRandomStarRobot, ClashOutcome);
	DOREPLIFETIME(ARRRandomStarRobot, ShoutWord);
	DOREPLIFETIME(ARRRandomStarRobot, ShoutCount);
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
			ShieldMaterial->SetVectorParameterValue(TEXT("Color"), GetTeamGlow());
		}
	}
	const auto Tint = [](UStaticMeshComponent* Part, const FLinearColor& Color, float Glow, float Fill, float Opacity)
	{
		if (UMaterialInstanceDynamic* Material = Part->CreateDynamicMaterialInstance(0, Part->GetMaterial(0)))
		{
			Material->SetVectorParameterValue(TEXT("Color"), Color);
			Material->SetScalarParameterValue(TEXT("Glow"), Glow);
			Material->SetScalarParameterValue(TEXT("Fill"), Fill);
			Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		}
	};
	Tint(SwordBlade, GetTeamGlow(), 1.6f, 0.7f, 0.9f);
	Tint(SwordCore, FLinearColor::White, 4.f, 1.f, 1.f);
	if (UMaterialInstanceDynamic* Hilt = SwordHilt->CreateDynamicMaterialInstance(0, SwordHilt->GetMaterial(0)))
	{
		Hilt->SetVectorParameterValue(TEXT("Color"), StarGold);
	}
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

ARRRandomStarRobot* ARRRandomStarRobot::Get(const UObject* WorldContext, int32 InTeam)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ARRRandomStarRobot> It(World); It; ++It)
	{
		if (IsValid(*It) && It->Team == InTeam)
		{
			return *It;
		}
	}
	return nullptr;
}

ARRRandomStarRobot* ARRRandomStarRobot::GetRival() const
{
	for (TActorIterator<ARRRandomStarRobot> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && *It != this && It->Team != Team && It->bAssembled)
		{
			return *It;
		}
	}
	return nullptr;
}

bool ARRRandomStarRobot::FindGathered(const UObject* WorldContext, int32 InTeam, ARRRandomVehicle* OutCars[3], FVector& OutCenter)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || Get(World, InTeam))
	{
		return false;
	}
	OutCars[0] = OutCars[1] = OutCars[2] = nullptr;
	for (TActorIterator<ARRRandomVehicle> It(World); It; ++It)
	{
		const int32 Part = It->GetStarPart();
		if (Part >= 0 && Part < 3 && It->GetStarTeam() == InTeam && !It->IsCombined() && !OutCars[Part])
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
	return FindGathered(Who, Who->GetTeam(), Found, Center) && FVector::Dist2D(Who->GetActorLocation(), Center) <= CombineReach;
}

ARRRandomStarRobot* ARRRandomStarRobot::StartCombine(ARRRandomCharacter* Who)
{
	UWorld* World = Who ? Who->GetWorld() : nullptr;
	ARRRandomVehicle* Found[3];
	FVector Center;
	if (!World || !Who->HasAuthority() || !CanStartCombine(Who) || !FindGathered(Who, Who->GetTeam(), Found, Center))
	{
		return nullptr;
	}
	// Facing the one combining it, so it comes together (and strikes its pose) toward that one's camera
	const FVector Toward = (Who->GetActorLocation() - Center) * FVector(1.f, 1.f, 0.f);
	const float Yaw = Toward.IsNearlyZero() ? Who->GetActorRotation().Yaw + 180.f : Toward.Rotation().Yaw;
	ARRRandomStarRobot* Robot = SpawnCombining(Who, Found, Center, Yaw);
	if (Robot)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) starts combining the star robot, facing it."), *Who->GetName(), Who->GetTeam());
	}
	return Robot;
}

ARRRandomStarRobot* ARRRandomStarRobot::SpawnCombining(ARRRandomCharacter* Who, ARRRandomVehicle* const Found[3], const FVector& Center, float Yaw)
{
	const FTransform SpawnTransform(FRotator(0.f, Yaw, 0.f), Center);
	ARRRandomStarRobot* Robot = Who->GetWorld()->SpawnActorDeferred<ARRRandomStarRobot>(ARRRandomStarRobot::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Robot)
	{
		return nullptr;
	}
	Robot->Team = Who->GetTeam();
	Robot->Combiner = Who;
	Robot->CombineStartTime = ServerNow(Who->GetWorld());
	Robot->Health = Robot->MaxHealth;
	Robot->Crew.Init(nullptr, 3);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		ARRRandomVehicle* Car = Found[Index];
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
	return Robot;
}

ARRRandomStarRobot* ARRRandomStarRobot::CheatAssemble(ARRRandomCharacter* Who, const FVector& Where, float Yaw)
{
	UWorld* World = Who ? Who->GetWorld() : nullptr;
	if (!World || !Who->HasAuthority() || !Who->IsAlive() || Who->GetRobot() || Get(World, Who->GetTeam()))
	{
		return nullptr;
	}
	ARRRandomVehicle* Found[3] = { nullptr, nullptr, nullptr };
	for (TActorIterator<ARRRandomVehicle> It(World); It; ++It)
	{
		const int32 Part = It->GetStarPart();
		if (Part >= 0 && Part < 3 && It->GetStarTeam() == Who->GetTeam() && !It->IsCombined() && !Found[Part])
		{
			Found[Part] = *It;
		}
	}
	if (!Found[0] || !Found[1] || !Found[2])
	{
		return nullptr;
	}
	// Done at once: started a whole combining ago (its show skipped), its team piling in
	Who->TeleportTo(Where + FRotator(0.f, Yaw, 0.f).Vector() * 300.f + FVector(0.f, 0.f, 100.f), FRotator(0.f, Yaw + 180.f, 0.f));
	ARRRandomStarRobot* Robot = SpawnCombining(Who, Found, Where, Yaw);
	if (Robot)
	{
		Robot->CombineStartTime -= CombineSeconds;
		Robot->BeatsPlayed = UE_ARRAY_COUNT(BeatTimes);
		Robot->bCheatFillCrew = true;
		Robot->BotAttackNext = ServerNow(World) + CheatBotHold;
		// Done on the spot, its combiner and team in at once (out in the open a frame, it could be knocked out first)
		Robot->FinishCombine();
		UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, team %d's star robot combined at once."), Who->GetTeam());
	}
	return Robot;
}

void ARRRandomStarRobot::CheatSwingAt(const ARRRandomStarRobot* Other)
{
	if (!HasAuthority() || !bAssembled || !Other || IsHeldStill())
	{
		return;
	}
	// Turned to it, a first swing from scratch (whatever was under way, and the cooldown, set aside)
	const FVector Toward = (Other->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (!Toward.IsNearlyZero())
	{
		AimYaw = FRotator::NormalizeAxis(Toward.Rotation().Yaw - GetActorRotation().Yaw);
	}
	bShieldUp = false;
	SwingStage = 0;
	bComboQueued = false;
	SwordReadyTime = 0.f;
	LastAttackTime = ServerNow(GetWorld());
	StartSwing(1);
}

void ARRRandomStarRobot::CheatFillCrew()
{
	if (!HasAuthority() || !bAssembled)
	{
		return;
	}
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It && GetFreeSeat() != ERRRobotSeat::None; ++It)
	{
		if (CanBoard(*It, true))
		{
			Board(*It, true);
		}
	}
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
		// (checked: the cars may have been gathered round a hedge or a crate, and a capsule put inside one can't walk; the
		// nearest free spot instead, a little to either side if need be, the robot snapping onto it below)
		const FRotator Facing(0.f, GetActorRotation().Yaw, 0.f);
		const FVector Feet = GetActorLocation() + FVector(0.f, 0.f, HalfHeight + 2.f);
		bool bPlaced = false;
		for (const FVector& Offset : { FVector::ZeroVector, FVector(0.f, 250.f, 0.f), FVector(0.f, -250.f, 0.f), FVector(250.f, 0.f, 0.f), FVector(-250.f, 0.f, 0.f) })
		{
			if (Who->TeleportTo(Feet + Facing.RotateVector(Offset), Facing))
			{
				bPlaced = true;
				break;
			}
		}
		if (!bPlaced)
		{
			Who->TeleportTo(Feet, Facing, false, true);
		}
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

void ARRRandomStarRobot::CrewFire(ARRRandomCharacter* Who, const FVector& InTarget)
{
	UWorld* World = GetWorld();
	// Reeling, knocked down or locked in a clash: nothing
	if (!HasAuthority() || !bAssembled || !World || !Who || IsHeldStill() || ARRRandomGameState::IsPlayLocked(this))
	{
		return;
	}
	const float Now = ServerNow(World);
	if (Who == GetCrew(ERRRobotSeat::Defend))
	{
		// A bot only puts it up under fire (held up through a whole fight, it would keep its crew from attacking)
		if ((Who->IsPlayerControlled() || Now - LastHitTime < BotShieldAfterHit) && !bShieldBroken && ShieldHealth > 0.f)
		{
			RaiseShield(Now, ShieldHold);
			// The attacks stop: a swing under way is cut off (before its blow, it never lands), the combo over
			if (SwingStage > 0)
			{
				SwingStage = 0;
				bSwingLanded = true;
				bComboQueued = false;
				SwordReadyTime = Now + SwordCooldown;
			}
		}
		return;
	}
	if (Who != GetCrew(ERRRobotSeat::Attack))
	{
		return;
	}
	// A bot goes for the rival robot when it's about (the crew it would aim at are hidden inside it anyway)
	FVector Target = InTarget;
	const ARRRandomStarRobot* Rival = Who->IsPlayerControlled() ? nullptr : GetRival();
	if (Rival && FVector::Dist2D(Rival->GetActorLocation(), GetActorLocation()) <= BotRivalRange)
	{
		Target = Rival->GetActorLocation() + FVector(0.f, 0.f, RobotHeight * 0.45f);
	}
	// A bot starts a new attack only when its pause is over (UpdateBotAttacker sets it; a cheat robot holds off a moment)
	if (!Who->IsPlayerControlled() && SwingStage == 0 && Now < BotAttackNext)
	{
		return;
	}
	// A bot sends the missiles at what's out of the sword's reach
	if (!Who->IsPlayerControlled() && FVector::Dist2D(Target, GetActorLocation()) > BotSwordRange)
	{
		CrewAltFire(Who, Target);
		return;
	}
	if (bShieldUp)
	{
		return;
	}
	// The upper body turns to the aim (relative to the legs)
	const FVector Toward = (Target - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (!Toward.IsNearlyZero())
	{
		AimYaw = FRotator::NormalizeAxis(Toward.Rotation().Yaw - GetActorRotation().Yaw);
	}
	LastAttackTime = Now;
	// Pressed during a swing: the next of the combo follows it (Tick); otherwise a new combo, once the sword is ready
	if (SwingStage > 0)
	{
		if (SwingStage < UE_ARRAY_COUNT(SwingSeconds) && Now - SwingStartTime >= ComboFrom * SwingSeconds[SwingStage - 1])
		{
			bComboQueued = true;
		}
		return;
	}
	if (Now >= SwordReadyTime)
	{
		StartSwing(1);
	}
}

void ARRRandomStarRobot::CrewAltFire(ARRRandomCharacter* Who, const FVector& Target)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !bAssembled || !World || !Who || Who != GetCrew(ERRRobotSeat::Attack) || bShieldUp || IsHeldStill() || ARRRandomGameState::IsPlayLocked(this))
	{
		return;
	}
	const float Now = ServerNow(World);
	const FVector Toward = (Target - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (!Toward.IsNearlyZero())
	{
		AimYaw = FRotator::NormalizeAxis(Toward.Rotation().Yaw - GetActorRotation().Yaw);
	}
	LastAttackTime = Now;
	if (Now < MissileReadyTime)
	{
		return;
	}
	MissileReadyTime = Now + MissileCooldown;
	++MissileVolleys;
	OnRep_Missiles();
	// From the launchers on its shoulders, turned with the upper body: each pod's missiles fan out to its own side, the
	// outer ones wider and flatter, then they curve in on the aim
	const FRotator Facing(0.f, GetActorRotation().Yaw + AimYaw, 0.f);
	const int32 PerSide = MissileCount / 2;
	for (int32 Index = 0; Index < MissileCount; ++Index)
	{
		const bool bLeft = (Index & 1) == 0;
		const float Spread = static_cast<float>(Index / 2 + 1) / PerSide;
		const FVector Pod = GetActorLocation() + Facing.RotateVector(bLeft ? MissilePodL : MissilePodR);
		const FRotator Out(MissileFanPitch * (1.f - 0.5f * Spread) + FMath::FRandRange(-5.f, 5.f), Facing.Yaw + (bLeft ? -1.f : 1.f) * MissileFanYaw * Spread, 0.f);
		const FTransform SpawnTransform(Out, Pod);
		if (ARRRandomProjectile* Missile = World->SpawnActorDeferred<ARRRandomProjectile>(ARRRandomProjectile::StaticClass(), SpawnTransform, this, Who, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Missile->MakeMissile(Target, MissileRadius);
			Missile->DamageMultiplier = MissileDamage;
			Missile->MaxRange = MissileRange;
			Missile->SetSpeedMultiplier(MissileSpeed);
			Missile->HitEffect = ERRHitEffect::Cannon;
			Missile->Tier = ERRWeaponTier::Epic;
			Missile->FinishSpawning(SpawnTransform);
		}
	}
}

void ARRRandomStarRobot::StartSwing(int32 Stage)
{
	SwingStage = Stage;
	SwingStartTime = ServerNow(GetWorld());
	bSwingLanded = false;
	bComboQueued = false;
	++SwingCount;
	OnRep_Swing();
}

void ARRRandomStarRobot::SwingHits()
{
	bSwingLanded = true;
	UWorld* World = GetWorld();
	SwingLandedTime = ServerNow(World);
	ARRRandomCharacter* Who = GetCrew(ERRRobotSeat::Attack);
	const int32 Index = FMath::Clamp<int32>(SwingStage, 1, UE_ARRAY_COUNT(SwingSeconds)) - 1;
	const float Facing = GetActorRotation().Yaw + AimYaw;
	const FVector Feet = GetActorLocation();
	// The blow is its swinger's (whose kill it is; its teammates are spared): the robot stands in for it meanwhile
	SetInstigator(Who);
	AController* By = Who ? Who->GetController() : nullptr;
	for (TActorIterator<ARRRandomCharacter> It(World); It; ++It)
	{
		ARRRandomCharacter* Victim = *It;
		const FVector To = Victim->GetActorLocation() - Feet;
		if (!Victim->IsAlive() || Victim->GetTeam() == Team || Victim->IsInRobot() || To.Size2D() > SwordReach + Victim->GetCapsuleComponent()->GetScaledCapsuleRadius()
			|| To.Z > RobotHeight || FMath::Abs(FRotator::NormalizeAxis(To.Rotation().Yaw - Facing)) > SwingArc[Index])
		{
			continue;
		}
		const FVector Way = To.GetSafeNormal2D();
		const FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Victim->GetActorLocation() - Way * Victim->GetCapsuleComponent()->GetScaledCapsuleRadius(), -Way);
		UGameplayStatics::ApplyPointDamage(Victim, SwingDamage[Index], Way, Hit, By, this, UDamageType::StaticClass());
	}
	SetInstigator(nullptr);
	// The rival robot in the sweep (its crew hidden inside it were passed over just now)
	if (ARRRandomStarRobot* Rival = GetRival())
	{
		SwingAtRival(Rival, Index, Who);
	}
	// What the blade meets is smashed, a ball out of it where it meets it: across the sweep at the height it cuts, or (the
	// slam) a crater where it comes down on something. Traced first, so a blow at nothing leaves no wreck.
	FCollisionObjectQueryParams Solid;
	Solid.AddObjectTypesToQuery(ECC_WorldStatic);
	Solid.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRobotSword), false, this);
	const auto Smash = [&](const FVector& From, const FVector& To, float Radius)
	{
		TArray<FHitResult> Hits;
		World->LineTraceMultiByObjectType(Hits, From, To, Solid, Params);
		for (const FHitResult& Hit : Hits)
		{
			const AActor* Struck = Hit.GetActor();
			if (Hit.bBlockingHit && !Cast<APawn>(Struck) && !Cast<ARRRandomProjectile>(Struck) && !Cast<ARRRandomStarRobot>(Struck))
			{
				ARRRandomWreckage::Wreck(this, Hit.ImpactPoint, Hit.ImpactPoint, Radius, false);
				return;
			}
		}
	};
	if (Index == 2)
	{
		const FVector Landing = Feet + FRotator(0.f, Facing, 0.f).Vector() * SwordReach * 0.75f;
		Smash(Landing + FVector(0.f, 0.f, RobotHeight), Landing + FVector(0.f, 0.f, SwordCutHeight * 0.5f), SlamWreckRadius);
		return;
	}
	for (float Off = -SwingArc[Index]; Off <= SwingArc[Index] + 1.f; Off += SwingArc[Index] / 3.f)
	{
		const FVector Way = FRotator(0.f, Facing + Off, 0.f).Vector();
		Smash(Feet + Way * HullRadius + FVector(0.f, 0.f, SwordCutHeight), Feet + Way * SwordReach + FVector(0.f, 0.f, SwordCutHeight), SwordWreckRadius);
	}
}

void ARRRandomStarRobot::CheatShield(float Seconds)
{
	if (HasAuthority() && bAssembled && !IsHeldStill() && !bShieldBroken && ShieldHealth > 0.f)
	{
		RaiseShield(ServerNow(GetWorld()), Seconds);
	}
}

void ARRRandomStarRobot::SwingAtRival(ARRRandomStarRobot* Rival, int32 Index, ARRRandomCharacter* Who)
{
	const float Now = ServerNow(GetWorld());
	const FVector To = (Rival->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (To.Size() > SwordReach + HullRadius || FMath::Abs(FRotator::NormalizeAxis(To.Rotation().Yaw - GetActorRotation().Yaw - AimYaw)) > SwingArc[Index]
		|| IsClashing() || Rival->IsClashing())
	{
		return;
	}
	// Down: only the slam finishes it off
	if (Rival->bDowned)
	{
		if (Index == 2)
		{
			Rival->FinishOff(this);
		}
		return;
	}
	// How far off the way it faces the blow comes at it (for its shield, and its own blow at this one)
	const float Off = FMath::Abs(FRotator::NormalizeAxis((-To).Rotation().Yaw - Rival->GetActorRotation().Yaw - Rival->AimYaw));
	// Its own blow coming at this one too (or just landed): the blades meet
	const bool bRivalSwinging = Rival->SwingStage > 0 && (!Rival->bSwingLanded || Now - Rival->SwingLandedTime <= ClashWindow);
	if (bRivalSwinging && Off <= SwingArc[FMath::Clamp<int32>(Rival->SwingStage, 1, UE_ARRAY_COUNT(SwingArc)) - 1] && Now - FMath::Max(ClashEndTime, Rival->ClashEndTime) >= ClashCooldown)
	{
		StartClash(Rival);
		return;
	}
	if (Rival->bShieldUp && Off <= ShieldArc)
	{
		// Raised just in time: the blow bounces off, and this one reels
		if (Now - Rival->ShieldRaisedTime <= ParryWindow)
		{
			Rival->Shout(ERRRobotShout::Parry);
			Stagger(ParryReel);
			UE_LOG(LogTemp, Log, TEXT("RRRandom: team %d's star robot parries team %d's sword."), Rival->Team, Team);
			return;
		}
		// The slam smashes the shield down (broken if it empties) and the defender reels
		if (Index == 2)
		{
			Rival->ShieldHealth = FMath::Max(0.f, Rival->ShieldHealth - GuardBreakDamage);
			Rival->bShieldBroken |= Rival->ShieldHealth <= 0.f;
			Rival->ShieldMendFrom = Now + ShieldMendDelay;
			Rival->Shout(ERRRobotShout::GuardBreak);
			Rival->Stagger(GuardBreakReel);
			UE_LOG(LogTemp, Log, TEXT("RRRandom: team %d's star robot breaks team %d's guard."), Team, Rival->Team);
			return;
		}
	}
	// A hit (a raised shield facing it takes it first: TakeDamage), the blow its swinger's
	const FVector Way = To.GetSafeNormal();
	const FHitResult Hit(Rival, Rival->Hull, Rival->GetActorLocation() + FVector(0.f, 0.f, RobotHeight * 0.5f) - Way * HullRadius, -Way);
	SetInstigator(Who);
	UGameplayStatics::ApplyPointDamage(Rival, SwingDamage[Index] * SwordOnRobot, Way, Hit, Who ? Who->GetController() : nullptr, this, UDamageType::StaticClass());
	SetInstigator(nullptr);
}

void ARRRandomStarRobot::Stagger(float Seconds)
{
	const float Now = ServerNow(GetWorld());
	StaggerUntil = FMath::Max(StaggerUntil, Now + Seconds);
	// Cut off: the swing (before its blow, it never lands) and the combo, and the shield
	if (SwingStage > 0)
	{
		SwingStage = 0;
		bSwingLanded = true;
		bComboQueued = false;
	}
	SwordReadyTime = FMath::Max(SwordReadyTime, StaggerUntil);
	bShieldUp = false;
}

bool ARRRandomStarRobot::IsStaggered() const
{
	return GetWorld() && ServerNow(GetWorld()) < StaggerUntil;
}

void ARRRandomStarRobot::Shout(ERRRobotShout Word)
{
	ShoutWord = Word;
	++ShoutCount;
	OnRep_Shout();
}

void ARRRandomStarRobot::OnRep_Shout()
{
	if (ShoutCount == LastShoutSeen)
	{
		return;
	}
	LastShoutSeen = ShoutCount;
	ShoutSeenTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

ERRRobotShout ARRRandomStarRobot::GetShout(float& OutAge) const
{
	OutAge = GetWorld() ? GetWorld()->GetTimeSeconds() - ShoutSeenTime : UE_BIG_NUMBER;
	return LastShoutSeen > 0 ? ShoutWord : ERRRobotShout::None;
}

void ARRRandomStarRobot::RaiseShield(float Now, float Hold)
{
	if (!bShieldUp)
	{
		ShieldRaisedTime = Now;
	}
	bShieldUp = true;
	ShieldHoldUntil = FMath::Max(ShieldHoldUntil, Now + Hold);
	ShieldMendFrom = ShieldHoldUntil + ShieldMendDelay;
}

void ARRRandomStarRobot::StartClash(ARRRandomStarRobot* Rival)
{
	const float Now = ServerNow(GetWorld());
	for (ARRRandomStarRobot* Side : { this, Rival })
	{
		ARRRandomStarRobot* Other = Side == this ? Rival : this;
		Side->ClashRival = Other;
		Side->ClashStartTime = Now;
		Side->ClashOutcome = 0;
		// A row of random keys for each seat
		Side->ClashKeys.SetNum(3 * ClashKeyCount);
		for (uint8& Key : Side->ClashKeys)
		{
			Key = static_cast<uint8>(FMath::RandRange(0, 3));
		}
		Side->ClashDone.Init(0, 3);
		Side->ClashMisses.Init(0, 3);
		for (int32 Seat = 0; Seat < 3; ++Seat)
		{
			Side->ClashLockUntil[Seat] = 0.f;
			Side->ClashBotNext[Seat] = Now + FMath::FRandRange(BotClashPace.X, BotClashPace.Y) + 0.3f;
		}
		// Blade to blade: no swing, no shield, facing each other (held so till it's over)
		Side->SwingStage = 0;
		Side->bSwingLanded = true;
		Side->bComboQueued = false;
		Side->bShieldUp = false;
		const FVector Toward = (Other->GetActorLocation() - Side->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (!Toward.IsNearlyZero())
		{
			Side->AimYaw = FRotator::NormalizeAxis(Toward.Rotation().Yaw - Side->GetActorRotation().Yaw);
		}
		Side->LastAttackTime = Now + ClashSeconds;
		Side->Shout(ERRRobotShout::Clash);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the star robots of team %d and team %d lock blades (a clash)."), Team, Rival->Team);
}

void ARRRandomStarRobot::UpdateClash(float Now)
{
	ARRRandomStarRobot* Rival = ClashRival;
	if (ClashOutcome != 0)
	{
		// The outcome shown a moment, then it's over
		if (Now - ClashEndTime >= ClashOutcomeShown)
		{
			ClashRival = nullptr;
			ClashOutcome = 0;
		}
		return;
	}
	if (!IsValid(Rival) || !Rival->bAssembled)
	{
		EndClash(3);
		return;
	}
	// Bots press their keys at a human pace, now and then the wrong one
	for (int32 Seat = 0; Seat < 3; ++Seat)
	{
		const ARRRandomCharacter* Member = Crew.IsValidIndex(Seat) ? Crew[Seat].Get() : nullptr;
		if (Member && !Member->IsPlayerControlled() && ClashDone[Seat] < ClashKeyCount && Now >= ClashBotNext[Seat])
		{
			if (FMath::FRand() < BotClashMiss)
			{
				++ClashMisses[Seat];
				ClashBotNext[Seat] = Now + ClashWrongLock;
			}
			else
			{
				++ClashDone[Seat];
			}
			ClashBotNext[Seat] = FMath::Max(ClashBotNext[Seat], Now + FMath::FRandRange(BotClashPace.X, BotClashPace.Y));
		}
	}
	// Whoever's crew is done first wins; out of time, whoever got further
	const float Mine = GetClashShare();
	const float Theirs = Rival->GetClashShare();
	if (Mine >= 1.f || Theirs >= 1.f || Now - ClashStartTime >= ClashSeconds)
	{
		EndClash(Mine > Theirs ? 1 : Mine < Theirs ? 2 : 3);
	}
}

void ARRRandomStarRobot::EndClash(uint8 Outcome)
{
	const float Now = ServerNow(GetWorld());
	ARRRandomStarRobot* Rival = ClashRival;
	const uint8 TheirOutcome = Outcome == 1 ? 2 : Outcome == 2 ? 1 : 3;
	for (const TPair<ARRRandomStarRobot*, uint8>& Side : { TPair<ARRRandomStarRobot*, uint8>(this, Outcome), TPair<ARRRandomStarRobot*, uint8>(Rival, TheirOutcome) })
	{
		ARRRandomStarRobot* Robot = Side.Key;
		if (!IsValid(Robot) || !Robot->ClashRival)
		{
			continue;
		}
		Robot->ClashOutcome = Side.Value;
		Robot->ClashEndTime = Now;
		Robot->LastAttackTime = Now;
		// The winner's sword is ready at once; the loser reels, wide open; even, both are pushed back a little
		if (Side.Value == 1)
		{
			Robot->SwordReadyTime = Now;
			Robot->Shout(ERRRobotShout::ClashWin);
		}
		else
		{
			Robot->Stagger(Side.Value == 2 ? ClashLoseReel : ClashEvenReel);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the clash is over, team %d's star robot %s."), Team, Outcome == 1 ? TEXT("wins") : Outcome == 2 ? TEXT("loses") : TEXT("draws"));
}

void ARRRandomStarRobot::ClashKey(ARRRandomCharacter* Who, uint8 Key)
{
	if (!HasAuthority() || !IsClashing() || Key > 3)
	{
		return;
	}
	const int32 Seat = Crew.IndexOfByKey(Who);
	const float Now = ServerNow(GetWorld());
	if (Seat == INDEX_NONE || !ClashDone.IsValidIndex(Seat) || Now < ClashLockUntil[Seat] || ClashDone[Seat] >= ClashKeyCount)
	{
		return;
	}
	if (ClashKeys[Seat * ClashKeyCount + ClashDone[Seat]] == Key)
	{
		++ClashDone[Seat];
	}
	else
	{
		// Wrong: locked out a moment
		++ClashMisses[Seat];
		ClashLockUntil[Seat] = Now + ClashWrongLock;
	}
}

float ARRRandomStarRobot::GetClashTime() const
{
	return FMath::Max(0.f, ServerNow(GetWorld()) - ClashStartTime);
}

uint8 ARRRandomStarRobot::GetClashKey(int32 Seat, int32 Index) const
{
	const int32 At = Seat * ClashKeyCount + Index;
	return ClashKeys.IsValidIndex(At) && Index >= 0 && Index < ClashKeyCount ? ClashKeys[At] : 0;
}

int32 ARRRandomStarRobot::GetClashDone(int32 Seat) const
{
	return ClashDone.IsValidIndex(Seat) ? ClashDone[Seat] : 0;
}

int32 ARRRandomStarRobot::GetClashMisses(int32 Seat) const
{
	return ClashMisses.IsValidIndex(Seat) ? ClashMisses[Seat] : 0;
}

float ARRRandomStarRobot::GetClashShare() const
{
	// All its crew's rows together: everyone in it has to get through theirs
	int32 Needed = 0;
	int32 Done = 0;
	for (int32 Seat = 0; Seat < 3; ++Seat)
	{
		if (Crew.IsValidIndex(Seat) && Crew[Seat] && ClashDone.IsValidIndex(Seat))
		{
			Needed += ClashKeyCount;
			Done += ClashDone[Seat];
		}
	}
	return Needed > 0 ? static_cast<float>(Done) / Needed : 0.f;
}

void ARRRandomStarRobot::KnockDown()
{
	const float Now = ServerNow(GetWorld());
	bDowned = true;
	Health = 1.f;
	DownedUntil = Now + DownSeconds;
	Stagger(0.f);
	if (IsClashing())
	{
		EndClash(2);
	}
	Shout(ERRRobotShout::Down);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: team %d's star robot is knocked down."), Team);
}

void ARRRandomStarRobot::FinishOff(ARRRandomStarRobot* By)
{
	if (By)
	{
		By->Shout(ERRRobotShout::Finish);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: team %d's star robot is finished off by team %d's slam."), Team, By ? By->Team : -1);
	BreakApart(false, true);
}

void ARRRandomStarRobot::UpdateBotDefender(float Now)
{
	const ARRRandomCharacter* Defender = GetCrew(ERRRobotSeat::Defend);
	const ARRRandomStarRobot* Rival = GetRival();
	if (!Defender || Defender->IsPlayerControlled() || !Rival || Rival->SwingStage == 0 || Rival->bSwingLanded || Rival->SwingCount == RivalSwingJudged
		|| SwingStage > 0 || IsHeldStill() || bShieldBroken || ShieldHealth <= 0.f)
	{
		return;
	}
	// Judged once a rival swing, just before its blow would land: in reach and coming from the front, a coin toss
	const float Lands = Rival->SwingStartTime + SwingHitAt * SwingSeconds[FMath::Clamp<int32>(Rival->SwingStage, 1, UE_ARRAY_COUNT(SwingSeconds)) - 1];
	if (Lands - Now > BotParryLead)
	{
		return;
	}
	RivalSwingJudged = Rival->SwingCount;
	const FVector From = (Rival->GetActorLocation() - GetActorLocation()) * FVector(1.f, 1.f, 0.f);
	if (From.Size() <= SwordReach + HullRadius * 2.f && FMath::Abs(FRotator::NormalizeAxis(From.Rotation().Yaw - GetActorRotation().Yaw - AimYaw)) <= ShieldArc
		&& FMath::FRand() < BotParryChance)
	{
		RaiseShield(Now, 0.5f);
	}
}

void ARRRandomStarRobot::UpdateBotAttacker(float Now)
{
	ARRRandomCharacter* Attacker = GetCrew(ERRRobotSeat::Attack);
	const ARRRandomStarRobot* Rival = GetRival();
	if (!Attacker || Attacker->IsPlayerControlled() || !Rival || IsHeldStill() || bShieldUp
		|| FVector::Dist2D(Rival->GetActorLocation(), GetActorLocation()) > BotRivalRange)
	{
		return;
	}
	const FVector Target = Rival->GetActorLocation() + FVector(0.f, 0.f, RobotHeight * 0.45f);
	if (SwingStage == 0)
	{
		// A moment between combos, then one of one to three swings (or missiles, from afar: CrewFire)
		if (Now < BotAttackNext)
		{
			return;
		}
		BotComboGoal = Rival->IsDowned() ? 3 : FMath::RandRange(1, 3);
		CrewFire(Attacker, Target);
		BotAttackNext = Now + FMath::FRandRange(0.2f, 0.8f);
	}
	else if (SwingStage < BotComboGoal || (Rival->IsDowned() && SwingStage < 3))
	{
		CrewFire(Attacker, Target);
	}
}

int32 ARRRandomStarRobot::GetShownSwing(float& OutInto) const
{
	if (ShownStage <= 0 || !GetWorld())
	{
		OutInto = 0.f;
		return 0;
	}
	OutInto = (GetWorld()->GetTimeSeconds() - SwingSeenTime) / SwingSeconds[ShownStage - 1];
	return ShownStage;
}

void ARRRandomStarRobot::HideHeadOnThisScreen(bool bHide)
{
	if (Head->bHiddenInGame != bHide)
	{
		Head->SetHiddenInGame(bHide);
	}
}

FVector ARRRandomStarRobot::GetClashFocus() const
{
	return ClashRival ? (GetActorLocation() + ClashRival->GetActorLocation()) * 0.5f + FVector(0.f, 0.f, ClashBindHeight) : GetActorLocation();
}

FLinearColor ARRRandomStarRobot::GetTeamGlow() const
{
	return FMath::Lerp(ARRRandomCharacter::GetTeamColor(Team), FLinearColor::White, 0.2f);
}

float ARRRandomStarRobot::GetSwordReadyIn() const
{
	return FMath::Max(0.f, SwordReadyTime - ServerNow(GetWorld()));
}

float ARRRandomStarRobot::GetMissileReadyIn() const
{
	return FMath::Max(0.f, MissileReadyTime - ServerNow(GetWorld()));
}

float ARRRandomStarRobot::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const ARRRandomCharacter* Attacker = DamageCauser ? Cast<ARRRandomCharacter>(DamageCauser->GetInstigator()) : nullptr;
	if (!Attacker && EventInstigator)
	{
		Attacker = Cast<ARRRandomCharacter>(EventInstigator->GetPawn());
	}
	if (!HasAuthority() || !bAssembled || IsActorBeingDestroyed() || DamageAmount <= 0.f || bDowned || (Attacker && Attacker->GetTeam() == Team) || (Attacker && Attacker->IsDecoy())
		|| ARRRandomGameState::IsPlayLocked(this))
	{
		return 0.f;
	}
	const float Now = ServerNow(GetWorld());
	LastHitTime = Now;
	float Taken = DamageAmount;
	// Coming at it from the front (where the upper body faces) with the shield up: the shield takes it, as far as its
	// health goes; emptied, it breaks
	if (bShieldUp && ShieldHealth > 0.f)
	{
		FVector From = DamageCauser ? DamageCauser->GetActorLocation() - GetActorLocation() : FVector::ZeroVector;
		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			From = -static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection;
		}
		if (!From.IsNearlyZero() && FMath::Abs(FRotator::NormalizeAxis(From.Rotation().Yaw - GetActorRotation().Yaw - AimYaw)) <= ShieldArc)
		{
			const float Blocked = FMath::Min(ShieldHealth, Taken);
			ShieldHealth -= Blocked;
			Taken -= Blocked;
			if (ShieldHealth <= 0.f)
			{
				ShieldHealth = 0.f;
				bShieldBroken = true;
				bShieldUp = false;
				ShieldMendFrom = Now + ShieldMendDelay;
				UE_LOG(LogTemp, Log, TEXT("RRRandom: the star robot's shield (team %d) breaks apart."), Team);
			}
		}
	}
	// A missile landing in the middle of a swing stops it
	const ARRRandomProjectile* Shot = Cast<ARRRandomProjectile>(DamageCauser);
	if (Shot && Shot->bMissile && SwingStage > 0)
	{
		Stagger(MissileReel);
	}
	if (Taken <= 0.f)
	{
		return 0.f;
	}
	Health = FMath::Max(0.f, Health - Taken);
	// The rival's sword makes it reel, not again for a moment (it gets a chance to swing back)
	if (Cast<ARRRandomStarRobot>(DamageCauser) && Now >= ReelGuardUntil)
	{
		Stagger(HitReel);
		ReelGuardUntil = Now + HitReelGuard;
	}
	if (Health <= 0.f)
	{
		// Brought down with the rival close by: it kneels, for the rival to finish off with a slam (or it gets up again)
		const ARRRandomStarRobot* Rival = GetRival();
		if (Rival && FVector::Dist2D(Rival->GetActorLocation(), GetActorLocation()) <= DownNear)
		{
			KnockDown();
			return Taken;
		}
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
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the star robot (team %d) is combined (the haetae)."), Team);
	// The one who combined it gets the first seat, wherever it stood; having struck its pose toward that one, it turns
	// round to face the way that one was looking, ready to go (the driver turns it: ARRRandomCharacter)
	IntroTurnYaw = FRotator::NormalizeAxis(GetActorRotation().Yaw + 180.f);
	bIntroTurn = !bCheatFillCrew;
	if (Who && Who->IsAlive())
	{
		Board(Who, true);
	}
	if (bCheatFillCrew)
	{
		CheatFillCrew();
	}
}

void ARRRandomStarRobot::BreakApart(bool bBackToStart, bool bFinished)
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
	MulticastBreak(bBackToStart, bFinished);
	Destroy();
}

void ARRRandomStarRobot::MulticastBreak_Implementation(bool bFizzle, bool bFinished)
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
	// Finished off by the rival's slam: a pillar of light and a ring of gold on top
	if (bFinished)
	{
		Effects->UltimateDieBurst(Middle + FVector(0.f, 0.f, RobotHeight * 0.5f), 120.f, GetActorLocation());
		Effects->GiantBurst(GetActorLocation(), 5.f, StarGold);
		Effects->FireBurst(Middle, 3.5f);
	}
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
		for (UStaticMeshComponent* Part : { SwordHilt.Get(), SwordBlade.Get(), SwordCore.Get() })
		{
			Part->SetVisibility(true);
		}
		Hips->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Upper->SetRelativeLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
		Neck->SetRelativeLocationAndRotation(NeckPivot, FRotator::ZeroRotator);
		ShoulderL->SetRelativeLocationAndRotation(ShoulderPivotL, FRotator::ZeroRotator);
		ShoulderR->SetRelativeLocationAndRotation(ShoulderPivotR, FRotator::ZeroRotator);
		for (USceneComponent* Pivot : { Neck.Get(), ShoulderL.Get(), ShoulderR.Get() })
		{
			Pivot->SetRelativeScale3D(FVector::OneVector);
		}
		Legs->SetRelativeLocation(FVector::ZeroVector);
		Torso->SetRelativeLocation(FVector::ZeroVector);
		LastPoseYaw = GetActorRotation().Yaw;
		if (AssembledSeenTime < 0.f && GetWorld())
		{
			AssembledSeenTime = GetWorld()->GetTimeSeconds();
		}
	}
}

void ARRRandomStarRobot::OnRep_Missiles()
{
	// A volley: a puff of sparks at each launcher (the missiles carry their own trails)
	if (MissileVolleys == LastVolleySeen)
	{
		return;
	}
	LastVolleySeen = MissileVolleys;
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		for (const FVector& Pod : { MissilePodL, MissilePodR })
		{
			Effects->HitBurst(Upper->GetComponentTransform().TransformPosition(Pod), FVector::DownVector, 2.f, StarGold);
		}
	}
}

void ARRRandomStarRobot::OnRep_Swing()
{
	// A swing starts (the next of a combo cuts the last one's follow-through short)
	if (SwingCount == LastSwingSeen)
	{
		return;
	}
	LastSwingSeen = SwingCount;
	SwingSeenTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	ShownStage = FMath::Clamp<int32>(SwingStage, 1, UE_ARRAY_COUNT(SwingSeconds));
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
			// The shield: up while the DEFEND seat holds fire; lowered (or broken), it mends after a moment, and broken it
			// can't go up again until it has mended a share
			const float Now = ServerNow(World);
			if (bShieldUp && (Now > ShieldHoldUntil || !GetCrew(ERRRobotSeat::Defend)))
			{
				bShieldUp = false;
			}
			if (!bShieldUp && Now >= ShieldMendFrom)
			{
				ShieldHealth = FMath::Min(ShieldMaxHealth, ShieldHealth + ShieldMendRate * DeltaSeconds);
			}
			bShieldBroken &= ShieldHealth < ShieldRearm;
			// The sword's swing under way: its blow lands, the next of the combo follows if it was asked for, or the combo
			// is over and the cooldown starts
			if (SwingStage > 0)
			{
				const float Into = (Now - SwingStartTime) / SwingSeconds[SwingStage - 1];
				if (!bSwingLanded && Into >= SwingHitAt)
				{
					SwingHits();
				}
				if (bComboQueued && Into >= ComboChainAt && GetCrew(ERRRobotSeat::Attack))
				{
					StartSwing(SwingStage + 1);
				}
				else if (Into >= 1.f)
				{
					SwingStage = 0;
					bComboQueued = false;
					SwordReadyTime = Now + SwordCooldown;
				}
			}
			// Robot against robot: up again if not finished off in time; the clash under way (or showing how it ended); a bot
			// in the DEFEND seat parrying
			if (bDowned && Now >= DownedUntil)
			{
				bDowned = false;
				Health = MaxHealth * RiseHealthShare;
				Stagger(0.5f);
				Shout(ERRRobotShout::Rise);
			}
			if (ClashRival)
			{
				UpdateClash(Now);
			}
			UpdateBotDefender(Now);
			UpdateBotAttacker(Now);
			// Facing front again a while after the last attack
			if (SwingStage == 0 && Now - LastAttackTime > AimHoldTime)
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
		Effects->StarImpact(Feet + Up * 370.f, Up, 3.5f, StarGold);
		break;
	case 5:
		// The arms clamp on
		Effects->HitBurst(GetActorTransform().TransformPosition(ShoulderPivotL), GetActorRightVector(), 4.f, StarGold);
		Effects->HitBurst(GetActorTransform().TransformPosition(ShoulderPivotR), -GetActorRightVector(), 4.f, StarGold);
		break;
	case 6:
		// The fists shoot out of the arms: a burst of stars at each as they start (no rocket smoke: it would roll up over the
		// head coming out next)
		for (const FVector& Tip : { FistTipL, FistTipR })
		{
			const FVector Where = GetActorTransform().TransformPosition(Tip);
			Effects->StarImpact(Where, -Up, 2.f, StarGold);
			Effects->HitBurst(Where, Up, 3.f, StarViolet);
		}
		break;
	case 7:
		// The chest's floor opens: light pours out where the head is coming from
		Effects->StarImpact(GetActorTransform().TransformPosition(NeckPivot), Up, 2.5f, StarCyan);
		Effects->DiceLandBurst(GetActorTransform().TransformPosition(NeckPivot), 60.f);
		break;
	case 8:
		// The head springs up, its eyes lighting: the glow behind it, small enough not to hide the face
		Effects->MoonFlash(GetActorTransform().TransformPosition(FVector(-110.f, 0.f, 545.f)), 0.8f);
		Effects->WingsBurst(GetActorTransform().TransformPosition(FVector(-60.f, 0.f, 545.f)), StarCyan);
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
	for (UStaticMeshComponent* Part : { Legs.Get(), Torso.Get(), Head.Get() })
	{
		Part->SetVisibility(bParts);
	}
	// The arms come jointed (when the pieces are there), so their fists can stay folded into them until they shoot out
	ShowJointed(false);
	for (UStaticMeshComponent* Part : { ArmL.Get(), ArmR.Get() })
	{
		Part->SetVisibility(bParts && !bHasJoints);
	}
	for (UStaticMeshComponent* Part : { UpperArmL.Get(), UpperArmR.Get(), ForearmL.Get(), ForearmR.Get() })
	{
		Part->SetVisibility(bParts && bHasJoints);
	}
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
		// The fists shooting out kick the arms up a little
		const float Kick = FMath::Sin(Phase(Time, FistsStart, FistsOut + 0.2f) * PI) * 18.f;
		ShoulderL->SetRelativeLocationAndRotation(ShoulderPivotL + Lift + Drift + FVector(0.f, -Out, Kick), FRotator(Swing, 0.f, Side * 40.f));
		ShoulderR->SetRelativeLocationAndRotation(ShoulderPivotR + Lift + Drift + FVector(0.f, Out, Kick), FRotator(Swing, 0.f, -Side * 40.f));
		// Grown at the shoulders (the arm pieces' own pivot is the robot's, down at its feet)
		const float Scale = Pop(Time, WhirlEnd + 0.1f);
		ShoulderL->SetRelativeScale3D(FVector(Scale));
		ShoulderR->SetRelativeScale3D(FVector(Scale));
		// The fists: folded up into the arms at the elbows, then shooting out past full length and back
		const float Fist = Time < FistsStart ? FistHidden : FMath::Lerp(FistHidden, 1.f, BackOut(Phase(Time, FistsStart, FistsOut), 2.5f));
		ElbowL->SetRelativeScale3D(FVector(Fist));
		ElbowR->SetRelativeScale3D(FVector(Fist));
	}

	// Head: hidden under the chest's floor (the chest car carries it there); the floor opens, it peeks out small,
	// looking about, then springs up to full size with a wobble. Grown at the neck (the head piece's own pivot is the
	// robot's, down at its feet)
	{
		float Sink = HeadSunk;
		float Size = 0.01f;
		float Look = 0.f;
		if (Time >= HeadStart && Time < HeadPeek)
		{
			const float T = EaseOut(Phase(Time, HeadStart, HeadPeek));
			Sink = FMath::Lerp(HeadSunk, HeadPeekSink, T);
			Size = FMath::Lerp(0.45f, 0.7f, T);
			Look = FMath::Sin((Time - HeadStart) * 18.f) * 20.f * T;
		}
		else if (Time >= HeadPeek)
		{
			const float Rise = BackOut(Phase(Time, HeadPeek, HeadUp), 3.f);
			Sink = HeadPeekSink * (1.f - Rise);
			Size = FMath::Lerp(0.7f, 1.f, Rise);
		}
		const float Wobble = Time > HeadUp ? FMath::Sin((Time - HeadUp) * 22.f) * 8.f * FMath::Max(0.f, 1.f - (Time - HeadUp) * 2.f) : 0.f;
		Neck->SetRelativeLocationAndRotation(NeckPivot - FVector(0.f, 0.f, Sink), FRotator(0.f, Look, Wobble));
		Neck->SetRelativeScale3D(FVector(Size));
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
	// The sword's swing on show: winding up (from rest, or in a combo from the last swing's follow-through), the blow,
	// held, then back to rest; the shield going up cuts it off
	ShieldShown = FMath::FInterpTo(ShieldShown, bShieldUp ? 1.f : 0.f, DeltaSeconds, 12.f);
	const float ShieldEase = EaseInOut(ShieldShown);
	FArmPose Blow = SwordRest;
	float SwingWeight = 0.f;
	float SwingInto = 1.f;
	float SwingWasInto = 1.f;
	if (ShownStage > 0)
	{
		const int32 Index = ShownStage - 1;
		SwingInto = (Now - SwingSeenTime) / SwingSeconds[Index];
		SwingWasInto = (Now - DeltaSeconds - SwingSeenTime) / SwingSeconds[Index];
		if (SwingInto >= 1.f || bShieldUp || IsStaggered() || IsClashing())
		{
			ShownStage = 0;
		}
		else
		{
			const FArmPose& From = Index > 0 ? SwingFollow[Index - 1] : SwordRest;
			Blow = SwingInto < SwingWindEnd ? BlendPose(From, SwingWindUp[Index], EaseInOut(SwingInto / SwingWindEnd))
				: BlendPose(SwingWindUp[Index], SwingFollow[Index], EaseOut(Phase(SwingInto, SwingWindEnd, SwingStrikeEnd)));
			SwingWeight = 1.f - EaseInOut(Phase(SwingInto, SwingRecoverFrom, 1.f));
		}
	}
	// Locked in a clash: three exchanges of blows, each winding up, striking the rival's blade (sparks fly where they
	// meet) and pushing against it, shaking, then drawing back for the next
	const ARRRandomStarRobot* Rival = ClashRival;
	const FLinearColor Glow = GetTeamGlow();
	float ClashInto = -1.f;
	float ClashWasInto = -1.f;
	if (Rival && ClashOutcome == 0)
	{
		constexpr float Exchanges = 3.f;
		const float Period = ClashSeconds / Exchanges;
		const float Time = GetClashTime();
		const int32 Exchange = FMath::Clamp(FMath::FloorToInt(Time / Period), 0, 2);
		const float Into = Time / Period - Exchange;
		const float WasInto = (Time - DeltaSeconds) / Period - Exchange;
		const FArmPose& Wind = SwingWindUp[Exchange];
		// Blade to blade (the bind): the sword arm held out a little below a point between the two and the blade tipped up
		// through it, so the two blades cross there in an X; pushing, it shakes
		const float Push = Into >= ClashStrikeAt && Into < ClashPushEnd ? 1.f : 0.f;
		const FVector Bind = (GetActorLocation() + Rival->GetActorLocation()) * 0.5f + FVector(0.f, 0.f, ClashBindHeight + FMath::Sin(Now * 40.f) * 8.f * Push);
		const FTransform Facing(FRotator(0.f, GetActorRotation().Yaw + AimYaw, 0.f), GetActorLocation());
		const FVector BindHere = Facing.InverseTransformPosition(Bind);
		const FVector Aim = (BindHere - FVector(0.f, 0.f, ClashBindDrop) - ShoulderPivotR).GetSafeNormal();
		const FVector Fist = ShoulderPivotR + Aim * FVector::Dist(ShoulderPivotR, FistTipR);
		const float Tip = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp((BindHere - Fist).GetSafeNormal().Z, -1.f, 1.f)) - FMath::Asin(FMath::Clamp(Aim.Z, -1.f, 1.f)));
		const FArmPose Meet = { 0.f, -4.f * Push,
			FRotator(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(-Aim.Z, -1.f, 1.f))), FMath::RadiansToDegrees(FMath::Atan2(Aim.Y, Aim.X)), 0.f), 0.f, -90.f + Tip, 0.3f * Push };
		const FArmPose& From = Exchange > 0 ? SwingWindUp[Exchange - 1] : SwordRest;
		Blow = Into < ClashWindEnd ? BlendPose(From, Wind, EaseInOut(Into / ClashWindEnd))
			: Into < ClashStrikeAt ? BlendPose(Wind, Meet, EaseIn(Phase(Into, ClashWindEnd, ClashStrikeAt)))
			: Into < ClashPushEnd ? Meet : BlendPose(Meet, Wind, EaseInOut(Phase(Into, ClashPushEnd, 1.f)));
		SwingWeight = 1.f;
		ClashInto = Into;
		ClashWasInto = WasInto;
	}
	// Reeling (wobbling, dizzy stars over its head) and knocked down (kneeling, head hung)
	ReelShown = FMath::FInterpTo(ReelShown, IsStaggered() ? 1.f : 0.f, DeltaSeconds, 10.f);
	DownShown = FMath::FInterpTo(DownShown, bDowned ? 1.f : 0.f, DeltaSeconds, 6.f);
	const float Wobble = FMath::Sin(Now * 11.f) * 8.f * ReelShown;
	if (ReelShown > 0.3f && !bDowned)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->DiceSpinSparkle(GetTopLocation() + FVector(0.f, 0.f, 40.f), 110.f, DeltaSeconds);
		}
	}

	// Walking with its driver's steps
	const ARRRandomCharacter* Driver = GetCrew(ERRRobotSeat::Move);
	// Turning on the spot steps too (degrees a second counted as this much walking)
	const float Yaw = GetActorRotation().Yaw;
	const float TurnSpeed = DeltaSeconds > 0.f ? FMath::Abs(FRotator::NormalizeAxis(Yaw - LastPoseYaw)) / DeltaSeconds * TurnStepShare : 0.f;
	LastPoseYaw = Yaw;
	const float Speed = FMath::Max(Driver ? Driver->GetVelocity().Size2D() : 0.f, TurnSpeed);

	// Jumping (its driver in the air, ARRRandomCharacter::CanJumpRobot): the legs tuck up and the arms go up for balance,
	// rockets under the feet fire on the way up; it comes down in a deep crouch with a thud of dust and a shock ring
	const UCharacterMovementComponent* DriverMovement = Driver ? Driver->GetCharacterMovement() : nullptr;
	const bool bAirborne = DriverMovement && DriverMovement->IsFalling();
	AirAmount = FMath::FInterpTo(AirAmount, bAirborne ? 1.f : 0.f, DeltaSeconds, bAirborne ? 10.f : 16.f);
	ARRRandomEffects* JumpEffects = bHasJoints && (bAirborne || bWasAirborne) ? ARRRandomEffects::Get(this) : nullptr;
	const auto Soles = [this](TFunctionRef<void(const FVector&)> Visit)
	{
		Visit(ShinL->GetComponentTransform().TransformPosition(SoleL));
		Visit(ShinR->GetComponentTransform().TransformPosition(SoleR));
	};
	if (JumpEffects && bAirborne && !bWasAirborne)
	{
		Soles([&](const FVector& Sole) { JumpEffects->Footstep(FVector(Sole.X, Sole.Y, GetActorLocation().Z), 1.5f); });
	}
	if (!bAirborne && bWasAirborne)
	{
		LandJolt = 1.f;
		if (JumpEffects)
		{
			Soles([&](const FVector& Sole) { JumpEffects->Footstep(FVector(Sole.X, Sole.Y, GetActorLocation().Z), 2.f); });
			JumpEffects->GiantBurst(GetActorLocation(), JumpLandRing, StarGold);
		}
	}
	if (JumpEffects && bAirborne && Driver->GetVelocity().Z > 0.f)
	{
		Soles([&](const FVector& Sole) { JumpEffects->RocketExhaust(Sole, FVector::DownVector, 1.3f, DeltaSeconds); });
	}
	bWasAirborne = bAirborne;
	LandJolt = FMath::FInterpConstantTo(LandJolt, 0.f, DeltaSeconds, 1.f / JumpLandTime);
	const float Crouch = FMath::Max3(FMath::Sin(LandJolt * HALF_PI), Blow.Crouch * SwingWeight, 1.5f * DownShown);
	const float Tuck = AirAmount;

	WalkAmount = FMath::FInterpTo(WalkAmount, FMath::Clamp(Speed / 250.f, 0.f, 1.f) * (1.f - AirAmount), DeltaSeconds, 5.f);
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
	// The crouch of a landing (thighs forward, knees bent, the feet staying on the floor: the hips sink by what the bent
	// legs lose in height), the tuck of a jump (in the air: no floor to keep to)
	const float BendHip = JumpCrouchHip * Crouch + JumpTuckHip * Tuck;
	const float BendKnee = JumpCrouchKnee * Crouch + JumpTuckKnee * Tuck;
	const float CrouchHip = FMath::DegreesToRadians(JumpCrouchHip * Crouch);
	const float CrouchShin = FMath::DegreesToRadians(JumpCrouchHip * Crouch - JumpCrouchKnee * Crouch);
	const float CrouchSink = ThighLength * (1.f - FMath::Cos(CrouchHip)) + (LegLength - ThighLength) * (1.f - FMath::Cos(CrouchShin));
	const float Sink = (LegLength * (1.f - FMath::Cos(FMath::DegreesToRadians(HipAngleL))) + StepJoltSink * FMath::Sin(StepJolt * PI)) * (1.f - Tuck) + CrouchSink;
	const float HipYaw = WalkHipYaw * S * WalkAmount;
	Hips->SetRelativeLocationAndRotation(FVector(0.f, 0.f, Breath - Sink), FRotator(0.f, HipYaw, -WalkRoll * C * WalkAmount));
	Legs->SetRelativeScale3D(FVector::OneVector);
	if (bHasJoints)
	{
		HipL->SetRelativeRotation(FRotator(HipAngleL + KneeBendL * SwingHipLift + BendHip, 0.f, 0.f));
		HipR->SetRelativeRotation(FRotator(-HipAngleL + KneeBendR * SwingHipLift + BendHip, 0.f, 0.f));
		KneeL->SetRelativeRotation(FRotator(-KneeBendL - BendKnee, 0.f, 0.f));
		KneeR->SetRelativeRotation(FRotator(-KneeBendR - BendKnee, 0.f, 0.f));
	}

	// The upper body turns to the aim, twists back against the hips and leans into the walk (about the hips' height), and
	// over its knees in a landing
	ShownAimYaw = FMath::FixedTurn(ShownAimYaw, AimYaw, AimTurnRate * DeltaSeconds);
	const FRotator Chest(-WalkLean * WalkAmount - JumpCrouchLean * Crouch + Blow.Lean * SwingWeight + 12.f * ReelShown - 20.f * DownShown,
		ShownAimYaw - HipYaw * ChestCounterYaw + Blow.ChestYaw * SwingWeight, Wobble);
	const FVector Waist(0.f, 0.f, LegLength);
	Upper->SetRelativeLocationAndRotation(Waist - Chest.RotateVector(Waist), Chest);
	// The head nods a little with each step
	Neck->SetRelativeLocationAndRotation(NeckPivot, FRotator(2.5f * WalkAmount * FMath::Cos(2.f * WalkPhase) + 2.f * StepJolt - 22.f * DownShown, 0.f, Breath * 0.5f + Wobble));

	// Arms: swinging against the legs as it walks (the elbow bending as the arm comes forward), up for balance in a jump.
	// The right one holds the sword out ahead and swings it; the left one holds up the shield.
	const float SwingL = -WalkArmSwing * S * WalkAmount;
	const float SwingR = -SwingL;
	const float Up = JumpArmsUp * Tuck;
	const auto WalkElbowAt = [](float Arm) { return RestElbow + WalkElbow * FMath::Max(0.f, Arm / WalkArmSwing); };
	ShoulderL->SetRelativeLocationAndRotation(ShoulderPivotL, FMath::Lerp(FRotator(Up + SwingL, 0.f, 0.f), ShieldShoulder, ShieldEase));
	ShoulderR->SetRelativeLocationAndRotation(ShoulderPivotR, FMath::Lerp(FRotator(SwordRest.Shoulder.Pitch + Up + SwingR * 0.5f, 0.f, 0.f), Blow.Shoulder, SwingWeight));
	ElbowL->SetRelativeRotation(FRotator(FMath::Lerp(WalkElbowAt(SwingL), ShieldElbow, ShieldEase), 0.f, 0.f));
	ElbowR->SetRelativeRotation(FRotator(FMath::Lerp(SwordRest.Elbow, Blow.Elbow, SwingWeight), 0.f, 0.f));
	SwordGrip->SetRelativeRotation(FRotator(FMath::Lerp(SwordRest.Wrist, Blow.Wrist, SwingWeight), 0.f, 0.f));

	// The blade leaves a glowing sweep behind it through the blow; the slam's blow strikes the ground with a shock
	const FTransform Grip = SwordGrip->GetComponentTransform();
	const FVector Hilt = Grip.TransformPosition(FVector(BladeStart, 0.f, 0.f));
	const FVector Tip = Grip.TransformPosition(FVector(BladeStart + BladeLength, 0.f, 0.f));
	if (ARRRandomEffects* Effects = SwingWeight > 0.f ? ARRRandomEffects::Get(this) : nullptr)
	{
		if (SwingInto > SwingWindEnd * 0.8f && SwingInto < SwingRecoverFrom)
		{
			Effects->SaberSweep(Hilt, Tip, LastTip, Glow);
		}
		if (ShownStage == 3 && SwingInto >= SwingHitAt && SwingWasInto < SwingHitAt)
		{
			const FVector Ground(Tip.X, Tip.Y, GetActorLocation().Z);
			Effects->TigerStrike(Ground, 320.f, Glow);
			Effects->StarImpact(Ground, FRotator(0.f, GetActorRotation().Yaw + AimYaw, 0.f).Vector(), 3.f, Glow);
		}
	}
	LastTip = Tip;

	// Locked in a clash: where the two blades cross (the nearest points of the two, or between the two robots while they
	// are drawn back), a flash at each strike, a white-hot crackling point while they push; one of the pair plays it
	const bool bPlaysClash = Rival && ClashInto >= 0.f && Team < Rival->Team;
	bool bPressing = false;
	if (bPlaysClash)
	{
		const FTransform RivalGrip = Rival->SwordGrip->GetComponentTransform();
		FVector Mine;
		FVector Theirs;
		FMath::SegmentDistToSegmentSafe(Hilt, Tip, RivalGrip.TransformPosition(FVector(BladeStart, 0.f, 0.f)), RivalGrip.TransformPosition(FVector(BladeStart + BladeLength, 0.f, 0.f)), Mine, Theirs);
		const FVector Between = (GetActorLocation() + Rival->GetActorLocation()) * 0.5f + FVector(0.f, 0.f, RobotHeight * 0.55f);
		const FVector Where = FVector::Dist(Mine, Theirs) < ClashTouch ? (Mine + Theirs) * 0.5f : Between;
		bPressing = ClashInto >= ClashStrikeAt && ClashInto < ClashPushEnd;
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			if (ClashInto >= ClashStrikeAt && ClashWasInto < ClashStrikeAt)
			{
				Effects->SaberClashBurst(Where, Glow, Rival->GetTeamGlow());
			}
			if (bPressing)
			{
				Effects->SaberClash(Where, Glow, Rival->GetTeamGlow(), DeltaSeconds);
			}
		}
		ClashLight->SetWorldLocation(Where);
		ClashLight->SetLightColor(FMath::Lerp(FLinearColor::White, FMath::RandBool() ? Glow : Rival->GetTeamGlow(), 0.35f));
		ClashLight->SetIntensity(ClashLightIntensity * (bPressing ? FMath::FRandRange(0.6f, 1.f) : 0.f) + ClashLightIntensity * 2.f * FMath::Max(0.f, 1.f - (ClashInto - ClashStrikeAt) * 12.f) * (ClashInto >= ClashStrikeAt ? 1.f : 0.f));
	}
	ClashLight->SetVisibility(bPlaysClash);

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

	// The shield: a glowing disc at the left fist, facing where the upper body faces, popping up and fading; a hit
	// flashes it, breaking it bursts it
	ARRRandomEffects* ShieldEffects = ARRRandomEffects::Get(this);
	if (ShieldHealth < ShieldHealthSeen - 0.5f)
	{
		ShieldFlash = 1.f;
		if (ShieldEffects && ShieldShown > 0.1f)
		{
			ShieldEffects->HitBurst(Shield->GetComponentLocation(), -Shield->GetForwardVector(), ShieldHealth <= 0.f ? 6.f : 3.f, Glow);
			if (ShieldHealth <= 0.f)
			{
				ShieldEffects->DiceLandBurst(Shield->GetComponentLocation(), 120.f);
			}
		}
	}
	ShieldHealthSeen = ShieldHealth;
	ShieldFlash = FMath::FInterpConstantTo(ShieldFlash, 0.f, DeltaSeconds, 4.f);
	Shield->SetVisibility(ShieldShown > 0.02f);
	if (ShieldShown > 0.02f)
	{
		const FRotator Facing(0.f, Upper->GetComponentRotation().Yaw, 0.f);
		const FVector Fist = ElbowL->GetComponentTransform().TransformPosition(FistTipL - ElbowPivotL);
		const float Size = ShieldRadius / 50.f * FMath::Lerp(0.3f, 1.f, BackOut(ShieldShown, 2.f)) * (1.f + FMath::Sin(Now * 9.f) * 0.02f + ShieldFlash * 0.08f);
		Shield->SetWorldLocationAndRotation(Fist + Facing.Vector() * 60.f, Facing);
		Shield->SetWorldScale3D(FVector(Size * 0.12f, Size, Size));
		if (ShieldMaterial)
		{
			ShieldMaterial->SetScalarParameterValue(TEXT("Opacity"), ShieldShown);
			ShieldMaterial->SetScalarParameterValue(TEXT("Fill"), 0.25f * ShieldShown + 0.6f * ShieldFlash);
		}
	}
}
