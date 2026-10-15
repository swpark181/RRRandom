#include "RRRandomDragonFlight.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomDiceLand.h"
#include "RRRandomDiceTown.h"
#include "RRRandomIsland.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The strips' height (the hologram is a low glowing wall round the line)
	constexpr float StripHeight = 70.f;
	// How far over the floor the scorched strip lies (clear of it, under anyone standing on it)
	constexpr float ScorchLift = 4.f;
	// The model's snout tip in its component space (Blender 0, -0.495, 0.22 m): the fire comes out just in front of it
	const FVector FlightSnoutTip(0.f, 54.f, 21.f);

	/** Rotation about a component-space axis, in degrees. */
	FQuat Turn(const FVector& Axis, float Degrees)
	{
		return FQuat(Axis, FMath::DegreesToRadians(Degrees));
	}
}

ARRRandomDragonFlight::ARRRandomDragonFlight()
{
	CursorHoloColor = FLinearColor(1.f, 0.55f, 0.1f);
	TargetHoloColor = FLinearColor(1.f, 0.15f, 0.02f);

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> Flying(TEXT("/Game/Characters/DragonFlight/SK_DragonFlight.SK_DragonFlight"));
	FlightBody = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("FlightBody"));
	FlightBody->SetupAttachment(Root);
	// The model faces +Y; the actor faces +X
	FlightBody->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	FlightBody->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Flying.Succeeded())
	{
		FlightBody->SetSkinnedAssetAndUpdate(Flying.Object);
	}

	auto MakeStrip = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Strip = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Strip->SetupAttachment(Root);
		Strip->SetUsingAbsoluteLocation(true);
		Strip->SetUsingAbsoluteRotation(true);
		Strip->SetUsingAbsoluteScale(true);
		Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Strip->SetCastShadow(false);
		Strip->SetVisibility(false);
		return Strip;
	};
	PathHolo = MakeStrip(TEXT("PathHolo"));
	WarnHolo = MakeStrip(TEXT("WarnHolo"));
	Scorch = MakeStrip(TEXT("Scorch"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		PathHolo->SetStaticMesh(Cube.Object);
		WarnHolo->SetStaticMesh(Cube.Object);
		Scorch->SetStaticMesh(Cube.Object);
	}
}

void ARRRandomDragonFlight::BeginPlay()
{
	FlightBody->SetVisibility(false);
	Super::BeginPlay();
}

bool ARRRandomDragonFlight::IsBusy() const
{
	// Still in the air, or the ground still burning
	const bool bBurning = PassTime >= 0.f && PassTime < LineLength / FlySpeed + BurnSeconds;
	return Phase == EPhase::Circle || Phase == EPhase::Pass || Phase == EPhase::Leave || bBurning;
}

FString ARRRandomDragonFlight::GetStatusLine() const
{
	if (IsPicking())
	{
		return FString::Printf(TEXT("%s  [CLICK THE WAY TO BREATHE FIRE]"), *GetTitle());
	}
	return FString::Printf(TEXT("%s  LV%d"), *GetTitle(), Face);
}

