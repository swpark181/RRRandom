#include "RRRandomResultsDirector.h"
#include "RRRandomCharacter.h"
#include "RRRandomFirework.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	// Saturated party colors
	const FLinearColor FireworkColors[] =
	{
		FLinearColor(1.f, 0.15f, 0.1f),
		FLinearColor(1.f, 0.6f, 0.05f),
		FLinearColor(1.f, 0.95f, 0.2f),
		FLinearColor(0.2f, 1.f, 0.3f),
		FLinearColor(0.15f, 0.55f, 1.f),
		FLinearColor(0.75f, 0.25f, 1.f),
		FLinearColor(1.f, 0.3f, 0.75f),
	};
	// Camera angles tried, round from straight in front of the MVP: three-quarter views first, the back last
	const float CandidateAngles[] = { 25.f, -25.f, 0.f, 50.f, -50.f, 75.f, -75.f, 105.f, -105.f, 140.f, -140.f, 180.f };
	// Aims at the capsule's middle
	constexpr float AimLift = 0.f;
	// The view's line is checked this thick, so it clears cover by a little
	constexpr float ViewRadius = 25.f;
	// Where across the frame the openness lines go (-1 the left edge, 1 the right), and how far out the edges are
	// counted (a share of the frame's half width at the MVP's distance, near the camera)
	const float ViewSideLines[] = { -1.f, -0.5f, 0.f, 0.5f, 1.f };
	constexpr float ViewSideShare = 0.8f;
	// Keeps the camera this far in front of a wall between it and the MVP
	constexpr float WallClearance = 20.f;
	// The first fireworks come fast, while the camera closes in
	constexpr int32 OpeningVolley = 3;
}

ARRRandomResultsDirector::ARRRandomResultsDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	// After the brawlers have moved, so the camera doesn't trail a frame behind
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	bReplicates = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	RootComponent = Camera;

	// Placed by hand each frame, not with the swaying camera
	KeyLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("KeyLight"));
	KeyLight->SetupAttachment(RootComponent);
	KeyLight->SetUsingAbsoluteLocation(true);
	KeyLight->SetUsingAbsoluteRotation(true);
	KeyLight->SetIntensityUnits(ELightUnits::Unitless);
	KeyLight->SetLightColor(FLinearColor(1.f, 0.93f, 0.82f));
	KeyLight->SetInnerConeAngle(10.f);
	KeyLight->SetOuterConeAngle(24.f);
	KeyLight->SetAttenuationRadius(2500.f);
	KeyLight->SetCastShadows(false);
	KeyLight->SetIntensity(0.f);

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(RootComponent);
	Glow->SetUsingAbsoluteLocation(true);
	Glow->SetIntensityUnits(ELightUnits::Unitless);
	Glow->SetAttenuationRadius(700.f);
	Glow->SetCastShadows(false);
	Glow->SetIntensity(0.f);
}

void ARRRandomResultsDirector::Setup(ARRRandomCharacter* InMVP)
{
	MVP = InMVP;
	Stream.GenerateNewSeed();
	Camera->SetFieldOfView(FieldOfView);
	// In front of the MVP and a little to the side, or the first angle round from there with the most open view (cover
	// nearby): the whole sway checked, the frame's sides as well as its middle
	const float Facing = (InMVP ? InMVP->GetActorRotation().Yaw : 0.f) + 180.f;
	StartYaw = Facing - CandidateAngles[0];
	float BestScore = -1.f;
	for (const float Angle : CandidateAngles)
	{
		float Score = UE_BIG_NUMBER;
		for (const float Sway : { -SwayAngle, 0.f, SwayAngle })
		{
			Score = FMath::Min(Score, GetViewOpenness(FRotator(Pitch, Facing - Angle + Sway, 0.f)));
		}
		if (Score > BestScore + 0.01f)
		{
			BestScore = Score;
			StartYaw = Facing - Angle;
		}
		if (Score >= 0.999f)
		{
			break;
		}
	}
	const float BestReach = GetClearReach(FRotator(Pitch, StartYaw, 0.f), Distance);
	UpdateCamera();

	UE_LOG(LogTemp, Log, TEXT("RRRandom: results camera on %s from yaw %.0f (clear %.0f of %.0f, view %.0f%% open)."), InMVP ? *InMVP->GetName() : TEXT("nobody"), StartYaw, BestReach, Distance, BestScore * 100.f);

	// Dusk: turn down the sun and the sky (lights that can change at run time)
	for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->Mobility != EComponentMobility::Static)
		{
			DimmedLights.Emplace(*It, It->Intensity);
		}
	}
	for (TObjectIterator<USkyLightComponent> It; It; ++It)
	{
		if (It->GetWorld() == GetWorld() && It->Mobility != EComponentMobility::Static)
		{
			DimmedSkies.Emplace(*It, It->Intensity);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RRRandom: results dims %d sun and %d sky lights."), DimmedLights.Num(), DimmedSkies.Num());
	for (int32 Index = 0; Index < OpeningVolley; ++Index)
	{
		LaunchFirework();
	}
	FireworkTimer = Stream.FRandRange(FireworkInterval.X, FireworkInterval.Y);
}

void ARRRandomResultsDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TPair<TWeakObjectPtr<ULightComponent>, float>& Light : DimmedLights)
	{
		if (Light.Key.IsValid())
		{
			Light.Key->SetIntensity(Light.Value);
		}
	}
	for (const TPair<TWeakObjectPtr<USkyLightComponent>, float>& Sky : DimmedSkies)
	{
		if (Sky.Key.IsValid())
		{
			Sky.Key->SetIntensity(Sky.Value);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ARRRandomResultsDirector::UpdateCamera()
{
	const ARRRandomCharacter* Hero = MVP.Get();
	if (!Hero)
	{
		return;
	}
	const float Scale = Hero->GetActorScale3D().Z;
	const FVector Target = Hero->GetActorLocation() + FVector(0.f, 0.f, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * AimLift);
	const FRotator View(Pitch, StartYaw + SwayAngle * FMath::Sin(2.f * PI * Elapsed / FMath::Max(SwayPeriod, 1.f)), 0.f);
	// Don't end up inside cover or a hill: stop in front of whatever is in the way
	const float Reach = GetClearReach(View, Distance * Scale);
	SetActorLocationAndRotation(Target - View.Vector() * Reach, View);
}

float ARRRandomResultsDirector::GetViewOpenness(const FRotator& View) const
{
	// Lines from the MVP fanning out to the middle and the sides of where the camera would stand, as far as the frame's
	// edges near the camera: how much of each is clear, on average (1 = nothing anywhere near the view)
	const FVector Side = FRotationMatrix(View).GetUnitAxis(EAxis::Y) * Distance * FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f)) * ViewSideShare;
	float Open = 0.f;
	for (const float Across : ViewSideLines)
	{
		const FVector Toward = -View.Vector() * Distance + Side * Across;
		Open += GetClearReach((-Toward).Rotation(), Toward.Size()) / Toward.Size();
	}
	return Open / UE_ARRAY_COUNT(ViewSideLines);
}

