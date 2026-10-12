#include "RRRandomBodyAnimInstance.h"
#include "RRRandomCharacter.h"
#include "Animation/AnimNodeBase.h"
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

void FRRRandomBodyAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimSingleNodeInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);

	const URRRandomBodyAnimInstance* Instance = CastChecked<URRRandomBodyAnimInstance>(InAnimInstance);
	TailBones = Instance->GetTailBones();
	TailSwing = Instance->GetTailSwing();
	TailLift = Instance->GetTailLift();
}

bool FRRRandomBodyAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	FAnimSingleNodeInstanceProxy::Evaluate(Output);

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