void ARRRandomDragonFlight::OnStarted()
{
	// The die's face: a bigger dragon, a wider breath, flying higher
	Scale = BaseScale + ScalePerFace * Face;
	BreathRadius = BaseBreathRadius + BreathRadiusPerFace * Face;
	Height = BaseHeight + HeightPerScale * Scale;
	AreaRadius = BreathRadius;
	if (HasAuthority())
	{
		// One click: the way to go
		Left = 1;
	}
	for (TActorIterator<ARRRandomIsland> It(GetWorld()); It; ++It)
	{
		MapBounds = It->GetMapBounds();
	}
	for (TActorIterator<ARRRandomDiceLand> It(GetWorld()); It; ++It)
	{
		MapBounds = It->GetMapBounds();
	}
	for (TActorIterator<ARRRandomDiceTown> It(GetWorld()); It; ++It)
	{
		MapBounds = It->GetMapBounds();
	}

	// The bones the code moves
	SetupRig(FlightRig, FlightBody, { TEXT("Hips"), TEXT("Neck1"), TEXT("Neck2"), TEXT("Neck3"), TEXT("Tail1"), TEXT("Tail2"),
		TEXT("Tail3"), TEXT("Tail4"), TEXT("WingL1"), TEXT("WingL2"), TEXT("WingR1"), TEXT("WingR2") }, FlightSnoutTip);

	// The way being picked is an arrow streaming chevrons toward the far end (char-pipeline/scripts/ue_make_holo_arrow.py);
	// the warning over the line in flight the plain hologram (ue_make_holo.py)
	for (UStaticMeshComponent* Strip : { PathHolo.Get(), WarnHolo.Get() })
	{
		const TCHAR* HoloPath = Strip == PathHolo ? TEXT("/Game/Effects/DiceFX/M_FXHoloArrow.M_FXHoloArrow") : TEXT("/Game/Effects/DiceFX/M_FXHolo.M_FXHolo");
		if (UMaterialInterface* Holo = LoadObject<UMaterialInterface>(nullptr, HoloPath))
		{
			UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Holo, this);
			Material->SetVectorParameterValue(TEXT("Color"), Strip == PathHolo ? FLinearColor(1.f, 0.32f, 0.02f) : TargetHoloColor);
			Material->SetScalarParameterValue(TEXT("Glow"), Strip == PathHolo ? 1.5f : 1.2f);
			Strip->SetMaterial(0, Material);
			(Strip == PathHolo ? PathHoloMaterial : WarnHoloMaterial) = Material;
		}
	}
	if (UMaterialInterface* Scorched = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/DiceFX/M_FXScorch.M_FXScorch")))
	{
		ScorchMaterial = UMaterialInstanceDynamic::Create(Scorched, this);
		ScorchMaterial->SetScalarParameterValue(TEXT("Radius"), BreathRadius);
		ScorchMaterial->SetScalarParameterValue(TEXT("FlySpeed"), FlySpeed);
		ScorchMaterial->SetScalarParameterValue(TEXT("BurnSeconds"), BurnSeconds);
		Scorch->SetMaterial(0, ScorchMaterial);
	}

	// The little dragon goes up in a burst of flame; its true form shows itself once the way is picked (MulticastFly)
	Yaw = Brawler->GetActorRotation().Yaw;
	Hover = StartFeet + FVector(0.f, 0.f, Height);
	Location = Hover;
	Phase = EPhase::Hover;
	PhaseTime = 0.f;
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->FireBurst(StartFeet + FVector(0.f, 0.f, 90.f), 1.5f);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s takes its dragon form (level %d: scale %.1f, breath %.0f wide each side, %.0f up)."), *Brawler->GetName(), Face,
		Scale, BreathRadius, Height);
}

FVector ARRRandomDragonFlight::GetDefaultSpot() const
{
	// The way the little dragon faced
	return StartFeet + FRotator(0.f, Brawler ? Brawler->GetActorRotation().Yaw : Yaw, 0.f).Vector() * 1000.f;
}

FVector ARRRandomDragonFlight::PickTarget() const
{
	// Toward where most enemies are (the base's pick, the default when there are none)
	return Super::PickTarget();
}

FVector ARRRandomDragonFlight::GetWayTo(const FVector& Spot) const
{
	const FVector Way = (Spot - StartFeet) * FVector(1.f, 1.f, 0.f);
	return Way.Size() > 50.f ? Way.GetSafeNormal() : FRotator(0.f, Brawler ? Brawler->GetActorRotation().Yaw : Yaw, 0.f).Vector();
}

float ARRRandomDragonFlight::GetLineLength(const FVector& From, const FVector& Direction) const
{
	if (!MapBounds.bIsValid)
	{
		return MaxLineLength * 0.6f;
	}
	// Out of the map's square the way it points: the nearest of the sides it heads for
	float Out = UE_BIG_NUMBER;
	for (int32 Axis = 0; Axis < 2; ++Axis)
	{
		const float Step = Direction[Axis];
		if (FMath::Abs(Step) > KINDA_SMALL_NUMBER)
		{
			const float Edge = Step > 0.f ? MapBounds.Max[Axis] : MapBounds.Min[Axis];
			Out = FMath::Min(Out, (Edge - From[Axis]) / Step);
		}
	}
	return FMath::Clamp(Out, MinLineLength, MaxLineLength);
}

void ARRRandomDragonFlight::StartAction(const FVector& Target)
{
	--Left;
	// From where the little dragon stood, that way, on to the map's edge
	const FVector Way = GetWayTo(Target);
	MulticastFly(StartFeet, StartFeet + Way * GetLineLength(StartFeet, Way));
}

