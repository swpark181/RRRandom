#include "RRRandomBodyAnimInstance.h"
#include "RRRandomCharacter.h"
#include "RRRandomMovementComponent.h"
#include "RRRandomVehicle.h"
#include "AnimationRuntime.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
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
}

void URRRandomBodyAnimInstance::UpdateSprintPose(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	const float Wanted = Brawler ? Brawler->GetSprintPoseShare() * (1.f - GetAimWeight()) : 0.f;
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

FAnimInstanceProxy* URRRandomBodyAnimInstance::CreateAnimInstanceProxy()
{
	return new FRRRandomBodyAnimInstanceProxy(this);
}

void URRRandomBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	UpdateAim(DeltaSeconds);
	if (DeltaSeconds > 0.f)
	{
		UpdateFlightPose(DeltaSeconds);
		UpdateJumpPose(DeltaSeconds);
		UpdateSprintPose(DeltaSeconds);
		UpdateRidePose(DeltaSeconds);
		UpdateWings(DeltaSeconds);
	}

	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	const AActor* Owner = GetOwningActor();
	const int32 Count = TailBoneNames.Num();
	if (!Mesh || !Owner || Count == 0 || DeltaSeconds <= 0.f)
	{
		return;
	}

	// The mesh's +X is the brawler's left (the rig's left leg side), so its right is -X
	const FTransform& MeshTransform = Mesh->GetComponentTransform();
	const FVector Right = -MeshTransform.GetUnitAxis(EAxis::X);
	const float Yaw = MeshTransform.Rotator().Yaw;
	const FVector Velocity = Owner->GetVelocity();
	if (!bHasLast)
	{
		bHasLast = true;
		LastYaw = Yaw;
		LastVelocity = Velocity;
	}

	// Turning: the tail keeps pointing where it did for a moment, so relative to the body it swings toward the turn's
	// outside (turning right leaves it bent to the right of the body)
	Swing += FMath::Clamp(FMath::FindDeltaAngleDegrees(LastYaw, Yaw), -MaxTurnPerFrame, MaxTurnPerFrame) * TurnDrag;
	// Speeding up sideways: it trails the other way while the push lasts
	const FVector Acceleration = ((Velocity - LastVelocity) / DeltaSeconds).GetClampedToMaxSize(MaxAcceleration);
	const float Trail = -FVector::DotProduct(Acceleration, Right) * SideAccelSwing;
	LastYaw = Yaw;
	LastVelocity = Velocity;

	// A damped spring back to straight (or to the trail), a little bouncy
	const float Omega = 2.f * PI * SpringFrequency;
	const float Stiffness = Omega * Omega;
	const float Damping = 2.f * FMath::Lerp(1.f, 0.15f, SpringBounce) * Omega;
	for (float Left = DeltaSeconds; Left > 0.f; Left -= MaxStep)
	{
		const float Step = FMath::Min(Left, MaxStep);
		SwingSpeed += (Stiffness * (Trail - Swing) - Damping * SwingSpeed) * Step;
		Swing += SwingSpeed * Step;
	}
	if (FMath::Abs(Swing) > MaxSwing)
	{
		Swing = FMath::Sign(Swing) * MaxSwing;
		SwingSpeed = 0.f;
	}

	// Wagging and lifting with speed; a knocked-out brawler's tail goes still
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(Owner);
	Life = FMath::FInterpConstantTo(Life, !Brawler || Brawler->IsAlive() ? 1.f : 0.f, DeltaSeconds, LifeFadeSpeed);
	const float Run = FMath::Clamp(Velocity.Size2D() / FMath::Max(WagRunSpeed, 1.f), 0.f, 1.f);
	WagPhase = FMath::Fmod(WagPhase + 2.f * PI * FMath::Lerp(WagFrequency.X, WagFrequency.Y, Run) * DeltaSeconds, 2.f * PI);
	// Riding, the tail lifts up off the seat and out of the vehicle's body
	Lift = FMath::FInterpTo(Lift, RunLift * Run + RideTailLift * RideWeight, DeltaSeconds, LiftSpeed);
	const float Wag = FMath::Lerp(WagAmplitude.X, WagAmplitude.Y, Run);
	const float TotalLift = (Lift + RunBob * Run * FMath::Sin(2.f * WagPhase)) * Life;

	// Spread over the bones, more toward the tip so the tail curls rather than pivoting stiffly at the root
	BoneSwing.SetNum(Count);
	BoneLift.SetNum(Count);
	const float Total = Count * (Count + 1) * 0.5f;
	for (int32 Bone = 0; Bone < Count; ++Bone)
	{
		const float Share = (Bone + 1) / Total;
		const float Delay = Count > 1 ? WagDelay * Bone / (Count - 1) : 0.f;
		BoneSwing[Bone] = (Swing + Wag * FMath::Sin(WagPhase - Delay)) * Share * Life;
		BoneLift[Bone] = TotalLift * (Count == 1 ? 1.f : (Bone == 0 ? RootLiftShare : (1.f - RootLiftShare) / (Count - 1)));
	}
}

