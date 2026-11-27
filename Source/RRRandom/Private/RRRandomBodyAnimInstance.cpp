#include "RRRandomBodyAnimInstance.h"
#include "RRRandomCharacter.h"
#include "RRRandomSpecies.h"
#include "RRRandomMotionMatching.h"
#include "RRRandomMovementComponent.h"
#include "RRRandomVehicle.h"
#include "AnimationRuntime.h"
#include "TwoBoneIK.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PoseSearch/AnimNode_MotionMatching.h"

namespace
{
	// Round 99 (the user: "캐릭터 머리가 좌우 90도 초과해서 돌아가는건 어색하다. 90도를 넘어서 돌려야 할 때는 아예 몸통까지 실제
	// 포워드가 바뀌도록"): the head turns at most this far to either side of the chest; a look further round than
	// HeadTurnBodyAt is the body's to make, which then swings round at the faster rates instead of leaving the head to
	// crane back over the shoulder while the legs catch up (turning back the way it came, the head used to lead the body
	// by over a hundred degrees for a moment)
	constexpr float HeadTurnMax = 80.f;
	constexpr float HeadTurnBodyAt = 90.f;
	TAutoConsoleVariable<int32> CVarLogHeadTurn(TEXT("rr.Anim.LogHeadTurn"), 0,
		TEXT("1: log when a head was asked to turn over 90 degrees from its chest (it is held to 80 now)."));
	TAutoConsoleVariable<int32> CVarMotionUpper(TEXT("rr.MotionMatch.Upper"), 1,
		TEXT("Motion matching test: 1 the spine, neck and head come from the matched clips too; 0 only the hips and legs."));
	TAutoConsoleVariable<float> CVarMotionLean(TEXT("rr.MotionMatch.Lean"), -1.f,
		TEXT("Motion matching test: degrees the upper body leans forward at the walking speed (below 0: the built-in amount, URRRandomBodyAnimInstance::MotionRunLean)."));
	TAutoConsoleVariable<int32> CVarGunTwoHands(TEXT("rr.Gun.TwoHands"), 1,
		TEXT("1: the left hand holds the gun (its fist on each gun's support point, URRRandomGunComponent); 0: the animation's own left hand."));
	TAutoConsoleVariable<int32> CVarGunFingers(TEXT("rr.Gun.Fingers"), 1,
		TEXT("1: the left hand's fingers on the gun curl like the right hand's (mirrored); 0: as the animation has them."));
	// The most the left collarbone swings forward to bring a gun's support point within the short arm's reach (degrees)
	constexpr float GunShoulderReach = 25.f;
	TAutoConsoleVariable<int32> CVarGunHandsLog(TEXT("rr.Gun.HandsLog"), 0,
		TEXT("1: log, twice a second, how far my left hand is from its place on the gun and how far its arm can reach."));
	TAutoConsoleVariable<int32> CVarLogMotion(TEXT("rr.MotionMatch.Log"), 0,
		TEXT("1: log, twice a second, the motion matching driver's root and hips and what my animal friend's body takes from them (test)."));

	// The tail as a chain of springs (2026-11-24, the user asked for the recommended way on the tails): 1 on, 0 the four bones bent
	// together as before. The others, when 0 or more, stand in for the animation instance's own numbers (tuning from the
	// console, no rebuild).
	TAutoConsoleVariable<int32> CVarTailChain(TEXT("rr.Tail.Chain"), 1,
		TEXT("1: every tail bone is its own spring, softer toward the tip, and reacts to jumps, braking and shots. 0: as before (four bones bent together)."));
	TAutoConsoleVariable<float> CVarTailRootHz(TEXT("rr.Tail.RootHz"), -1.f, TEXT("Tail chain: oscillations a second at the root (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailTipHz(TEXT("rr.Tail.TipHz"), -1.f, TEXT("Tail chain: oscillations a second at the tip (-1: ChainTipFrequency)."));
	TAutoConsoleVariable<float> CVarTailDamping(TEXT("rr.Tail.Damping"), -1.f, TEXT("Tail chain: damping ratio, 1 = no overshoot (-1: ChainDamping)."));
	TAutoConsoleVariable<float> CVarTailInertia(TEXT("rr.Tail.Inertia"), -1.f, TEXT("Tail chain: 0 .. 1, how much a bone is left behind by the bones above it (-1: ChainInertia)."));
	TAutoConsoleVariable<float> CVarTailFallLift(TEXT("rr.Tail.FallLift"), -1.f, TEXT("Tail chain: degrees up per cm/s of falling (-1: FallLift)."));
	TAutoConsoleVariable<float> CVarTailBrakeLift(TEXT("rr.Tail.BrakeLift"), -1.f, TEXT("Tail chain: degrees up per cm/s2 of braking (-1: BrakeLift)."));
	TAutoConsoleVariable<float> CVarTailRootBias(TEXT("rr.Tail.RootBias"), -1.f, TEXT("Tail chain: 0 bends toward the tip .. 1 toward the root (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailIdleCurl(TEXT("rr.Tail.IdleCurl"), -999.f, TEXT("Tail chain: degrees the tail curls to the right standing still, negative to the left (-999: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailIdleLift(TEXT("rr.Tail.IdleLift"), -999.f, TEXT("Tail chain: degrees the tail lifts at its root standing still (-999: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailIdleTipLift(TEXT("rr.Tail.IdleTipLift"), -999.f, TEXT("Tail chain: degrees the tail's far end turns up standing still (-999: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailGain(TEXT("rr.Tail.Gain"), -1.f, TEXT("Tail chain: how many times over the body's motion moves the tail (-1: ChainGain)."));
	TAutoConsoleVariable<float> CVarTailShotKick(TEXT("rr.Tail.ShotKick"), -1.f, TEXT("Tail chain: degrees a second a shot flicks the tip up (-1: ShotTailKick)."));
	TAutoConsoleVariable<int32> CVarTailFloor(TEXT("rr.Tail.Floor"), 1, TEXT("1: no tail joint goes nearer the floor than rr.Tail.FloorMargin (the bones above lift it). 0: off."));
	TAutoConsoleVariable<float> CVarTailFloorMargin(TEXT("rr.Tail.FloorMargin"), 7.f, TEXT("Tail chain: how far above the floor every tail joint stays, mesh units at full tail size (its thickness and fur)."));
	TAutoConsoleVariable<float> CVarTailFollow(TEXT("rr.Tail.Follow"), -1.f, TEXT("Tail chain: 0 .. 1, how much of its way in the world the tail's end keeps as the hips turn, tip and bob (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailFollowCurve(TEXT("rr.Tail.FollowCurve"), -1.f, TEXT("Tail chain: how much more toward the tip that lag is kept, 1 = evenly (-1: 1.2)."));
	TAutoConsoleVariable<float> CVarTailMoveLift(TEXT("rr.Tail.MoveLift"), -999.f, TEXT("Tail chain: degrees the tail lifts at its root walking, negative hangs it down (-999: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailMoveTipLift(TEXT("rr.Tail.MoveTipLift"), -999.f, TEXT("Tail chain: degrees the tail's far end turns up walking (-999: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailSquiggle(TEXT("rr.Tail.Squiggle"), -1.f, TEXT("Tail chain: degrees each bone of an 8-bone tail bends up and down in the running S-curve (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailSquiggleSide(TEXT("rr.Tail.SquiggleSide"), -1.f, TEXT("Tail chain: the same to the sides (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailSquiggleWaves(TEXT("rr.Tail.SquiggleWaves"), -1.f, TEXT("Tail chain: how many waves of the S-curve fit along the tail (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarTailSquiggleHz(TEXT("rr.Tail.SquiggleHz"), -1.f, TEXT("Tail chain: S-curve waves a second running down the tail (-1: the animal friend's own)."));
	TAutoConsoleVariable<int32> CVarTailLog(TEXT("rr.Tail.Log"), 0, TEXT("1: log my animal friend's tail chain ten times a second (test)."));
	// The ears (2026-11-25): rr.Ear 0 leaves them as modelled; the others, when 0 or more, stand in for the numbers
	// (rr.Ear.Back / rr.Ear.Out: a pose held for looking at, degrees for the whole ear, -999 = the game's own)
	TAutoConsoleVariable<int32> CVarEar(TEXT("rr.Ear"), 1, TEXT("1: the ears' bones spring and take poses (running, aiming, hit, knocked out, twitching). 0: as modelled."));
	TAutoConsoleVariable<float> CVarEarThrow(TEXT("rr.Ear.Throw"), -1.f, TEXT("Ears: how many times over the head's motion throws them (-1: EarThrow x the animal friend's EarGain)."));
	TAutoConsoleVariable<float> CVarEarPose(TEXT("rr.Ear.Pose"), -1.f, TEXT("Ears: how far into the poses, 1 = the fox's (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarEarRush(TEXT("rr.Ear.Rush"), -1.f, TEXT("Ears: how many times the sweep back from speed past a walk (-1: 1, the animal friend's own EarRush)."));
	TAutoConsoleVariable<float> CVarEarRigid(TEXT("rr.Ear.Rigid"), -1.f, TEXT("Ears: 0 .. 1, how much the root bone bends alone - 1 the ear stays straight (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarEarDamping(TEXT("rr.Ear.Damping"), -1.f, TEXT("Ears: the springs' damping ratio, 1 = no overshoot (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarEarHz(TEXT("rr.Ear.Hz"), -1.f, TEXT("Ears: oscillations a second at the root (-1: the animal friend's own)."));
	TAutoConsoleVariable<float> CVarEarBack(TEXT("rr.Ear.Back"), -999.f, TEXT("Ears: hold both ears this many degrees back (negative: forward), for looking (-999: off)."));
	TAutoConsoleVariable<float> CVarEarOut(TEXT("rr.Ear.Out"), -999.f, TEXT("Ears: hold both ears this many degrees out and down, for looking (-999: off)."));
	TAutoConsoleVariable<int32> CVarEarLog(TEXT("rr.Ear.Log"), 0, TEXT("1: log my animal friend's ears ten times a second (test)."));
	// The most tail bones looked for on a mesh (Tail1 .. Tail16)
	constexpr int32 MaxTailBones = 16;
	// A bone may bend this many times its even share of the whole tail's most (MaxChainSwing, MaxChainLift)
	constexpr float BoneRoom = 2.f;
	// ...and the most the whole tail goes below where the wag and the lift hold it, all bones together
	constexpr float MaxChainDown = 14.f;

	// Long frames are simulated in steps this short, so the spring stays steady
	constexpr float MaxStep = 1.f / 60.f;
	// A turn or a speed change bigger than this in one frame is a teleport (respawn), not something to swing from
	constexpr float MaxTurnPerFrame = 90.f;
	constexpr float MaxAcceleration = 8000.f;
	// How fast the tail lifts and settles, and how fast the wag fades in and out
	constexpr float LiftSpeed = 4.f;
	constexpr float LifeFadeSpeed = 1.5f;
	// Share of the lift taken by the root bone; the rest is spread over the others
	constexpr float RootLiftShare = 0.7f;

	// Moving slower than this, the run directions stay as they were (no direction to go by)
	constexpr float MinStrafeSpeed = 10.f;
	// How fast the standing/running mix and the fire/aim mix follow (exponential, per second)
	constexpr float MoveBlendSpeed = 10.f;
	constexpr float FireBlendSpeed = 12.f;
	// Third person: the most the spine tips the gun up or down after the aim (degrees)
	constexpr float MaxAimPitch = 55.f;
	// The spine bones that turn the upper body toward the aim, hips up, and each one's share of the turn;
	// the chest (last) also takes the tilt and the kick
	const FName SpineBones[] = { TEXT("Spine"), TEXT("Spine1"), TEXT("Spine2") };
	constexpr float SpineShares[] = { 0.3f, 0.3f, 0.4f };
	// The gun's ends in the hands, as URRRandomGunComponent takes them: the right palm (halfway from the hand to the
	// index finger's base) and the left hand
	const FName RightHandBone(TEXT("RightHand"));
	const FName RightPalmBone(TEXT("RightHandIndex1"));
	const FName LeftHandBone(TEXT("LeftHand"));

	/** A bone's transform in the mesh's space, from the local pose. */
	FTransform GetMeshSpaceTransform(const FCompactPose& Pose, FCompactPoseBoneIndex Index)
	{
		FTransform Result = Pose[Index];
		for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Index); Parent.IsValid(); Parent = Pose.GetParentBoneIndex(Parent))
		{
			Result = Result * Pose[Parent];
		}
		return Result;
	}

	/** Turns a bone (and so everything below it) by Turn, given in the mesh's space. */
	void TurnBone(FCompactPose& Pose, FCompactPoseBoneIndex Index, const FQuat& Turn)
	{
		FQuat ParentRotation = FQuat::Identity;
		for (FCompactPoseBoneIndex Parent = Pose.GetParentBoneIndex(Index); Parent.IsValid(); Parent = Pose.GetParentBoneIndex(Parent))
		{
			ParentRotation = Pose[Parent].GetRotation() * ParentRotation;
		}
		FTransform& Local = Pose[Index];
		Local.SetRotation((ParentRotation.Inverse() * Turn * ParentRotation * Local.GetRotation()).GetNormalized());
	}

	FCompactPoseBoneIndex FindBone(const FBoneContainer& Bones, FName Name)
	{
		const int32 MeshIndex = Bones.GetReferenceSkeleton().FindBoneIndex(Name);
		return MeshIndex == INDEX_NONE ? FCompactPoseBoneIndex(INDEX_NONE) : Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	}
}

URRRandomBodyAnimInstance::URRRandomBodyAnimInstance()
{
	TailBoneNames = { TEXT("Tail1"), TEXT("Tail2"), TEXT("Tail3"), TEXT("Tail4") };
	WingBoneNames = { TEXT("WingL1"), TEXT("WingL2"), TEXT("WingR1"), TEXT("WingR2") };
	FlightBoneNames = { TEXT("LeftUpLeg"), TEXT("LeftLeg"), TEXT("LeftFoot"), TEXT("RightUpLeg"), TEXT("RightLeg"), TEXT("RightFoot"), TEXT("Neck"), TEXT("Head") };
	JumpBoneNames = { TEXT("LeftUpLeg"), TEXT("LeftLeg"), TEXT("LeftFoot"), TEXT("RightUpLeg"), TEXT("RightLeg"), TEXT("RightFoot"), TEXT("Spine"),
		TEXT("LeftArm"), TEXT("LeftForeArm"), TEXT("RightArm"), TEXT("RightForeArm") };
	SprintBoneNames = { TEXT("LeftArm"), TEXT("RightArm") };
	RideBoneNames = JumpBoneNames;
}

void URRRandomBodyAnimInstance::UpdateFlightPose(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	if (!Brawler)
	{
		FlightBends.Reset();
		return;
	}
	const bool bInFlight = Brawler->IsInFlight();
	const float Climb = Brawler->GetVelocity().Z;
	FlightWeight = FMath::FInterpConstantTo(FlightWeight, bInFlight ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(FlightBlendTime, 0.01f));
	// Gliding down lays the legs out behind; hovering or rising they hang and tread
	FlightGlide = FMath::FInterpTo(FlightGlide, bInFlight && Climb < -20.f ? 1.f : 0.f, DeltaSeconds, 4.f);
	KickPhase = FMath::Fmod(KickPhase + 2.f * PI * FlightKick.X * DeltaSeconds, 2.f * PI);
	if (FlightWeight <= 0.f)
	{
		FlightBends.Reset();
		return;
	}

	const float W = FlightWeight;
	const float G = FlightGlide;
	const float Kick = FlightKick.Y * FMath::Sin(KickPhase) * (1.f - 0.6f * G);
	const float Thigh = FMath::Lerp(FlightThigh.X, FlightThigh.Y, G);
	const float Knee = FMath::Lerp(FlightKnee.X, FlightKnee.Y, G);
	const float Toes = FMath::Lerp(FlightToes.X, FlightToes.Y, G);
	// Against the body's lean, split between neck and head
	const float HeadLift = Brawler->GetFlightLean() * FlightHeadLift * 0.5f;
	// About the mesh's side axis (+X, the brawler's left): positive tips the front (+Y) up, negative swings a leg back
	const float Angles[] = { Thigh + Kick, Knee - 0.5f * Kick, Toes, Thigh - Kick, Knee + 0.5f * Kick, Toes, HeadLift, HeadLift };
	FlightBends.SetNum(UE_ARRAY_COUNT(Angles));
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Angles); ++Index)
	{
		FlightBends[Index] = FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Angles[Index] * W));
	}
}

namespace
{
	/**
	 * The jumping pose (round 64), over the idle while ARRRandomCharacter::IsJumping, in degrees in mesh space. Legs and spine
	 * about the side axis (positive swings a leg forward, tips the chest back); arms raised forward (Fwd), out to the side
	 * (Out) and the forearm bent up (Bend).
	 * - Going up: the front leg's knee pulled high, the back leg tucked, toes pointed, leaning in, arms up in front.
	 * - Coming down: the legs reaching for the floor (one ahead, one a little behind), arms spread for balance, leaning back.
	 * - Landing (ARRRandomCharacter::GetLandSquash): both knees bent deep with the feet kept flat, leaning in, arms forward.
	 */
	struct FJumpPose
	{
		float ThighL, KneeL, FootL, ThighR, KneeR, FootR, Spine, Fwd, Out, Bend;
	};
	constexpr FJumpPose JumpRisePose = { 48.f, -82.f, -20.f, 14.f, -48.f, -25.f, -6.f, 55.f, 20.f, 35.f };
	constexpr FJumpPose JumpFallPose = { 18.f, -22.f, -10.f, -8.f, -30.f, -15.f, 4.f, 15.f, 65.f, 15.f };
	constexpr FJumpPose JumpLandPose = { 34.f, -64.f, 28.f, 34.f, -64.f, 28.f, -12.f, 25.f, 15.f, 20.f };
	// Into the pose and out of it this fast (seconds), and the rise / fall mix by the speed up (from all fall to all rise)
	constexpr float JumpInTime = 0.08f;
	constexpr float JumpOutTime = 0.12f;
	const FVector2D JumpRiseSpeeds(-150.f, 250.f);
}

void URRRandomBodyAnimInstance::UpdateJumpPose(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	if (!Brawler)
	{
		JumpBends.Reset();
		return;
	}
	const bool bJumping = Brawler->IsJumping();
	JumpWeight = FMath::FInterpConstantTo(JumpWeight, bJumping ? 1.f : 0.f, DeltaSeconds, 1.f / (bJumping ? JumpInTime : JumpOutTime));
	const float Rise = FMath::GetMappedRangeValueClamped(JumpRiseSpeeds, FVector2D(0.f, 1.f), Brawler->GetVelocity().Z);
	JumpRise = bJumping ? FMath::FInterpTo(JumpRise, Rise, DeltaSeconds, 8.f) : Rise;
	const float Land = FMath::Sin(Brawler->GetLandSquash() * HALF_PI);
	if (JumpWeight <= 0.f && Land <= 0.f)
	{
		JumpBends.Reset();
		return;
	}
	// The air's pose (rise and fall mixed by the speed up) faded by its weight, the landing's added on
	const auto Mix = [&](float FJumpPose::* Field)
	{
		return FMath::Lerp(JumpFallPose.*Field, JumpRisePose.*Field, JumpRise) * JumpWeight + JumpLandPose.*Field * Land;
	};
	// The arms only where they aren't holding the gun up to aim
	const float Arms = 1.f - GetAimWeight();
	const float Fwd = Mix(&FJumpPose::Fwd) * Arms;
	const float Out = Mix(&FJumpPose::Out) * Arms;
	const float Bend = Mix(&FJumpPose::Bend) * Arms;
	const auto Side = [](float Degrees) { return FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Degrees)); };
	// Out to the side: the left arm (on +X) about -Y, the right about +Y
	const auto ArmTurn = [&](float OutSign) { return Side(Fwd) * FQuat(FVector::RightVector, FMath::DegreesToRadians(Out * OutSign)); };
	JumpBends = { Side(Mix(&FJumpPose::ThighL)), Side(Mix(&FJumpPose::KneeL)), Side(Mix(&FJumpPose::FootL)),
		Side(Mix(&FJumpPose::ThighR)), Side(Mix(&FJumpPose::KneeR)), Side(Mix(&FJumpPose::FootR)), Side(Mix(&FJumpPose::Spine)),
		ArmTurn(-1.f), Side(Bend), ArmTurn(1.f), Side(Bend) };
}

namespace
{
	// Running (Shift, round 67): both upper arms swing down this far about the mesh's side axis (the same turn for both
	// keeps the hands, and the gun between them, together; its muzzle then points down and ahead), easing in and out
	constexpr float SprintArmDrop = 42.f;
	constexpr float SprintPoseInTime = 0.15f;
	constexpr float SprintPoseOutTime = 0.12f;
	// Walking (round 90): Mixamo's rifle walk holds the hands at the chest, which on the animal friends' big heads is right
	// under the chin, so the small gun in the right fist sank into the head; the arms come down this far while walking
	constexpr float WalkArmDrop = 30.f;
}

void URRRandomBodyAnimInstance::UpdateSprintPose(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	// (the weight is a share of SprintArmDrop: the walk's smaller drop is a smaller share)
	const float Wanted = Brawler ? FMath::Max(Brawler->GetSprintPoseShare(), Brawler->GetWalkPoseShare() * WalkArmDrop / SprintArmDrop)
		* (1.f - GetAimWeight()) : 0.f;
	SprintWeight = FMath::FInterpConstantTo(SprintWeight, Wanted, DeltaSeconds, 1.f / (Wanted > SprintWeight ? SprintPoseInTime : SprintPoseOutTime));
	if (SprintWeight <= 0.f)
	{
		SprintBends.Reset();
		return;
	}
	// About the side axis (+X): negative tips the front (+Y) down
	const FQuat Down(FVector::ForwardVector, FMath::DegreesToRadians(-SprintArmDrop * SprintWeight));
	SprintBends = { Down, Down };
}

namespace
{
	/** A riding pose (UpdateRidePose), in degrees in mesh space like the jump's (FJumpPose). */
	struct FRidePose
	{
		float ThighL, KneeL, FootL, ThighR, KneeR, FootR, Spine, Fwd, Out, Bend;
	};
	const FRidePose RidePoses[] =
	{
		// Kickboard: standing on the deck, the left foot ahead, knees soft, leaning on the bar with both hands
		{ 12.f, -22.f, 10.f, -8.f, -12.f, 20.f, -8.f, 50.f, 8.f, 15.f },
		// Motorbike: astride, knees up by the tank, leaning on the bars held wide
		{ 65.f, -95.f, 25.f, 65.f, -95.f, 25.f, -10.f, 70.f, 18.f, 10.f },
		// Car (the star team's too): sitting, the hands on the wheel in front
		{ 85.f, -85.f, 0.f, 85.f, -85.f, 0.f, 4.f, 50.f, 12.f, 40.f },
		// A car's rear seat: sitting, the arms left free to shoot
		{ 85.f, -85.f, 0.f, 85.f, -85.f, 0.f, 0.f, 0.f, 0.f, 0.f },
	};
	constexpr float RidePoseInTime = 0.2f;
	constexpr float RidePoseOutTime = 0.15f;
}

void URRRandomBodyAnimInstance::UpdateRidePose(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	const ARRRandomVehicle* Vehicle = Brawler && Brawler->IsAlive() ? Brawler->GetVehicle() : nullptr;
	if (Vehicle)
	{
		const ERRVehicleKind Kind = Vehicle->GetKind();
		RidePose = !Brawler->IsDriving() ? 3 : (Kind == ERRVehicleKind::Kickboard ? 0 : (Kind == ERRVehicleKind::Motorbike ? 1 : 2));
	}
	RideWeight = FMath::FInterpConstantTo(RideWeight, Vehicle ? 1.f : 0.f, DeltaSeconds, 1.f / (Vehicle ? RidePoseInTime : RidePoseOutTime));
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (RideWeight <= 0.f || !Mesh || !Mesh->GetSkinnedAsset())
	{
		RideBends.Reset();
		RideDrop = 0.f;
		return;
	}
	const FRidePose& Pose = RidePoses[RidePose];
	const float W = RideWeight;
	const auto Side = [W](float Degrees) { return FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Degrees * W)); };
	// Out to the side: the left arm (on +X) about -Y, the right about +Y
	const auto ArmTurn = [&](float OutSign) { return Side(Pose.Fwd) * FQuat(FVector::RightVector, FMath::DegreesToRadians(Pose.Out * W * OutSign)); };
	RideBends = { Side(Pose.ThighL), Side(Pose.KneeL), Side(Pose.FootL), Side(Pose.ThighR), Side(Pose.KneeR), Side(Pose.FootR), Side(Pose.Spine),
		ArmTurn(-1.f), Side(Pose.Bend), ArmTurn(1.f), Side(Pose.Bend) };

	// Bending the legs lifts the feet: the body sinks by as much (the lower foot's), so they stay down and it sits.
	// The thigh and shin lengths are the mesh's own, from its reference pose.
	const FReferenceSkeleton& Skeleton = Mesh->GetSkinnedAsset()->GetRefSkeleton();
	const int32 Hip = Skeleton.FindBoneIndex(TEXT("LeftUpLeg"));
	const int32 Knee = Skeleton.FindBoneIndex(TEXT("LeftLeg"));
	const int32 Ankle = Skeleton.FindBoneIndex(TEXT("LeftFoot"));
	if (Hip == INDEX_NONE || Knee == INDEX_NONE || Ankle == INDEX_NONE)
	{
		RideDrop = 0.f;
		return;
	}
	const FVector HipAt = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Hip).GetLocation();
	const FVector KneeAt = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Knee).GetLocation();
	const FVector AnkleAt = FAnimationRuntime::GetComponentSpaceTransformRefPose(Skeleton, Ankle).GetLocation();
	const float Thigh = FVector::Dist(HipAt, KneeAt);
	const float Shin = FVector::Dist(KneeAt, AnkleAt);
	const auto Rise = [&](float ThighAngle, float KneeAngle)
	{
		return Thigh * (1.f - FMath::Cos(FMath::DegreesToRadians(ThighAngle * W))) + Shin * (1.f - FMath::Cos(FMath::DegreesToRadians((ThighAngle + KneeAngle) * W)));
	};
	RideDrop = FMath::Min(Rise(Pose.ThighL, Pose.KneeL), Rise(Pose.ThighR, Pose.KneeR)) * Mesh->GetRelativeScale3D().Z;
}

void FRRRandomBodyAnimInstanceProxy::TurnBones(FPoseContext& Output, const TArray<FName>& Names, const TArray<FQuat>& Bends)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	for (int32 Bone = 0; Bone < Names.Num() && Bone < Bends.Num(); ++Bone)
	{
		const FCompactPoseBoneIndex Index = FindBone(Bones, Names[Bone]);
		if (!Index.IsValid())
		{
			continue;
		}
		FQuat ParentRotation = FQuat::Identity;
		for (FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Index); Parent.IsValid(); Parent = Output.Pose.GetParentBoneIndex(Parent))
		{
			ParentRotation = Output.Pose[Parent].GetRotation() * ParentRotation;
		}
		FTransform& Local = Output.Pose[Index];
		Local.SetRotation((ParentRotation.Inverse() * Bends[Bone] * ParentRotation * Local.GetRotation()).GetNormalized());
	}
}