void ARRRandomDragonFlight::MulticastFly_Implementation(FVector_NetQuantize From, FVector_NetQuantize To)
{
	if (!HasStarted() || PassTime >= 0.f || Phase == EPhase::Circle || Phase == EPhase::Gone)
	{
		return;
	}
	LineStart = From;
	LineEnd = To;
	const FVector Way = (LineEnd - LineStart) * FVector(1.f, 1.f, 0.f);
	LineLength = FMath::Max(Way.Size(), 1.f);
	LineDirection = Way.Size() > 1.f ? Way.GetSafeNormal() : FRotator(0.f, Yaw, 0.f).Vector();
	// The breath reaches the floor this far ahead of the dragon: from its snout, slanting down
	Lead = FlightSnoutTip.Y * Scale + Height * 0.55f;

	ReleaseTargetHolo();
	PathHolo->SetVisibility(false);
	PlaceStrip(WarnHolo, LineStart, LineEnd, BreathRadius);
	WarnHolo->SetVisibility(true);
	// The scorched strip: the cube flattened just over the floor along the whole line (round ends past both), shown once
	// the breath begins
	const FVector Rise = LineEnd - LineStart;
	Scorch->SetWorldLocationAndRotation((LineStart + LineEnd) * 0.5f + FVector(0.f, 0.f, ScorchLift), Rise.Size() > 1.f ? Rise.Rotation() : FRotator(0.f, Yaw, 0.f));
	Scorch->SetWorldScale3D(FVector((Rise.Size() + BreathRadius * 2.4f) / 100.f, BreathRadius * 2.4f / 100.f, 0.01f));
	if (ScorchMaterial)
	{
		ScorchMaterial->SetScalarParameterValue(TEXT("Length"), Rise.Size() + BreathRadius * 2.4f);
		ScorchMaterial->SetScalarParameterValue(TEXT("Pad"), BreathRadius * 1.2f);
		ScorchMaterial->SetScalarParameterValue(TEXT("Width"), BreathRadius * 2.4f);
		ScorchMaterial->SetScalarParameterValue(TEXT("Front"), 0.f);
		ScorchMaterial->SetScalarParameterValue(TEXT("PassTime"), 0.f);
	}

	// It appears in a burst of flame on the line behind its start (far enough that the fire begins right there) and flies
	// one quick lap anticlockwise beside where the little dragon stood, back to the same spot heading along the line at
	// the pace it then dashes along it: one unbroken flight
	LapExit = LineStart + LineDirection * -Lead + FVector(0.f, 0.f, Height);
	const FVector Aside(-LineDirection.Y, LineDirection.X, 0.f);
	CircleRadius = Lead + CircleExtraRadius;
	CircleCenter = LapExit + Aside * CircleRadius;
	CircleSeconds = CircleTime + CirclePerScale * Scale;
	CircleEnd = FMath::Atan2(-Aside.Y, -Aside.X);
	Location = LapExit;
	FlightBody->SetVisibility(true);
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->FireBurst(Location, Scale * 0.3f);
	}
	Phase = EPhase::Circle;
	PhaseTime = 0.f;
	FrontHit.Reset();
	ViewFrom = Overview->GetComponentTransform();
	ViewBlend = 0.f;
	ViewFocus = Location;
	ViewExtra = 0.f;
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the dragon flies a %.0f long line from %s to %s."), LineLength, *LineStart.ToCompactString(),
		*LineEnd.ToCompactString());
}

FVector ARRRandomDragonFlight::OnLine(float Along) const
{
	const float Alpha = FMath::Clamp(Along / LineLength, 0.f, 1.f);
	FVector Point = LineStart + LineDirection * Along;
	Point.Z = FMath::Lerp(LineStart.Z, LineEnd.Z, Alpha);
	return Point;
}

void ARRRandomDragonFlight::PlaceStrip(UStaticMeshComponent* Strip, const FVector& From, const FVector& To, float Radius) const
{
	// The cube is 100 on a side about its middle: as long as the line and the breath's reach at each end, as wide as it
	const FVector Way = (To - From) * FVector(1.f, 1.f, 0.f);
	const float Length = Way.Size() + Radius * 2.f;
	const float StripYaw = Way.Size() > 1.f ? Way.Rotation().Yaw : Yaw;
	const FVector Middle = (From + To) * 0.5f + FVector(0.f, 0.f, StripHeight * 0.5f + 3.f);
	Strip->SetWorldLocationAndRotation(Middle, FRotator(0.f, StripYaw, 0.f));
	Strip->SetWorldScale3D(FVector(Length / 100.f, Radius * 2.f / 100.f, StripHeight / 100.f));
}