void URRRandomBodyAnimInstance::UpdateAim(float DeltaSeconds)
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwningActor());
	const USkeletalMeshComponent* Mesh = GetSkelMeshComponent();
	if (!Brawler || !Mesh || !Brawler->GetAimAnimations().IsComplete() || DeltaSeconds <= 0.f)
	{
		Aim.Weight = 0.f;
		return;
	}
	const FRRAimAnimations& Animations = Brawler->GetAimAnimations();
	Aim.Animations = Animations;

	const bool bWanted = Brawler->WantsAimPose();
	Aim.Weight = FMath::FInterpConstantTo(Aim.Weight, bWanted ? 1.f : 0.f, DeltaSeconds,
		1.f / FMath::Max(bWanted ? AimInTime : AimOutTime, KINDA_SMALL_NUMBER));
	if (Aim.Weight <= 0.f)
	{
		// Starts from the standing aim next time
		Aim.MoveWeight = 0.f;
		Aim.FireWeight = 0.f;
		return;
	}

	// Which way it goes relative to where it faces: the runs are forward, backward, left, right. On a vehicle the legs
	// stand still.
	const FVector Velocity = Brawler->GetVehicle() ? FVector::ZeroVector : Brawler->GetVelocity();
	const float Speed = Velocity.Size2D();
	const float Ahead = FVector::DotProduct(Velocity, Brawler->GetActorForwardVector());
	const float Side = FVector::DotProduct(Velocity, Brawler->GetActorRightVector());
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
	Aim.RunPhase = FMath::Fmod(Aim.RunPhase + DeltaSeconds * Rate / FMath::Max(RunLength, KINDA_SMALL_NUMBER), 1.f);

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
	for (const FCompactPoseBoneIndex Index : Output.Pose.ForEachBoneIndex())
	{
		Output.Pose[Index].BlendWith(Blended[Index], Aim.Weight);
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

bool FRRRandomBodyAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	FAnimSingleNodeInstanceProxy::Evaluate(Output);

	if (Aim.Weight > 0.f && Aim.Animations.IsComplete())
	{
		EvaluateAim(Output);
		PointGun(Output);
	}

	const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
	const FReferenceSkeleton& RefSkeleton = Bones.GetReferenceSkeleton();
	const int32 Count = FMath::Min3(TailBones.Num(), TailSwing.Num(), TailLift.Num());
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

		// The tail points down the mesh's -Y: swinging right turns it about -Z, lifting it about -X
		const FQuat Bend = FQuat(FVector::UpVector, FMath::DegreesToRadians(-TailSwing[Bone]))
			* FQuat(FVector::ForwardVector, FMath::DegreesToRadians(-TailLift[Bone]));
		FTransform& Local = Output.Pose[Index];
		Local.SetRotation((ParentRotation.Inverse() * Bend * ParentRotation * Local.GetRotation()).GetNormalized());
	}

	// The flying or jumping pose, then the wings (roots before tips, so a tip's parent is already turned)
	TurnBones(Output, FlightBones, FlightBends);
	TurnBones(Output, JumpBones, JumpBends);
	TurnBones(Output, SprintBones, SprintBends);
	TurnBones(Output, RideBones, RideBends);
	TurnBones(Output, WingBones, WingBends);
	return true;
}