void URRRandomBodyAnimInstance::UpdateWings(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Brawler || !Mesh || WingBoneNames.Num() < 4 || Mesh->GetBoneIndex(WingBoneNames[0]) == INDEX_NONE)
	{
		WingBends.Reset();
		return;
	}

	// What the wings are doing: gliding (spread and still), flapping in the air (harder rising on the flight power), or
	// stirring on the ground. Other machines don't know the glide flag, so a slow fall after a while in the air counts.
	const UCharacterMovementComponent* Movement = Brawler->GetCharacterMovement();
	const URRRandomMovementComponent* RandomMovement = Cast<URRRandomMovementComponent>(Movement);
	const FVector Velocity = Brawler->GetVelocity();
	const bool bInAir = Movement && Movement->IsFalling() && Brawler->IsAlive();
	AirTime = bInAir ? AirTime + DeltaSeconds : 0.f;
	const bool bSlowFall = Velocity.Z <= 0.f && Velocity.Z > -280.f && AirTime > 0.35f;
	const bool bGlide = bInAir && ((RandomMovement && RandomMovement->bGliding && Velocity.Z <= 0.f) || Brawler->IsDashing()
		|| (!Brawler->IsLocallyControlled() && !Brawler->HasAuthority() && Brawler->CanFly() && bSlowFall));
	const float Run = FMath::Clamp(Velocity.Size2D() / FMath::Max(WagRunSpeed, 1.f), 0.f, 1.f);
	const FVector2D Flap = !bInAir ? FMath::Lerp(GroundFlap, WalkFlap, Run) : Brawler->CanFly() && Velocity.Z > 0.f ? RisingFlap : AirFlap;

	GlideWeight = FMath::FInterpConstantTo(GlideWeight, bGlide ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(GlideBlendTime, 0.01f));
	FlapRate = FMath::FInterpTo(FlapRate, Flap.X, DeltaSeconds, 6.f);
	FlapSize = FMath::FInterpTo(FlapSize, Flap.Y, DeltaSeconds, 6.f);
	FlapPhase = FMath::Fmod(FlapPhase + 2.f * PI * FlapRate * DeltaSeconds, 2.f * PI);

	// The left wing's turns (degrees down, about the mesh's front axis, and forward, about its up axis); a knocked-out
	// brawler's wings settle back to rest. Gliding, the roots hold the spread and only the tips flutter, harder the faster
	// it goes: two shivers out of step, and the right tip out of step with the left, so it reads as wind, not a beat
	const float Glide = GlideWeight;
	const float Still = 1.f - Glide;
	const float Time = GetWorld()->GetTimeSeconds();
	const float Wind = 0.5f + 0.5f * FMath::Clamp(Velocity.Size() / FMath::Max(GlideFlutterSpeed, 1.f), 0.f, 1.f);
	const float RootDown = GlideRootDownForward.X * Glide + FlapSize * FMath::Sin(FlapPhase) * Still;
	const float RootForward = GlideRootDownForward.Y * Glide;
	const float TipDown = GlideTipDownForward.X * Glide + 0.5f * FlapSize * FMath::Sin(FlapPhase - 0.9f) * Still;
	const float TipForward = GlideTipDownForward.Y * Glide;
	const float Alive = Brawler->IsAlive() ? 1.f : 0.f;

	// Down turns the left wing (+X) toward -Z: about +Y. Forward turns it toward +Y: about +Z. The right wing is the mirror
	// image: the same angles about -Y and -Z. The tip's turn is taken in the root's turned frame, so it rides on it.
	WingBends.SetNum(4);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Mirror = Side == 0 ? 1.f : -1.f;
		const FVector Front(0.f, Mirror, 0.f);
		const FVector Up(0.f, 0.f, Mirror);
		const float Beat = 2.f * PI * GlideTipFlutter.X * Time + Side * 1.9f;
		const float Flutter = GlideTipFlutter.Y * Wind * Glide * (0.7f * FMath::Sin(Beat) + 0.3f * FMath::Sin(1.73f * Beat + 1.1f));
		// The pitch is about the side axis for both wings alike (a mirror image of that turn is the same turn)
		const FQuat Pitch(FVector::ForwardVector, FMath::DegreesToRadians(GlideWingPitch * Glide * Alive));
		const FQuat Root = Pitch * FQuat(Up, FMath::DegreesToRadians(RootForward * Alive)) * FQuat(Front, FMath::DegreesToRadians(RootDown * Alive));
		const FQuat Tip = FQuat(Up, FMath::DegreesToRadians(TipForward * Alive))
			* FQuat(Front, FMath::DegreesToRadians((TipDown + Flutter) * Alive));
		WingBends[Side * 2] = Root;
		WingBends[Side * 2 + 1] = Root * Tip * Root.Inverse();
	}
}

void URRRandomBodyAnimInstance::UpdateEars(float DeltaSeconds)
{
	Aim.ListeningWeight = 0.f;
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const USkeletalMesh* Asset = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	// The ears this mesh has (looked for again when the mesh changes: another animal friend's body)
	if (EarMesh.Get() != Asset)
	{
		EarMesh = Asset;
		Ears.Reset();
		EarHead = NAME_None;
		EarFrequency = 3.f;
		EarGain = EarPose = 1.f;
		EarRushDegrees = 15.f;
		EarRigidity = 0.f;
		EarDampingRatio = EarDamping;
		const FReferenceSkeleton* Ref = Asset ? &Asset->GetRefSkeleton() : nullptr;
		const int32 Head = Ref ? Ref->FindBoneIndex(TEXT("Head")) : INDEX_NONE;
		if (Head != INDEX_NONE)
		{
			for (const FRRSpeciesLook& Look : FRRSpeciesLook::GetAll())
			{
				if (Look.Mesh == Asset)
				{
					EarFrequency = Look.EarFrequency;
					EarGain = Look.EarGain;
					EarPose = Look.EarPose;
					EarRushDegrees = Look.EarRush;
					EarRigidity = Look.EarRigid;
					EarDampingRatio = Look.EarDamping;
					break;
				}
			}
			TArray<FTransform> Space;
			FAnimationRuntime::FillUpComponentSpaceTransforms(*Ref, Ref->GetRefBonePose(), Space);
			const FVector HeadAt = Space[Head].GetLocation();
			for (const TCHAR* Side : { TEXT("L"), TEXT("R") })
			{
				FEar Ear;
				for (int32 Number = 1; Number <= 8; ++Number)
				{
					const FName Name(*FString::Printf(TEXT("Ear%s%d"), Side, Number));
					if (Ref->FindBoneIndex(Name) == INDEX_NONE)
					{
						break;
					}
					Ear.Bones.Add(Name);
				}
				const int32 End = Ref->FindBoneIndex(FName(*FString::Printf(TEXT("Ear%sEnd"), Side)));
				if (Ear.Bones.Num() == 0 || End == INDEX_NONE)
				{
					continue;
				}
				const FVector Root = Space[Ref->FindBoneIndex(Ear.Bones[0])].GetLocation();
				const FVector Tip = Space[End].GetLocation();
				const FVector Along = (Tip - Root).GetSafeNormal();
				Ear.Length = FMath::Max(FVector::Dist(Root, Tip), 1.f);
				// Back: toward the back of the head (the mesh faces +Y); an ear already pointing back folds down instead
				FVector BackWay = -FVector::YAxisVector;
				if (FVector::CrossProduct(Along, BackWay).Size() < 0.35f)
				{
					BackWay = -FVector::ZAxisVector;
				}
				Ear.BackAxis = FVector::CrossProduct(Along, BackWay).GetSafeNormal();
				// Out: away from the head's middle and down, square to the ear
				FVector OutWay = FVector(Tip.X >= HeadAt.X ? 1.f : -1.f, 0.f, -1.f).GetSafeNormal();
				OutWay = (OutWay - Along * FVector::DotProduct(OutWay, Along)).GetSafeNormal();
				Ear.OutAxis = FVector::CrossProduct(Along, OutWay).GetSafeNormal();
				// (a small turn about an axis moves the tip along axis x ear)
				Ear.BackMove = FVector::CrossProduct(Ear.BackAxis, Along);
				Ear.OutMove = FVector::CrossProduct(Ear.OutAxis, Along);
				Ear.TipOnHead = Space[Head].InverseTransformPosition(Tip);
				const int32 Count = Ear.Bones.Num();
				Ear.Back.SetNumZeroed(Count);
				Ear.Out.SetNumZeroed(Count);
				Ear.BackSpeed.SetNumZeroed(Count);
				Ear.OutSpeed.SetNumZeroed(Count);
				Ears.Add(MoveTemp(Ear));
			}
			if (Ears.Num() > 0)
			{
				EarHead = Ref->GetBoneName(Head);
				EarHeadRest = Space[Head].GetRotation();
				UE_LOG(LogTemp, Log, TEXT("RRRandom: %s has %d ear(s) of %d bone(s)."), *GetNameSafe(GetOwningActor()), Ears.Num(), Ears[0].Bones.Num());
			}
		}
	}
	EarBoneNames.Reset();
	EarBends.Reset();
	const AActor* Owner = GetOwningActor();
	if (!Owner || CVarEar.GetValueOnGameThread() == 0)
	{
		return;
	}

	const auto Tuned = [](const TAutoConsoleVariable<float>& Var, float Default)
	{
		const float Value = Var.GetValueOnGameThread();
		return Value >= 0.f ? Value : Default;
	};
	const float Pose = Tuned(CVarEarPose, EarPose);
	const float Throw = Tuned(CVarEarThrow, EarThrow * EarGain);
	const float RootHz = Tuned(CVarEarHz, EarFrequency);
	const float Rigidity = FMath::Clamp(Tuned(CVarEarRigid, EarRigidity), 0.f, 1.f);
	const float DampingRatio = Tuned(CVarEarDamping, EarDampingRatio);
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(Owner);
	const bool bAlive = !Brawler || Brawler->IsAlive();
	const FVector Velocity = Owner->GetVelocity();
	const bool bListening = Brawler && bAlive
		&& Brawler->WantsMotionMatch() && Velocity.SizeSquared2D() < FMath::Square(30.f)
		&& !Brawler->WantsAimFacing() && !Brawler->IsEmoting();
	const float Age = bListening ? Brawler->GetHeardStepAge() : 10.f;
	const float Attention = FMath::SmoothStep(0.f, 0.08f, Age) * (1.f - FMath::SmoothStep(0.48f, 0.85f, Age));
	const FVector Heard = Attention > 0.f ? (Brawler->GetHeardStepLocation() - Owner->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
	const float SoundSide = FVector::DotProduct(Heard, Owner->GetActorRightVector());
	Aim.ListeningDirection = Heard;
	Aim.ListeningWeight = FMath::SmoothStep(0.10f, 0.30f, Age) * Attention * 0.78f;
	if (Attention > 0.f && CVarLogMotion.GetValueOnGameThread() && Brawler->IsLocallyControlled() && Brawler->IsPlayerControlled() && GFrameCounter % 6 == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("ANIM LISTEN age %.2f side %.2f ears %d head %.2f"), Age, SoundSide, Ears.Num(), Aim.ListeningWeight);
	}
	if (Ears.IsEmpty()) { return; }

	// The pose of the moment, degrees for the whole ear: swept back with speed, forward and alert while aiming (less on
	// the run), flat back and a little out when hit, out and down when knocked out
	// (riding, the driver's speed - a vehicle is carried along by its driver, and a passenger sits still on it: the wind
	// the ears feel; all ways, a plane climbs and dives)
	const ARRRandomVehicle* Ridden = Brawler ? Brawler->GetVehicle() : nullptr;
	const ARRRandomCharacter* Driver = Ridden ? Ridden->GetRider() : nullptr;
	const float Speed = Driver ? Driver->GetVelocity().Size() : Velocity.Size2D();
	const float Pace = FMath::Clamp(Speed / FMath::Max(WagRunSpeed, 1.f), 0.f, 1.f);
	const float Rush = (1.f - FMath::Exp(-FMath::Max(0.f, Speed - EarRushFrom) / FMath::Max(EarRushSpeed, 1.f))) * Life;
	EarTime += DeltaSeconds;
	const float Flutter = 0.6f * FMath::Sin(EarTime * 2.f * PI * 7.f) + 0.4f * FMath::Sin(EarTime * 2.f * PI * 11.3f + 1.f);
	const float HitFlash = Brawler ? Brawler->GetHitFlash() : 0.f;
	if (HitFlash > LastHitFlash + 0.3f)
	{
		EarHit = 1.f;
	}
	LastHitFlash = HitFlash;
	EarHit = FMath::Max(0.f, EarHit - DeltaSeconds / FMath::Max(EarHitTime, 0.05f));
	const float Hit = FMath::Sin(EarHit * HALF_PI);
	const float Down = 1.f - Life;
	float BackPose = Pose * (EarRunBack * Pace - EarAimForward * Aim.Weight * (1.f - 0.6f * Pace) + EarHitBack * Hit + EarDownBack * Down)
		+ (EarRushDegrees * Tuned(CVarEarRush, 1.f) + EarFlutter * Flutter) * Rush;
	float OutPose = Pose * (EarHitOut * Hit + EarDownOut * Down);
	if (CVarEarBack.GetValueOnGameThread() > -900.f) { BackPose = CVarEarBack.GetValueOnGameThread(); }
	if (CVarEarOut.GetValueOnGameThread() > -900.f) { OutPose = CVarEarOut.GetValueOnGameThread(); }

	// A shot: both ears flinch back. Standing about: now and then one ear twitches.
	const float SinceShot = Brawler ? Brawler->GetTimeSinceShot() : EarSinceShot;
	const bool bShot = SinceShot < EarSinceShot - KINDA_SMALL_NUMBER;
	EarSinceShot = SinceShot;
	int32 Twitching = INDEX_NONE;
	if (bAlive && Attention == 0.f && Velocity.SizeSquared2D() < FMath::Square(30.f) && Aim.Weight < 0.1f)
	{
		EarTwitchIn -= DeltaSeconds;
		if (EarTwitchIn <= 0.f)
		{
			Twitching = FMath::RandRange(0, Ears.Num() - 1);
			EarTwitchIn = FMath::FRandRange(EarTwitchEvery.X, EarTwitchEvery.Y);
		}
	}

	const FTransform HeadWorld = Mesh->GetSocketTransform(EarHead, RTS_World);
	const float Scale = FMath::Max(HeadWorld.GetScale3D().X, 0.01f);
	for (int32 EarIndex = 0; EarIndex < Ears.Num(); ++EarIndex)
	{
		FEar& Ear = Ears[EarIndex];
		const bool bSoundEar = (SoundSide >= 0.f) == (Ear.Bones[0] == TEXT("EarR1"));
		const float ListenBack = Attention * Pose * (bSoundEar ? -23.f : 9.f);
		const float ListenOut = Attention * Pose * (bSoundEar ? 16.f * FMath::Abs(SoundSide) : 0.f);
		const int32 Count = Ear.Bones.Num();
		// How the tip would move if the ear were rigid on the head: its acceleration (eased: a frame's jitter is not a
		// throw), turned into the head's reference frame, where the ear's axes are
		const FVector Tip = HeadWorld.TransformPosition(Ear.TipOnHead);
		if (!Ear.bHasLast || DeltaSeconds < 1e-3f)
		{
			Ear.bHasLast = true;
			Ear.LastTip = Tip;
			Ear.LastTipVelocity = FVector::ZeroVector;
		}
		else
		{
			const FVector TipVelocity = (Tip - Ear.LastTip) / DeltaSeconds;
			const FVector Raw = ((TipVelocity - Ear.LastTipVelocity) / DeltaSeconds).GetClampedToMaxSize(6000.f);
			Ear.TipAcceleration = FMath::VInterpTo(Ear.TipAcceleration, Raw, DeltaSeconds, 20.f);
			Ear.LastTip = Tip;
			// (a teleport or a respawn: no throw from it)
			Ear.LastTipVelocity = TipVelocity.SizeSquared() < FMath::Square(5000.f) ? TipVelocity : FVector::ZeroVector;
		}
		const FVector Felt = EarHeadRest.RotateVector(HeadWorld.GetRotation().UnrotateVector(Ear.TipAcceleration));
		// (degrees a second squared: the tip lags behind the way it is pushed; a landing or a jump's push up also flops
		// the ears out and down, as if they were softer that way)
		const float Reach = Ear.Length * Scale;
		const float ThrowBack = -FVector::DotProduct(Felt, Ear.BackMove) / Reach * Throw * (180.f / PI);
		const float ThrowOut = (-FVector::DotProduct(Felt, Ear.OutMove) + 0.5f * FMath::Max(Felt.Z, 0.f)) / Reach * Throw * (180.f / PI);
		const float Total = Count * (Count + 1) * 0.5f;
		const float Most = Count > 1 ? 1.5f * EarMost / Count : EarMost;
		for (int32 Bone = 0; Bone < Count; ++Bone)
		{
			// (a stiff ear - EarRigidity - turns at its root bone; a soft one bends along, the throw more toward the tip)
			const float RootOnly = Bone == 0 ? 1.f : 0.f;
			const float TipWard = FMath::Lerp((Bone + 1) / Total, RootOnly, Rigidity);
			const float Even = FMath::Lerp(1.f / Count, RootOnly, Rigidity);
			const float BoneMost = FMath::Lerp(Most, Bone == 0 ? 1.2f * EarMost : Most, Rigidity);
			const float Along = Count > 1 ? float(Bone) / (Count - 1) : 0.f;
			if (bShot && Life > 0.5f)
			{
				Ear.BackSpeed[Bone] += EarShotKick * Pose * TipWard;
			}
			if (EarIndex == Twitching)
			{
				Ear.BackSpeed[Bone] += EarTwitch * Pose * TipWard;
				Ear.OutSpeed[Bone] += 0.4f * EarTwitch * Pose * TipWard;
			}
			const float Omega = 2.f * PI * RootHz * FMath::Lerp(1.f, EarTipFrequencyShare, Along);
			const float Damping = 2.f * DampingRatio * Omega;
			for (float Left = DeltaSeconds; Left > 0.f; Left -= MaxStep)
			{
				const float Step = FMath::Min(Left, MaxStep);
				Ear.BackSpeed[Bone] += (Omega * Omega * ((BackPose + ListenBack) * Even - Ear.Back[Bone]) - Damping * Ear.BackSpeed[Bone] + ThrowBack * TipWard) * Step;
				Ear.OutSpeed[Bone] += (Omega * Omega * ((OutPose + ListenOut) * Even - Ear.Out[Bone]) - Damping * Ear.OutSpeed[Bone] + ThrowOut * TipWard) * Step;
				Ear.Back[Bone] += Ear.BackSpeed[Bone] * Step;
				Ear.Out[Bone] += Ear.OutSpeed[Bone] * Step;
			}
			if (FMath::Abs(Ear.Back[Bone]) > BoneMost)
			{
				Ear.Back[Bone] = FMath::Sign(Ear.Back[Bone]) * BoneMost;
				Ear.BackSpeed[Bone] = 0.f;
			}
			if (FMath::Abs(Ear.Out[Bone]) > BoneMost)
			{
				Ear.Out[Bone] = FMath::Sign(Ear.Out[Bone]) * BoneMost;
				Ear.OutSpeed[Bone] = 0.f;
			}
			EarBoneNames.Add(Ear.Bones[Bone]);
			EarBends.Add(FQuat(Ear.OutAxis, FMath::DegreesToRadians(Ear.Out[Bone])) * FQuat(Ear.BackAxis, FMath::DegreesToRadians(Ear.Back[Bone])));
		}
	}
	if (CVarEarLog.GetValueOnGameThread() != 0 && Brawler && Brawler->IsLocallyControlled() && Brawler->IsPlayerControlled() && (GFrameCounter % 6) == 0)
	{
		float Back = 0.f, Out = 0.f;
		for (int32 Bone = 0; Bone < Ears[0].Bones.Num(); ++Bone)
		{
			Back += Ears[0].Back[Bone];
			Out += Ears[0].Out[Bone];
		}
		UE_LOG(LogTemp, Log, TEXT("RREar: pose back %.1f out %.1f | first ear back %.1f out %.1f | felt %.0f | hit %.2f down %.2f"), BackPose, OutPose, Back, Out,
			Ears[0].TipAcceleration.Size(), Hit, Down);
	}
}

FAnimInstanceProxy* URRRandomBodyAnimInstance::CreateAnimInstanceProxy()
{
	return new FRRRandomBodyAnimInstanceProxy(this);
}

void URRRandomBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	UpdateMotion(DeltaSeconds);
	UpdateAim(DeltaSeconds);
	UpdateMelee(DeltaSeconds);
	UpdateGunHands(DeltaSeconds);
	if (DeltaSeconds > 0.f)
	{
		UpdateFlightPose(DeltaSeconds);
		UpdateJumpPose(DeltaSeconds);
		UpdateSprintPose(DeltaSeconds);
		UpdateRidePose(DeltaSeconds);
		UpdateWings(DeltaSeconds);
		UpdateEars(DeltaSeconds);
	}

	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const AActor* Owner = GetOwningActor();
	if (!Mesh || !Owner || TailBoneNames.Num() == 0 || DeltaSeconds <= 0.f)
	{
		return;
	}
	// The chain bends every tail bone the mesh has: Tail5, Tail6 ... after the named ones (looked for again when the
	// mesh changes: another animal friend's body)
	const bool bChain = CVarTailChain.GetValueOnGameThread() != 0;
	if (TailBonesMesh.Get() != Mesh->GetSkeletalMeshAsset())
	{
		TailBonesMesh = Mesh->GetSkeletalMeshAsset();
		ActiveTailBones = TailBoneNames;
		// (where this animal friend's tail bends and how stiff its root is)
		TailRootBias = ChainRootBias;
		TailRootHz = ChainRootFrequency;
		bTailBigWag = true;
		TailIdleCurl = TailIdleLift = TailIdleTipLift = 0.f;
		TailAmount = 1.f;
		TailWave = TailWagSize = TailWagSpeed = 1.f;
		TailTipHz = -1.f;
		TailMoveLift = TailMoveTipLift = 0.f;
		TailDampingRatio = -1.f;
		TailSquiggle = TailSquiggleSide = 0.f;
		TailSquiggleWaves = 1.f;
		TailSquiggleSpeed = 0.4f;
		TailSize = 1.f;
		TailFollow = 0.f;
		TailTurnDamping = TailTurnFollow = -1.f;
		for (const FRRSpeciesLook& Look : FRRSpeciesLook::GetAll())
		{
			if (TailBonesMesh.IsValid() && Look.Mesh == TailBonesMesh.Get())
			{
				TailRootBias = Look.TailRootBias;
				TailRootHz = Look.TailRootFrequency;
				bTailBigWag = Look.bTailBigWag;
				TailIdleCurl = Look.TailIdleCurl;
				TailIdleLift = Look.TailIdleLift;
				TailIdleTipLift = Look.TailIdleTipLift;
				TailAmount = Look.TailAmount;
				TailWave = Look.TailWave;
				TailWagSize = Look.TailWagSize;
				TailWagSpeed = Look.TailWagSpeed;
				TailTipHz = Look.TailTipFrequency;
				TailMoveLift = Look.TailMoveLift;
				TailMoveTipLift = Look.TailMoveTipLift;
				TailDampingRatio = Look.TailDamping;
				TailSquiggle = Look.TailSquiggle;
				TailSquiggleSide = Look.TailSquiggleSide;
				TailSquiggleWaves = Look.TailSquiggleWaves;
				TailSquiggleSpeed = Look.TailSquiggleSpeed;
				TailSize = Look.TailSize;
				TailFollow = Look.TailFollow;
				TailTurnDamping = Look.TailTurnDamping;
				TailTurnFollow = Look.TailTurnFollow;
				break;
			}
		}
		for (int32 Number = TailBoneNames.Num() + 1; TailBonesMesh.IsValid() && Number <= MaxTailBones; ++Number)
		{
			const FName Name(*FString::Printf(TEXT("Tail%d"), Number));
			if (Mesh->GetBoneIndex(Name) == INDEX_NONE)
			{
				break;
			}
			ActiveTailBones.Add(Name);
		}
		// (only the ones this mesh has, from Tail1 on: the rabbit's round tail is one bone, 2026-11-25 - counting four
		// shared its bend among three bones that are not there)
		for (int32 Index = 0; Index < ActiveTailBones.Num(); ++Index)
		{
			if (TailBonesMesh.IsValid() && Mesh->GetBoneIndex(ActiveTailBones[Index]) == INDEX_NONE)
			{
				ActiveTailBones.SetNum(Index);
				break;
			}
		}
		if (ActiveTailBones.Num() != TailBoneNames.Num())
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s has a tail of %d bones."), *Owner->GetName(), ActiveTailBones.Num());
		}
	}
	const int32 Count = bChain ? ActiveTailBones.Num() : TailBoneNames.Num();
	// (no tail bones on this mesh)
	bTailFloor = false;
	if (Count == 0)
	{
		BoneSwing.Reset();
		BoneLift.Reset();
		return;
	}

	// The floor under the tail (2026-11-25, the user: "바닥을 뚫고 안 보이는 부분은 없게"): straight down from the tail's root,
	// static ground only (not other characters or cars), in the mesh's space for the proxy, which lifts each bone just
	// enough that the joint after it stays clear. Not riding (the tail is up off the seat).
	if (CVarTailFloor.GetValueOnGameThread() != 0 && RideWeight < 0.5f && GetWorld())
	{
		const FVector Root = Mesh->GetBoneLocation(GetTailBones()[0]);
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RRTailFloor), false, Owner);
		if (GetWorld()->LineTraceSingleByObjectType(Hit, Root + FVector(0.f, 0.f, 10.f), Root - FVector(0.f, 0.f, 400.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			const FTransform& ToWorld = Mesh->GetComponentTransform();
			TailFloorPoint = ToWorld.InverseTransformPosition(Hit.ImpactPoint);
			TailFloorNormal = ToWorld.InverseTransformVectorNoScale(Hit.ImpactNormal).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
			TailFloorMargin = CVarTailFloorMargin.GetValueOnGameThread() * TailSize;
			bTailFloor = true;
		}
	}

	// Where the tail's base points and how high its root is, from the posed hips (2026-11-25, the user: "꼬리가 너무 막대기
	// 같은데 뛰거나 방향전환을 할 때 체인같은 느낌을 조금 더"): how much they turned and rose since the last frame - every
	// sway and bob of the hips in a run, every turn - is what the chain's far end lags behind (FRRSpeciesLook::TailFollow)
	TailFollowYaw = TailFollowPitch = TailFollowRise = 0.f;
	{
		const TArray<FName>& Bones = GetTailBones();
		const FReferenceSkeleton& Ref = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
		if (TailDirMesh.Get() != Mesh->GetSkeletalMeshAsset())
		{
			TailDirMesh = Mesh->GetSkeletalMeshAsset();
			TailParent = NAME_None;
			bTailFollowLast = false;
			const int32 First = Ref.FindBoneIndex(Bones[0]);
			const int32 Second = Bones.Num() > 1 ? Ref.FindBoneIndex(Bones[1]) : INDEX_NONE;
			const int32 Parent = First != INDEX_NONE ? Ref.GetParentIndex(First) : INDEX_NONE;
			if (Second != INDEX_NONE && Parent != INDEX_NONE)
			{
				const FTransform ParentRest = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Parent);
				const FVector FirstAt = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, First).GetLocation();
				TailDirLocal = ParentRest.InverseTransformVectorNoScale(FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Second).GetLocation() - FirstAt).GetSafeNormal();
				TailParent = Ref.GetBoneName(Parent);
				// (its length: the bones' own, and the last one's once more)
				TailLengthMesh = 0.f;
				FVector At = FirstAt, Step = FVector::ZeroVector;
				for (int32 Bone = 1; Bone < Bones.Num(); ++Bone)
				{
					const int32 Index = Ref.FindBoneIndex(Bones[Bone]);
					if (Index == INDEX_NONE)
					{
						break;
					}
					const FVector Next = FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Index).GetLocation();
					Step = Next - At;
					TailLengthMesh += static_cast<float>(Step.Size());
					At = Next;
				}
				TailLengthMesh += static_cast<float>(Step.Size());
			}
		}
		if (TailParent != NAME_None && !TailDirLocal.IsNearlyZero())
		{
			const FVector Dir = Mesh->GetBoneQuaternion(TailParent).RotateVector(TailDirLocal);
			const float DirYaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
			const float DirPitch = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(static_cast<float>(Dir.Z), -1.f, 1.f)));
			const float RootZ = static_cast<float>(Mesh->GetBoneLocation(Bones[0]).Z);
			const float RiseMost = 10.f;
			if (bTailFollowLast && FMath::Abs(RootZ - LastTailRootZ) < 4.f * RiseMost)
			{
				TailFollowYaw = FMath::Clamp(FMath::FindDeltaAngleDegrees(LastTailYaw, DirYaw), -MaxTurnPerFrame, MaxTurnPerFrame);
				TailFollowPitch = FMath::Clamp(DirPitch - LastTailPitch, -10.f, 10.f);
				TailFollowRise = FMath::Clamp(RootZ - LastTailRootZ, -RiseMost, RiseMost);
			}
			LastTailYaw = DirYaw;
			LastTailPitch = DirPitch;
			LastTailRootZ = RootZ;
			bTailFollowLast = true;
		}
	}

	// Follow the rendered pelvis, not the gameplay actor's immediate aim turn.
	const float Yaw = Aim.bSeparateBody ? Aim.LowerYaw : Owner->GetActorRotation().Yaw;
	const FVector Right = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
	const FVector Velocity = Owner->GetVelocity();
	if (!bHasLast)
	{
		bHasLast = true;
		LastYaw = Yaw;
		LastVelocity = Velocity;
		// Different animals do not sway in lockstep when standing together.
		WagPhase = (Owner->GetUniqueID() % 101) * (2.f * PI / 101.f);
		TailTime = WagPhase;
	}

	// Turning: the tail keeps pointing where it did for a moment, so relative to the body it swings toward the turn's
	// outside (turning right leaves it bent to the right of the body)
	Swing += FMath::Clamp(FMath::FindDeltaAngleDegrees(LastYaw, Yaw), -MaxTurnPerFrame, MaxTurnPerFrame) * TurnDrag;
	// How hard the body is turning, 0 .. 1 (2026-11-25, the user: "걸을 땐 묵직하게, 방향 틀 때만 격하게 나눠줘"): up at once
	// with the turn, back down over about a second after it, so the whip has time to play out. The tail's springs and
	// its chain lag go from the walking numbers to the turning ones with it (FRRSpeciesLook::TailTurnDamping, TailTurnFollow).
	{
		const float TurnRate = FMath::Abs(FMath::FindDeltaAngleDegrees(LastYaw, Yaw)) / DeltaSeconds;
		const float Hard = FMath::Clamp((TurnRate - TailTurnFrom) / FMath::Max(TailTurnFull - TailTurnFrom, 1.f), 0.f, 1.f);
		TailTurning = Hard > TailTurning ? Hard : FMath::FInterpTo(TailTurning, Hard, DeltaSeconds, 1.5f);
	}
	// Speeding up sideways: it trails the other way while the push lasts
	const FVector Acceleration = ((Velocity - LastVelocity) / DeltaSeconds).GetClampedToMaxSize(MaxAcceleration);
	const float AlongBrake = -FVector::DotProduct(Acceleration, LastVelocity.GetSafeNormal2D());
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(Owner);
	const bool bGroundMotion = Brawler && Brawler->WantsMotionMatch();
	if (bGroundMotion && LastVelocity.Size2D() > 210.f && AlongBrake > 380.f) { TailBrakeHold = 0.60f; }
	else { TailBrakeHold = bGroundMotion ? FMath::Max(0.f, TailBrakeHold - DeltaSeconds) : 0.f; }
	TailTrail = FMath::FInterpTo(TailTrail, -FVector::DotProduct(Acceleration, Right) * SideAccelSwing, DeltaSeconds, 6.f);
	LastYaw = Yaw;
	LastVelocity = Velocity;

	// A damped spring back to straight (or to the trail), a little bouncy
	const float Omega = 2.f * PI * SpringFrequency;
	const float Stiffness = Omega * Omega;
	const float Damping = 2.f * FMath::Lerp(1.f, 0.15f, SpringBounce) * Omega;
	for (float Left = DeltaSeconds; Left > 0.f; Left -= MaxStep)
	{
		const float Step = FMath::Min(Left, MaxStep);
		SwingSpeed += (Stiffness * (TailTrail - Swing) - Damping * SwingSpeed) * Step;
		Swing += SwingSpeed * Step;
	}
	if (FMath::Abs(Swing) > MaxSwing)
	{
		Swing = FMath::Sign(Swing) * MaxSwing;
		SwingSpeed = 0.f;
	}

	// Wagging and lifting with speed; a knocked-out brawler's tail goes still
	Life = FMath::FInterpConstantTo(Life, !Brawler || Brawler->IsAlive() ? 1.f : 0.f, DeltaSeconds, LifeFadeSpeed);
	TailPace = FMath::FInterpTo(TailPace, FMath::Clamp(Velocity.Size2D() / FMath::Max(WagRunSpeed, 1.f), 0.f, 1.f), DeltaSeconds, 3.f);
	const float Run = TailPace;
	TailTime += DeltaSeconds;
	WagPhase = FMath::Fmod(WagPhase + 2.f * PI * FMath::Lerp(WagFrequency.X, WagFrequency.Y, Run) * TailWagSpeed
		* (1.f + 0.08f * FMath::Sin(TailTime * 0.73f)) * DeltaSeconds, 2.f * PI);
	// Riding, the tail lifts up off the seat and out of the vehicle's body
	Lift = FMath::FInterpTo(Lift, RunLift * Run + RideTailLift * RideWeight, DeltaSeconds, LiftSpeed);
	const float Wag = FMath::Lerp(WagAmplitude.X, WagAmplitude.Y, Run) * TailWagSize
		* (1.f - 0.90f * FMath::SmoothStep(0.f, 0.35f, TailBrakeHold));
	const float TotalLift = (Lift + RunBob * Run * FMath::Sin(2.f * WagPhase) + 0.8f * FMath::Sin(TailTime * 1.3f)) * Life;

	// Spread over the bones, more toward the tip so the tail curls rather than pivoting stiffly at the root
	BoneSwing.SetNumZeroed(Count);
	BoneLift.SetNumZeroed(Count);
	const float Total = Count * (Count + 1) * 0.5f;
	if (bChain)
	{
		// Each bone a spring of its own toward its share of the wag, the lift and the turn swing; softer toward the tip,
		// and left behind by the bones above it
		const auto Tuned = [](const TAutoConsoleVariable<float>& Var, float Default)
		{
			const float Value = Var.GetValueOnGameThread();
			return Value >= 0.f ? Value : Default;
		};
		const float RootHz = Tuned(CVarTailRootHz, TailRootHz);
		const float Bias = FMath::Clamp(Tuned(CVarTailRootBias, TailRootBias), 0.f, 1.f);
		const float TipHz = Tuned(CVarTailTipHz, TailTipHz > 0.f ? TailTipHz : ChainTipFrequency);
		const float CalmRatio = TailDampingRatio > 0.f ? TailDampingRatio : ChainDamping;
		const float Ratio = Tuned(CVarTailDamping, TailTurnDamping > 0.f ? FMath::Lerp(CalmRatio, TailTurnDamping, TailTurning) : CalmRatio);
		const float Inertia = FMath::Clamp(Tuned(CVarTailInertia, ChainInertia), 0.f, 1.f);
		// (the user, 2026-11-24: too faint, only the tip showed anything, three times as much would do, and the springs
		// were too stiff out to the tip. So what the body's motion does to the tail is multiplied by Gain, the bends are
		// shared more evenly along the tail (Even: the root takes part), and the tip's spring is much softer.)
		const float Gain = Tuned(CVarTailGain, ChainGain);
		// What the body's own way does to it: rising drags the tail down and falling floats it up (a jump), braking
		// throws it up and forward and speeding up leaves it trailing low
		const FVector Forward = FRotator(0.f, Yaw, 0.f).Vector();
		const float Fall = -Velocity.Z * Tuned(CVarTailFallLift, FallLift) * Gain;
		const float Brake = (TailBrakeHold > 0.f ? AlongBrake : -FVector::DotProduct(Acceleration, Forward)) * Tuned(CVarTailBrakeLift, BrakeLift) * Gain;
		const float SwingBig = FMath::Clamp(Swing * Gain, -MaxChainSwing, MaxChainSwing);
		// (the wag itself too: walking and running straight it is all that moves the tail, and at its old size the chain
		// looked no different from before. Half as much again standing, Gain times on the run.)
		const float WagBig = bTailBigWag ? Wag * FMath::Lerp(FMath::Min(1.5f, Gain), Gain, Run) : Wag;
		// (down by much less than up: the tail hangs close to the legs and the floor already)
		LiftTrail = FMath::FInterpTo(LiftTrail, FMath::Clamp(Fall + Brake, -MaxChainLift * 0.3f, MaxChainLift) * (1.f - RideWeight), DeltaSeconds, 12.f);
		// A shot flicks it up, the tip most
		const float SinceShot = Brawler ? Brawler->GetTimeSinceShot() : LastSinceShot;
		const float Flick = SinceShot < LastSinceShot - KINDA_SMALL_NUMBER ? Tuned(CVarTailShotKick, ShotTailKick) * Life : 0.f;
		LastSinceShot = SinceShot;

		// The idle pose: standing still the tail curls to one side, more toward the tip, and lifts a little at its root;
		// moving, it straightens out behind the body again (about a second either way, then the springs)
		const bool bStanding = Velocity.SizeSquared2D() < FMath::Square(30.f) && RideWeight < 0.5f && Life > 0.5f;
		TailIdle = FMath::FInterpTo(TailIdle, bStanding ? 1.f : 0.f, DeltaSeconds, bStanding ? 2.5f : 5.f);
		const float IdleCurl = (CVarTailIdleCurl.GetValueOnGameThread() > -900.f ? CVarTailIdleCurl.GetValueOnGameThread() : TailIdleCurl) * TailIdle;
		const float IdleLift = (CVarTailIdleLift.GetValueOnGameThread() > -900.f ? CVarTailIdleLift.GetValueOnGameThread() : TailIdleLift) * TailIdle;
		const float IdleTipLift = (CVarTailIdleTipLift.GetValueOnGameThread() > -900.f ? CVarTailIdleTipLift.GetValueOnGameThread() : TailIdleTipLift) * TailIdle;
		// (the lift at the root raises the whole tail; the tip's is shared toward the far end, which after the curl
		// points sideways - the bones' lift is about their own side axis, see the proxy - so the end turns up)
		// (moving: a cat carries its tail up, the end hooked - TailMoveLift, TailMoveTipLift - fading in with pace; at a sprint
		// a quarter as high, nearer level, the way a running cat holds it)
		const float Moving = (1.f - TailIdle) * FMath::Min(1.f, Run * 2.f) * (1.f - 0.75f * FMath::SmoothStep(0.6f, 0.9f, Run)) * (1.f - RideWeight) * Life;
		// The squiggle (FRRSpeciesLook::TailSquiggle): bone against bone, an S-curve running slowly down the tail, up and down
		// and a quarter wave later to the sides, so it never holds as one straight stick. Fullest walking, a little less
		// standing, less at a sprint, none riding or knocked out.
		const auto Own = [](const TAutoConsoleVariable<float>& Var, float Default)
		{
			const float Value = Var.GetValueOnGameThread();
			return Value >= 0.f ? Value : Default;
		};
		const float SquiggleUp = Own(CVarTailSquiggle, TailSquiggle), SquiggleSide = Own(CVarTailSquiggleSide, TailSquiggleSide);
		const float SquiggleWaves = Own(CVarTailSquiggleWaves, TailSquiggleWaves);
		const float MoveLift = CVarTailMoveLift.GetValueOnGameThread() > -900.f ? CVarTailMoveLift.GetValueOnGameThread() : TailMoveLift;
		const float MoveTipLift = CVarTailMoveTipLift.GetValueOnGameThread() > -900.f ? CVarTailMoveTipLift.GetValueOnGameThread() : TailMoveTipLift;
		SquigglePhase = FMath::Fmod(SquigglePhase + 2.f * PI * Own(CVarTailSquiggleHz, TailSquiggleSpeed) * (1.f + 0.5f * Run) * DeltaSeconds, 2.f * PI);
		const float Squiggling = FMath::Lerp(0.6f, 1.f, 1.f - TailIdle) * (1.f - 0.6f * FMath::SmoothStep(0.6f, 0.9f, Run)) * (1.f - RideWeight) * Life
			* 8.f / FMath::Max(Count, 1);
		TArray<float, TInlineAllocator<16>> Rest, LiftRest, SquiggleLift;
		Rest.SetNumZeroed(Count);
		LiftRest.SetNumZeroed(Count);
		SquiggleLift.SetNumZeroed(Count);
		for (int32 Bone = 0; Bone < Count; ++Bone)
		{
			const float Along = Count > 1 ? float(Bone) / (Count - 1) : 1.f;
			const float At = SquigglePhase - 2.f * PI * SquiggleWaves * Along;
			const float Loose = Squiggling * FMath::Lerp(0.6f, 1.4f, Along);
			SquiggleLift[Bone] = SquiggleUp * Loose * FMath::Sin(At);
			Rest[Bone] = IdleCurl * (Bone + 1) / Total + SquiggleSide * Loose * FMath::Sin(At - 0.5f * PI);
			LiftRest[Bone] = IdleLift * (Count - Bone) / Total + IdleTipLift * (Bone + 1) / Total
				+ Moving * (MoveLift * (Count - Bone) / Total + MoveTipLift * (Bone + 1) / Total) + SquiggleLift[Bone];
		}
		BoneSwingSpeed.SetNumZeroed(Count);
		BoneLiftSpeed.SetNumZeroed(Count);
		TArray<float, TInlineAllocator<16>> SwingTo, LiftTo;
		SwingTo.SetNumUninitialized(Count);
		LiftTo.SetNumUninitialized(Count);
		for (int32 Bone = 0; Bone < Count; ++Bone)
		{
			const float Share = (Bone + 1) / Total;
			const float Delay = Count > 1 ? WagDelay * TailWave * Bone / (Count - 1) : 0.f;
			const float Wave = 0.85f * FMath::Sin(WagPhase - Delay) + 0.15f * FMath::Sin(TailTime * 1.07f - Delay * 1.4f);
			// (the bend's share: toward the tip, as the wag was always shared, or toward the root - the whole tail swings)
			const float Even = FMath::Lerp(Share, (Count - Bone) / Total, Bias);
			SwingTo[Bone] = (Rest[Bone] + (SwingBig + WagBig * Wave) * Even * Life) * TailAmount;
			// (the lift from falling and braking: half as the running lift is shared, at the root, so the whole tail goes
			// up, and half toward the tip, so it curls)
			const float RootShare = Count == 1 ? 1.f : (Bone == 0 ? RootLiftShare : (1.f - RootLiftShare) / (Count - 1));
			LiftTo[Bone] = (TotalLift + (Gain - 1.f) * RunBob * Run * FMath::Sin(2.f * WagPhase) * Life) * RootShare + LiftTrail * 0.5f * (RootShare + Even) * Life
				+ LiftRest[Bone];
			LiftTo[Bone] *= TailAmount;
			// (the flick: about the same turn all told however many bones the tail has)
			const float TipWard = Count > 1 ? float(Bone) / (Count - 1) : 1.f;
			BoneLiftSpeed[Bone] += Flick * TailAmount * FMath::Lerp(0.5f + TipWard, 1.5f - TipWard, Bias) * 4.f / FMath::Max(Count, 4);
		}
		// The chain's lag (FRRSpeciesLook::TailFollow): as the hips turn, tip and bob, each bone keeps a share of where it
		// pointed in the world - little at the root, most at the tip - so the bend runs down the tail and the springs bring
		// it after, a beat later toward the end, instead of the whole tail swinging round with the body like a stick
		const float Follow = Tuned(CVarTailFollow, TailTurnFollow > 0.f ? FMath::Lerp(TailFollow, TailTurnFollow, TailTurning) : TailFollow)
			* (1.f - RideWeight) * Life;
		if (Follow > 0.f)
		{
			const float World = TailLengthMesh * static_cast<float>(Mesh->GetComponentTransform().GetScale3D().Z) * TailSize;
			// (a root that rises leaves the far end behind, below: about as if the tail turned down about a point two
			// thirds of the way along)
			const float Dip = -TailFollowPitch - FMath::RadiansToDegrees(TailFollowRise / FMath::Max(0.65f * World, 1.f));
			// (1.5 at first; 1.2 with the tiger's stronger lag, 2026-11-25: more of it toward the middle)
			const float Curve = FMath::Max(0.1f, Tuned(CVarTailFollowCurve, 1.2f));
			float Kept = 0.f;
			for (int32 Bone = 0; Bone < Count; ++Bone)
			{
				const float Upto = FMath::Pow((Bone + 1.f) / Count, Curve);
				const float Share = (Upto - Kept) * Follow;
				Kept = Upto;
				BoneSwing[Bone] += TailFollowYaw * Share;
				BoneLift[Bone] += Dip * Share;
			}
		}
		for (float Left = DeltaSeconds; Left > 0.f; Left -= MaxStep)
		{
			const float Step = FMath::Min(Left, MaxStep);
			// (what the bones above have done this step, all together: a bone's own turning is taken off it by Inertia)
			float SwingAbove = 0.f, LiftAbove = 0.f;
			for (int32 Bone = 0; Bone < Count; ++Bone)
			{
				const float Along = Count > 1 ? float(Bone) / (Count - 1) : 0.f;
				const float BoneOmega = 2.f * PI * FMath::Lerp(RootHz, TipHz, Along);
				const float BoneDamping = 2.f * Ratio * FMath::Lerp(1.f, 0.6f, Along) * BoneOmega;
				const float SwingPull = BoneOmega * BoneOmega * (SwingTo[Bone] - BoneSwing[Bone]) - BoneDamping * BoneSwingSpeed[Bone] - Inertia * SwingAbove;
				const float LiftPull = BoneOmega * BoneOmega * (LiftTo[Bone] - BoneLift[Bone]) - BoneDamping * BoneLiftSpeed[Bone] - Inertia * LiftAbove;
				BoneSwingSpeed[Bone] += SwingPull * Step;
				BoneLiftSpeed[Bone] += LiftPull * Step;
				// (a bone's own most, by how many bones share the bend: past it the tail rolled up into a ball on landing)
				const float SwingMost = BoneRoom * MaxChainSwing / Count, LiftMost = BoneRoom * MaxChainLift / Count;
				BoneSwing[Bone] += BoneSwingSpeed[Bone] * Step;
				if (FMath::Abs(BoneSwing[Bone] - Rest[Bone]) > SwingMost)
				{
					BoneSwing[Bone] = Rest[Bone] + FMath::Sign(BoneSwing[Bone] - Rest[Bone]) * SwingMost;
					BoneSwingSpeed[Bone] = 0.f;
				}
				BoneLift[Bone] += BoneLiftSpeed[Bone] * Step;
				if (BoneLift[Bone] > LiftRest[Bone] + LiftMost)
				{
					BoneLift[Bone] = LiftRest[Bone] + LiftMost;
					BoneLiftSpeed[Bone] = FMath::Min(BoneLiftSpeed[Bone], 0.f);
				}
				// (the floor: the tail hangs close to the legs and the ground, so it only goes a little way down, and
				// stops there with a small bounce - swinging through, it rolled up into a ball under the body)
				// (a squiggle's own downward bend is let through: bone against bone, it never rolls the tail under; and so is a
				// pose that hangs the tail down on purpose - the tiger's, 2026-11-25 - the floor is then that far under its pose)
				const float Floor = -MaxChainDown / Count + FMath::Min(0.f, LiftRest[Bone]);
				if (BoneLift[Bone] < Floor)
				{
					BoneLift[Bone] = Floor;
					BoneLiftSpeed[Bone] = FMath::Max(BoneLiftSpeed[Bone], 0.f) - FMath::Min(BoneLiftSpeed[Bone], 0.f) * 0.2f;
				}
				SwingAbove += SwingPull;
				LiftAbove += LiftPull;
			}
		}
		if (CVarTailLog.GetValueOnGameThread() != 0 && Brawler && Brawler->IsLocallyControlled() && Brawler->IsPlayerControlled() && (GFrameCounter % 6) == 0)
		{
			float SwingAll = 0.f, LiftAll = 0.f;
			for (int32 Bone = 0; Bone < Count; ++Bone)
			{
				SwingAll += BoneSwing[Bone];
				LiftAll += BoneLift[Bone];
			}
			UE_LOG(LogTemp, Log, TEXT("RRTail: %d bones, speed %.0f up %.0f, trail %.1f, lift %.1f (tip %.1f), swing %.1f (tip %.1f), turning %.2f damping %.2f follow %.2f"), Count, Velocity.Size2D(), Velocity.Z,
				LiftTrail, LiftAll, BoneLift.Last(), SwingAll, BoneSwing.Last(), TailTurning, Ratio, Follow);
		}
		return;
	}
	for (int32 Bone = 0; Bone < Count; ++Bone)
	{
		const float Share = (Bone + 1) / Total;
		const float Delay = Count > 1 ? WagDelay * TailWave * Bone / (Count - 1) : 0.f;
		const float Wave = 0.85f * FMath::Sin(WagPhase - Delay) + 0.15f * FMath::Sin(TailTime * 1.07f - Delay * 1.4f);
		// The soft tip follows the root later instead of all four joints reversing together.
		const float Follow = FMath::Lerp(18.f, 8.f, Count > 1 ? float(Bone) / (Count - 1) : 0.f);
		BoneSwing[Bone] = FMath::FInterpTo(BoneSwing[Bone], (Swing + Wag * Wave) * Share * Life, DeltaSeconds, Follow);
		BoneLift[Bone] = FMath::FInterpTo(BoneLift[Bone], TotalLift * (Count == 1 ? 1.f : (Bone == 0 ? RootLiftShare : (1.f - RootLiftShare) / (Count - 1))), DeltaSeconds, Follow);
	}
}