void ARRRandomDragonFlight::ShowCursorSpot(bool bShow, const FVector& Spot)
{
	// The whole line the fire would take: from the little dragon toward the cursor, to the map's edge
	Super::ShowCursorSpot(false, Spot);
	const bool bVisible = bShow && IsPicking() && Phase != EPhase::Gone;
	if (bVisible)
	{
		const FVector Way = GetWayTo(Spot);
		const float Length = GetLineLength(StartFeet, Way);
		PlaceStrip(PathHolo, StartFeet, StartFeet + Way * Length, BreathRadius);
		if (PathHoloMaterial)
		{
			// The arrow's pattern in world units: the strip's length (the line and the breath's reach at each end) and width
			PathHoloMaterial->SetScalarParameterValue(TEXT("Length"), Length + BreathRadius * 2.f);
			PathHoloMaterial->SetScalarParameterValue(TEXT("Radius"), BreathRadius);
			PathHoloMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.9f + 0.1f * FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f));
		}
	}
	if (PathHolo->IsVisible() != bVisible)
	{
		PathHolo->SetVisibility(bVisible);
	}
}

void ARRRandomDragonFlight::SetupRig(FRig& Rig, UPoseableMeshComponent* Mesh, const TArray<FName>& Bones, const FVector& Snout)
{
	Rig = FRig();
	Rig.Mesh = Mesh;
	const FReferenceSkeleton* RefSkeleton = Mesh->GetSkinnedAsset() ? &Mesh->GetSkinnedAsset()->GetRefSkeleton() : nullptr;
	if (!RefSkeleton)
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: a dragon model for %s is missing."), *Mesh->GetName());
		return;
	}
	auto RefComponent = [RefSkeleton](FName Bone)
	{
		FTransform Pose = FTransform::Identity;
		for (int32 Index = RefSkeleton->FindBoneIndex(Bone); Index != INDEX_NONE; Index = RefSkeleton->GetParentIndex(Index))
		{
			Pose = Pose * RefSkeleton->GetRefBonePose()[Index];
		}
		return Pose;
	};
	for (const FName& Bone : Bones)
	{
		const FString Name = Bone.ToString();
		const bool bFollows = Name.EndsWith(TEXT("2")) || Name.EndsWith(TEXT("3")) || Name.EndsWith(TEXT("4"));
		const int32 Index = Rig.Bones.Num();
		Rig.Bones.Add(Bone);
		Rig.Parents.Add(Index == 0 ? INDEX_NONE : (bFollows ? Index - 1 : 0));
		Rig.RefRotations.Add(RefComponent(Bone).GetRotation());
	}
	Rig.Turns.Init(FQuat::Identity, Rig.Bones.Num());
	Rig.HipsHeight = RefComponent(Bones[0]).GetLocation().Z;
	Rig.Head = Rig.Bones.IndexOfByKey(FName(TEXT("Neck3")));
	Rig.SnoutOffset = Snout - RefComponent(TEXT("Neck3")).GetLocation();
}

void ARRRandomDragonFlight::ApplyPose(FRig& Rig, const TMap<FName, FQuat>& Want)
{
	for (int32 Index = 0; Index < Rig.Bones.Num(); ++Index)
	{
		const FQuat* Own = Want.Find(Rig.Bones[Index]);
		const FQuat Parent = Rig.Parents[Index] == INDEX_NONE ? FQuat::Identity : Rig.Turns[Rig.Parents[Index]];
		Rig.Turns[Index] = Own ? Parent * *Own : Parent;
		Rig.Mesh->SetBoneRotationByName(Rig.Bones[Index], (Rig.Turns[Index] * Rig.RefRotations[Index]).Rotator(), EBoneSpaces::ComponentSpace);
	}
}

FVector ARRRandomDragonFlight::GetMouth() const
{
	const FRig& Rig = FlightRig;
	if (Rig.Head == INDEX_NONE)
	{
		return GetActorLocation();
	}
	// The head bone where the pose has it, and the snout turned with it
	const FVector HeadAt = Rig.Mesh->GetBoneTransformByName(Rig.Bones[Rig.Head], EBoneSpaces::ComponentSpace).GetLocation();
	return Rig.Mesh->GetComponentTransform().TransformPosition(HeadAt + Rig.Turns[Rig.Head].RotateVector(Rig.SnoutOffset));
}

