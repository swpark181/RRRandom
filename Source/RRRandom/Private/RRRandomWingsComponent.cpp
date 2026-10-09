#include "RRRandomWingsComponent.h"
#include "RRRandomCharacter.h"
#include "RRRandomMovementComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The wings sit on this bone of the fox (Mixamo upper spine), so they ride along with the animation.
	// Without it (the mannequin) they stay where the owner attached them.
	const FName BackBone(TEXT("spine2"));
	// From that bone to the wing roots, in the brawler's own frame (forward, right, up), at normal size
	const FVector BackOffset(-16.f, 0.f, 4.f);

	// How fast the wings move from one pose to the next
	constexpr float PoseBlendSpeed = 8.f;
	// Rising faster than this counts as beating upward rather than gliding
	constexpr float RisingSpeed = 30.f;

	// Where on SM_Wing stars come off: this share of the span out to the tip, this far back from the leading edge
	constexpr float WingLength = 142.f;
	constexpr float SparkleSpanStart = 0.3f;
	constexpr float SparkleChordDepth = 40.f;
	// Star motion: a random push, a gentle sink, and air drag
	constexpr float SparkleKick = 45.f;
	constexpr float SparkleSink = 35.f;
	constexpr float SparkleDrag = 1.5f;
	// Stars twinkle by pulsing their size this much, this fast (radians a second)
	constexpr float TwinkleAmount = 0.3f;
	constexpr float TwinkleSpeed = 28.f;
}

URRRandomWingsComponent::URRRandomWingsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Wing(TEXT("/Game/Effects/Wings/SM_Wing.SM_Wing"));
	if (Wing.Succeeded())
	{
		WingMesh = Wing.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sparkle(TEXT("/Game/Effects/Wings/SM_WingSparkle.SM_WingSparkle"));
	if (Sparkle.Succeeded())
	{
		SparkleMesh = Sparkle.Object;
	}
}

void URRRandomWingsComponent::OnRegister()
{
	Super::OnRegister();

	// The wing and star meshes are made here so the owner only adds this component; only in game worlds
	const UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (LeftWing || !WingMesh || !Owner || !World || !World->IsGameWorld())
	{
		return;
	}
	auto Setup = [this](UStaticMeshComponent* Mesh)
	{
		Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->CastShadow = false;
		Mesh->bReceivesDecals = false;
		Mesh->SetupAttachment(this);
	};
	for (TObjectPtr<UStaticMeshComponent>* Slot : { &LeftWing, &RightWing })
	{
		UStaticMeshComponent* Wing = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Wing->SetStaticMesh(WingMesh);
		Setup(Wing);
		Wing->SetVisibleFlag(false);
		Wing->RegisterComponent();
		*Slot = Wing;
	}
	if (SparkleMesh)
	{
		// Stars live in the world (they stay behind as the brawler flies on): the component sits at the world origin
		Sparkles = NewObject<UInstancedStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		Sparkles->SetStaticMesh(SparkleMesh);
		Setup(Sparkles);
		Sparkles->SetUsingAbsoluteLocation(true);
		Sparkles->SetUsingAbsoluteRotation(true);
		Sparkles->SetUsingAbsoluteScale(true);
		Sparkles->RegisterComponent();
		Sparkles->SetWorldTransform(FTransform::Identity);
	}
}

void URRRandomWingsComponent::PlaceWing(UStaticMeshComponent* Wing, float Side, float Lift, float Sweep, float Size) const
{
	// The mesh is a right wing (span +Y, feathers toward -X); the left one is the same mesh mirrored across Y.
	// Sweep turns the tip back around the up axis, then Lift raises it around the forward axis.
	const FQuat SweepBack(FVector::UpVector, Side * FMath::DegreesToRadians(Sweep));
	const FQuat Raise(FVector::ForwardVector, Side * FMath::DegreesToRadians(Lift));
	Wing->SetRelativeTransform(FTransform(Raise * SweepBack, FVector(0.f, Side * WingRootGap * 0.5f, 0.f),
		FVector(Size, Side * Size, Size) * WingScale));
}

void URRRandomWingsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwner());
	if (!Brawler || !LeftWing || !RightWing)
	{
		return;
	}

	const UCharacterMovementComponent* Movement = Brawler->GetCharacterMovement();
	const URRRandomMovementComponent* RandomMovement = Cast<URRRandomMovementComponent>(Movement);
	const bool bInAir = Movement->IsFalling();
	const bool bGliding = RandomMovement && RandomMovement->bGliding && bInAir;
	const bool bWanted = Brawler->IsAlive() && (Brawler->CanFly() || bGliding);

	// A knockout drops them at once; otherwise they unfold and fold away
	Shown = Brawler->IsAlive() ? FMath::FInterpConstantTo(Shown, bWanted ? 1.f : 0.f, DeltaTime, ShowTime > 0.f ? 1.f / ShowTime : UE_BIG_NUMBER) : 0.f;
	const bool bShow = Shown > 0.f;
	const bool bJustShown = bShow && !LeftWing->IsVisible();
	if (LeftWing->IsVisible() != bShow)
	{
		LeftWing->SetVisibility(bShow);
		RightWing->SetVisibility(bShow);
	}
	if (!bShow)
	{
		// Stars already shed still fade out
		UpdateSparkles(DeltaTime, 0.f);
		return;
	}

	// Ride on the upper back
	const USkeletalMeshComponent* Body = Brawler->GetMesh();
	if (Body && Body->GetBoneIndex(BackBone) != INDEX_NONE)
	{
		const FTransform Actor = Brawler->GetActorTransform();
		SetWorldLocationAndRotation(Body->GetBoneLocation(BackBone) + Actor.TransformVector(BackOffset), Actor.GetRotation());
	}

	FWingPose Target;
	if (Brawler->IsDashing())
	{
		// Swept back like a diving bird, with a fast flutter and a streak of stars
		Target = { 5.f, 60.f, 4.f, 7.f, 0.9f, 55.f };
	}
	else if (bInAir && Brawler->GetVelocity().Z > RisingSpeed)
	{
		// Beating hard to climb, showering stars
		Target = { 15.f, 10.f, 38.f, 4.f, 1.f, 45.f };
	}
	else if (bInAir)
	{
		// Held open, a slow ripple
		Target = { 10.f, 15.f, 6.f, 1.2f, 1.f, 20.f };
	}
	else
	{
		// On the ground with the power: folded up behind the back, breathing, a few stars
		Target = { 50.f, 55.f, 3.f, 0.7f, 0.65f, 5.f };
	}
	if (bJustShown)
	{
		Pose = Target;
		BeatPhase = 0.f;
	}
	Pose.Lift = FMath::FInterpTo(Pose.Lift, Target.Lift, DeltaTime, PoseBlendSpeed);
	Pose.Sweep = FMath::FInterpTo(Pose.Sweep, Target.Sweep, DeltaTime, PoseBlendSpeed);
	Pose.BeatAngle = FMath::FInterpTo(Pose.BeatAngle, Target.BeatAngle, DeltaTime, PoseBlendSpeed);
	Pose.BeatRate = FMath::FInterpTo(Pose.BeatRate, Target.BeatRate, DeltaTime, PoseBlendSpeed);
	Pose.Size = FMath::FInterpTo(Pose.Size, Target.Size, DeltaTime, PoseBlendSpeed);
	Pose.SparkleRate = FMath::FInterpTo(Pose.SparkleRate, Target.SparkleRate, DeltaTime, PoseBlendSpeed);
	BeatPhase = FMath::Fmod(BeatPhase + 2.f * PI * Pose.BeatRate * DeltaTime, 2.f * PI);

	const float Lift = Pose.Lift + Pose.BeatAngle * FMath::Sin(BeatPhase);
	// Unfolding grows them out from the back
	const float Size = Pose.Size * (1.f - FMath::Square(1.f - Shown));
	PlaceWing(RightWing, 1.f, Lift, Pose.Sweep, Size);
	PlaceWing(LeftWing, -1.f, Lift, Pose.Sweep, Size);

	UpdateSparkles(DeltaTime, Pose.SparkleRate * Shown);
}

void URRRandomWingsComponent::UpdateSparkles(float DeltaTime, float SpawnRate)
{
	if (!Sparkles)
	{
		return;
	}
	if (LiveSparkles.Num() == 0 && SpawnRate <= 0.f)
	{
		SparkleDebt = 0.f;
		if (Sparkles->GetInstanceCount() > 0)
		{
			Sparkles->ClearInstances();
		}
		return;
	}

	// Shed new stars from random spots on the outer part of either wing, where the wing is this frame
	SparkleDebt += SpawnRate * DeltaTime;
	for (; SparkleDebt >= 1.f; SparkleDebt -= 1.f)
	{
		if (LiveSparkles.Num() >= MaxSparkles)
		{
			continue;
		}
		const UStaticMeshComponent* Wing = FMath::RandBool() ? LeftWing : RightWing;
		const FVector OnWing(-FMath::FRandRange(0.f, SparkleChordDepth), WingLength * FMath::FRandRange(SparkleSpanStart, 1.f), 6.f);
		FSparkle& Star = LiveSparkles.AddDefaulted_GetRef();
		Star.Location = Wing->GetComponentTransform().TransformPosition(OnWing);
		Star.Velocity = FMath::VRand() * SparkleKick * FMath::FRand();
		Star.Life = FMath::FRandRange(SparkleLife.X, SparkleLife.Y);
		Star.Size = FMath::FRandRange(SparkleScale.X, SparkleScale.Y);
		Star.Spin = FMath::FRandRange(0.f, 2.f * PI);
	}

	// Drift, sink, age out
	for (FSparkle& Star : LiveSparkles)
	{
		Star.Age += DeltaTime;
		Star.Velocity.Z -= SparkleSink * DeltaTime;
		Star.Velocity *= FMath::Max(0.f, 1.f - SparkleDrag * DeltaTime);
		Star.Location += Star.Velocity * DeltaTime;
	}
	LiveSparkles.RemoveAll([](const FSparkle& Star) { return Star.Age >= Star.Life; });

	// Flat stars turned toward this machine's camera, popping in and shrinking away, twinkling and slowly turning
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	SparkleTransforms.Reset(LiveSparkles.Num());
	if (Camera)
	{
		const FQuat View = Camera->GetCameraRotation().Quaternion();
		const FQuat Facing = FRotationMatrix::MakeFromXZ(-View.GetForwardVector(), View.GetUpVector()).ToQuat();
		for (const FSparkle& Star : LiveSparkles)
		{
			const float Alpha = Star.Age / Star.Life;
			const float Twinkle = 1.f + TwinkleAmount * FMath::Sin(Star.Age * TwinkleSpeed + Star.Spin);
			const float Scale = Star.Size * FMath::Sqrt(FMath::Sin(PI * Alpha)) * Twinkle;
			const FQuat Turn(FVector::ForwardVector, Star.Spin + Star.Age * 3.f);
			SparkleTransforms.Emplace(Facing * Turn, Star.Location, FVector(FMath::Max(Scale, KINDA_SMALL_NUMBER)));
		}
	}
	Sparkles->ClearInstances();
	Sparkles->AddInstances(SparkleTransforms, false);
}