void URRRandomBodyAnimInstance::UpdateMotion(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	USkeletalMeshComponent* Driver = Brawler ? Brawler->GetMotionDriver() : nullptr;
	const USkeletalMesh* DriverMesh = Driver ? Driver->GetSkeletalMeshAsset() : nullptr;
	const USkeletalMesh* BodyMesh = Mesh ? Mesh->GetSkeletalMeshAsset() : nullptr;
	if (!DriverMesh || !BodyMesh)
	{
		MotionWeight = 0.f;
		MotionWholeWeight = 0.f;
		PivotTime = PivotLoad = 0.f;
		MotionLean = 0.f;
		MotionMappedMesh.Reset();
		return;
	}
	const FReferenceSkeleton& Body = BodyMesh->GetRefSkeleton();
	const FReferenceSkeleton& Source = DriverMesh->GetRefSkeleton();
	const int32 UpperMode = CVarMotionUpper.GetValueOnGameThread();
	const bool bRetarget = Brawler->UsesSharedMotionRig();
	// (round 103: mapped again too when it comes to retargeting after being mapped without (a joiner's animal friend, its
	// species arriving after its meshes): the retarget poses were empty and the next read crashed)
	if (MotionMappedMesh.Get() != DriverMesh || MotionMappedBody.Get() != BodyMesh || MotionUpperMode != UpperMode
		|| (bRetarget && (SourceComponentPose.Num() != Source.GetNum() || TargetComponentPose.Num() != Body.GetNum() || MotionBindRotations.Num() != Body.GetNum())))
	{
		// Bone for bone by name. 1: taken whenever matched (hips, legs, and with rr.MotionMatch.Upper 1 the spine and head);
		// 2: taken only while the driver's whole pose is (a climbing clip): the arms, which otherwise keep the gun-holding
		// animation, and the spine with Upper 0; 0: never (the tail, bent by code)
		MotionMappedMesh = DriverMesh;
		MotionMappedBody = BodyMesh;
		MotionUpperMode = UpperMode;
		MotionBoneMap.Init(INDEX_NONE, Body.GetNum());
		MotionMask.Init(0, Body.GetNum());
		const int32 Arms[] = { Body.FindBoneIndex(TEXT("LeftShoulder")), Body.FindBoneIndex(TEXT("RightShoulder")),
			UpperMode == 0 ? Body.FindBoneIndex(TEXT("Spine")) : INDEX_NONE };
		const int32 Tail = Body.FindBoneIndex(TEXT("Tail1"));
		for (int32 Bone = 0; Bone < Body.GetNum(); ++Bone)
		{
			bool bWhole = false, bNever = false;
			for (int32 Walk = Bone; Walk != INDEX_NONE; Walk = Body.GetParentIndex(Walk))
			{
				bWhole |= Walk == Arms[0] || Walk == Arms[1] || Walk == Arms[2];
				bNever |= Walk == Tail;
			}
			MotionBoneMap[Bone] = Source.FindBoneIndex(Body.GetBoneName(Bone));
			MotionMask[Bone] = bNever || MotionBoneMap[Bone] == INDEX_NONE ? 0 : bWhole ? 2 : 1;
		}
		if (bRetarget)
		{
			Source.GetBoneAbsoluteTransforms(SourceComponentPose);
			Body.GetBoneAbsoluteTransforms(TargetComponentPose);
			MotionBindRotations.SetNum(Body.GetNum());
			for (int32 Bone = 0; Bone < Body.GetNum(); ++Bone)
			{
				if (MotionMask[Bone])
				{
					MotionBindRotations[Bone] = SourceComponentPose[MotionBoneMap[Bone]].GetRotation().Inverse()
						* TargetComponentPose[Bone].GetRotation();
				}
			}
			MotionHips = Body.FindBoneIndex(TEXT("Hips"));
			if (MotionHips != INDEX_NONE && MotionBoneMap[MotionHips] != INDEX_NONE)
			{
				SourceBindHips = SourceComponentPose[MotionBoneMap[MotionHips]].GetLocation();
				TargetBindHips = TargetComponentPose[MotionHips].GetLocation();
			}
		}
	}
	// (waits for the driver's pose if it is still being worked out)
	const TArrayView<const FTransform> Local = Driver->GetBoneSpaceTransformsView();
	const bool bPosed = Local.Num() == Source.GetNum();
	const auto ReadDriverPose = [&](TArrayView<const FTransform> BonePose, bool bInPlace, TArray<FTransform>& OutPose)
	{
		OutPose.SetNum(Body.GetNum());
		if (bRetarget)
		{
			// Transfer motion relative to each skeleton's bind pose in mesh space. Only
			// the pelvis travels; all other local offsets keep the target limb lengths.
			for (int32 Bone = 0; Bone < Source.GetNum(); ++Bone)
			{
				const int32 Parent = Source.GetParentIndex(Bone);
				SourceComponentPose[Bone] = Parent == INDEX_NONE
					? (bInPlace ? Source.GetRefBonePose()[Bone] : BonePose[Bone])
					: BonePose[Bone] * SourceComponentPose[Parent];
			}
			for (int32 Bone = 0; Bone < Body.GetNum(); ++Bone)
			{
				const int32 Parent = Body.GetParentIndex(Bone);
				const FTransform& ParentPose = Parent != INDEX_NONE ? TargetComponentPose[Parent] : FTransform::Identity;
				FTransform Pose = Body.GetRefBonePose()[Bone];
				if (MotionMask[Bone])
				{
					const FTransform& From = SourceComponentPose[MotionBoneMap[Bone]];
					Pose.SetRotation((ParentPose.GetRotation().Inverse() * From.GetRotation() * MotionBindRotations[Bone]).GetNormalized());
					if (Bone == MotionHips)
					{
						const FVector Position = TargetBindHips + (From.GetLocation() - SourceBindHips) * Brawler->GetMotionStrideRatio();
						Pose.SetLocation(ParentPose.InverseTransformPosition(Position));
					}
				}
				TargetComponentPose[Bone] = Pose * ParentPose;
				OutPose[Bone] = Pose;
			}
			return;
		}
		for (int32 Bone = 0; Bone < Body.GetNum(); ++Bone)
		{
			if (!MotionMask[Bone])
			{
				continue;
			}
			// A bone that is a root here but has parents in the driver (the hips, under the driver's root bone) takes them along
			FTransform Pose = BonePose[MotionBoneMap[Bone]];
			if (Body.GetParentIndex(Bone) == INDEX_NONE)
			{
				for (int32 Parent = Source.GetParentIndex(MotionBoneMap[Bone]); Parent != INDEX_NONE; Parent = Source.GetParentIndex(Parent))
				{
					Pose = Pose * (bInPlace ? Source.GetRefBonePose()[Parent] : BonePose[Parent]);
				}
			}
			OutPose[Bone] = Pose;
		}
	};
	if (bPosed) { ReadDriverPose(Local, false, MotionPose); }
	if (bPosed && CVarLogMotion.GetValueOnGameThread() != 0 && Brawler->IsPlayerControlled() && Brawler->IsLocallyControlled() && (GFrameCounter % 15) == 0)
	{
		// Which clip the Motion Matching node is playing now
		FString Clip = TEXT("?");
		float ClipTime = 0.f;
		if (const UAnimInstance* DriverInstance = Driver->GetAnimInstance())
		{
			if (const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(DriverInstance->GetClass()))
			{
				for (const FStructProperty* Property : AnimClass->GetAnimNodeProperties())
				{
					if (Property->Struct->IsChildOf(FAnimNode_MotionMatching::StaticStruct()))
					{
						const FAnimNode_MotionMatching* Node = Property->ContainerPtrToValuePtr<FAnimNode_MotionMatching>(DriverInstance);
						if (const UAnimationAsset* Asset = Node->GetAnimAsset()) { Clip = Asset->GetName(); }
						ClipTime = Node->GetAccumulatedTime();
					}
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("MOTIONCLIP %s at %.2f | base clip %s | actor yaw %.0f velocity yaw %.0f control yaw %.0f"), *Clip, ClipTime,
			*GetNameSafe(GetCurrentAsset()), Brawler->GetActorRotation().Yaw, Brawler->GetVelocity().Rotation().Yaw, Brawler->GetControlRotation().Yaw);
		// What the search is asked: how far the trajectory's first (past) and last (future) samples are from now, in the clips' units
		if (const URRRandomMotionMatchInstance* Matcher = Cast<URRRandomMotionMatchInstance>(Driver->GetAnimInstance()))
		{
			const TArray<FTransformTrajectorySample>& Samples = Matcher->Trajectory.Samples;
			int32 Now = 0;
			for (int32 Index = 1; Index < Samples.Num(); ++Index)
			{
				if (FMath::Abs(Samples[Index].TimeInSeconds) < FMath::Abs(Samples[Now].TimeInSeconds)) { Now = Index; }
			}
			const FVector Velocity = Brawler->GetVelocity();
			UE_LOG(LogTemp, Log, TEXT("MOTIONTRAJ speed %.0f accel %.0f max %.0f wants %d weight %.2f driver tick %d scale %.3f | samples %d now %d | past t %.2f dist %.1f | future t %.2f dist %.1f | time dilation %.2f"),
				Velocity.Size2D(), Brawler->GetCharacterMovement()->GetCurrentAcceleration().Size2D(), Brawler->GetCharacterMovement()->GetMaxSpeed(),
				Brawler->WantsMotionMatch(), MotionWeight, Driver->IsComponentTickEnabled(), Matcher->DistanceScale, Samples.Num(), Now,
				Samples.Num() ? Samples[0].TimeInSeconds : 0.f, Samples.Num() ? FVector::Dist2D(Samples[0].Position, Samples[Now].Position) : 0.f,
				Samples.Num() ? Samples.Last().TimeInSeconds : 0.f, Samples.Num() ? FVector::Dist2D(Samples.Last().Position, Samples[Now].Position) : 0.f,
				Brawler->GetActorTimeDilation());
		}
		const int32 Hips = Body.FindBoneIndex(TEXT("Hips"));
		const int32 Foot = Body.FindBoneIndex(TEXT("LeftFoot"));
		const int32 SourceRoot = Source.FindBoneIndex(TEXT("Root"));
		const int32 SourceHips = Source.FindBoneIndex(TEXT("Hips"));
		UE_LOG(LogTemp, Log, TEXT("MOTIONMATCH weight %.2f | body ref hips %s | driver ref root %s hips %s | driver now root %s hips %s | taken hips %s foot %s"),
			MotionWeight, *Body.GetRefBonePose()[Hips].ToHumanReadableString().Replace(TEXT("\n"), TEXT(" ")),
			SourceRoot != INDEX_NONE ? *Source.GetRefBonePose()[SourceRoot].ToHumanReadableString().Replace(TEXT("\n"), TEXT(" ")) : TEXT("-"),
			*Source.GetRefBonePose()[SourceHips].ToHumanReadableString().Replace(TEXT("\n"), TEXT(" ")),
			SourceRoot != INDEX_NONE ? *Local[SourceRoot].ToHumanReadableString().Replace(TEXT("\n"), TEXT(" ")) : TEXT("-"),
			*Local[SourceHips].ToHumanReadableString().Replace(TEXT("\n"), TEXT(" ")),
			*MotionPose[Hips].ToHumanReadableString().Replace(TEXT("\n"), TEXT(" ")),
			Foot != INDEX_NONE ? *MotionPose[Foot].ToHumanReadableString().Replace(TEXT("\n"), TEXT(" ")) : TEXT("-"));
	}
	// A climbing clip on the second hidden body: its whole pose, arms too, in place (its root kept where the reference
	// pose has it: the capsule does the climbing), until the clip has run out
	USkeletalMeshComponent* Climber = bPosed && Brawler->WantsClimbMotion() ? Brawler->GetClimbDriver() : nullptr;
	const TArrayView<const FTransform> ClimbLocal = Climber ? Climber->GetBoneSpaceTransformsView() : TArrayView<const FTransform>();
	const bool bClimb = Climber && ClimbLocal.Num() == Source.GetNum();
	if (bClimb)
	{
		ReadDriverPose(ClimbLocal, true, MotionWholePose);
	}
	const bool bWalk = bPosed && !bClimb && Brawler->WantsMotionMatch();
	const bool bWanted = bWalk || bClimb;
	// (in quickly, out as slowly as the clip runs out)
	MotionWholeWeight = FMath::FInterpConstantTo(MotionWholeWeight, bClimb ? 1.f : 0.f, DeltaSeconds, bClimb ? 1.f / 0.06f : 1.f / 0.3f);
	MotionWeight = FMath::FInterpConstantTo(MotionWeight, bWanted ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(MotionBlendTime, KINDA_SMALL_NUMBER));
	// The matched clips are of people running empty-handed and upright (the user, 2026-11-20: "총을 들고 달리는데 너무 꼿꼿하게
	// 서서 간다. 자연스럽게 살짝 숙이고 뛸 수 있도록"): the upper body leans into the run, in step with its ground speed up to
	// the walking speed (a sprint adds the whole body's own lean on top, ARRRandomCharacter::SprintLean). Only with the
	// upper body matched; with the hips and legs only the torso already is the gun-holding animation's.
	const float WalkMax = FMath::Max(Brawler->GetCharacterMovement()->MaxWalkSpeed, 1.f);
	const float Pace = bWalk && UpperMode != 0 ? FMath::Clamp(Brawler->GetAnimationMoveSpeed() / WalkMax, 0.f, 1.f) : 0.f;
	const float LeanAtRun = CVarMotionLean.GetValueOnGameThread() >= 0.f ? CVarMotionLean.GetValueOnGameThread() : MotionRunLean;
	PivotTime = FMath::Max(0.f, PivotTime - DeltaSeconds);
	if (bWalk)
	{
		const FVector Intent = Brawler->GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
		if (PivotTime == 0.f && LastVelocity.Size2D() > 175.f && FVector::DotProduct(LastVelocity.GetSafeNormal2D(), Intent) < -0.55f)
		{
			PivotTime = 0.32f;
			if (CVarLogMotion.GetValueOnGameThread() && Brawler->IsPlayerControlled())
			{
				UE_LOG(LogTemp, Log, TEXT("ANIM PIVOT entry speed %.0f"), LastVelocity.Size2D());
			}
		}
	}
	else { PivotTime = 0.f; }
	PivotLoad = FMath::Sin(PI * PivotTime / 0.32f);
	MotionLean = FMath::FInterpTo(MotionLean, LeanAtRun * FMath::SmoothStep(0.15f, 0.9f, Pace) * (1.f - PivotLoad), DeltaSeconds, 6.f);
}

void URRRandomBodyAnimInstance::UpdateAim(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Brawler || !Mesh || DeltaSeconds <= 0.f)
	{
		Aim.Weight = 0.f;
		Aim.GazeWeight = 0.f;
		return;
	}
	const bool bGaze = Brawler->IsAlive() && !Brawler->IsClimbing() && !Brawler->GetVehicle() && !Brawler->GetRobot()
		&& !Brawler->IsRidingCannon() && !Brawler->IsCelebrating() && !Brawler->IsDespairing() && !Brawler->IsEmoting() && !Brawler->IsUsingUltimate();
	const bool bAimGaze = bGaze && Brawler->WantsAimFacing();
	Aim.bCombatFacing = bAimGaze;
	const bool bMoving = bGaze && Brawler->GetAnimationMoveSpeed() > 10.f;
	const float ActorYaw = Brawler->GetActorRotation().Yaw;
	const FVector Location = Brawler->GetActorLocation();
	// (motion matching test: the matched clips strafe and turn by themselves, so the legs are not turned to the way it goes)
	const bool bMatched = MotionWeight > 0.5f;
	Aim.bSeparateBody = bGaze && Brawler->GetCharacterMovement()->IsMovingOnGround() && !Brawler->IsDashing() && !bMatched;
	if (!bHasBodyHeading || !Aim.bSeparateBody || FVector::DistSquared(Location, LastBodyLocation) > FMath::Square(300.f))
	{
		Aim.LowerYaw = Aim.UpperYaw = Aim.GazeYaw = ActorYaw;
		bHasBodyHeading = true;
	}
	LastBodyLocation = Location;
	if (Aim.bSeparateBody)
	{
		float LowerTarget = bMoving ? Brawler->GetVelocity().Rotation().Yaw : ActorYaw;
		// Side/back steps take over beyond a comfortable waist twist.
		if (bAimGaze) { LowerTarget = ActorYaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(ActorYaw, LowerTarget), -70.f, 70.f); }
		const auto FollowYaw = [DeltaSeconds](float From, float To, float Rate, float Ease)
		{
			const float Delta = FMath::FindDeltaAngleDegrees(From, To);
			return FRotator::NormalizeAxis(From + FMath::Clamp(Delta * FMath::Min(DeltaSeconds * Ease, 1.f), -Rate * DeltaSeconds, Rate * DeltaSeconds));
		};
		// (round 99: a look more than a quarter turn off the chest: the whole body comes round fast)
		const float WantLook = bAimGaze ? ActorYaw : bMoving ? Brawler->GetVelocity().Rotation().Yaw : Aim.UpperYaw;
		const bool bTurnBody = FMath::Abs(FMath::FindDeltaAngleDegrees(Aim.UpperYaw, WantLook)) > HeadTurnBodyAt;
		Aim.LowerYaw = FollowYaw(Aim.LowerYaw, LowerTarget, bTurnBody ? 900.f : 360.f, bTurnBody ? 22.f : 12.f);
		Aim.UpperYaw = FollowYaw(Aim.UpperYaw, bAimGaze ? ActorYaw : Aim.LowerYaw, bTurnBody ? 1080.f : 540.f, bTurnBody ? 26.f : 14.f);
		const float LookTarget = bAimGaze ? ActorYaw : bMoving ? Brawler->GetVelocity().Rotation().Yaw : Aim.UpperYaw;
		Aim.GazeYaw = FollowYaw(Aim.GazeYaw, LookTarget, 900.f, 22.f);
		// (and the head no further round than its limit from the chest, however far ahead of the body the look is)
		Aim.GazeYaw = FRotator::NormalizeAxis(Aim.UpperYaw + FMath::Clamp(FMath::FindDeltaAngleDegrees(Aim.UpperYaw, Aim.GazeYaw), -HeadTurnMax, HeadTurnMax));
	}
	// (matched: the aiming set never takes the legs, standing or moving)
	Aim.KeepLocomotion = FMath::FInterpConstantTo(Aim.KeepLocomotion, bMatched || (Aim.bSeparateBody && bMoving) ? 1.f : 0.f, DeltaSeconds, 8.f);
	Aim.GazeWeight = bAimGaze ? 1.f : FMath::FInterpConstantTo(Aim.GazeWeight, bMoving ? 1.f : 0.f, DeltaSeconds, 10.f);
	if (!Brawler->GetAimAnimations().IsComplete()) { Aim.Weight = 0.f; return; }
	const FRRAimAnimations& Animations = Brawler->GetAimAnimations();
	Aim.Animations = Animations;

	const bool bWanted = Brawler->WantsAimPose();
	Aim.Weight = FMath::FInterpConstantTo(Aim.Weight, bWanted ? 1.f : 0.f, DeltaSeconds,
		1.f / FMath::Max(bWanted ? AimInTime : AimOutTime, KINDA_SMALL_NUMBER));
	if (Aim.Weight <= 0.f)
	{
		Aim.FireWeight = 0.f;
	}

	// Which way it goes relative to where it faces: the runs are forward, backward, left, right. On a vehicle the legs
	// stand still.
	const FVector Velocity = Brawler->GetVehicle() ? FVector::ZeroVector : Brawler->GetVelocity();
	const float Speed = Velocity.Size2D();
	const float LegYaw = Aim.bSeparateBody ? Aim.LowerYaw : ActorYaw;
	const float Ahead = FVector::DotProduct(Velocity, FRotator(0.f, LegYaw, 0.f).Vector());
	const float Side = FVector::DotProduct(Velocity, FRotator(0.f, LegYaw + 90.f, 0.f).Vector());
	const float LegAngle = FMath::Abs(FMath::RadiansToDegrees(FMath::Atan2(Side, Ahead)));
	Aim.StrafeWeight = FMath::FInterpConstantTo(Aim.StrafeWeight,
		bMatched ? 0.f : Aim.KeepLocomotion * FMath::SmoothStep(25.f, 65.f, LegAngle), DeltaSeconds, 6.f);
	// (in flight no running legs: the standing aim over the flying pose)
	Aim.MoveWeight = FMath::FInterpTo(Aim.MoveWeight, Brawler->IsInFlight() || Brawler->IsJumping() ? 0.f : FMath::Clamp(Speed / FMath::Max(AimRunSpeed, 1.f), 0.f, 1.f), DeltaSeconds, MoveBlendSpeed);
	if (Speed > MinStrafeSpeed)
	{
		// 0 ahead, 90 right, -90 left, 180 behind; between two runs, a share of each
		const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Side, Ahead));
		const float Off = FMath::Abs(Angle);
		TargetMoveWeights[0] = FMath::Max(0.f, 1.f - Off / 90.f);
		TargetMoveWeights[1] = FMath::Max(0.f, (Off - 90.f) / 90.f);
		TargetMoveWeights[2] = Angle < 0.f ? 1.f - FMath::Abs(Off - 90.f) / 90.f : 0.f;
		TargetMoveWeights[3] = Angle >= 0.f ? 1.f - FMath::Abs(Off - 90.f) / 90.f : 0.f;
	}
	float Total = 0.f;
	for (int32 Direction = 0; Direction < 4; ++Direction)
	{
		Aim.MoveWeights[Direction] = FMath::FInterpConstantTo(Aim.MoveWeights[Direction], TargetMoveWeights[Direction], DeltaSeconds, DirectionBlendSpeed);
		Total += Aim.MoveWeights[Direction];
	}
	// The runs go in step (all the same length): one phase, at a rate that keeps the feet from sliding
	float RunSpeed = 0.f;
	float RunLength = 0.f;
	for (int32 Direction = 0; Direction < 4; ++Direction)
	{
		Aim.MoveWeights[Direction] = Total > KINDA_SMALL_NUMBER ? Aim.MoveWeights[Direction] / Total : (Direction == 0 ? 1.f : 0.f);
		RunSpeed += Aim.MoveWeights[Direction] * Animations.MoveSpeed[Direction];
		RunLength += Aim.MoveWeights[Direction] * Animations.Move[Direction]->GetPlayLength();
	}
	// A giant's stride is longer
	RunSpeed *= Brawler->GetActorScale3D().Z;
	const float Rate = RunSpeed > 0.f ? FMath::Clamp(Speed / RunSpeed, RunRateRange.X, RunRateRange.Y) : 1.f;
	// Enter directional steps at the current walking phase; firing alone never restarts the feet.
	Aim.RunPhase = Aim.StrafeWeight <= 0.001f && Aim.KeepLocomotion > 0.f
		? FMath::Fmod(GetCurrentTime() / FMath::Max(GetLength(), KINDA_SMALL_NUMBER), 1.f)
		: FMath::Fmod(Aim.RunPhase + DeltaSeconds * Rate / FMath::Max(RunLength, KINDA_SMALL_NUMBER), 1.f);

	// Standing: the fire animation from each shot, then the aim
	const float SinceShot = Brawler->GetTimeSinceShot();
	const float IdleLength = FMath::Max(Animations.Idle->GetPlayLength(), KINDA_SMALL_NUMBER);
	Aim.IdleTime = FMath::Fmod(Aim.IdleTime + DeltaSeconds, IdleLength);
	Aim.FireTime = FMath::Clamp(SinceShot, 0.f, Animations.Fire->GetPlayLength());
	Aim.FireWeight = FMath::FInterpTo(Aim.FireWeight, SinceShot < FireHold ? 1.f : 0.f, DeltaSeconds, FireBlendSpeed);

	// On the run the chest kicks with each shot (the standing fire animation has its own recoil): up fast, down slower
	const float Kick = FMath::Clamp(SinceShot / FMath::Max(KickTime, KINDA_SMALL_NUMBER), 0.f, 1.f);
	Aim.Kick = SinceShot < KickTime ? KickAngle * (Kick < 0.2f ? Kick / 0.2f : FMath::Square((1.f - Kick) / 0.8f)) * Aim.MoveWeight : 0.f;

	// Where the gun should point: ahead, level; in third person where the player aims, up or down too (the pawn's aim
	// rotation: the camera's on its own machine and the server, the replicated view pitch elsewhere), within reach of a spine
	FVector Look = Brawler->GetActorForwardVector();
	if (Brawler->IsThirdPerson())
	{
		FRotator AimRotation = Brawler->GetBaseAimRotation();
		AimRotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(AimRotation.Pitch), -MaxAimPitch, MaxAimPitch);
		Look = FRotator(AimRotation.Pitch, Brawler->GetActorRotation().Yaw, 0.f).Vector();
	}
	Aim.Facing = Mesh->GetComponentTransform().InverseTransformVectorNoScale(Look);
}

void FRRRandomBodyAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimSingleNodeInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const URRRandomBodyAnimInstance* Instance = CastChecked<URRRandomBodyAnimInstance>(InAnimInstance);
	TailBones = Instance->GetTailBones();
	TailSwing = Instance->GetTailSwing();
	TailLift = Instance->GetTailLift();
	TailSize = Instance->GetTailSize();
	bTailFloor = Instance->GetTailFloor(TailFloorPoint, TailFloorNormal, TailFloorMargin);
	EarBones = Instance->GetEarBones();
	EarBends = Instance->GetEarBends();
	EarHead = Instance->GetEarHead();
	EarHeadRest = Instance->GetEarHeadRest();
	FlightBones = Instance->GetFlightBones();
	FlightBends = Instance->GetFlightBends();
	JumpBones = Instance->GetJumpBones();
	JumpBends = Instance->GetJumpBends();
	SprintBones = Instance->GetSprintBones();
	SprintBends = Instance->GetSprintBends();
	RideBones = Instance->GetRideBones();
	RideBends = Instance->GetRideBends();
	WingBones = Instance->GetWingBones();
	WingBends = Instance->GetWingBends();
	Aim = Instance->GetAimFrame();
	Melee = Instance->GetMeleeFrame();
	GunHands = Instance->GetGunHandsFrame();
	MotionWeight = Instance->GetMotionWeight();
	MotionLean = Instance->GetMotionLean();
	PivotLoad = Instance->GetPivotLoad();
	MotionWholeWeight = Instance->GetMotionWholeWeight();
	if (MotionWholeWeight > 0.f)
	{
		MotionWholePose = Instance->GetMotionWholePose();
	}
	if (MotionWeight > 0.f)
	{
		MotionPose = Instance->GetMotionPose();
		MotionMask = Instance->GetMotionMask();
	}
}

void FRRRandomBodyAnimInstanceProxy::EvaluateMotion(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		const int32 MeshBone = Bones.MakeMeshPoseIndex(Index).GetInt();
		if (MotionMask.IsValidIndex(MeshBone) && MotionMask[MeshBone] && MotionPose.IsValidIndex(MeshBone))
		{
			if (MotionMask[MeshBone] == 1) { Output.Pose[Index].BlendWith(MotionPose[MeshBone], MotionWeight); }
			// (a climbing clip over that, on the arms as well)
			if (MotionWholeWeight > 0.f && MotionWholePose.IsValidIndex(MeshBone)) { Output.Pose[Index].BlendWith(MotionWholePose[MeshBone], MotionWholeWeight); }
		}
	}
	Output.Pose.NormalizeRotations();
	if (PivotLoad > KINDA_SMALL_NUMBER)
	{
		const FCompactPoseBoneIndex Hips = FindBone(Bones, TEXT("Hips"));
		const FCompactPoseBoneIndex Feet[] = {FindBone(Bones, TEXT("LeftFoot")), FindBone(Bones, TEXT("RightFoot"))};
		if (Hips.IsValid() && Feet[0].IsValid() && Feet[1].IsValid())
		{
			const FVector Targets[] = {GetMeshSpaceTransform(Output.Pose, Feet[0]).GetLocation(), GetMeshSpaceTransform(Output.Pose, Feet[1]).GetLocation()};
			const auto Parent = Output.Pose.GetParentBoneIndex(Hips);
			const FVector Dip(0, 0, -5.f * PivotLoad * MotionWeight);
			Output.Pose[Hips].AddToTranslation(Parent.IsValid() ? GetMeshSpaceTransform(Output.Pose, Parent).InverseTransformVector(Dip) : Dip);
			for (int32 I = 0; I < 2; ++I)
			{
				const auto Knee = Output.Pose.GetParentBoneIndex(Feet[I]);
				if (Knee.IsValid()) { Reach(Output.Pose, Feet[I], Targets[I], GetMeshSpaceTransform(Output.Pose, Knee).GetLocation() + FVector(0, 12, 0)); }
			}
		}
	}
	if (MotionLean > KINDA_SMALL_NUMBER)
	{
		// Forward about the mesh's side axis (+X is its left, +Y its front; a negative turn tips the front down), spread up
		// the spine like the aim's turn; the neck and head lift most of it back so it keeps looking where it runs
		const float Lean = -FMath::DegreesToRadians(MotionLean * MotionWeight);
		for (int32 Bone = 0; Bone < UE_ARRAY_COUNT(SpineBones); ++Bone)
		{
			const FCompactPoseBoneIndex Spine = FindBone(Bones, SpineBones[Bone]);
			if (Spine.IsValid()) { TurnBone(Output.Pose, Spine, FQuat(FVector::ForwardVector, Lean * SpineShares[Bone])); }
		}
		for (const TCHAR* Name : { TEXT("Neck"), TEXT("Head") })
		{
			const FCompactPoseBoneIndex Bone = FindBone(Bones, Name);
			if (Bone.IsValid()) { TurnBone(Output.Pose, Bone, FQuat(FVector::ForwardVector, -Lean * MotionHeadLift * 0.5f)); }
		}
	}
}

void FRRRandomBodyAnimInstanceProxy::PreEvaluateAnimation(UAnimInstance* InAnimInstance)
{
	FAnimSingleNodeInstanceProxy::PreEvaluateAnimation(InAnimInstance);
	// Root-motion sources tick animation before CharacterMovement translates the
	// capsule. Copy the final transform here on the game thread; using Update's
	// transform leaves the planted feet one movement frame ahead of their anchors.
	Melee.MeshToWorld = InAnimInstance->GetSkelMeshComponent()->GetComponentTransform();
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(InAnimInstance->GetOwningActor());
	bRightStrafe = Brawler && MotionWeight > 0.85f
		&& Brawler->WantsMotionMatch() && Brawler->WantsAimFacing() && Brawler->GetVelocity().Size2D() > 85.f
		&& FVector::DotProduct(Brawler->GetVelocity().GetSafeNormal2D(), Brawler->GetActorRightVector()) > 0.8f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		FVector Goal;
		GripWeights[Side] = Brawler ? Brawler->GetLedgeGrip(Side, Goal) : 0.f;
		if (GripWeights[Side] > 0.f) { GripGoals[Side] = Melee.MeshToWorld.InverseTransformPosition(Goal); }
	}
	if (Brawler && Aim.bSeparateBody)
	{
		const float MeshYaw = Melee.MeshToWorld.TransformVectorNoScale(FVector::RightVector).Rotation().Yaw;
		Aim.LowerTurn = FMath::FindDeltaAngleDegrees(MeshYaw, Aim.LowerYaw);
		Aim.UpperTurn = FMath::FindDeltaAngleDegrees(Brawler->GetActorRotation().Yaw, Aim.UpperYaw);
	}
	if (Brawler && Aim.GazeWeight > 0.f)
	{
		const float BodyYaw = Brawler->GetActorRotation().Yaw;
		float Yaw = Aim.bSeparateBody ? Aim.GazeYaw : BodyYaw, Pitch = 0.f;
		if (Brawler->WantsAimFacing())
		{
			if (Brawler->IsThirdPerson()) { Pitch = FMath::Clamp(FRotator::NormalizeAxis(Brawler->GetBaseAimRotation().Pitch), -40.f, 40.f); }
		}
		else if (!Aim.bSeparateBody && Brawler->GetAnimationMoveSpeed() > 10.f)
		{
			// Lead the body into a turn without twisting the neck backwards.
			Yaw += FMath::Clamp(FMath::FindDeltaAngleDegrees(BodyYaw, Brawler->GetVelocity().Rotation().Yaw), -80.f, 80.f);
		}
		Aim.GazeFacing = Melee.MeshToWorld.InverseTransformVectorNoScale(FRotator(Pitch, Yaw, 0.f).Vector()).GetSafeNormal();
	}
	if (Aim.ListeningWeight > 0.f)
	{
		Aim.GazeFacing = Melee.MeshToWorld.InverseTransformVectorNoScale(Aim.ListeningDirection);
		Aim.GazeWeight = Aim.ListeningWeight;
	}
	const UPrimitiveComponent* Support = Brawler ? Cast<UPrimitiveComponent>(Brawler->GetMovementBaseObject()) : nullptr;
	Melee.SupportToWorld = Support ? Support->GetComponentTransform() : FTransform::Identity;
	Melee.SupportId = Support ? Support->GetUniqueID() : 0;
	Melee.FloorNormal = Brawler ? Brawler->GetCharacterMovement()->CurrentFloor.HitResult.ImpactNormal.GetSafeNormal(SMALL_NUMBER, FVector::UpVector) : FVector::UpVector;
}