void ARRRandomDragonFlight::PoseBody(float Time, bool bBreathing)
{
	if (FlightRig.Bones.Num() == 0)
	{
		return;
	}
	// The model's axes: +Y ahead, +X its left, +Z up. (Rotations about X and Z are mirrored from Blender's: the FBX flips Y
	// on the way in.) Streamlined: the wings beat quick and shallow about their swept-back rest, the outer half trailing,
	// the neck and tail ripple a little, the head dips at the fire
	const FVector Ahead(0.f, 1.f, 0.f);
	const FVector Side(1.f, 0.f, 0.f);
	const FVector Up(0.f, 0.f, 1.f);
	const float Beat = FlapPhase;
	TMap<FName, FQuat> Want;
	const float Flap = 8.f + 24.f * FMath::Sin(Beat);
	const float Tip = 4.f + 12.f * FMath::Sin(Beat - 0.7f);
	Want.Add(TEXT("Neck1"), Turn(Side, 2.f * FMath::Sin(Beat + 1.f)));
	Want.Add(TEXT("Neck3"), Turn(Side, bBreathing ? -24.f : -3.f));
	Want.Add(TEXT("WingL1"), Turn(Ahead, Flap));
	Want.Add(TEXT("WingR1"), Turn(Ahead, -Flap));
	Want.Add(TEXT("WingL2"), Turn(Ahead, Tip));
	Want.Add(TEXT("WingR2"), Turn(Ahead, -Tip));
	for (int32 Index = 1; Index <= 4; ++Index)
	{
		const float Wave = Time * 3.f - Index * 0.8f;
		Want.Add(FName(*FString::Printf(TEXT("Tail%d"), Index)), Turn(Up, 4.f * FMath::Sin(Wave)) * Turn(Side, 3.f * FMath::Sin(Wave + 1.3f)));
	}
	ApplyPose(FlightRig, Want);
}

void ARRRandomDragonFlight::PlaceBody(const FVector& Where, const FVector& Velocity, float DeltaSeconds)
{
	const FVector Flat = Velocity * FVector(1.f, 1.f, 0.f);
	float Pitch = 0.f;
	if (Flat.Size() > 50.f)
	{
		const float NewYaw = Flat.Rotation().Yaw;
		// Banking into the turn, the harder the faster it turns
		const float TurnRate = DeltaSeconds > 0.f ? FMath::FindDeltaAngleDegrees(Yaw, NewYaw) / DeltaSeconds : 0.f;
		Bank = FMath::FInterpTo(Bank, FMath::Clamp(TurnRate * 0.35f, -35.f, 35.f), DeltaSeconds, 4.f);
		Yaw = NewYaw;
		Pitch = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(Velocity.Z, Flat.Size())) * 0.6f, -20.f, 30.f);
	}
	else
	{
		Bank = FMath::FInterpTo(Bank, 0.f, DeltaSeconds, 4.f);
	}
	Location = Where;
	SetActorLocationAndRotation(Where, FRotator(Pitch, Yaw, Bank));
}