float ARRRandomResultsDirector::GetClearReach(const FRotator& View, float Reach) const
{
	const ARRRandomCharacter* Hero = MVP.Get();
	if (!Hero)
	{
		return Reach;
	}
	const FVector Target = Hero->GetActorLocation() + FVector(0.f, 0.f, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * AimLift);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomResultsCamera), false, Hero);
	if (GetWorld()->SweepSingleByObjectType(Hit, Target, Target - View.Vector() * Reach, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(ViewRadius), Params))
	{
		return FMath::Max(Hit.Distance - WallClearance, 50.f);
	}
	return Reach;
}

void ARRRandomResultsDirector::OnFireworkBurst(const FLinearColor& Color)
{
	SinceBurst = 0.f;
	GlowColor = Color;
}

void ARRRandomResultsDirector::UpdateLights(float DeltaSeconds)
{
	const ARRRandomCharacter* Hero = MVP.Get();
	if (!Hero)
	{
		return;
	}
	const FVector Center = Hero->GetActorLocation();
	const FVector ToCamera = (GetActorLocation() - Center).GetSafeNormal2D();

	// Key: from above the camera down onto the MVP, coming up with the dusk
	const FVector KeySpot = GetActorLocation() + FVector(0.f, 0.f, KeyLightHeight);
	KeyLight->SetWorldLocationAndRotation(KeySpot, (Center - KeySpot).Rotation());
	KeyLight->SetIntensity(KeyLightIntensity * FMath::Clamp(Elapsed / FMath::Max(DimTime, KINDA_SMALL_NUMBER), 0.f, 1.f));

	// Glow: in front of the MVP, flashing the last burst's color and fading
	SinceBurst += DeltaSeconds;
	Glow->SetWorldLocation(Center + ToCamera * GlowOffset.X + FVector(0.f, 0.f, GlowOffset.Y));
	Glow->SetLightColor(GlowColor);
	Glow->SetIntensity(GlowIntensity * FMath::Square(FMath::Max(0.f, 1.f - SinceBurst / FMath::Max(GlowFadeTime, KINDA_SMALL_NUMBER))));
}

void ARRRandomResultsDirector::LaunchFirework()
{
	const ARRRandomCharacter* Hero = MVP.Get();
	if (!Hero)
	{
		return;
	}
	// Behind the MVP as the camera sees it, spread across the frame
	const FVector Forward = FRotator(0.f, GetActorRotation().Yaw, 0.f).Vector();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector Feet = Hero->GetActorLocation() - FVector(0.f, 0.f, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Ground = Feet + Forward * Stream.FRandRange(FireworkDepth.X, FireworkDepth.Y) + Right * Stream.FRandRange(-FireworkSpread, FireworkSpread);

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ARRRandomFirework* Firework = GetWorld()->SpawnActor<ARRRandomFirework>(Ground, FRotator::ZeroRotator, SpawnParams))
	{
		TWeakObjectPtr<ARRRandomResultsDirector> WeakThis(this);
		Firework->OnBurst = [WeakThis](const FLinearColor& Color)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnFireworkBurst(Color);
			}
		};
		Firework->Launch(Ground, Stream.FRandRange(FireworkHeight.X, FireworkHeight.Y),
			FireworkColors[Stream.RandRange(0, UE_ARRAY_COUNT(FireworkColors) - 1)], Stream.RandHelper(MAX_int32));
	}
}

void ARRRandomResultsDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Elapsed += DeltaSeconds;
	UpdateCamera();
	UpdateLights(DeltaSeconds);

	const float Dim = FMath::Lerp(1.f, DimTo, FMath::Clamp(Elapsed / FMath::Max(DimTime, KINDA_SMALL_NUMBER), 0.f, 1.f));
	for (const TPair<TWeakObjectPtr<ULightComponent>, float>& Light : DimmedLights)
	{
		if (Light.Key.IsValid())
		{
			Light.Key->SetIntensity(Light.Value * Dim);
		}
	}
	for (const TPair<TWeakObjectPtr<USkyLightComponent>, float>& Sky : DimmedSkies)
	{
		if (Sky.Key.IsValid())
		{
			Sky.Key->SetIntensity(Sky.Value * Dim);
		}
	}

	FireworkTimer -= DeltaSeconds;
	if (FireworkTimer <= 0.f)
	{
		LaunchFirework();
		FireworkTimer = Stream.FRandRange(FireworkInterval.X, FireworkInterval.Y);
	}
}