void FRRRandomBodyAnimInstanceProxy::EvaluateAim(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	FCompactPose Blended;
	Blended.SetBoneContainer(&Bones);
	FCompactPose Pose;
	Pose.SetBoneContainer(&Bones);
	FBlendedCurve Curve;
	UE::Anim::FStackAttributeContainer Attributes;
	bool bFirst = true;
	const auto Add = [&](const UAnimSequence* Sequence, float Time, float Weight)
	{
		if (Weight <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		Curve.InitFrom(Output.Curve);
		Attributes.Empty();
		FAnimationPoseData Data(Pose, Curve, Attributes);
		Sequence->GetAnimationPose(Data, FAnimExtractContext(double(Time), false));
		for (const FCompactPoseBoneIndex Index : Pose.ForEachBoneIndex())
		{
			if (bFirst)
			{
				Blended[Index] = Pose[Index] * ScalarRegister(Weight);
			}
			else
			{
				Blended[Index].AccumulateWithShortestRotation(Pose[Index], ScalarRegister(Weight));
			}
		}
		bFirst = false;
	};

	const FRRAimAnimations& Animations = Aim.Animations;
	const float Stand = 1.f - Aim.MoveWeight;
	Add(Animations.Idle, Aim.IdleTime, Stand * (1.f - Aim.FireWeight));
	Add(Animations.Fire, Aim.FireTime, Stand * Aim.FireWeight);
	for (int32 Direction = 0; Direction < 4; ++Direction)
	{
		Add(Animations.Move[Direction], Aim.RunPhase * Animations.Move[Direction]->GetPlayLength(), Aim.MoveWeight * Aim.MoveWeights[Direction]);
	}
	if (bFirst)
	{
		return;
	}
	Blended.NormalizeRotations();
	const FCompactPoseBoneIndex Spine = FindBone(Bones, TEXT("Spine"));
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		bool bUpper = false;
		for (FCompactPoseBoneIndex Walk = Index; Walk.IsValid(); Walk = Output.Pose.GetParentBoneIndex(Walk))
		{
			if (Walk == Spine) { bUpper = true; break; }
		}
		Output.Pose[Index].BlendWith(Blended[Index], Aim.Weight * (bUpper ? 1.f : 1.f - Aim.KeepLocomotion));
	}
	Output.Pose.NormalizeRotations();
}

void FRRRandomBodyAnimInstanceProxy::PointGun(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex RightHand = FindBone(Bones, RightHandBone);
	const FCompactPoseBoneIndex RightPalm = FindBone(Bones, RightPalmBone);
	const FCompactPoseBoneIndex LeftHand = FindBone(Bones, LeftHandBone);
	FCompactPoseBoneIndex Spine[UE_ARRAY_COUNT(SpineBones)];
	for (int32 Bone = 0; Bone < UE_ARRAY_COUNT(SpineBones); ++Bone)
	{
		Spine[Bone] = FindBone(Bones, SpineBones[Bone]);
		if (!Spine[Bone].IsValid())
		{
			return;
		}
	}
	const FVector Facing = FVector(Aim.Facing.X, Aim.Facing.Y, 0.f).GetSafeNormal();
	if (!RightHand.IsValid() || !RightPalm.IsValid() || !LeftHand.IsValid() || Facing.IsNearlyZero())
	{
		return;
	}
	const FVector Palm = (GetMeshSpaceTransform(Output.Pose, RightHand).GetLocation() + GetMeshSpaceTransform(Output.Pose, RightPalm).GetLocation()) * 0.5f;
	const FVector Gun = (GetMeshSpaceTransform(Output.Pose, LeftHand).GetLocation() - Palm).GetSafeNormal();
	if (Gun.IsNearlyZero())
	{
		return;
	}

	// Turned about the mesh's up axis, spread up the spine, until the gun lies over the facing; then tilted level at the
	// chest (about the axis across the facing: positive tips the front up), plus the shot's kick back
	const float Yaw = FMath::FindDeltaAngleRadians(FMath::Atan2(Gun.Y, Gun.X), FMath::Atan2(Facing.Y, Facing.X)) * Aim.Weight;
	// (Facing's own tilt is zero from above, the aim's pitch in third person)
	const float Elevation = FMath::Asin(FMath::Clamp(Aim.Facing.GetSafeNormal().Z, -1.f, 1.f));
	const float Pitch = (Elevation - FMath::Asin(FMath::Clamp(Gun.Z, -1.f, 1.f))) * Aim.Weight + FMath::DegreesToRadians(Aim.Kick) * Aim.Weight;
	for (int32 Bone = 0; Bone < UE_ARRAY_COUNT(SpineBones); ++Bone)
	{
		TurnBone(Output.Pose, Spine[Bone], FQuat(FVector::UpVector, Yaw * SpineShares[Bone]));
	}
	TurnBone(Output.Pose, Spine[UE_ARRAY_COUNT(SpineBones) - 1], FQuat(FVector::CrossProduct(Facing, FVector::UpVector), Pitch));
}

void URRRandomBodyAnimInstance::UpdateGunHands(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	const URRRandomGunComponent* Gun = Brawler ? Brawler->GetGun() : nullptr;
	FRRGunSupportHold Hold;
	// (not while a climbing clip has the whole body: its hands are on the ledge)
	const bool bWanted = Gun && CVarGunTwoHands.GetValueOnGameThread() != 0 && MotionWholeWeight <= 0.f && Gun->GetSupportHold(Hold);
	if (bWanted)
	{
		GunHands.Hold = Hold;
	}
	GunHands.Weight = DeltaSeconds > 0.f ? FMath::FInterpConstantTo(GunHands.Weight, bWanted ? 1.f : 0.f, DeltaSeconds, 1.f / 0.15f) : (bWanted ? 1.f : 0.f);
}

void FRRRandomBodyAnimInstanceProxy::EvaluateGunHands(FPoseContext& Output) const
{
	// 2026-11-20 (the user: "총기류에 손 위치를 어디에 둬야 적당한지 파악해서 체크해두고 모션 매칭을 사용해서 해당 위치에 손을 맞쳐줘.
	// 지금은 공중에 뜨거나 허공에 손을 올리는 장면이 많이 나온다"): the toy guns sit in the right fist like in a socket, but the
	// left hand kept wherever its animation had it (the Mixamo rifle poses and the matched clips alike): out in front where a
	// rifle's barrel would be, open, holding nothing. Here, after every layer that moves the arms, the gun is rebuilt from the
	// posed right hand exactly as URRRandomGunComponent places it, and the left arm is bent (two bones, elbow out and down)
	// so the left fist sits on the gun's support point, turned like the right fist mirrored, its fingers curled like the
	// right hand's.
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex RightHand = FindBone(Bones, RightHandBone);
	const FCompactPoseBoneIndex RightPalm = FindBone(Bones, RightPalmBone);
	const FCompactPoseBoneIndex Shoulder = FindBone(Bones, TEXT("LeftShoulder"));
	const FCompactPoseBoneIndex Upper = FindBone(Bones, TEXT("LeftArm"));
	const FCompactPoseBoneIndex Fore = FindBone(Bones, TEXT("LeftForeArm"));
	const FCompactPoseBoneIndex Hand = FindBone(Bones, LeftHandBone);
	const FCompactPoseBoneIndex LeftIndex = FindBone(Bones, TEXT("LeftHandIndex1"));
	const FCompactPoseBoneIndex Chest = FindBone(Bones, TEXT("Spine2"));
	if (!RightHand.IsValid() || !RightPalm.IsValid() || !Shoulder.IsValid() || !Upper.IsValid() || !Fore.IsValid() || !Hand.IsValid()
		|| !LeftIndex.IsValid() || !Chest.IsValid())
	{
		return;
	}
	const FRRGunSupportHold& Hold = GunHands.Hold;
	// World centimetres per mesh unit (the mesh's own scale times a giant's); the gun's lengths are in the world's
	const float MeshScale = FMath::Max(static_cast<float>(Melee.MeshToWorld.GetScale3D().Z), KINDA_SMALL_NUMBER);
	const float Unit = Hold.Length / 100.f * Hold.Size / MeshScale;
	const float Fist = Hold.Size / MeshScale;

	FCompactPose Reference;
	Reference.SetBoneContainer(&Bones);
	Reference.ResetToRefPose();

	// The gun, from the right hand (URRRandomGunComponent::TickComponent, in the mesh's space)
	const FTransform RightNow = GetMeshSpaceTransform(Output.Pose, RightHand);
	const FVector Palm = (RightNow.GetLocation() + GetMeshSpaceTransform(Output.Pose, RightPalm).GetLocation()) * 0.5f;
	const FQuat RightFist = RightNow.GetRotation() * Hold.FixTurn;
	const FQuat GunRotation = RightFist * Hold.GripTurn;
	const FVector GunOrigin = Palm + RightFist.RotateVector(Hold.FixOffset) * Fist - GunRotation.RotateVector(Hold.Grip) * Unit;
	const FVector FistCenter = GunOrigin + GunRotation.RotateVector(Hold.Support) * Unit;

	// The left fist: the right one's hold mirrored across the body (mesh X is the side axis), as the light sword's grip
	// finds it from the reference pose; its frame is the gun's (muzzle ahead, top up), turned by SupportTurn
	const auto Mirror = [](FVector V) { V.X = -V.X; return V; };
	const FQuat RightRestFist = GetMeshSpaceTransform(Reference, RightHand).GetRotation() * Hold.FixTurn;
	const FQuat LeftRestFist = FRotationMatrix::MakeFromXZ(Mirror(RightRestFist.GetAxisX()), Mirror(RightRestFist.GetAxisZ())).ToQuat();
	const FQuat LeftRestHand = GetMeshSpaceTransform(Reference, Hand).GetRotation();
	const FQuat LeftFix = LeftRestHand.Inverse() * LeftRestFist;
	const FQuat LeftFist = (GunRotation * Hold.SupportTurn).GetNormalized();
	const FQuat HandRotation = (LeftFist * LeftFix.Inverse()).GetNormalized();
	// (the fist's own offset from the palm, mirrored: across the fist, Y flips)
	const FVector LeftPalm = FistCenter - LeftFist.RotateVector(FVector(Hold.FixOffset.X, -Hold.FixOffset.Y, Hold.FixOffset.Z)) * Fist;
	const FVector Wrist = LeftPalm - HandRotation.RotateVector(Reference[LeftIndex].GetTranslation() * 0.5f);

	// Kept to blend back by the weight: the collarbone down to the fingertips
	TArray<FCompactPoseBoneIndex, TInlineAllocator<24>> ArmBones;
	TArray<FTransform, TInlineAllocator<24>> Before;
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		for (FCompactPoseBoneIndex Walk = Index; Walk.IsValid(); Walk = Output.Pose.GetParentBoneIndex(Walk))
		{
			if (Walk == Shoulder)
			{
				ArmBones.Add(Index);
				Before.Add(Output.Pose[Index]);
				break;
			}
		}
	}

	const FQuat ChestTurn = (GetMeshSpaceTransform(Output.Pose, Chest).GetRotation() * GetMeshSpaceTransform(Reference, Chest).GetRotation().Inverse()).GetNormalized();
	const float UpperLength = Output.Pose[Fore].GetTranslation().Size();
	const float ForeLength = Output.Pose[Hand].GetTranslation().Size();
	const float Reach = (UpperLength + ForeLength) * 0.995f;
	// Out of reach (short arms, a gun held out on the right): the collarbone swings the shoulder toward it first, up to
	// ShoulderReach degrees
	FVector Root = GetMeshSpaceTransform(Output.Pose, Upper).GetLocation();
	const float Short = static_cast<float>((Wrist - Root).Size()) - Reach;
	if (Short > 0.f)
	{
		const FVector Pivot = GetMeshSpaceTransform(Output.Pose, Shoulder).GetLocation();
		const FVector Arm = Root - Pivot;
		const FVector Axis = FVector::CrossProduct(Arm, Wrist - Pivot).GetSafeNormal();
		if (!Axis.IsNearlyZero() && Arm.Size() > KINDA_SMALL_NUMBER)
		{
			const float Angle = FMath::Min(Short / static_cast<float>(Arm.Size()), FMath::DegreesToRadians(GunShoulderReach));
			TurnBone(Output.Pose, Shoulder, FQuat(Axis, Angle));
			Root = GetMeshSpaceTransform(Output.Pose, Upper).GetLocation();
		}
	}
	const FVector ToWrist = Wrist - Root;
	const FVector Direction = ToWrist.GetSafeNormal();
	const float Distance = FMath::Clamp(static_cast<float>(ToWrist.Size()), 0.1f, Reach);
	const float Along = (UpperLength * UpperLength - ForeLength * ForeLength + Distance * Distance) / (2.f * Distance);
	// The elbow out to the left, down, a little back, turning with the chest
	const FVector Pole = ChestTurn.RotateVector(FVector(1.f, -0.25f, -1.2f));
	FVector Bend = Pole - Direction * FVector::DotProduct(Pole, Direction);
	Bend = Bend.IsNearlyZero() ? ChestTurn.RotateVector(FVector(0.f, 0.f, -1.f)) : Bend.GetSafeNormal();
	const FVector Elbow = Root + Direction * Along + Bend * FMath::Sqrt(FMath::Max(0.f, UpperLength * UpperLength - Along * Along));
	const FVector OldUpper = GetMeshSpaceTransform(Output.Pose, Fore).GetLocation() - Root;
	TurnBone(Output.Pose, Upper, FQuat::FindBetweenNormals(OldUpper.GetSafeNormal(), (Elbow - Root).GetSafeNormal()));
	const FVector ElbowNow = GetMeshSpaceTransform(Output.Pose, Fore).GetLocation();
	const FVector OldFore = GetMeshSpaceTransform(Output.Pose, Hand).GetLocation() - ElbowNow;
	TurnBone(Output.Pose, Fore, FQuat::FindBetweenNormals(OldFore.GetSafeNormal(), (Wrist - ElbowNow).GetSafeNormal()));
	const FQuat ForeNow = GetMeshSpaceTransform(Output.Pose, Fore).GetRotation();
	Output.Pose[Hand].SetRotation((ForeNow.Inverse() * HandRotation).GetNormalized());

	// The fingers curled round the gun like the right hand's: each right finger's turn in its fist, mirrored into the left
	// fist (across the fist its Y flips: a rotation (x, y, z, w) becomes (-x, y, -z, w)), with the left bone's own axes
	// taken from the reference pose. Parents come first in the pose, so each finger's parent is already set.
	const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
	const auto MirrorTurn = [](const FQuat& Q) { return FQuat(-Q.X, Q.Y, -Q.Z, Q.W); };
	const FQuat RightRestHand = GetMeshSpaceTransform(Reference, RightHand).GetRotation();
	const FQuat LeftFistNow = GetMeshSpaceTransform(Output.Pose, Hand).GetRotation() * LeftFix;
	if (CVarGunFingers.GetValueOnAnyThread() != 0)
	{
		for (const FCompactPoseBoneIndex Index : ArmBones)
		{
			const FString Name = RefSkeleton.GetBoneName(Bones.MakeMeshPoseIndex(Index).GetInt()).ToString();
			if (!Name.StartsWith(TEXT("LeftHand")) || Name.Len() <= 8)
			{
				continue;
			}
			const FCompactPoseBoneIndex Twin = FindBone(Bones, FName(TEXT("RightHand") + Name.Mid(8)));
			if (!Twin.IsValid())
			{
				continue;
			}
			const FQuat RestTurn = (RightRestHand * Hold.FixTurn).Inverse() * GetMeshSpaceTransform(Reference, Twin).GetRotation();
			const FQuat NowTurn = RightFist.Inverse() * GetMeshSpaceTransform(Output.Pose, Twin).GetRotation();
			const FQuat Correction = MirrorTurn(RestTurn).Inverse() * LeftRestFist.Inverse() * GetMeshSpaceTransform(Reference, Index).GetRotation();
			const FQuat Wanted = LeftFistNow * MirrorTurn(NowTurn) * Correction;
			const FQuat Parent = GetMeshSpaceTransform(Output.Pose, Output.Pose.GetParentBoneIndex(Index)).GetRotation();
			Output.Pose[Index].SetRotation((Parent.Inverse() * Wanted).GetNormalized());
		}
	}

	if (CVarGunHandsLog.GetValueOnAnyThread() != 0 && (GFrameCounter % 30) == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("GUNHANDS weight %.2f wrist off target %.2f reach %.1f short %.1f (mesh units) | right palm to left palm %.1f cm"), GunHands.Weight,
			FVector::Dist(GetMeshSpaceTransform(Output.Pose, Hand).GetLocation(), Wrist), Reach, Short, FVector::Dist(Palm, LeftPalm) * MeshScale);
	}
	for (int32 I = 0; I < ArmBones.Num(); ++I)
	{
		Output.Pose[ArmBones[I]].Blend(Before[I], Output.Pose[ArmBones[I]], GunHands.Weight);
	}
}

bool FRRRandomBodyAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	FAnimSingleNodeInstanceProxy::Evaluate(Output);
	// Motion matching test: the driver's legs, hips, spine and head under everything else
	if (MotionWeight > 0.f) { EvaluateMotion(Output); }
	if (Aim.StrafeWeight > 0.f && Aim.Animations.IsComplete()) { EvaluateMovingLegs(Output); }

	if (Aim.Weight > 0.f && Aim.Animations.IsComplete())
	{
		EvaluateAim(Output);
		PointGun(Output);
	}
	// The light sword's slash and guard over the upper body (round 97)
	UpdateSwordContacts(Output);
	if (Melee.SlashWeight > 0.f || Melee.GuardWeight > 0.f || ContactWeight > 0.f || (Melee.Carry && Melee.CarryWeight > 0.f))
	{
		EvaluateMelee(Output);
	}

	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
	const int32 Count = FMath::Min3(TailBones.Num(), TailSwing.Num(), TailLift.Num());
	float TailHeading = 0.f;
	for (int32 Bone = 0; Bone < Count; ++Bone)
	{
		const int32 MeshIndex = RefSkeleton.FindBoneIndex(TailBones[Bone]);
		if (MeshIndex == INDEX_NONE)
		{
			break;
		}
		const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
		if (!Index.IsValid())
		{
			break;
		}

		// The parent's rotation in mesh space, bends on the bones above included
		FQuat ParentRotation = FQuat::Identity;
		for (FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Index); Parent.IsValid(); Parent = Output.Pose.GetParentBoneIndex(Parent))
		{
			ParentRotation = Output.Pose[Parent].GetRotation() * ParentRotation;
		}

		// The tail points down the mesh's -Y: swinging right turns it about -Z, lifting it about -X - the bone's own
		// -X, turned by the swing of the chain down to it (2026-11-24: a tail curled round to a side lifts its end up,
		// not over; with the mesh's X the far bones of a curl only rolled)
		TailHeading += TailSwing[Bone];
		const FQuat Swung = FQuat(FVector::UpVector, FMath::DegreesToRadians(-TailSwing[Bone]));
		const FVector SideAxis = FQuat(FVector::UpVector, FMath::DegreesToRadians(-TailHeading)).RotateVector(FVector::ForwardVector);
		const FQuat Bend = FQuat(SideAxis, FMath::DegreesToRadians(-TailLift[Bone])) * Swung;
		FTransform& Local = Output.Pose[Index];
		Local.SetRotation((ParentRotation.Inverse() * Bend * ParentRotation * Local.GetRotation()).GetNormalized());
	}

	// The tail's size (FRRSpeciesLook::TailSize: the tiger's 0.7, 2026-11-25): its first bone scaled, the rest with it
	TArray<FCompactPoseBoneIndex, TInlineAllocator<16>> Tail;
	for (int32 Bone = 0; Bone < Count; ++Bone)
	{
		const int32 MeshIndex = RefSkeleton.FindBoneIndex(TailBones[Bone]);
		const FCompactPoseBoneIndex Index = MeshIndex != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)) : FCompactPoseBoneIndex(INDEX_NONE);
		if (!Index.IsValid())
		{
			break;
		}
		Tail.Add(Index);
	}
	if (Tail.Num() > 0 && !FMath::IsNearlyEqual(TailSize, 1.f))
	{
		Output.Pose[Tail[0]].SetScale3D(Output.Pose[Tail[0]].GetScale3D() * TailSize);
	}
	// No joint under the floor (2026-11-25, the user: "바닥을 뚫고 안 보이는 부분은 없게"): root to tip, a bone whose next joint
	// (the last one's end: one more of its own length on) comes nearer the floor than the margin is turned up toward the
	// floor's normal just enough; the bones after it go with it, then are looked at in turn. The springs don't know: when
	// the tail swings back up it simply leaves the floor.
	if (bTailFloor && Tail.Num() > 0)
	{
		const FVector Up = TailFloorNormal;
		for (int32 Bone = 0; Bone < Tail.Num(); ++Bone)
		{
			const FTransform Here = GetMeshSpaceTransform(Output.Pose, Tail[Bone]);
			const FVector Joint = Here.GetLocation();
			const FVector Next = Bone + 1 < Tail.Num() ? GetMeshSpaceTransform(Output.Pose, Tail[Bone + 1]).GetLocation()
				: Here.TransformPosition(Output.Pose[Tail[Bone]].GetTranslation());
			const FVector Along = Next - Joint;
			const float Length = static_cast<float>(Along.Size());
			if (Length < KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const FVector Direction = Along / Length;
			const float Height = static_cast<float>(FVector::DotProduct(Joint - TailFloorPoint, Up));
			const float Wanted = FMath::Asin(FMath::Clamp((TailFloorMargin - Height) / Length, -1.f, 1.f));
			const float Now = FMath::Asin(FMath::Clamp(static_cast<float>(FVector::DotProduct(Direction, Up)), -1.f, 1.f));
			const FVector Axis = FVector::CrossProduct(Direction, Up).GetSafeNormal();
			if (Now < Wanted && !Axis.IsNearlyZero())
			{
				TurnBone(Output.Pose, Tail[Bone], FQuat(Axis, Wanted - Now));
			}
		}
	}

	// The ears (2026-11-25): each bone's turn was worked out in the reference pose's mesh space; turned with the head as it
	// stands now, then put on the bone like the tail's (the head's later turns - gaze, facing - carry the ears along)
	const int32 EarHeadMesh = EarBones.Num() > 0 && EarBends.Num() == EarBones.Num() ? RefSkeleton.FindBoneIndex(EarHead) : INDEX_NONE;
	const FCompactPoseBoneIndex EarHeadIndex = EarHeadMesh != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(EarHeadMesh)) : FCompactPoseBoneIndex(INDEX_NONE);
	if (EarHeadIndex.IsValid())
	{
		FQuat HeadNow = FQuat::Identity;
		for (FCompactPoseBoneIndex Up = EarHeadIndex; Up.IsValid(); Up = Output.Pose.GetParentBoneIndex(Up))
		{
			HeadNow = Output.Pose[Up].GetRotation() * HeadNow;
		}
		const FQuat Turned = HeadNow * EarHeadRest.Inverse();
		for (int32 Bone = 0; Bone < EarBones.Num(); ++Bone)
		{
			const int32 MeshIndex = RefSkeleton.FindBoneIndex(EarBones[Bone]);
			const FCompactPoseBoneIndex Index = MeshIndex != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex)) : FCompactPoseBoneIndex(INDEX_NONE);
			if (!Index.IsValid())
			{
				continue;
			}
			FQuat ParentRotation = FQuat::Identity;
			for (FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Index); Parent.IsValid(); Parent = Output.Pose.GetParentBoneIndex(Parent))
			{
				ParentRotation = Output.Pose[Parent].GetRotation() * ParentRotation;
			}
			const FQuat Bend = Turned * EarBends[Bone] * Turned.Inverse();
			FTransform& Local = Output.Pose[Index];
			Local.SetRotation((ParentRotation.Inverse() * Bend * ParentRotation * Local.GetRotation()).GetNormalized());
		}
	}

	// The flying or jumping pose, then the wings (roots before tips, so a tip's parent is already turned)
	TurnBones(Output, FlightBones, FlightBends);
	TurnBones(Output, JumpBones, JumpBends);
	TurnBones(Output, SprintBones, SprintBends);
	TurnBones(Output, RideBones, RideBends);
	TurnBones(Output, WingBones, WingBends);
	// Solve last so locomotion/jump arm layers cannot pull either hand off the hilt.
	if (Melee.GripWeight > 0.f)
	{
		EvaluateSwordGrip(Output);
	}
	if (Aim.bSeparateBody) { EvaluateBodyFacing(Output); }
	else { bHasUpperHistory = bWasCombatFacing = bEaseUpperEntry = false; }
	// The left hand onto the gun, once the spine and the right arm are where they end up
	if (GunHands.Weight > 0.f) { EvaluateGunHands(Output); }
	if (Aim.GazeWeight > 0.f) { EvaluateGaze(Output); }
	PlantStrafeFeet(Output);
	if (GripWeights[0] > 0.f || GripWeights[1] > 0.f) { EvaluateClimbGrip(Output); }
	StoreSwordContacts(Output);
	return true;
}

void FRRRandomBodyAnimInstanceProxy::Reach(FCompactPose& Pose, FCompactPoseBoneIndex End, const FVector& Target, const FVector& Pole)
{
	const auto Joint = Pose.GetParentBoneIndex(End);
	if (!Joint.IsValid()) { return; }
	const auto Root = Pose.GetParentBoneIndex(Joint);
	if (!Root.IsValid()) { return; }
	const FVector A = GetMeshSpaceTransform(Pose, Root).GetLocation();
	const FVector B = GetMeshSpaceTransform(Pose, Joint).GetLocation();
	const FVector C = GetMeshSpaceTransform(Pose, End).GetLocation();
	FVector JointGoal, EndGoal;
	AnimationCore::SolveTwoBoneIK(A, B, C, Pole, Target, JointGoal, EndGoal, false, 1.f, 1.f);
	TurnBone(Pose, Root, FQuat::FindBetweenNormals((B-A).GetSafeNormal(), (JointGoal-A).GetSafeNormal()));
	const FVector NewJoint = GetMeshSpaceTransform(Pose, Joint).GetLocation();
	TurnBone(Pose, Joint, FQuat::FindBetweenNormals((GetMeshSpaceTransform(Pose, End).GetLocation()-NewJoint).GetSafeNormal(), (EndGoal-NewJoint).GetSafeNormal()));
}