void ARRRandomDragonFlight::TickUltimate(float DeltaSeconds)
{
	if (Phase == EPhase::Gone && PassTime < 0.f)
	{
		return;
	}
	PhaseTime += DeltaSeconds;
	FlightTime += DeltaSeconds;
	// In flight the wings beat faster (Dash eases it in)
	Dash = FMath::FInterpTo(Dash, Phase == EPhase::Circle || Phase == EPhase::Pass || Phase == EPhase::Leave ? 1.f : 0.f,
		DeltaSeconds, 3.f);
	FlapPhase += DeltaSeconds * FlapRate * (1.f + 0.6f * Dash) * 2.f * PI;
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	bool bBreathing = false;
	float Size = Scale;
	switch (Phase)
	{
	case EPhase::Hover:
	{
		// Not shown yet: waiting over the start for the way
		PlaceBody(Hover, FVector::ZeroVector, DeltaSeconds);
		break;
	}
	case EPhase::Circle:
	{
		// Growing out of the flames as it sets off and once round, quick at first, easing at the end into the pace it dashes
		// along the line at (the lap's angle a cubic in time: a whole turn, starting at 1.5 turns' pace, ending at the
		// line's), rising a little mid-lap; then straight on into the dash
		const float Alpha = FMath::Clamp(PhaseTime / CircleSeconds, 0.f, 1.f);
		const float StartRate = UE_TWO_PI * 1.5f;
		const float EndRate = FlySpeed * CircleSeconds / CircleRadius;
		const float A2 = Alpha * Alpha;
		const float A3 = A2 * Alpha;
		const float Turned = (A3 - 2.f * A2 + Alpha) * StartRate + (-2.f * A3 + 3.f * A2) * UE_TWO_PI + (A3 - A2) * EndRate;
		const float TurnRate = ((3.f * A2 - 4.f * Alpha + 1.f) * StartRate + (-6.f * A2 + 6.f * Alpha) * UE_TWO_PI + (3.f * A2 - 2.f * Alpha) * EndRate) / CircleSeconds;
		const float Angle = CircleEnd - UE_TWO_PI + Turned;
		const FVector Out(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		const FVector Along(-Out.Y, Out.X, 0.f);
		const float Rise = 300.f * FMath::Square(FMath::Sin(PI * Alpha));
		const FVector Velocity = Along * CircleRadius * TurnRate + FVector(0.f, 0.f, 300.f * PI / CircleSeconds * FMath::Sin(UE_TWO_PI * Alpha));
		Size = Scale * FMath::Lerp(0.3f, 1.f, FMath::InterpEaseOut(0.f, 1.f, FMath::Min(PhaseTime / 0.35f, 1.f), 2.f));
		PlaceBody(CircleCenter + Out * CircleRadius + FVector(0.f, 0.f, Rise), Velocity, DeltaSeconds);
		PoseBody(FlightTime, false);
		if (Alpha >= 1.f)
		{
			Phase = EPhase::Pass;
			PhaseTime = 0.f;
			PassTime = 0.f;
		}
		break;
	}
	case EPhase::Pass:
	{
		// Along the line, the breath's front FlySpeed ahead every second
		const float Front = PassTime * FlySpeed;
		PlaceBody(OnLine(Front - Lead) * FVector(1.f, 1.f, 0.f) + FVector(0.f, 0.f, FMath::Lerp(LineStart.Z, LineEnd.Z, FMath::Clamp(Front / LineLength, 0.f, 1.f)) + Height),
			LineDirection * FlySpeed, DeltaSeconds);
		bBreathing = true;
		PoseBody(FlightTime, true);
		if (Effects)
		{
			Effects->DragonBreath(GetMouth(), OnLine(FMath::Min(Front, LineLength)), BreathRadius, DeltaSeconds,
				Front < LineLength ? LineDirection * FlySpeed : FVector::ZeroVector);
		}
		if (Front >= LineLength)
		{
			Phase = EPhase::Leave;
			PhaseTime = 0.f;
			LeaveFrom = Location;
		}
		break;
	}
	case EPhase::Leave:
	{
		// On across the map, climbing away, shrinking into a last puff of flame
		const float Alpha = FMath::Clamp(PhaseTime / LeaveTime, 0.f, 1.f);
		const FVector Point = LeaveFrom + LineDirection * FlySpeed * 1.2f * PhaseTime + FVector(0.f, 0.f, 1400.f * Alpha * Alpha);
		PlaceBody(Point, LineDirection * FlySpeed * 1.2f + FVector(0.f, 0.f, 2800.f * Alpha / LeaveTime), DeltaSeconds);
		Size = Scale * (Alpha > 0.8f ? FMath::Max(0.05f, (1.f - Alpha) / 0.2f) : 1.f);
		PoseBody(FlightTime, false);
		if (Alpha >= 1.f)
		{
			Phase = EPhase::Gone;
			FlightBody->SetVisibility(false);
			if (Effects)
			{
				Effects->FireBurst(Location, 1.f);
			}
		}
		break;
	}
	default:
		break;
	}
	FollowWithView(DeltaSeconds);
	if (Phase != EPhase::Gone)
	{
		FlightBody->SetRelativeScale3D(FVector(Size));
		FlightBody->SetRelativeLocation(FVector(0.f, 0.f, -FlightRig.HipsHeight * Size));
	}

	// The fire on the ground, and the warning over it until the breath has gone by
	if (PassTime >= 0.f)
	{
		if (Phase != EPhase::Pass)
		{
			PassTime += DeltaSeconds;
		}
		const float Front = FMath::Min(PassTime * FlySpeed, LineLength);
		const float Back = FMath::Max(0.f, (PassTime - BurnSeconds) * FlySpeed);
		if (Effects && Front > Back)
		{
			for (float Along = Back; Along <= Front; Along += 170.f)
			{
				// Flames where the breath just went by, dying down into embers that glow on to the end of the burn (the two
				// overlap a little, so the embers are there as the flames go)
				const float Age = PassTime - Along / FlySpeed;
				const FVector Spot = OnLine(Along);
				if (Age < BlazeSeconds)
				{
					Effects->GroundFire(Spot, BreathRadius * 0.85f, 1.f - FMath::Clamp(Age / BlazeSeconds, 0.f, 1.f) * 0.8f, DeltaSeconds);
				}
				if (Age > BlazeSeconds * 0.5f)
				{
					const float Out = FMath::Clamp((Age - BlazeSeconds) / FMath::Max(BurnSeconds - BlazeSeconds, 0.1f), 0.f, 1.f);
					Effects->GroundEmbers(Spot, BreathRadius * 0.9f, 1.f - 0.7f * Out * Out, DeltaSeconds);
				}
			}
		}
		// Behind the breath, the scorched ground with its embers; the warning only over what is still to burn
		if (ScorchMaterial)
		{
			ScorchMaterial->SetScalarParameterValue(TEXT("Front"), Front);
			ScorchMaterial->SetScalarParameterValue(TEXT("PassTime"), PassTime);
			Scorch->SetVisibility(Front > Back);
		}
		if (Phase == EPhase::Pass && Front < LineLength)
		{
			PlaceStrip(WarnHolo, OnLine(Front), LineEnd, BreathRadius);
		}
		if (WarnHoloMaterial && WarnHolo->IsVisible())
		{
			const float Fade = Phase == EPhase::Pass ? 1.f : FMath::Max(0.f, 1.f - PhaseTime / 0.4f);
			WarnHoloMaterial->SetScalarParameterValue(TEXT("Opacity"), (0.16f + 0.06f * FMath::Sin(FlightTime * 14.f)) * Fade);
			if (Fade <= 0.f)
			{
				WarnHolo->SetVisibility(false);
			}
		}
		if (HasAuthority())
		{
			BurnEnemies(DeltaSeconds);
		}
		if (Phase == EPhase::Pass)
		{
			PassTime += DeltaSeconds;
		}
	}
	else if (WarnHoloMaterial && WarnHolo->IsVisible())
	{
		WarnHoloMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.3f + 0.1f * FMath::Sin(FlightTime * 14.f));
	}
}

