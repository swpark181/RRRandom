#include "RRRandomBodyAnimInstance.h"
#include "RRRandomCharacter.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AttributesRuntime.h"
#include "Components/SkeletalMeshComponent.h"

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
}

FAnimInstanceProxy* URRRandomBodyAnimInstance::CreateAnimInstanceProxy()
{
	return new FRRRandomBodyAnimInstanceProxy(this);
}

void URRRandomBodyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	UpdateAim(DeltaSeconds);

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
	Lift = FMath::FInterpTo(Lift, RunLift * Run, DeltaSeconds, LiftSpeed);
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
	Aim.MoveWeight = FMath::FInterpTo(Aim.MoveWeight, FMath::Clamp(Speed / FMath::Max(AimRunSpeed, 1.f), 0.f, 1.f), DeltaSeconds, MoveBlendSpeed);
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
	return true;
}