void FRRRandomBodyAnimInstanceProxy::PlantStrafeFeet(FPoseContext& Output)
{
	if (!bRightStrafe && !StrafeFeet[0].bPlanted && !StrafeFeet[1].bPlanted
		&& StrafeFeet[0].Offset.IsNearlyZero() && StrafeFeet[1].Offset.IsNearlyZero()) { return; }
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex Feet[] = {FindBone(Bones, TEXT("LeftFoot")), FindBone(Bones, TEXT("RightFoot"))};
	if (!Feet[0].IsValid() || !Feet[1].IsValid()) { StrafeFeet[0] = {}; StrafeFeet[1] = {}; return; }
	const FTransform& World = Melee.MeshToWorld;
	const FVector At[] = {World.TransformPosition(GetMeshSpaceTransform(Output.Pose, Feet[0]).GetLocation()),
		World.TransformPosition(GetMeshSpaceTransform(Output.Pose, Feet[1]).GetLocation())};
	for (int32 I = 0; I < 2; ++I)
	{
		FStrafeFoot& Foot = StrafeFeet[I];
		const float Below = At[1-I].Z - At[I].Z;
		if (bRightStrafe && !Foot.bPlanted && Below > 2.4f)
		{
			Foot.Anchor = At[I] + Foot.Offset;
			Foot.bPlanted = true;
		}
		else if (!bRightStrafe || Below < -0.5f || FVector::DistSquared2D(At[I], Foot.Anchor) > FMath::Square(32.f))
		{
			Foot.bPlanted = false;
		}
		if (Foot.bPlanted)
		{
			Foot.Offset = ((Foot.Anchor - At[I]) * FVector(1, 1, 0)).GetClampedToMaxSize(17.f);
		}
		else
		{
			Foot.Offset *= FMath::Exp(-28.f * Melee.DeltaSeconds);
			if (Foot.Offset.SizeSquared() < 0.04f) { Foot.Offset = FVector::ZeroVector; }
		}
		const auto Knee = Output.Pose.GetParentBoneIndex(Feet[I]);
		if (!Foot.Offset.IsNearlyZero() && Knee.IsValid())
		{
			const FVector Pole = GetMeshSpaceTransform(Output.Pose, Knee).GetLocation() + FVector(0, 12, 0);
			Reach(Output.Pose, Feet[I], World.InverseTransformPosition(At[I]+Foot.Offset), Pole);
		}
	}
}

void FRRRandomBodyAnimInstanceProxy::EvaluateClimbGrip(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex Hands[] = {FindBone(Bones, TEXT("LeftHand")), FindBone(Bones, TEXT("RightHand"))};
	const FCompactPoseBoneIndex Fingers[] = {FindBone(Bones, TEXT("LeftHandIndex1")), FindBone(Bones, TEXT("RightHandIndex1"))};
	FVector WristGoals[2], BodyPull = FVector::ZeroVector;
	FQuat HandRotations[2];
	float LargestPull = 0.f;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		if (!Hands[Side].IsValid() || GripWeights[Side] <= 0.f) { continue; }
		const auto Elbow = Output.Pose.GetParentBoneIndex(Hands[Side]);
		if (!Elbow.IsValid()) { continue; }
		const auto Shoulder = Output.Pose.GetParentBoneIndex(Elbow);
		const FTransform HandPose = GetMeshSpaceTransform(Output.Pose, Hands[Side]);
		HandRotations[Side] = HandPose.GetRotation();
		const FVector PalmOffset = Fingers[Side].IsValid() ? (GetMeshSpaceTransform(Output.Pose, Fingers[Side]).GetLocation() - HandPose.GetLocation()) * 0.5f : FVector::ZeroVector;
		WristGoals[Side] = GripGoals[Side] - PalmOffset;
		if (!Shoulder.IsValid()) { continue; }
		const FVector Root = GetMeshSpaceTransform(Output.Pose, Shoulder).GetLocation();
		const FVector Joint = GetMeshSpaceTransform(Output.Pose, Elbow).GetLocation();
		const FVector ToGoal = WristGoals[Side] - Root;
		const float ReachLength = FVector::Dist(Root, Joint) + FVector::Dist(Joint, HandPose.GetLocation());
		const float Short = static_cast<float>(ToGoal.Size()) - ReachLength * 0.94f;
		const float Pull = FMath::Clamp(Short, 0.f, 14.f) * (1.f - FMath::SmoothStep(20.f, 38.f, Short)) * GripWeights[Side];
		if (Pull > LargestPull) { LargestPull = Pull; BodyPull = ToGoal.GetSafeNormal() * Pull; }
	}
	const auto Hips = FindBone(Bones, TEXT("Hips"));
	if (LargestPull > 0.f && Hips.IsValid())
	{
		const auto Parent = Output.Pose.GetParentBoneIndex(Hips);
		Output.Pose[Hips].AddToTranslation(Parent.IsValid() ? GetMeshSpaceTransform(Output.Pose, Parent).InverseTransformVector(BodyPull) : BodyPull);
	}
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const auto Hand = Hands[Side];
		if (!Hand.IsValid() || GripWeights[Side] <= 0.f) { continue; }
		const auto Elbow = Output.Pose.GetParentBoneIndex(Hand);
		if (!Elbow.IsValid()) { continue; }
		const FVector Wrist = GetMeshSpaceTransform(Output.Pose, Hand).GetLocation();
		const FVector Target = FMath::Lerp(Wrist, WristGoals[Side], GripWeights[Side]);
		const FVector Pole = GetMeshSpaceTransform(Output.Pose, Elbow).GetLocation() + FVector(Side == 0 ? 10.f : -10.f, -7.f, -10.f);
		Reach(Output.Pose, Hand, Target, Pole);
		Output.Pose[Hand].SetRotation((GetMeshSpaceTransform(Output.Pose, Elbow).GetRotation().Inverse() * HandRotations[Side]).GetNormalized());
	}
}

void FRRRandomBodyAnimInstanceProxy::UpdateSwordContacts(const FPoseContext& Output)
{
	const bool bTeleported = bContactHistory && (FVector::DistSquared(LastMeshWorld.GetLocation(), Melee.MeshToWorld.GetLocation()) > FMath::Square(80.f)
		|| LastMeshWorld.GetRotation().AngularDistance(Melee.MeshToWorld.GetRotation()) > FMath::DegreesToRadians(35.f));
	if (!Melee.bDedicated || bTeleported || (bContactsValid && ContactSupportId != Melee.SupportId))
	{
		bContactsValid = false;
		ContactWeight = 0.f;
		bContactHistory = false;
		bRecovering = false;
		bGuardContacts = false;
	}
	if (bContactsValid && ContactFacing.AngularDistance(Melee.SupportToWorld.GetRotation().Inverse() * Melee.MeshToWorld.GetRotation()) > FMath::DegreesToRadians(25.f))
	{
		bContactsValid = false;
	}
	if (Melee.bPlantAttack && !Melee.bGuarding && Melee.bPlantEligible && !FMath::IsNearlyEqual(ContactStamp, Melee.SwingStamp, 0.005f))
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		FCompactPose Reference;
		Reference.SetBoneContainer(&Bones);
		Reference.ResetToRefPose();
		const TCHAR* FootNames[] = {TEXT("LeftFoot"), TEXT("RightFoot")};
		const TCHAR* ToeNames[] = {TEXT("LeftToeBase"), TEXT("RightToeBase")};
		for (int32 I = 0; I < 2; ++I)
		{
			const FCompactPoseBoneIndex Foot = FindBone(Bones, FootNames[I]);
			const FCompactPoseBoneIndex Toe = FindBone(Bones, ToeNames[I]);
			if (!Foot.IsValid() || !Toe.IsValid()) { return; }
			const FTransform FootWorld = bContactHistory ? LastFeetWorld[I] : GetMeshSpaceTransform(Output.Pose, Foot) * Melee.MeshToWorld;
			const FTransform ToeWorld = bContactHistory ? LastToesWorld[I] : GetMeshSpaceTransform(Output.Pose, Toe) * Melee.MeshToWorld;
			ContactFeet[I] = FootWorld.GetRelativeTransform(Melee.SupportToWorld);
			ContactToes[I] = ToeWorld.GetRelativeTransform(Melee.SupportToWorld);
		}
		const FVector Forward = Melee.MeshToWorld.GetUnitAxis(EAxis::Y).GetSafeNormal2D();
		// Alternate from the actual stance, including when a fresh combo starts
		// with the opposite foot forward after the previous three-cut phrase.
		const FVector ToeDifference = (ContactToes[0] * Melee.SupportToWorld).GetLocation() - (ContactToes[1] * Melee.SupportToWorld).GetLocation();
		const float AirborneDifference = FVector::DotProduct(ToeDifference, Melee.FloorNormal);
		// If recovery is interrupted, continue the foot already in the air rather
		// than snapping it down and lifting the other foot on the same frame.
		ContactLeadFoot = FMath::Abs(AirborneDifference) > 0.3f ? (AirborneDifference > 0.f ? 0 : 1)
			: (FVector::DotProduct(ToeDifference, Forward) > 0.f ? 1 : 0);
		const int32 Front = ContactLeadFoot;
		const FVector Back = (ContactToes[1 - Front] * Melee.SupportToWorld).GetLocation();
		const FVector Start = (ContactToes[Front] * Melee.SupportToWorld).GetLocation();
		const float Stride = 10.f * (19.8f / 23.2f) * Melee.BodyScale * Melee.MeshToWorld.GetScale3D().Y;
		const FVector EntryOrigin = bContactHistory ? LastMeshWorld.GetLocation() : Melee.MeshToWorld.GetLocation();
		const FVector EndStance = EntryOrigin + Forward * (Stride + 8.f * Melee.BodyScale * Melee.MeshToWorld.GetScale3D().Y);
		// Anchor each landing to the capsule's new stance, rather than accumulating
		// the small difference between nominal and discretized root-motion distance.
		FVector Goal = Start + Forward * FVector::DotProduct(EndStance - Start, Forward);
		if (Melee.FloorNormal.Z > 0.5f) { Goal.Z -= FVector::DotProduct(Goal - Back, Melee.FloorNormal) / Melee.FloorNormal.Z; }
		LandingToe = Melee.SupportToWorld.InverseTransformPosition(Goal);
		const FQuat GroundTilt = FQuat::FindBetweenNormals(FVector::UpVector, Melee.FloorNormal);
		LandingFootRotation = Melee.SupportToWorld.GetRotation().Inverse() * GroundTilt * Melee.MeshToWorld.GetRotation()
			* GetMeshSpaceTransform(Reference, FindBone(Bones, FootNames[Front])).GetRotation();
		LandingToeRotation = Melee.SupportToWorld.GetRotation().Inverse() * GroundTilt * Melee.MeshToWorld.GetRotation()
			* GetMeshSpaceTransform(Reference, FindBone(Bones, ToeNames[Front])).GetRotation();
		ContactSupportId = Melee.SupportId;
		ContactFacing = Melee.SupportToWorld.GetRotation().Inverse() * Melee.MeshToWorld.GetRotation();
		ContactStamp = Melee.SwingStamp;
		bContactsValid = true;
		ContactWeight = 1.f;
		bRecovering = false;
		bGuardContacts = false;
	}
	if (Melee.bGuarding && Melee.bPlantEligible && (!bGuardContacts || !bContactsValid))
	{
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		const TCHAR* Feet[] = {TEXT("LeftFoot"), TEXT("RightFoot")};
		const TCHAR* Toes[] = {TEXT("LeftToeBase"), TEXT("RightToeBase")};
		for (int32 I = 0; I < 2; ++I)
		{
			const FCompactPoseBoneIndex Foot = FindBone(Bones, Feet[I]), Toe = FindBone(Bones, Toes[I]);
			if (!Foot.IsValid() || !Toe.IsValid()) { return; }
			const FTransform FootWorld = bContactHistory ? LastFeetWorld[I] : GetMeshSpaceTransform(Output.Pose, Foot) * Melee.MeshToWorld;
			const FTransform ToeWorld = bContactHistory ? LastToesWorld[I] : GetMeshSpaceTransform(Output.Pose, Toe) * Melee.MeshToWorld;
			ContactFeet[I] = FootWorld.GetRelativeTransform(Melee.SupportToWorld);
			ContactToes[I] = ToeWorld.GetRelativeTransform(Melee.SupportToWorld);
		}
		ContactFacing = Melee.SupportToWorld.GetRotation().Inverse() * Melee.MeshToWorld.GetRotation();
		ContactSupportId = Melee.SupportId;
		bGuardContacts = true;
		bContactsValid = true;
		bRecovering = false;
		ContactWeight = 1.f;
	}
	if (bContactsValid && bContactHistory && Melee.bPlantEligible && !Melee.bGuarding && !bGuardContacts && !bRecovering
		&& Melee.AttackAge > Melee.AttackDuration * 1.14f)
	{
		// Recover with two small airborne placements. The opposite foot keeps its
		// ground contact throughout each placement; no blend back to sliding idle feet.
		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		FCompactPose Reference;
		Reference.SetBoneContainer(&Bones);
		Reference.ResetToRefPose();
		const TCHAR* Feet[] = {TEXT("LeftFoot"), TEXT("RightFoot")};
		const TCHAR* Toes[] = {TEXT("LeftToeBase"), TEXT("RightToeBase")};
		const FVector Ground = LastToesWorld[ContactLeadFoot].GetLocation();
		const FQuat GroundTilt = FQuat::FindBetweenNormals(FVector::UpVector, Melee.FloorNormal);
		for (int32 I = 0; I < 2; ++I)
		{
			RecoveryFeet[I] = LastFeetWorld[I].GetRelativeTransform(Melee.SupportToWorld);
			RecoveryToes[I] = LastToesWorld[I].GetRelativeTransform(Melee.SupportToWorld);
			const float Side = I == 0 ? 1.f : -1.f;
			FVector Goal = Melee.MeshToWorld.TransformPosition(FVector(Side * 9.2f, I == ContactLeadFoot ? 5.5f : 0.8f, 0.f) * Melee.BodyScale);
			if (Melee.FloorNormal.Z > 0.5f) { Goal.Z -= FVector::DotProduct(Goal - Ground, Melee.FloorNormal) / Melee.FloorNormal.Z; }
			const FQuat Basis = GroundTilt * Melee.MeshToWorld.GetRotation() * FQuat(FVector::UpVector, FMath::DegreesToRadians(-Side * 10.f));
			RecoveryGoals[I] = FTransform(Basis * GetMeshSpaceTransform(Reference, FindBone(Bones, Toes[I])).GetRotation(), Goal).GetRelativeTransform(Melee.SupportToWorld);
			RecoveryFootRotations[I] = Melee.SupportToWorld.GetRotation().Inverse() * Basis * GetMeshSpaceTransform(Reference, FindBone(Bones, Feet[I])).GetRotation();
		}
		RecoveryStartAge = Melee.AttackAge;
		bRecovering = true;
	}
	// Keep the final recovered stance planted until locomotion or another action.
	const bool bHoldContacts = bContactsValid && Melee.bPlantEligible && (!bGuardContacts || Melee.bGuarding || Melee.ParryWeight > 0.f);
	ContactWeight = FMath::FInterpConstantTo(ContactWeight, bHoldContacts ? 1.f : 0.f, Melee.DeltaSeconds, 10.f);
	if (ContactWeight <= 0.f) { bContactsValid = false; }
}

void FRRRandomBodyAnimInstanceProxy::StoreSwordContacts(const FPoseContext& Output)
{
	if (!Melee.bDedicated) { bContactHistory = false; return; }
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const TCHAR* Feet[] = {TEXT("LeftFoot"), TEXT("RightFoot")};
	const TCHAR* Toes[] = {TEXT("LeftToeBase"), TEXT("RightToeBase")};
	for (int32 I = 0; I < 2; ++I)
	{
		const FCompactPoseBoneIndex Foot = FindBone(Bones, Feet[I]), Toe = FindBone(Bones, Toes[I]);
		if (!Foot.IsValid() || !Toe.IsValid()) { bContactHistory = false; return; }
		LastFeetWorld[I] = GetMeshSpaceTransform(Output.Pose, Foot) * Melee.MeshToWorld;
		LastToesWorld[I] = GetMeshSpaceTransform(Output.Pose, Toe) * Melee.MeshToWorld;
	}
	LastMeshWorld = Melee.MeshToWorld;
	bContactHistory = true;
}

// ---------------------------------------------------------------------------------------------------------------------
// The light sword's layer (round 97)