void ARRRandomDragonFlight::FollowWithView(float DeltaSeconds)
{
	if (ViewBlend < 0.f)
	{
		return;
	}
	// Looking across the line from its right, a little down: the dragon flies left to right across the view. The view
	// keeps up with it along the line, watches it fly off from the line's end, then drifts back over the burning line.
	// While it laps the start, the view backs off and looks down more to take in the whole lap
	ViewBlend = FMath::Min(1.f, ViewBlend + DeltaSeconds / 0.8f);
	const bool bLap = Phase == EPhase::Circle;
	const FVector Target = Phase == EPhase::Gone ? OnLine(LineLength * 0.5f)
		: Phase == EPhase::Leave ? OnLine(LineLength) + FVector(0.f, 0.f, Height * 0.3f)
		: bLap ? CircleCenter - FVector(0.f, 0.f, Height * 0.45f)
		: Location - FVector(0.f, 0.f, Height * 0.45f);
	ViewFocus = FMath::VInterpTo(ViewFocus, Target, DeltaSeconds, Phase == EPhase::Leave || Phase == EPhase::Gone ? 1.5f : 9.f);
	const float LapView = CircleRadius * 1.4f;
	ViewExtra = FMath::FInterpTo(ViewExtra, bLap ? LapView : 0.f, DeltaSeconds, bLap ? 4.f : 2.5f);
	const float LapShare = LapView > 0.f ? ViewExtra / LapView : 0.f;
	const FRotator Side(-28.f - 14.f * LapShare, LineDirection.Rotation().Yaw - 90.f, 0.f);
	const FTransform Follow(Side, ViewFocus - Side.Vector() * (FollowDistance + FollowPerScale * Scale + ViewExtra));
	const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, ViewBlend, 2.f);
	Overview->SetWorldLocationAndRotation(FMath::Lerp(ViewFrom.GetLocation(), Follow.GetLocation(), Alpha),
		FQuat::Slerp(ViewFrom.GetRotation(), Follow.GetRotation(), Alpha));
}

