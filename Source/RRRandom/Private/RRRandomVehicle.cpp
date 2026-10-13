#include "RRRandomVehicle.h"
#include "RRRandomCharacter.h"
#include "RRRandomSkidMarks.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// By ERRVehicleKind. Each a bit faster than the last; the motorbike and the car trade grip for drifts.
	const FRRVehicleHandling Handlings[] =
	{
		// Kickboard: a little faster, otherwise like walking
		{ 1.3f, 1800.f, 8.f, 720.f, 1600.f, false, 12.f, 0.f, 0.f, TEXT("KICKBOARD") },
		// Motorbike: faster, slides out in tight turns and leans into them; one rear wheel's mark
		{ 1.6f, 1500.f, 1.6f, 560.f, 700.f, true, 22.f, 0.7f, 28.f, TEXT("MOTORBIKE"), 0.f, false, 0.f, 16.f, 0.65f, 0.f },
		// Car: fastest, slides the most, rolls a little; a tandem two-seater, the rear seat right behind the driver's;
		// two rear wheels' marks
		{ 1.9f, 1300.f, 1.1f, 460.f, 600.f, true, 18.f, 0.15f, 6.f, TEXT("CAR"), -7.f, true, -93.f, 22.f, 0.55f, 0.72f },
	};
	const TCHAR* MeshPaths[] =
	{
		TEXT("/Game/Vehicles/SM_Kickboard.SM_Kickboard"),
		TEXT("/Game/Vehicles/SM_Motorbike.SM_Motorbike"),
		TEXT("/Game/Vehicles/SM_Car.SM_Car"),
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
}

const FRRVehicleHandling& ARRRandomVehicle::GetHandling(ERRVehicleKind InKind)
{
	return Handlings[FMath::Clamp(static_cast<int32>(InKind), 0, UE_ARRAY_COUNT(Handlings) - 1)];
}

void ARRRandomVehicle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomVehicle, Kind);
	DOREPLIFETIME(ARRRandomVehicle, Rider);
	DOREPLIFETIME(ARRRandomVehicle, Passenger);
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
	Body->SetStaticMesh(KindMeshes.IsValidIndex(Index) ? KindMeshes[Index] : nullptr);
}

float ARRRandomVehicle::GetHalfWidth() const
{
	const UStaticMesh* Mesh = Body->GetStaticMesh();
	return Mesh ? Mesh->GetBounds().BoxExtent.Y : 50.f;
}

int32 ARRRandomVehicle::Mount(ARRRandomCharacter* NewRider)
{
	if (!HasAuthority() || !NewRider || !HasRoom())
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
	// Under the rider's feet with the driver's seat there, turned with it (the vehicle's pivot is the middle of its bottom)
	UCapsuleComponent* Capsule = NewRider->GetCapsuleComponent();
	AttachToComponent(Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SetActorRelativeLocation(FVector(-GetHandling().DriverSeatX, 0.f, -Capsule->GetUnscaledCapsuleHalfHeight()));
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