void URRRandomBodyAnimInstance::UpdateMelee(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	const bool bSword = Brawler && Brawler->IsAlive() && Brawler->GetWeapon().bSword && !Brawler->IsDriving()
		&& !Brawler->IsClimbing() && !Brawler->IsUsingUltimate() && !Brawler->IsCelebrating() && !Brawler->IsDespairing() && !Brawler->IsEmoting();
	Melee.Guard = bSword ? Brawler->GetSwordGuard() : nullptr;
	// Round 101: carrying the hammer and the shield, the arms hold their own pose over whatever the legs are doing
	const UAnimSequence* Carry = bSword ? Brawler->GetShieldCarry() : nullptr;
	if (Carry)
	{
		Melee.Carry = Carry;
	}
	Melee.CarryWeight = FMath::FInterpConstantTo(Melee.CarryWeight, Carry ? 1.f : 0.f, DeltaSeconds, 5.f);
	if (Melee.CarryWeight <= 0.f)
	{
		Melee.Carry = nullptr;
	}
	Melee.bDedicated = bSword && Brawler->HasDedicatedSwordMotion();
	if (Melee.bDedicated)
	{
		Melee.BodyScale = Brawler->GetSwordBodyScale();
		const TArray<FRRSpeciesLook>& Looks = FRRSpeciesLook::GetAll();
		if (Looks.IsValidIndex(Brawler->GetSpecies())) { Melee.RightHandFix = Looks[Brawler->GetSpecies()].GunHandTurn.Quaternion(); }
	}
	Melee.DeltaSeconds = DeltaSeconds;
	Melee.bPlantEligible = Melee.bDedicated && Brawler->GetCharacterMovement()->IsMovingOnGround() && Brawler->GetAnimationMoveSpeed() < 15.f;
	Melee.bPlantAttack = false;
	Melee.FootPhase = 1.f;
	Melee.AttackAge = bSword ? Brawler->GetTimeSinceShot() : 0.f;
	Melee.AttackDuration = bSword ? FMath::Max(Brawler->GetSwordAttackDuration(), 0.01f) : 0.35f;
	const float ParryAge = bSword ? Brawler->GetTimeSinceDeflect() : 100.f;
	const bool bDedicatedParry = Melee.bDedicated && ParryAge >= 0.f && ParryAge < ARRRandomCharacter::DedicatedParryDuration
		&& (Brawler->IsDeflecting() || ParryAge < Melee.AttackAge);
	if (bDedicatedParry)
	{
		const float Stamp = GetWorld()->GetTimeSeconds() - ParryAge;
		if (!FMath::IsNearlyEqual(Melee.ParryStamp, Stamp, 0.005f))
		{
			Melee.ParryStamp = Stamp;
			Melee.ParryEntryGrip = Melee.GripOffset;
			Melee.ParryEntryRotation = Melee.GripRotation;
			Melee.ParryEntryHip = Melee.HipYaw;
			Melee.ParryEntryChest = Melee.ChestYaw;
			Melee.ParryEntryCarry = Melee.GripCarryYaw;
			Melee.ParryEntryLean = Melee.ChestLean;
			Melee.ParryEntryLoad = Melee.StanceLoad;
			Melee.ParryEntryShift = Melee.WeightShift;
			Melee.ParryEntryWeight = Melee.DriveWeight;
		}
	}
	Melee.ParryWeight = 0.f;
	Melee.ParrySway = 0.f;
	Melee.GripWeight = FMath::FInterpConstantTo(Melee.GripWeight, Melee.bDedicated ? 1.f : 0.f, DeltaSeconds, 8.f);
	if (Melee.bDedicated)
	{
		// One shared hilt path for BOTH hands. Mesh +X is left, +Y is forward.
		struct FGripKey { float Time; FVector Position; float Yaw; float Pitch; };
		const auto Rotation = [](float Yaw, float Pitch)
		{
			const FVector ActorDirection = FRotator(Pitch, Yaw, 0.f).Vector();
			return FRotationMatrix::MakeFromXZ(FVector(-ActorDirection.Y, ActorDirection.X, ActorDirection.Z), FVector::UpVector).ToQuat();
		};
		const FVector Ready(0.f, 10.f, -7.f);
		const FQuat ReadyRotation = Rotation(10.f, 38.f);
		FVector Grip = Ready;
		FQuat Turn = ReadyRotation;
		const float Since = Brawler->GetTimeSinceShot();
		const float Duration = Brawler->GetSwordAttackDuration();
		const float Stamp = GetWorld()->GetTimeSeconds() - Since;
		if (!FMath::IsNearlyEqual(Melee.GripSwingStamp, Stamp, 0.005f))
		{
			Melee.EntryGripOffset = Melee.GripOffset;
			Melee.EntryGripRotation = Melee.GripRotation;
			Melee.EntryHipYaw = Melee.DriveWeight > 0.f ? Melee.HipYaw : 0.f;
			Melee.EntryChestYaw = Melee.DriveWeight > 0.f ? Melee.ChestYaw : 0.f;
			Melee.EntryCarryYaw = Melee.DriveWeight > 0.f ? Melee.GripCarryYaw : 0.f;
			Melee.EntryChestLean = Melee.DriveWeight > 0.f ? Melee.ChestLean : 0.f;
			Melee.EntryWeightShift = Melee.DriveWeight > 0.f ? Melee.WeightShift : 0.f;
			Melee.EntryStanceLoad = Melee.DriveWeight > 0.f ? Melee.StanceLoad : 0.f;
			Melee.GripSwingStamp = Stamp;
		}
		const float TimingScale = Duration / 0.56f;
		if (Since >= 0.f && Since < Duration + 0.3f * TimingScale)
		{
			// Load the stance, accelerate through the cut, then carry the blade past
			// the strike and absorb its momentum. Each finish is the next cut's entry.
			const FGripKey Paths[3][6] = {
				{{0.f, {-3.f, 10.f, -3.f}, 55.f, 45.f}, {0.43f, {-3.5f, 10.f, -2.5f}, 60.f, 48.f},
				 {0.70f, {1.5f, 11.f, -7.f}, -50.f, -22.f}, {0.81f, {2.5f, 10.5f, -8.f}, -62.f, -30.f},
				 {0.92f, {1.8f, 10.f, -7.7f}, -58.f, -27.f}, {1.f, {1.5f, 10.f, -7.5f}, -55.f, -25.f}},
				{{0.f, {1.5f, 10.f, -7.5f}, -55.f, -25.f}, {0.43f, {3.f, 10.f, -4.5f}, -60.f, 10.f},
				 {0.70f, {-2.5f, 11.f, -5.f}, 50.f, 6.f}, {0.81f, {-3.5f, 10.5f, -5.5f}, 66.f, 4.f},
				 {0.92f, {-3.2f, 10.f, -5.2f}, 60.f, 7.f}, {1.f, {-3.f, 10.f, -5.f}, 55.f, 8.f}},
				// Keep the overhead preparation ahead of the fox's large muzzle.
				{{0.f, {-3.f, 10.f, -5.f}, 55.f, 8.f}, {0.45f, {0.f, 14.f, -3.f}, 0.f, 55.f},
				 {0.72f, {0.f, 13.f, -8.f}, 0.f, -35.f}, {0.83f, {0.f, 12.5f, -9.f}, 0.f, -50.f},
				 {0.93f, {0.f, 12.f, -8.4f}, 0.f, -43.f}, {1.f, {0.f, 12.f, -8.f}, 0.f, -40.f}}
			};
			const FGripKey* Keys = Paths[FMath::Clamp(Brawler->GetComboStep(), 0, 2)];
			const float Phase = FMath::Clamp(Since / FMath::Max(Duration, 0.01f), 0.f, 1.f);
			int32 Segment = 0;
			while (Segment < UE_ARRAY_COUNT(Paths[0]) - 2 && Phase > Keys[Segment + 1].Time) { ++Segment; }
			const FGripKey& A = Keys[Segment];
			const FGripKey& B = Keys[Segment + 1];
			const float Blend = FMath::SmoothStep(A.Time, B.Time, Phase);
			Grip = FMath::Lerp(A.Position, B.Position, Blend);
			Turn = FQuat::Slerp(Rotation(A.Yaw, A.Pitch), Rotation(B.Yaw, B.Pitch), Blend).GetNormalized();
			const float Entry = FMath::SmoothStep(0.f, 0.10f * TimingScale, Since);
			Grip = FMath::Lerp(Melee.EntryGripOffset, Grip, Entry);
			Turn = FQuat::Slerp(Melee.EntryGripRotation, Turn, Entry).GetNormalized();
			const float Release = FMath::SmoothStep(Duration + 0.12f * TimingScale, Duration + 0.3f * TimingScale, Since);
			Grip = FMath::Lerp(Grip, Ready, Release);
			Turn = FQuat::Slerp(Turn, ReadyRotation, Release).GetNormalized();
		}
		const float Guard = FMath::SmoothStep(0.f, 1.f, Melee.GuardWeight);
		Grip = FMath::Lerp(Grip, FVector(0.f, 11.f, -3.f), Guard);
		Turn = FQuat::Slerp(Turn, Rotation(-48.f, 48.f), Guard).GetNormalized();
		Melee.GripOffset = Grip;
		Melee.GripRotation = Turn;
	}
	const float LowerBody = Melee.bDedicated && !Brawler->GetCharacterMovement()->IsFalling()
		? 1.f - FMath::SmoothStep(15.f, 65.f, Brawler->GetAnimationMoveSpeed()) : 0.f;
	Melee.LowerBodyWeight = FMath::FInterpTo(Melee.LowerBodyWeight, LowerBody, DeltaSeconds, 12.f);
	if (bSword)
	{
		const float Pitch = Brawler->IsThirdPerson()
			? FMath::Clamp(FRotator::NormalizeAxis(Brawler->GetBaseAimRotation().Pitch), -40.f, 40.f) : 0.f;
		const FVector Look = FRotator(Pitch, Brawler->GetActorRotation().Yaw, 0.f).Vector();
		Melee.Facing = GetSkelMeshComponent()->GetComponentTransform().InverseTransformVectorNoScale(Look).GetSafeNormal();
	}
	// The blow: the combo step's own (SlashFrom .. SlashTo of it) over SlashTime after each swing, easing in quick and out
	// slower. On guard, each shot knocked away is the parry flick over the guard (ParryTime): the shot is struck, not just
	// blocked (the parry motion, or the first blow's)
	const UAnimSequence* LastSlash = Melee.Slash;
	const float LastSlashTime = Melee.SlashTime;
	const float LastSlashWeight = Melee.SlashWeight;
	Melee.Slash = nullptr;
	Melee.SlashWeight = 0.f;
	Melee.DriveWeight = 0.f;
	if (bSword && SlashTime > 0.f)
	{
		const float SinceSwing = Brawler->GetTimeSinceShot();
		const float SinceParry = Brawler->GetTimeSinceDeflect();
		const bool bParry = SinceParry < SinceSwing;
		const int32 Step = FMath::Clamp(Brawler->GetComboStep(), 0, 2);
		UAnimSequence* ParryMotion = Brawler->GetSwordParry();
		Melee.Slash = bParry ? (ParryMotion ? ParryMotion : Brawler->GetSwordSlash(0)) : Brawler->GetSwordSlash(Step);
		// (round 98: the hammer and the dagger have their own blows, with their own windows and pace)
		const bool bRole = Brawler->HasRoleMeleeMotion() && !bParry;
		float RoleFrom = 0.f;
		float RoleTo = 1.f;
		Brawler->GetMeleeWindow(Step, RoleFrom, RoleTo);
		const float From = bRole ? RoleFrom : bParry ? (ParryMotion ? ParryFrom : SlashFrom[0]) : (Melee.bDedicated ? 0.f : SlashFrom[Step]);
		const float To = bRole ? RoleTo : bParry ? (ParryMotion ? ParryTo : SlashTo[0]) : (Melee.bDedicated ? 1.f : SlashTo[Step]);
		const float Since = bParry ? SinceParry : SinceSwing;
		const float Time = bRole ? Brawler->GetSwordAttackDuration() : bParry ? ParryTime : (Melee.bDedicated ? Brawler->GetSwordAttackDuration() : SlashTime * (Step == 2 ? FinisherTimeShare : 1.f));
		const bool bConnected = Melee.bDedicated && !bParry;
		const float Stamp = GetWorld()->GetTimeSeconds() - Since;
		if (!FMath::IsNearlyEqual(Melee.SwingStamp, Stamp, 0.005f))
		{
			Melee.PreviousSlash = bConnected && LastSlashWeight > 0.05f ? LastSlash : nullptr;
			Melee.PreviousSlashTime = LastSlashTime;
			Melee.PreviousSlashWeight = LastSlashWeight;
			Melee.SwingStamp = Stamp;
		}
		// Retain the follow-through through the next input. Connected cuts blend from the
		// previous pose, never through idle. Only an interrupted combo releases to locomotion.
		const float Hold = bConnected ? Time * (0.12f / 0.56f) : 0.f;
		const float Release = bConnected ? Time * (0.18f / 0.56f) : 0.f;
		if (Melee.Slash && Since >= 0.f && Since < Time + Hold + Release)
		{
			float Share = FMath::Clamp(Since / Time, 0.f, 1.f);
			if (bConnected)
			{
				// Complete the authored step before the hand accelerates. Its in-place
				// advance stays matched to SwordStep's 0.40-duration capsule translation.
				const float Phase = Share;
				Melee.FootPhase = Phase;
				Melee.bPlantAttack = Since < Time;
				Share = Phase < 0.40f ? 0.8f * Phase / 0.40f
					: FMath::Lerp(0.8f, 1.f, (Phase - 0.40f) / 0.60f);
				const float HipLoad[] = {22.f, -28.f, 28.f};
				const float HipFinish[] = {-22.f, 22.f, -8.f};
				const float ChestLoad[] = {28.f, -32.f, 32.f};
				const float ChestFinish[] = {-28.f, 28.f, -8.f};
				const auto Drive = [&](float P, float Start, float Load, float Finish, float Begin, float End)
				{
					const float Coil = FMath::Lerp(Start, Load, FMath::SmoothStep(0.f, 0.18f, P));
					return FMath::Lerp(Coil, Finish, FMath::SmoothStep(Begin, End, P));
				};
				Melee.HipYaw = Drive(Phase, Melee.EntryHipYaw, HipLoad[Step], HipFinish[Step], 0.29f, 0.53f);
				Melee.ChestYaw = Drive(Phase, Melee.EntryChestYaw, ChestLoad[Step], ChestFinish[Step], 0.36f, 0.64f);
				Melee.GripCarryYaw = Drive(FMath::Max(0.f, Phase - 0.04f), Melee.EntryCarryYaw, ChestLoad[Step], ChestFinish[Step], 0.36f, 0.64f);
				const float ChestLean = Step == 2 ? FMath::Lerp(-4.f, 12.f, FMath::SmoothStep(0.36f, 0.67f, Phase))
					: 4.f + 3.f * FMath::Sin(PI * Phase);
				Melee.ChestLean = FMath::Lerp(Melee.EntryChestLean, ChestLean, FMath::SmoothStep(0.f, 0.18f, Phase));
				Melee.StanceLoad = FMath::Lerp(Melee.EntryStanceLoad, 1.f, FMath::SmoothStep(0.f, 0.20f, Phase)) * (1.f - FMath::SmoothStep(0.46f, 0.90f, Phase));
				Melee.StanceLoad += 0.65f * FMath::SmoothStep(0.30f, 0.48f, Phase) * (1.f - FMath::SmoothStep(0.70f, 1.f, Phase));
				Melee.WeightShift = Phase < 0.16f ? FMath::Lerp(Melee.EntryWeightShift, -0.9f, FMath::SmoothStep(0.f, 0.16f, Phase))
					: Phase < 0.58f ? FMath::Lerp(-0.9f, 3.2f, FMath::SmoothStep(0.16f, 0.58f, Phase))
					: FMath::Lerp(3.2f, 1.6f, FMath::SmoothStep(0.58f, 1.f, Phase));
			}
			const float Length = Melee.Slash->GetPlayLength();
			Melee.SlashTime = Length * FMath::Lerp(From, To, Share);
			Melee.SlashBlend = FMath::SmoothStep(0.f, bConnected ? Time * (0.10f / 0.56f) : 0.10f, Since);
			Melee.SlashWeight = bConnected
				? FMath::Lerp(Melee.PreviousSlash ? Melee.PreviousSlashWeight : 0.f, 1.f, Melee.SlashBlend)
					* (1.f - FMath::SmoothStep(Time + Hold, Time + Hold + Release, Since))
				: FMath::SmoothStep(0.f, 0.18f, Share) * (1.f - FMath::SmoothStep(0.64f, 1.f, Share));
			Melee.DriveWeight = bConnected ? Melee.SlashWeight : 0.f;
		}
	}
	// The guard: held while deflecting, looping its middle
	const bool bGuard = Melee.Guard && Brawler->IsDeflecting();
	if (bGuard && !Melee.bGuarding)
	{
		Melee.GuardTime = 0.f;
	}
	Melee.bGuarding = bGuard;
	Melee.GuardWeight = FMath::FInterpConstantTo(Melee.GuardWeight, bGuard ? 1.f : 0.f, DeltaSeconds, 1.f / FMath::Max(GuardBlendTime, KINDA_SMALL_NUMBER));
	if (bGuard && Brawler->GetTimeSinceShot() <= Brawler->GetTimeSinceDeflect())
	{
		// Raising the guard interrupts the attack recovery; an actual parry still plays over the guard.
		Melee.SlashWeight *= 1.f - FMath::SmoothStep(0.f, 1.f, Melee.GuardWeight);
	}
	Melee.DriveWeight *= 1.f - FMath::SmoothStep(0.f, 1.f, Melee.GuardWeight);
	if (Melee.Guard && Melee.GuardWeight > 0.f)
	{
		Melee.GuardTime = FMath::Fmod(Melee.GuardTime + DeltaSeconds, FMath::Max(Melee.Guard->GetPlayLength(), 0.1f));
	}
	else
	{
		Melee.GuardWeight = 0.f;
	}
	if (bDedicatedParry)
	{
		struct FParryArc
		{
			FVector Load, Finish;
			FVector2D LoadAngle, FinishAngle;
			float HipLoad, HipFinish, ChestLoad, ChestFinish, Lean;
		};
		// Downward diagonal, rising diagonal, and the opposite horizontal beat.
		// These drive the shared hilt; both arms and the visible blade follow it.
		static const FParryArc Arcs[] = {
			{{3.2f, 12.f, -2.5f}, {-2.8f, 12.f, -6.5f}, {-55.f, 52.f}, {55.f, -24.f}, -10.f, 12.f, -16.f, 22.f, 7.f},
			{{1.2f, 14.f, -6.5f}, {-2.5f, 13.f, -2.5f}, {-52.f, -24.f}, {48.f, 56.f}, -12.f, 10.f, -18.f, 20.f, -4.f},
			{{-3.5f, 12.f, -4.f}, {3.5f, 12.f, -4.5f}, {60.f, 9.f}, {-62.f, 7.f}, 14.f, -14.f, 22.f, -24.f, 4.f}
		};
		const FParryArc& Arc = Arcs[FMath::Clamp<int32>(Brawler->GetSwordParryVariant(), 0, 2)];
		const float Contact = Brawler->GetSwordParryContactTime();
		const float Windup = FMath::Max(0.02f, Contact * 0.45f);
		const float CutEnd = Contact + 0.045f;
		const float CarryEnd = Contact + 0.105f;
		const float End = ARRRandomCharacter::DedicatedParryDuration;
		const auto Rotation = [](FVector2D Angle)
		{
			const FVector V = FRotator(Angle.Y, Angle.X, 0.f).Vector();
			return FRotationMatrix::MakeFromXZ(FVector(-V.Y, V.X, V.Z), FVector::UpVector).ToQuat();
		};
		const FVector Shot = GetSkelMeshComponent()->GetComponentTransform().InverseTransformPosition(Brawler->GetParryPoint());
		const FVector MeetOffset(FMath::Clamp(Shot.X * 0.04f, -1.5f, 1.5f), 0.f, FMath::Clamp((Shot.Z - 45.f) * 0.08f, -1.5f, 1.5f));
		const FVector Positions[] = {Melee.ParryEntryGrip, Arc.Load + MeetOffset, Arc.Finish + MeetOffset,
			Arc.Finish + MeetOffset + (Arc.Finish - Arc.Load).GetSafeNormal() * 0.7f, Melee.GripOffset};
		const FQuat Rotations[] = {Melee.ParryEntryRotation, Rotation(Arc.LoadAngle), Rotation(Arc.FinishAngle),
			Rotation(Arc.FinishAngle + (Arc.FinishAngle - Arc.LoadAngle).GetSafeNormal() * 7.f), Melee.GripRotation};
		const float Times[] = {0.f, Windup, CutEnd, CarryEnd, End};
		int32 Segment = 0;
		while (Segment < 3 && ParryAge > Times[Segment + 1]) { ++Segment; }
		const float Blend = FMath::SmoothStep(Times[Segment], Times[Segment + 1], ParryAge);
		Melee.GripOffset = FMath::Lerp(Positions[Segment], Positions[Segment + 1], Blend);
		Melee.GripRotation = FQuat::Slerp(Rotations[Segment], Rotations[Segment + 1], Blend).GetNormalized();
		const float HipBegin = FMath::Max(0.015f, Contact - 0.09f);
		const float HipEnd = FMath::Max(HipBegin + 0.045f, Contact - 0.02f);
		const float ChestBegin = FMath::Max(0.025f, Contact - 0.055f);
		const float ChestEnd = FMath::Max(ChestBegin + 0.055f, Contact + 0.01f);
		const auto BodyDrive = [&](float Age, float Entry, float Load, float Finish, float Begin, float FinishTime)
		{
			return FMath::Lerp(FMath::Lerp(Entry, Load, FMath::SmoothStep(0.f, Windup * 0.75f, Age)),
				Finish, FMath::SmoothStep(Begin, FinishTime, Age));
		};
		Melee.HipYaw = BodyDrive(ParryAge, Melee.ParryEntryHip, Arc.HipLoad, Arc.HipFinish, HipBegin, HipEnd);
		Melee.ChestYaw = BodyDrive(ParryAge, Melee.ParryEntryChest, Arc.ChestLoad, Arc.ChestFinish, ChestBegin, ChestEnd);
		Melee.GripCarryYaw = BodyDrive(FMath::Max(0.f, ParryAge - 0.018f), Melee.ParryEntryCarry, Arc.ChestLoad, Arc.ChestFinish, ChestBegin, ChestEnd);
		Melee.ChestLean = FMath::Lerp(Melee.ParryEntryLean, Arc.Lean, FMath::SmoothStep(0.f, Contact + 0.035f, ParryAge));
		Melee.StanceLoad = FMath::Lerp(Melee.ParryEntryLoad, 1.25f, FMath::SmoothStep(0.f, Contact, ParryAge));
		Melee.WeightShift = FMath::Lerp(Melee.ParryEntryShift, -1.0f, FMath::SmoothStep(0.f, Windup, ParryAge));
		Melee.WeightShift = FMath::Lerp(Melee.WeightShift, 1.3f, FMath::SmoothStep(Windup, Contact + 0.05f, ParryAge));
		Melee.ParrySway = (Arc.HipFinish > 0.f ? -0.8f : 0.8f) * FMath::Sin(PI * FMath::Clamp(ParryAge / End, 0.f, 1.f));
		Melee.ParryWeight = FMath::Lerp(Melee.ParryEntryWeight, 1.f, FMath::SmoothStep(0.f, 0.035f, ParryAge))
			* (1.f - FMath::SmoothStep(Contact + 0.11f, End, ParryAge));
		Melee.DriveWeight = Melee.ParryWeight;
		Melee.SlashWeight = 0.f;
	}
}

void FRRRandomBodyAnimInstanceProxy::EvaluateMelee(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex Spine = FindBone(Bones, SpineBones[0]);
	const FCompactPoseBoneIndex MiddleSpine = FindBone(Bones, SpineBones[1]);
	if (!Spine.IsValid())
	{
		return;
	}
	const auto Sample = [&](const UAnimSequence* Sequence, float Time, FCompactPose& Pose)
	{
		Pose.SetBoneContainer(&Bones);
		FBlendedCurve Curve;
		Curve.InitFrom(Output.Curve);
		UE::Anim::FStackAttributeContainer Attributes;
		FAnimationPoseData Data(Pose, Curve, Attributes);
		Sequence->GetAnimationPose(Data, FAnimExtractContext(double(Time), false));
	};
	FCompactPose GuardPose;
	FCompactPose SlashPose;
	const FCompactPoseBoneIndex Hips = FindBone(Bones, TEXT("Hips"));
	const FCompactPoseBoneIndex LeftLeg = FindBone(Bones, TEXT("LeftUpLeg"));
	const FCompactPoseBoneIndex RightLeg = FindBone(Bones, TEXT("RightUpLeg"));
	const FCompactPoseBoneIndex LeftToe = FindBone(Bones, TEXT("LeftToeBase"));
	const FCompactPoseBoneIndex RightToe = FindBone(Bones, TEXT("RightToeBase"));
	const FCompactPoseBoneIndex ChestBone = FindBone(Bones, TEXT("Spine2"));
	const FQuat BaseHipRotation = Hips.IsValid() ? GetMeshSpaceTransform(Output.Pose, Hips).GetRotation() : FQuat::Identity;
	const FQuat BaseChestRotation = ChestBone.IsValid() ? GetMeshSpaceTransform(Output.Pose, ChestBone).GetRotation() : FQuat::Identity;
	const FCompactPoseBoneIndex FootBones[] = {FindBone(Bones, TEXT("LeftFoot")), FindBone(Bones, TEXT("RightFoot"))};
	const FTransform BaseFeet[] = {FootBones[0].IsValid() ? GetMeshSpaceTransform(Output.Pose, FootBones[0]) : FTransform::Identity,
		FootBones[1].IsValid() ? GetMeshSpaceTransform(Output.Pose, FootBones[1]) : FTransform::Identity};
	const bool bGroundable = Hips.IsValid() && LeftToe.IsValid() && RightToe.IsValid();
	const float Floor = bGroundable ? FMath::Min(GetMeshSpaceTransform(Output.Pose, LeftToe).GetLocation().Z,
		GetMeshSpaceTransform(Output.Pose, RightToe).GetLocation().Z) : 0.f;
	const bool bGuard = Melee.Guard && Melee.GuardWeight > 0.f;
	const bool bSlash = Melee.Slash && Melee.SlashWeight > 0.f;
	const bool bFullStroke = Melee.Slash && Melee.bDedicated && Melee.Slash->GetName().Contains(TEXT("SwordDedicated"));
	if (bGuard)
	{
		Sample(Melee.Guard, Melee.GuardTime, GuardPose);
	}
	if (bSlash || (bFullStroke && ContactWeight > 0.f))
	{
		Sample(Melee.Slash, Melee.SlashTime, SlashPose);
		if (bFullStroke && Melee.PreviousSlash && Melee.SlashBlend < 1.f)
		{
			FCompactPose Previous;
			Sample(Melee.PreviousSlash, Melee.PreviousSlashTime, Previous);
			for (const FCompactPoseBoneIndex Index : SlashPose.ForEachBoneIndex())
			{
				SlashPose[Index].Blend(Previous[Index], SlashPose[Index], Melee.SlashBlend);
			}
		}
	}
	// Round 101: first the carry pose on both arms (shoulder joint down), under the guard and the swings
	if (Melee.Carry && Melee.CarryWeight > 0.f)
	{
		FCompactPose CarryPose;
		Sample(Melee.Carry, 0.f, CarryPose);
		const FCompactPoseBoneIndex Arms[] = { FindBone(Bones, TEXT("LeftArm")), FindBone(Bones, TEXT("RightArm")) };
		const float Weight = FMath::SmoothStep(0.f, 1.f, Melee.CarryWeight);
		for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
		{
			bool bArm = false;
			for (FCompactPoseBoneIndex Walk = Index; Walk.IsValid() && !bArm; Walk = Output.Pose.GetParentBoneIndex(Walk))
			{
				bArm = Walk == Arms[0] || Walk == Arms[1];
			}
			if (bArm)
			{
				Output.Pose[Index].SetRotation(FQuat::Slerp(Output.Pose[Index].GetRotation(), CarryPose[Index].GetRotation(), Weight).GetNormalized());
			}
		}
	}
	// Guard/parry stay on the upper body. Dedicated attacks also carry the pelvis and both leg chains.
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		bool bUpper = false;
		bool bLeg = false;
		for (FCompactPoseBoneIndex Walk = Index; Walk.IsValid(); Walk = Output.Pose.GetParentBoneIndex(Walk))
		{
			bLeg |= Walk == LeftLeg || Walk == RightLeg;
			if (Walk == Spine)
			{
				bUpper = true;
				break;
			}
		}
		if (!bUpper)
		{
			// Planted attacks keep the authored hip/knee weight transfer; locomotion fades it out.
			if (bFullStroke && bSlash && Melee.LowerBodyWeight > 0.f && (Index == Hips || bLeg))
			{
				const float Weight = Melee.SlashWeight * Melee.LowerBodyWeight;
				FTransform& Local = Output.Pose[Index];
				Local.SetRotation(FQuat::Slerp(Local.GetRotation(), SlashPose[Index].GetRotation(), Weight).GetNormalized());
				if (Index == Hips)
				{
					// The bind-pose hips have an XY offset on this rig that locomotion does not.
					// Keep the locomotion origin, not that bind offset: only the authored crouch
					// belongs on the mesh; CharacterMovement carries the advancing footwork.
					FVector Planted = Local.GetTranslation();
					Planted.Z = SlashPose[Index].GetTranslation().Z;
					Local.SetTranslation(FMath::Lerp(Local.GetTranslation(), Planted, Weight));
				}
			}
			continue;
		}
		// Rotations only: each skeleton keeps its own bone lengths
		FTransform& Local = Output.Pose[Index];
		// Keep the pelvis-to-chest transition soft without weakening the authored arm/hand motion.
		const float BodyShare = bFullStroke ? 1.f : Index == Spine ? 0.65f
			: Index == MiddleSpine ? 0.85f : 1.f;
		if (bGuard)
		{
			const float Weight = FMath::SmoothStep(0.f, 1.f, Melee.GuardWeight) * BodyShare;
			Local.SetRotation(FQuat::Slerp(Local.GetRotation(), GuardPose[Index].GetRotation(), Weight).GetNormalized());
		}
		if (bSlash)
		{
			Local.SetRotation(FQuat::Slerp(Local.GetRotation(), SlashPose[Index].GetRotation(), Melee.SlashWeight * BodyShare).GetNormalized());
		}
	}
	const bool bSwordBody = bFullStroke || (Melee.bDedicated && (bGuard || Melee.ParryWeight > 0.f || bGuardContacts));
	if (bSwordBody && (Melee.DriveWeight > 0.f || ContactWeight > 0.f))
	{
		FCompactPose Reference;
		Reference.SetBoneContainer(&Bones);
		Reference.ResetToRefPose();
		const FCompactPoseBoneIndex Chest = FindBone(Bones, TEXT("Spine2"));
		const FCompactPoseBoneIndex Feet[] = {FindBone(Bones, TEXT("LeftFoot")), FindBone(Bones, TEXT("RightFoot"))};
		const FCompactPoseBoneIndex Knees[] = {FindBone(Bones, TEXT("LeftLeg")), FindBone(Bones, TEXT("RightLeg"))};
		const FCompactPoseBoneIndex Thighs[] = {LeftLeg, RightLeg};
		const FCompactPoseBoneIndex Toes[] = {LeftToe, RightToe};
		if (Hips.IsValid() && Feet[0].IsValid() && Feet[1].IsValid() && Knees[0].IsValid() && Knees[1].IsValid()
			&& LeftLeg.IsValid() && RightLeg.IsValid())
		{
			const float Weight = Melee.DriveWeight * Melee.LowerBodyWeight;
			const float SolveWeight = FMath::Max(Weight, ContactWeight);
			FTransform PlantedFeet[2];
			FQuat PlantedToeRotations[2];
			FVector ClipOrigin = bFullStroke ? GetMeshSpaceTransform(Output.Pose, Hips).GetLocation() - GetMeshSpaceTransform(SlashPose, Hips).GetLocation() : FVector::ZeroVector;
			ClipOrigin.Z = 0.f;
			for (int32 I = 0; I < 2; ++I)
			{
				FTransform AuthoredFoot = bFullStroke ? GetMeshSpaceTransform(SlashPose, Feet[I]) : BaseFeet[I];
				AuthoredFoot.AddToTranslation(ClipOrigin);
				// Blend ankle targets in mesh space. Blending hip/leg rotations first
				// swung the supporting ankle up in an arc during the attack's entry.
				PlantedFeet[I].Blend(BaseFeet[I], AuthoredFoot, Weight);
				if (ContactWeight > 0.f && Toes[I].IsValid())
				{
					const FTransform StartFoot = (bRecovering ? RecoveryFeet[I] : ContactFeet[I]) * Melee.SupportToWorld;
					const FTransform StartToe = (bRecovering ? RecoveryToes[I] : ContactToes[I]) * Melee.SupportToWorld;
					FVector ToePosition = StartToe.GetLocation();
					FQuat FootRotation = StartFoot.GetRotation();
					FQuat ToeRotation = StartToe.GetRotation();
					if (bGuardContacts)
					{
						// Both feet brace on the ground while hips and knees absorb a parry.
					}
					else if (bRecovering)
					{
						const float Age = (Melee.AttackAge - RecoveryStartAge) / Melee.AttackDuration;
						const float Begin = I == ContactLeadFoot ? 0.72f : 0.f;
						const float Step = FMath::SmoothStep(Begin, Begin + 0.62f, Age);
						const float Lift = FMath::Sin(PI * Step);
						const FTransform Goal = RecoveryGoals[I] * Melee.SupportToWorld;
						ToePosition = FMath::Lerp(ToePosition, Goal.GetLocation(), Step)
							+ Melee.FloorNormal * (2.6f * Melee.BodyScale * Melee.MeshToWorld.GetScale3D().Z * Lift);
						FootRotation = FQuat::Slerp(FootRotation, Melee.SupportToWorld.GetRotation() * RecoveryFootRotations[I], Step).GetNormalized();
						ToeRotation = FQuat::Slerp(ToeRotation, Goal.GetRotation(), Step).GetNormalized();
						const FQuat ToeLift(Melee.MeshToWorld.GetUnitAxis(EAxis::X), FMath::DegreesToRadians(7.f * Lift));
						FootRotation = ToeLift * FootRotation;
						ToeRotation = ToeLift * ToeRotation;
					}
					else if (I == ContactLeadFoot)
					{
						const float Step = FMath::SmoothStep(0.04f, 0.36f, Melee.FootPhase);
						const float Lift = FMath::Sin(PI * Step);
						const FVector Goal = Melee.SupportToWorld.TransformPosition(LandingToe);
						// A recovery foot may already be near its apex. Carry that height
						// into the next cut instead of adding a second full lift on top.
						const float ExistingLift = FMath::Max(0.f, static_cast<float>(FVector::DotProduct(ToePosition - Goal, Melee.FloorNormal)));
						const float LiftRoom = FMath::Max(0.f, 2.8f * Melee.BodyScale * static_cast<float>(Melee.MeshToWorld.GetScale3D().Z) - ExistingLift);
						ToePosition = FMath::Lerp(ToePosition, Goal, Step) + Melee.FloorNormal * (LiftRoom * Lift);
						FootRotation = FQuat::Slerp(FootRotation, Melee.SupportToWorld.GetRotation() * LandingFootRotation, Step).GetNormalized();
						ToeRotation = FQuat::Slerp(ToeRotation, Melee.SupportToWorld.GetRotation() * LandingToeRotation, Step).GetNormalized();
						const FQuat ToeLift(Melee.MeshToWorld.GetUnitAxis(EAxis::X), FMath::DegreesToRadians(8.f * Lift));
						FootRotation = ToeLift * FootRotation;
						ToeRotation = ToeLift * ToeRotation;
					}
					else
					{
						// Push through the planted toe: the ankle/heel rolls forward and up,
						// while both the toe pivot and its ground-facing orientation stay put.
						const FVector FootForward = FVector::VectorPlaneProject(FootRotation.RotateVector(Output.Pose[Toes[I]].GetTranslation()), Melee.FloorNormal).GetSafeNormal();
						const FVector RollAxis = FVector::CrossProduct(FootForward, Melee.FloorNormal).GetSafeNormal();
						const float Push = FMath::SmoothStep(0.06f, 0.26f, Melee.FootPhase) * (1.f - FMath::SmoothStep(0.66f, 0.96f, Melee.FootPhase));
						FootRotation = FQuat(RollAxis, FMath::DegreesToRadians(-24.f * Push)) * FootRotation;
					}
					const FQuat ComponentRotation = (Melee.MeshToWorld.GetRotation().Inverse() * FootRotation).GetNormalized();
					const FVector Ankle = Melee.MeshToWorld.InverseTransformPosition(ToePosition)
						- ComponentRotation.RotateVector(Output.Pose[Toes[I]].GetTranslation() * PlantedFeet[I].GetScale3D());
					const FTransform ContactTarget(ComponentRotation, Ankle, PlantedFeet[I].GetScale3D());
					PlantedFeet[I].Blend(PlantedFeet[I], ContactTarget, ContactWeight);
					PlantedToeRotations[I] = (Melee.MeshToWorld.GetRotation().Inverse() * ToeRotation).GetNormalized();
				}
			}
			const FQuat HipTurn = GetMeshSpaceTransform(Output.Pose, Hips).GetRotation()
				* GetMeshSpaceTransform(Reference, Hips).GetRotation().Inverse();
			const FVector HipForward = HipTurn.RotateVector(FVector::RightVector);
			const float HipYaw = FMath::Atan2(-HipForward.X, HipForward.Y);
			const FVector BaseHipForward = (BaseHipRotation * GetMeshSpaceTransform(Reference, Hips).GetRotation().Inverse()).RotateVector(FVector::RightVector);
			const float BaseYaw = FMath::Atan2(-BaseHipForward.X, BaseHipForward.Y);
			const float TargetYaw = BaseYaw + FMath::FindDeltaAngleRadians(BaseYaw, FMath::DegreesToRadians(Melee.HipYaw)) * Weight;
			TurnBone(Output.Pose, Hips, FQuat(FVector::UpVector,
				FMath::FindDeltaAngleRadians(HipYaw, TargetYaw)));
			Output.Pose[Hips].AddToTranslation(FVector(Melee.ParrySway * Weight, Melee.WeightShift * Weight, -1.2f * Melee.StanceLoad * Weight) * Melee.BodyScale);
			if (bRecovering && ContactWeight > 0.f)
			{
				const float Age = (Melee.AttackAge - RecoveryStartAge) / Melee.AttackDuration;
				const float Gather = FMath::Sin(PI * FMath::SmoothStep(0.f, 0.62f, Age));
				const float Settle = FMath::Sin(PI * FMath::SmoothStep(0.72f, 1.34f, Age));
				const float Side = ContactLeadFoot == 0 ? 1.f : -1.f;
				Output.Pose[Hips].AddToTranslation(FVector(Side * 0.7f * (Gather - Settle), 0.6f * Gather,
					-0.55f * (Gather + Settle)) * ContactWeight * Melee.BodyScale);
			}
			// Fit the pelvis into both leg reach spheres before solving either leg.
			// This preserves the ground anchors without stretching the short fox legs,
			// including the held stance after the upper body returns to ready.
			for (int32 Pass = 0; Pass < 8 && ContactWeight > 0.f; ++Pass)
			{
				for (int32 I = 0; I < 2; ++I)
				{
					const FVector Delta = PlantedFeet[I].GetLocation() - GetMeshSpaceTransform(Output.Pose, Thighs[I]).GetLocation();
					const float Reach = 0.97f * (Output.Pose[Knees[I]].GetTranslation().Size() + Output.Pose[Feet[I]].GetTranslation().Size());
					if (Delta.Size() <= Reach) { continue; }
					FVector Offset = Delta.GetSafeNormal() * (Delta.Size() - Reach);
					const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Hips);
					if (Parent.IsValid()) { Offset = GetMeshSpaceTransform(Output.Pose, Parent).InverseTransformVector(Offset); }
					Output.Pose[Hips].AddToTranslation(Offset);
				}
			}
			// Rotate the pelvis over the planted feet, then solve the unchanged leg lengths.
			for (int32 I = 0; I < 2 && SolveWeight > KINDA_SMALL_NUMBER; ++I)
			{
				const FVector Root = GetMeshSpaceTransform(Output.Pose, Thighs[I]).GetLocation();
				const FVector Target = PlantedFeet[I].GetLocation();
				const float UpperLength = Output.Pose[Knees[I]].GetTranslation().Size();
				const float LowerLength = Output.Pose[Feet[I]].GetTranslation().Size();
				const FVector Direction = (Target - Root).GetSafeNormal();
				const float Distance = FMath::Clamp(static_cast<float>((Target - Root).Size()), 0.1f, UpperLength + LowerLength - 0.01f);
				const float Along = (UpperLength * UpperLength - LowerLength * LowerLength + Distance * Distance) / (2.f * Distance);
				const FVector Pole(I == 0 ? 0.12f : -0.12f, 1.f, 0.f);
				const FVector Bend = (Pole - Direction * FVector::DotProduct(Pole, Direction)).GetSafeNormal();
				const FVector Knee = Root + Direction * Along + Bend * FMath::Sqrt(FMath::Max(0.f, UpperLength * UpperLength - Along * Along));
				const FVector BeforeKnee = GetMeshSpaceTransform(Output.Pose, Knees[I]).GetLocation();
				TurnBone(Output.Pose, Thighs[I], FQuat::FindBetweenNormals((BeforeKnee - Root).GetSafeNormal(), (Knee - Root).GetSafeNormal()));
				const FVector AtKnee = GetMeshSpaceTransform(Output.Pose, Knees[I]).GetLocation();
				const FVector AtFoot = GetMeshSpaceTransform(Output.Pose, Feet[I]).GetLocation();
				TurnBone(Output.Pose, Knees[I], FQuat::FindBetweenNormals((AtFoot - AtKnee).GetSafeNormal(), (Target - AtKnee).GetSafeNormal()));
				// Position-only IK retained the imported thigh/shin axial roll. With a
				// planted ankle this reached nearly 180 degrees and collapsed the skin
				// between the shin and paw even though every bone kept its scale/length.
				// Rebuild each segment's orientation from the fox's bind frame, then
				// fade only this twist correction with the lower-body attack weight.
				const FQuat SolvedThigh = GetMeshSpaceTransform(Output.Pose, Thighs[I]).GetRotation();
				const FQuat SolvedShin = GetMeshSpaceTransform(Output.Pose, Knees[I]).GetRotation();
				const FVector RefThigh = GetMeshSpaceTransform(Reference, Thighs[I]).GetLocation();
				const FVector RefKnee = GetMeshSpaceTransform(Reference, Knees[I]).GetLocation();
				const FVector RefAnkle = GetMeshSpaceTransform(Reference, Feet[I]).GetLocation();
				const FVector SolvedKnee = GetMeshSpaceTransform(Output.Pose, Knees[I]).GetLocation();
				const FVector SolvedAnkle = GetMeshSpaceTransform(Output.Pose, Feet[I]).GetLocation();
				const FQuat ThighFrame = FQuat::FindBetweenNormals((RefKnee - RefThigh).GetSafeNormal(), (SolvedKnee - Root).GetSafeNormal())
					* GetMeshSpaceTransform(Reference, Thighs[I]).GetRotation();
				const FQuat ShinFrame = FQuat::FindBetweenNormals((RefAnkle - RefKnee).GetSafeNormal(), (SolvedAnkle - SolvedKnee).GetSafeNormal())
					* GetMeshSpaceTransform(Reference, Knees[I]).GetRotation();
				const FQuat ThighRotation = FQuat::Slerp(SolvedThigh, ThighFrame, SolveWeight).GetNormalized();
				Output.Pose[Thighs[I]].SetRotation((GetMeshSpaceTransform(Output.Pose, Output.Pose.GetParentBoneIndex(Thighs[I])).GetRotation().Inverse()
					* ThighRotation).GetNormalized());
				const FQuat ShinRotation = FQuat::Slerp(SolvedShin, ShinFrame, SolveWeight).GetNormalized();
				Output.Pose[Knees[I]].SetRotation((ThighRotation.Inverse() * ShinRotation).GetNormalized());
				Output.Pose[Feet[I]].SetRotation((GetMeshSpaceTransform(Output.Pose, Knees[I]).GetRotation().Inverse()
					* PlantedFeet[I].GetRotation()).GetNormalized());
				if (ContactWeight > 0.f && Toes[I].IsValid())
				{
					const FQuat ToeRotation = FQuat::Slerp(GetMeshSpaceTransform(Output.Pose, Toes[I]).GetRotation(), PlantedToeRotations[I], ContactWeight).GetNormalized();
					Output.Pose[Toes[I]].SetRotation((PlantedFeet[I].GetRotation().Inverse() * ToeRotation).GetNormalized());
				}
			}
		}
		if (Chest.IsValid())
		{
			const FQuat Desired = FQuat(FVector::UpVector, FMath::DegreesToRadians(Melee.ChestYaw))
				* FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-Melee.ChestLean))
				* GetMeshSpaceTransform(Reference, Chest).GetRotation();
			const FQuat RestChest = bFullStroke ? BaseChestRotation : GetMeshSpaceTransform(Output.Pose, Chest).GetRotation();
			const FQuat Target = FQuat::Slerp(RestChest, Desired, Melee.DriveWeight).GetNormalized();
			const FQuat Drive = (Target * GetMeshSpaceTransform(Output.Pose, Chest).GetRotation().Inverse()).GetNormalized();
			TurnBone(Output.Pose, Spine, FQuat::Slerp(FQuat::Identity, Drive, 0.45f));
			if (MiddleSpine.IsValid()) { TurnBone(Output.Pose, MiddleSpine, FQuat::Slerp(FQuat::Identity, Drive, 0.35f)); }
			TurnBone(Output.Pose, Chest, FQuat::Slerp(FQuat::Identity, Drive, 0.20f));
		}
	}
	if (bFullStroke && bSlash && bGroundable && Melee.LowerBodyWeight > 0.f && ContactWeight <= 0.f)
	{
		// Retargeted leg lengths must not bury the planted foot. Keep the lower toe on the locomotion floor;
		// the other foot retains its authored lift and step. Convert the mesh-space correction to hips-parent space.
		const float FootFloor = FMath::Min(GetMeshSpaceTransform(Output.Pose, LeftToe).GetLocation().Z,
			GetMeshSpaceTransform(Output.Pose, RightToe).GetLocation().Z);
		FVector Offset(0.f, 0.f, (Floor - FootFloor) * Melee.SlashWeight * Melee.LowerBodyWeight);
		const FCompactPoseBoneIndex Parent = Output.Pose.GetParentBoneIndex(Hips);
		if (Parent.IsValid())
		{
			Offset = GetMeshSpaceTransform(Output.Pose, Parent).InverseTransformVector(Offset);
		}
		Output.Pose[Hips].AddToTranslation(Offset);
	}
}