void ARRRandomDragonFlight::BurnEnemies(float DeltaSeconds)
{
	if (!Brawler)
	{
		return;
	}
	const float Front = FMath::Min(PassTime * FlySpeed, LineLength);
	const float Back = FMath::Max(0.f, (PassTime - BurnSeconds) * FlySpeed);
	const bool bFront = Phase == EPhase::Pass;
	BurnClock += DeltaSeconds;
	const bool bBurnTick = BurnClock >= BurnTick;
	if (bBurnTick)
	{
		BurnClock -= BurnTick;
	}
	if (!bFront && !bBurnTick)
	{
		return;
	}
	const FVector FrontPoint = OnLine(Front);
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Victim = *It;
		// (Not one held by its own ultimate: hidden and untouchable meanwhile)
		if (!Victim->IsAlive() || Victim->GetTeam() == Brawler->GetTeam() || Victim->IsUsingUltimate())
		{
			continue;
		}
		const FVector Where = Victim->GetActorLocation();
		const float Reach = BreathRadius + Victim->GetCapsuleComponent()->GetScaledCapsuleRadius();
		// Not someone flying high over it
		if (Where.Z > FMath::Max(LineStart.Z, LineEnd.Z) + 700.f)
		{
			continue;
		}
		// Across the line from it (which side to be thrown to)
		const FVector Across = FVector::CrossProduct(FVector::UpVector, LineDirection);
		const float Along = FVector::DotProduct((Where - LineStart) * FVector(1.f, 1.f, 0.f), LineDirection);
		const float Side = FVector::DotProduct((Where - LineStart) * FVector(1.f, 1.f, 0.f), Across);
		if (bFront && !FrontHit.Contains(Victim) && FVector::Dist2D(Where, FrontPoint) <= Reach)
		{
			FrontHit.Add(Victim);
			const FVector Out = Across * (Side >= 0.f ? 1.f : -1.f);
			const float Before = Victim->GetHealth();
			FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Where, -Out);
			UGameplayStatics::ApplyPointDamage(Victim, FrontDamage, Out, Hit, Brawler->GetController(), this, UDamageType::StaticClass());
			if (Victim->IsAlive())
			{
				Victim->LaunchCharacter(Out * FrontKnock.X + FVector(0.f, 0.f, FrontKnock.Y), true, true);
			}
			++Hits;
			UE_LOG(LogTemp, Log, TEXT("RRRandom: dragon breath hits %s: %.0f -> %.0f%s."), *Victim->GetName(), Before, Victim->GetHealth(),
				Victim->IsAlive() ? TEXT("") : TEXT(", knocked out"));
			continue;
		}
		// Standing in what still burns
		const float Clamped = FMath::Clamp(Along, Back, Front);
		if (bBurnTick && Front > Back && FVector::Dist2D(Where, OnLine(Clamped)) <= Reach)
		{
			FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Where, FVector::UpVector);
			UGameplayStatics::ApplyPointDamage(Victim, BurnDamage, FVector::DownVector, Hit, Brawler->GetController(), this, UDamageType::StaticClass());
			++BurnHits;
			LastBurnAge = FMath::Max(LastBurnAge, PassTime - Clamped / FlySpeed);
			UE_LOG(LogTemp, Verbose, TEXT("RRRandom: %s burns, health %.0f."), *Victim->GetName(), Victim->GetHealth());
		}
	}
}

void ARRRandomDragonFlight::OnEnding()
{
	if (Phase != EPhase::Gone || FlightBody->IsVisible())
	{
		Phase = EPhase::Gone;
		FlightBody->SetVisibility(false);
	}
	PathHolo->SetVisibility(false);
	WarnHolo->SetVisibility(false);
	Scorch->SetVisibility(false);
	ReleaseTargetHolo();
	UE_CLOG(HasAuthority(), LogTemp, Log, TEXT("RRRandom: the dragon flight is over: %d hits, %d burn ticks (the last %.1f s after the breath went by)."), Hits, BurnHits, LastBurnAge);
	// The little dragon back where it stood, out of a puff of flame
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->FireBurst(StartFeet + FVector(0.f, 0.f, 90.f), 1.f);
	}
	Done();
}
