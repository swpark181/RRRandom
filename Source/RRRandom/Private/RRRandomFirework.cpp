#include "RRRandomFirework.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The engine sphere is 100 cm across
	constexpr float SphereSize = 100.f;
	// The rising shell drops a trail spark this often, which hangs this long
	constexpr float TrailInterval = 0.03f;
	constexpr float TrailLife = 0.4f;
	// Shell and trail color (warm white) and the shell's own small light
	const FLinearColor ShellColor(1.f, 0.75f, 0.4f);
	constexpr float ShellLight = 0.06f;
	// A few burst sparks are white-hot instead of the burst's color
	constexpr float WhiteSparkShare = 0.15f;
	// Burst sparks crackle (flicker) over the last part of their life
	constexpr float CrackleFrom = 0.6f;
	constexpr float CrackleRate = 45.f;
}

ARRRandomFirework::ARRRandomFirework()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetCanBeDamaged(false);

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Sparks = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Sparks"));
	Sparks->SetupAttachment(RootComponent);
	Sparks->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Sparks->SetCastShadow(false);
	Sparks->NumCustomDataFloats = 3;
	// Sparks fly far from where the actor stands
	Sparks->SetUsingAbsoluteLocation(true);
	Sparks->SetUsingAbsoluteRotation(true);
	Sparks->SetUsingAbsoluteScale(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SparkMaterial(TEXT("/Game/Effects/Fireworks/M_FireworkSpark.M_FireworkSpark"));
	if (Sphere.Succeeded())
	{
		Sparks->SetStaticMesh(Sphere.Object);
	}
	if (SparkMaterial.Succeeded())
	{
		Sparks->SetMaterial(0, SparkMaterial.Object);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(RootComponent);
	Light->SetCastShadows(false);
	Light->SetIntensityUnits(ELightUnits::Unitless);
	Light->SetIntensity(0.f);
	Light->SetAttenuationRadius(LightRadius);
	Light->SetUsingAbsoluteLocation(true);
}

void ARRRandomFirework::Launch(const FVector& Ground, float Height, const FLinearColor& Color, int32 Seed)
{
	Stream.Initialize(Seed);
	Start = Ground;
	Peak = Ground + FVector(Stream.FRandRange(-60.f, 60.f), Stream.FRandRange(-60.f, 60.f), Height);
	BurstColor = Color;
	Elapsed = 0.f;
	bLaunched = true;
	bBurst = false;
	Light->SetAttenuationRadius(LightRadius);
	Light->SetLightColor(ShellColor);
	Light->SetWorldLocation(Start);
}

void ARRRandomFirework::AddSpark(const FVector& Location, const FVector& Velocity, float Life, float Size, bool bInBurst)
{
	FSpark& Spark = SparkList.AddDefaulted_GetRef();
	Spark.Location = Location;
	Spark.Velocity = Velocity;
	Spark.Life = Life;
	Spark.Size = Size;
	Spark.bBurst = bInBurst;
	const float Floats[3] = { 0.f, 0.f, 0.f };
	const int32 Index = Sparks->AddInstance(FTransform(FQuat::Identity, Location, FVector(Size / SphereSize)), true);
	Sparks->SetCustomData(Index, Floats, false);
}

void ARRRandomFirework::Burst()
{
	bBurst = true;
	if (OnBurst)
	{
		OnBurst(BurstColor);
	}
	Light->SetLightColor(BurstColor);
	Light->SetWorldLocation(Peak);

	// A ball most of the time, now and then a ring tilted toward the viewer
	const bool bRing = Stream.FRand() < 0.3f;
	const FVector RingAxis = FVector(Stream.FRandRange(-0.5f, 0.5f), Stream.FRandRange(-0.5f, 0.5f), 1.f).GetSafeNormal();
	for (int32 Index = 0; Index < BurstSparks; ++Index)
	{
		FVector Direction = Stream.GetUnitVector();
		if (bRing)
		{
			Direction = FVector::VectorPlaneProject(Direction, RingAxis).GetSafeNormal();
		}
		AddSpark(Peak, Direction * Stream.FRandRange(BurstSpeed.X, BurstSpeed.Y), Stream.FRandRange(SparkLife.X, SparkLife.Y), SparkSize, true);
		SparkList.Last().bWhite = Stream.FRand() < WhiteSparkShare;
	}
}

void ARRRandomFirework::UpdateSparks(float DeltaSeconds)
{
	if (SparkList.IsEmpty())
	{
		return;
	}
	TArray<FTransform> Transforms;
	Transforms.Reserve(SparkList.Num());
	const float Slow = FMath::Exp(-Drag * DeltaSeconds);
	for (int32 Index = 0; Index < SparkList.Num(); ++Index)
	{
		FSpark& Spark = SparkList[Index];
		Spark.Age += DeltaSeconds;
		const float T = Spark.Age / Spark.Life;
		if (Spark.bBurst)
		{
			Spark.Velocity = Spark.Velocity * Slow - FVector(0.f, 0.f, Gravity * DeltaSeconds);
		}
		Spark.Location += Spark.Velocity * DeltaSeconds;

		// Bright, then fading and shrinking; burst sparks crackle near the end
		float Brightness = 0.f;
		float Scale = 0.f;
		if (T < 1.f)
		{
			Brightness = FMath::Square(1.f - T);
			if (Spark.bBurst && T > CrackleFrom)
			{
				Brightness *= 0.5f + 0.5f * FMath::Sin(Spark.Age * CrackleRate + Index * 1.7f);
			}
			Scale = Spark.Size * FMath::Lerp(1.f, 0.35f, T) / SphereSize;
		}
		Transforms.Add(FTransform(FQuat::Identity, Spark.Location, FVector(Scale)));

		const FLinearColor Color = (Spark.bBurst ? (Spark.bWhite ? FLinearColor::White : BurstColor) : ShellColor) * Brightness;
		const float Floats[3] = { Color.R, Color.G, Color.B };
		Sparks->SetCustomData(Index, Floats, false);
	}
	Sparks->BatchUpdateInstancesTransforms(0, Transforms, true, true, true);
}

void ARRRandomFirework::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bLaunched)
	{
		return;
	}
	Elapsed += DeltaSeconds;

	if (!bBurst)
	{
		// Climbing and slowing toward the top, with a trail and a small warm light
		const float T = FMath::Clamp(Elapsed / RiseTime, 0.f, 1.f);
		const FVector Shell = FMath::Lerp(Start, Peak, 1.f - FMath::Square(1.f - T));
		Light->SetWorldLocation(Shell);
		Light->SetIntensity(LightIntensity * ShellLight);
		TrailTimer -= DeltaSeconds;
		if (TrailTimer <= 0.f)
		{
			TrailTimer = TrailInterval;
			AddSpark(Shell, FVector(Stream.FRandRange(-15.f, 15.f), Stream.FRandRange(-15.f, 15.f), -40.f), TrailLife, SparkSize * 0.7f, false);
		}
		if (T >= 1.f)
		{
			Burst();
		}
	}
	UpdateSparks(DeltaSeconds);

	if (bBurst)
	{
		// The flash: brightest at the burst, falling off fast
		const float Since = Elapsed - RiseTime;
		Light->SetIntensity(LightIntensity * FMath::Square(FMath::Max(0.f, 1.f - Since / LightTime)));
		const bool bAllDone = !SparkList.ContainsByPredicate([](const FSpark& Spark) { return Spark.Age < Spark.Life; });
		if (bAllDone && Since >= LightTime)
		{
			Destroy();
		}
	}
}