void FRRRandomBodyAnimInstanceProxy::EvaluateMovingLegs(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex Hips = FindBone(Bones, TEXT("Hips"));
	const FCompactPoseBoneIndex Legs[] = {FindBone(Bones, TEXT("LeftUpLeg")), FindBone(Bones, TEXT("RightUpLeg"))};
	FCompactPose Blended, Sample;
	Blended.SetBoneContainer(&Bones);
	Sample.SetBoneContainer(&Bones);
	FBlendedCurve Curve;
	UE::Anim::FStackAttributeContainer Attributes;
	bool bFirst = true;
	for (int32 Direction = 0; Direction < 4; ++Direction)
	{
		const float Weight = Aim.MoveWeights[Direction];
		if (Weight <= KINDA_SMALL_NUMBER) { continue; }
		Curve.InitFrom(Output.Curve);
		Attributes.Empty();
		FAnimationPoseData Data(Sample, Curve, Attributes);
		const UAnimSequence* Sequence = Aim.Animations.Move[Direction];
		Sequence->GetAnimationPose(Data, FAnimExtractContext(double(Aim.RunPhase * Sequence->GetPlayLength()), false));
		for (const FCompactPoseBoneIndex Index : Sample.ForEachBoneIndex())
		{
			if (bFirst) { Blended[Index] = Sample[Index] * ScalarRegister(Weight); }
			else { Blended[Index].AccumulateWithShortestRotation(Sample[Index], ScalarRegister(Weight)); }
		}
		bFirst = false;
	}
	if (bFirst) { return; }
	Blended.NormalizeRotations();
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		bool bLeg = Index == Hips;
		for (FCompactPoseBoneIndex Walk = Index; Walk.IsValid(); Walk = Output.Pose.GetParentBoneIndex(Walk))
		{
			if (Walk == Legs[0] || Walk == Legs[1]) { bLeg = true; break; }
		}
		if (bLeg)
		{
			Output.Pose[Index].SetRotation(FQuat::Slerp(Output.Pose[Index].GetRotation(), Blended[Index].GetRotation(), Aim.StrafeWeight).GetNormalized());
		}
	}
}

void FRRRandomBodyAnimInstanceProxy::EvaluateBodyFacing(FPoseContext& Output)
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex Hips = FindBone(Bones, TEXT("Hips"));
	if (!Hips.IsValid()) { return; }
	// Planted sword contacts already solve in world space. Never rotate those feet away from their anchors.
	const float Lower = Aim.LowerTurn * (1.f - ContactWeight);
	TurnBone(Output.Pose, Hips, FQuat(FVector::UpVector, FMath::DegreesToRadians(Lower)));
	const float Upper = FMath::FindDeltaAngleDegrees(Lower, Aim.UpperTurn);
	for (int32 I = 0; I < UE_ARRAY_COUNT(SpineBones); ++I)
	{
		const FCompactPoseBoneIndex Spine = FindBone(Bones, SpineBones[I]);
		if (Spine.IsValid()) { TurnBone(Output.Pose, Spine, FQuat(FVector::UpVector, FMath::DegreesToRadians(Upper * SpineShares[I]))); }
	}
	// Changing rifle clips can turn the chest faster than the heading blend itself.
	// Bound just the entry into combat; the sword's fast strike is left untouched.
	const FCompactPoseBoneIndex Chest = FindBone(Bones, TEXT("Spine2"));
	if (!Chest.IsValid()) { return; }
	const FQuat MeshRotation = Melee.MeshToWorld.GetRotation();
	FQuat World = (MeshRotation * GetMeshSpaceTransform(Output.Pose, Chest).GetRotation()).GetNormalized();
	if (Aim.bCombatFacing && !bWasCombatFacing) { bEaseUpperEntry = bHasUpperHistory; UpperEntryAge = 0.f; }
	bWasCombatFacing = Aim.bCombatFacing;
	if (bEaseUpperEntry && bHasUpperHistory)
	{
		UpperEntryAge += Melee.DeltaSeconds;
		const float Angle = LastUpperWorld.AngularDistance(World);
		const float Step = FMath::DegreesToRadians(480.f) * Melee.DeltaSeconds;
		const FQuat Eased = FQuat::Slerp(LastUpperWorld, World, Angle > SMALL_NUMBER ? FMath::Min(Step / Angle, 1.f) : 1.f).GetNormalized();
		const FQuat Correction = (MeshRotation.Inverse() * Eased * World.Inverse() * MeshRotation).GetNormalized();
		for (int32 I = 0; I < UE_ARRAY_COUNT(SpineBones); ++I)
		{
			const FCompactPoseBoneIndex Spine = FindBone(Bones, SpineBones[I]);
			if (Spine.IsValid()) { TurnBone(Output.Pose, Spine, FQuat::Slerp(FQuat::Identity, Correction, SpineShares[I])); }
		}
		World = Eased;
		if (UpperEntryAge >= (Melee.bDedicated ? 0.1f : 0.25f) && Angle <= Step) { bEaseUpperEntry = false; }
	}
	LastUpperWorld = World;
	bHasUpperHistory = true;
}

void FRRRandomBodyAnimInstanceProxy::EvaluateGaze(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FCompactPoseBoneIndex Neck = FindBone(Bones, TEXT("Neck"));
	const FCompactPoseBoneIndex Head = FindBone(Bones, TEXT("Head"));
	if (Neck.IsValid() && Head.IsValid() && !Aim.GazeFacing.IsNearlyZero())
	{
		FCompactPose Reference;
		Reference.SetBoneContainer(&Bones);
		Reference.ResetToRefPose();
		// Round 99: whatever asked for the look, the head goes no further than HeadTurnMax to either side of where the chest
		// faces now (the pose as it stands: the aim, a sword's wind-up, a turn under way)
		FVector Facing = Aim.GazeFacing;
		if (const FCompactPoseBoneIndex Chest = FindBone(Bones, TEXT("Spine2")); Chest.IsValid())
		{
			const FVector ChestForward = (GetMeshSpaceTransform(Output.Pose, Chest).GetRotation() * GetMeshSpaceTransform(Reference, Chest).GetRotation().Inverse()).RotateVector(FVector::RightVector);
			const float Off = FMath::FindDeltaAngleRadians(FMath::Atan2(-ChestForward.X, ChestForward.Y), FMath::Atan2(-Facing.X, Facing.Y));
			const float Limit = FMath::DegreesToRadians(HeadTurnMax);
			if (FMath::Abs(Off) > Limit)
			{
				Facing = FQuat(FVector::UpVector, FMath::Clamp(Off, -Limit, Limit) - Off).RotateVector(Facing);
				if (FMath::Abs(Off) > FMath::DegreesToRadians(HeadTurnBodyAt) && CVarLogHeadTurn.GetValueOnAnyThread() != 0)
				{
					UE_LOG(LogTemp, Log, TEXT("RRRandom: head turn: asked %.0f degrees off the chest, held to %.0f."), FMath::RadiansToDegrees(Off), HeadTurnMax);
				}
			}
		}
		const FQuat Desired = FQuat::FindBetweenNormals(FVector::RightVector, Facing)
			* GetMeshSpaceTransform(Reference, Head).GetRotation();
		const FQuat Current = GetMeshSpaceTransform(Output.Pose, Head).GetRotation();
		const FQuat Correction = (Desired * Current.Inverse()).GetNormalized();
		const float Weight = Aim.GazeWeight;
		TurnBone(Output.Pose, Neck, FQuat::Slerp(FQuat::Identity, Correction, Weight * 0.65f));
		TurnBone(Output.Pose, Head, FQuat::Slerp(FQuat::Identity, Correction, Weight * 0.35f));
	}
}

void FRRRandomBodyAnimInstanceProxy::EvaluateSwordGrip(FPoseContext& Output) const
{
	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	FCompactPose Reference;
	Reference.SetBoneContainer(&Bones);
	Reference.ResetToRefPose();
	const FCompactPoseBoneIndex Chest = FindBone(Bones, TEXT("Spine2"));
	const FCompactPoseBoneIndex Head = FindBone(Bones, TEXT("Head"));
	if (!Chest.IsValid() || !Head.IsValid()) { return; }
	// A human-sized torso wind-up puts this short-armed fox's trailing shoulder
	// behind its chest while both wrists remain ahead. Limit the chest turn before
	// solving the arms, distributing the correction up the spine. Keep the eyes aimed.
	const FQuat HeadAim = GetMeshSpaceTransform(Output.Pose, Head).GetRotation();
	const FQuat ChestReference = GetMeshSpaceTransform(Reference, Chest).GetRotation();
	FQuat ChestTurn = GetMeshSpaceTransform(Output.Pose, Chest).GetRotation() * ChestReference.Inverse();
	const FVector ChestForward = ChestTurn.RotateVector(FVector::RightVector);
	const float Yaw = FMath::Atan2(-ChestForward.X, ChestForward.Y);
	// During a ground-up drive the hilt follows the chest, so the trailing arm
	// retains clearance through a full torso turn. Guard/idle keep the compact limit.
	const float Limit = FMath::DegreesToRadians(FMath::Lerp(12.f, 36.f, Melee.DriveWeight));
	const float Correction = (FMath::Clamp(Yaw, -Limit, Limit) - Yaw) * Melee.GripWeight;
	const TCHAR* SpineNames[] = {TEXT("Spine"), TEXT("Spine1"), TEXT("Spine2")};
	const float Shares[] = {0.5f, 0.3f, 0.2f};
	for (int32 I = 0; I < 3; ++I)
	{
		const FCompactPoseBoneIndex Bone = FindBone(Bones, SpineNames[I]);
		if (Bone.IsValid()) { TurnBone(Output.Pose, Bone, FQuat(FVector::UpVector, Correction * Shares[I])); }
	}
	Output.Pose[Head].SetRotation((GetMeshSpaceTransform(Output.Pose, Output.Pose.GetParentBoneIndex(Head)).GetRotation().Inverse() * HeadAim).GetNormalized());
	ChestTurn = (GetMeshSpaceTransform(Output.Pose, Chest).GetRotation() * ChestReference.Inverse()).GetNormalized();
	struct FArm
	{
		FCompactPoseBoneIndex Shoulder, Upper, Fore, Hand, Index;
		FVector Root, PalmOffset;
		FQuat HandRotation;
		float Reach = 0.f;
		float Side = 0.f;
	};
	FArm Arms[2];
	const TCHAR* Sides[2] = {TEXT("Right"), TEXT("Left")};
	for (int32 I = 0; I < 2; ++I)
	{
		const FString Side(Sides[I]);
		FArm& Arm = Arms[I];
		Arm.Shoulder = FindBone(Bones, FName(Side + TEXT("Shoulder")));
		Arm.Upper = FindBone(Bones, FName(Side + TEXT("Arm")));
		Arm.Fore = FindBone(Bones, FName(Side + TEXT("ForeArm")));
		Arm.Hand = FindBone(Bones, FName(Side + TEXT("Hand")));
		Arm.Index = FindBone(Bones, FName(Side + TEXT("HandIndex1")));
		Arm.Side = I == 0 ? -1.f : 1.f;
		if (!Arm.Shoulder.IsValid() || !Arm.Upper.IsValid() || !Arm.Fore.IsValid() || !Arm.Hand.IsValid() || !Arm.Index.IsValid())
		{
			return;
		}
	}
	// Start from the fox's bind rotations. The arm segments retain their bind lengths
	// and scales; the scapula alone glides over this character's deep, rounded chest.
	TArray<FCompactPoseBoneIndex, TInlineAllocator<24>> ArmBones;
	TArray<FTransform, TInlineAllocator<24>> Before;
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		for (FCompactPoseBoneIndex Walk = Index; Walk.IsValid(); Walk = Output.Pose.GetParentBoneIndex(Walk))
		{
			if (Walk == Arms[0].Shoulder || Walk == Arms[1].Shoulder)
			{
				ArmBones.Add(Index);
				Before.Add(Output.Pose[Index]);
				Output.Pose[Index].SetRotation(Reference[Index].GetRotation());
				break;
			}
		}
	}
	const FQuat RightFix = Melee.RightHandFix;
	const FQuat RightRestGrip = GetMeshSpaceTransform(Reference, Arms[0].Hand).GetRotation() * RightFix;
	const auto Mirror = [](FVector V) { V.X = -V.X; return V; };
	const FQuat LeftRestGrip = FRotationMatrix::MakeFromXZ(Mirror(RightRestGrip.GetAxisX()), Mirror(RightRestGrip.GetAxisZ())).ToQuat();
	const FQuat LeftFix = GetMeshSpaceTransform(Reference, Arms[1].Hand).GetRotation().Inverse() * LeftRestGrip;
	const FQuat Fixes[2] = {RightFix, LeftFix};
	// A small lag carries the loaded hands behind the chest, then the hand path
	// releases the blade. Keeping this in the chest's turning frame avoids the old
	// across-the-ribcage reach when the trailing shoulder rotates backward.
	const FQuat Carry(FVector::UpVector, FMath::DegreesToRadians(Melee.GripCarryYaw * Melee.DriveWeight));
	const FQuat GripRotation = Carry * Melee.GripRotation;
	const FVector Blade = GripRotation.GetAxisX();
	// Anchor the hilt to the chest BEFORE protraction. Moving it forward with the
	// clavicles would consume the elbow bend that provides clearance around the shirt.
	const FVector GripOrigin = (GetMeshSpaceTransform(Output.Pose, Arms[0].Upper).GetLocation()
		+ GetMeshSpaceTransform(Output.Pose, Arms[1].Upper).GetLocation()) * 0.5f;
	for (int32 I = 0; I < 2; ++I)
	{
		FArm& Arm = Arms[I];
		const FCompactPoseBoneIndex ShoulderParent = Output.Pose.GetParentBoneIndex(Arm.Shoulder);
		// The rising parry reverses with the wrist low. Give its loaded shoulder
		// extra forward clearance, fading it with the whole-body reaction.
		const FVector ScapulaGlide = ChestTurn.RotateVector(FVector(0.f, 4.f + 1.5f * Melee.ParryWeight, 0.f) * Melee.BodyScale);
		Output.Pose[Arm.Shoulder].AddToTranslation(GetMeshSpaceTransform(Output.Pose, ShoulderParent).InverseTransformVector(ScapulaGlide));
		// Protract the clavicle with the reach, bringing the whole upper arm ahead
		// of the shirt instead of asking the elbow to pull through the ribcage.
		TurnBone(Output.Pose, Arm.Shoulder, FQuat(ChestTurn.RotateVector(FVector::UpVector),
			FMath::DegreesToRadians(45.f * Arm.Side)));
		Arm.Root = GetMeshSpaceTransform(Output.Pose, Arm.Upper).GetLocation();
		Arm.Reach = (Output.Pose[Arm.Fore].GetTranslation().Size() + Output.Pose[Arm.Hand].GetTranslation().Size()) * 0.97f;
		Arm.HandRotation = (GripRotation * Fixes[I].Inverse()).GetNormalized();
		Arm.PalmOffset = Arm.HandRotation.RotateVector(Reference[Arm.Index].GetTranslation() * 0.5f);
	}
	FVector Center = GripOrigin + Carry.RotateVector((Melee.GripOffset + FVector(0.f, 4.5f, 0.f)) * Melee.BodyScale);
	const float HalfGrip = 2.1f * Melee.BodyScale;
	const auto Wrist = [&](int32 I) { return Center + Blade * (I == 0 ? HalfGrip : -HalfGrip) - Arms[I].PalmOffset; };
	// Move the shared hilt into the overlap of both reach spheres. Independently
	// clamping two wrists would let the off hand drift away from the handle.
	for (int32 Pass = 0; Pass < 6; ++Pass)
	{
		for (int32 I = 0; I < 2; ++I)
		{
			const FVector Delta = Wrist(I) - Arms[I].Root;
			if (Delta.Size() > Arms[I].Reach)
			{
				Center -= Delta.GetSafeNormal() * (Delta.Size() - Arms[I].Reach);
			}
		}
	}
	for (int32 I = 0; I < 2; ++I)
	{
		const FArm& Arm = Arms[I];
		const FVector Target = Wrist(I);
		const float UpperLength = Output.Pose[Arm.Fore].GetTranslation().Size();
		const float ForeLength = Output.Pose[Arm.Hand].GetTranslation().Size();
		const FVector Direction = (Target - Arm.Root).GetSafeNormal();
		const float Distance = FMath::Clamp(static_cast<float>((Target - Arm.Root).Size()), 0.1f, UpperLength + ForeLength - 0.01f);
		const float Along = (UpperLength * UpperLength - ForeLength * ForeLength + Distance * Distance) / (2.f * Distance);
		// The elbow stays outside AND ahead of the chest. Following the chest basis
		// is essential: a fixed mesh-space pole folded the trailing arm into the shirt.
		const FVector Pole = ChestTurn.RotateVector(FVector(Arm.Side * 20.f, 12.f + 4.f * Melee.ParryWeight, 0.f));
		const FVector Bend = (Pole - Direction * FVector::DotProduct(Pole, Direction)).GetSafeNormal();
		const FVector Elbow = Arm.Root + Direction * Along + Bend * FMath::Sqrt(FMath::Max(0.f, UpperLength * UpperLength - Along * Along));
		const FVector OriginalUpper = GetMeshSpaceTransform(Output.Pose, Arm.Fore).GetLocation() - Arm.Root;
		TurnBone(Output.Pose, Arm.Upper, FQuat::FindBetweenNormals(OriginalUpper.GetSafeNormal(), (Elbow - Arm.Root).GetSafeNormal()));
		const FVector CurrentElbow = GetMeshSpaceTransform(Output.Pose, Arm.Fore).GetLocation();
		const FVector CurrentFore = GetMeshSpaceTransform(Output.Pose, Arm.Hand).GetLocation() - CurrentElbow;
		TurnBone(Output.Pose, Arm.Fore, FQuat::FindBetweenNormals(CurrentFore.GetSafeNormal(), (Target - CurrentElbow).GetSafeNormal()));
		const FQuat Parent = GetMeshSpaceTransform(Output.Pose, Arm.Fore).GetRotation();
		Output.Pose[Arm.Hand].SetRotation((Parent.Inverse() * Arm.HandRotation).GetNormalized());
	}
	for (int32 I = 0; I < ArmBones.Num(); ++I)
	{
		Output.Pose[ArmBones[I]].Blend(Before[I], Output.Pose[ArmBones[I]], Melee.GripWeight);
	}
}
