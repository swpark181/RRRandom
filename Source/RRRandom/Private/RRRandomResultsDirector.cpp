#include "RRRandomResultsDirector.h"
#include "RRRandomCharacter.h"
#include "RRRandomFirework.h"
#include "RRRandomGameState.h"
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
	// For the dance line in front of the centerpiece: straight on first, never round the side where the centerpiece would
	// come between
	const float LineAngles[] = { 0.f, 12.f, -12.f, 24.f, -24.f };
	// Room in the frame beyond each end of the dance line
	constexpr float LineFrameMargin = 160.f;
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
	// Other brawlers count as this much wider than their capsules (at scale 1): the dance sways the hips about 60 out
	// of the capsule, and the ears and arms stick out of it too
	constexpr float BrawlerClearance = 70.f;
	// A brawler in the way nearer the MVP than this (at scale 1) isn't worth pulling the camera in front of: it would
	// end up in a close-up of the MVP's face, or inside the other brawler
	constexpr float MinBrawlerPull = 300.f;
	// The frame's width over its height (the window is 16:9; the letterbox bars only make it wider)
	constexpr float FrameAspect = 16.f / 9.f;
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

void ARRRandomResultsDirector::Setup(ARRRandomCharacter* InMVP, const FMinimalViewInfo& InFromView, float InBlendTime)
{
	MVP = InMVP;
	FromView = InFromView;
	BlendTime = InBlendTime;
	Stream.GenerateNewSeed();
	// The player's look (the toon post process), faded out as the camera moves in
	Camera->PostProcessSettings = FromView.PostProcessSettings;
	Camera->PostProcessBlendWeight = FromView.PostProcessBlendWeight;
	// The winners' dance line in front of the map's centerpiece: straight at it from its open side (the centerpiece behind
	// it), far enough back for the whole line, only a little to the side if something's in the way
	float LineYaw = 0.f;
	float LineHalfWidth = 0.f;
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	bDanceLine = State && State->GetDanceLine(DanceLineCenter, LineYaw, LineHalfWidth);
	TArrayView<const float> Angles(CandidateAngles);
	if (bDanceLine)
	{
		Angles = TArrayView<const float>(LineAngles);
		Distance = FMath::Max(Distance, (LineHalfWidth + LineFrameMargin) / FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f)));
	}
	// In front of the MVP and a little to the side, or the first angle round from there with the most open view (cover
	// nearby): the whole sway checked, the frame's sides as well as its middle
	const float Facing = (bDanceLine ? LineYaw : InMVP ? InMVP->GetActorRotation().Yaw : 0.f) + 180.f;
	StartYaw = Facing - Angles[0];
	float BestScore = -1.f;
	for (const float Angle : Angles)
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
	// No angle clear of the other brawlers: stop in front of the nearest one in the way all through the sway, once, so the
	// camera doesn't jump in and out as it sways past them (unless that's too close to the MVP to help)
	const float Scale = InMVP ? InMVP->GetActorScale3D().Z : 1.f;
	BrawlerReach = UE_BIG_NUMBER;
	for (const float Sway : { -SwayAngle, 0.f, SwayAngle })
	{
		BrawlerReach = FMath::Min(BrawlerReach, GetBrawlerReach(FRotator(Pitch, StartYaw + Sway, 0.f), Distance * Scale));
	}
	if (BrawlerReach >= Distance * Scale || BrawlerReach < MinBrawlerPull * Scale)
	{
		BrawlerReach = UE_BIG_NUMBER;
	}
	const float BestReach = GetClearReach(FRotator(Pitch, StartYaw, 0.f), FMath::Min(Distance * Scale, BrawlerReach));
	UpdateCamera();

	// Everyone dancing turns to face where the camera ends up
	if (InMVP)
	{
		const FVector ViewPoint = GetTarget() - FRotator(Pitch, StartYaw, 0.f).Vector() * BestReach;
		ShotPoint = ViewPoint;
		for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
		{
			It->SetCelebrationViewPoint(ViewPoint);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("RRRandom: results camera on %s from yaw %.0f (clear %.0f of %.0f%s, view %.0f%% open)."), bDanceLine ? TEXT("the dance line") : InMVP ? *InMVP->GetName() : TEXT("nobody"), StartYaw, BestReach, Distance * Scale,
		BrawlerReach < Distance * Scale ? TEXT(", in front of a brawler") : TEXT(""), BestScore * 100.f);

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

FVector ARRRandomResultsDirector::GetTarget() const
{
	// The line's middle stays put while the MVP dashes there
	if (bDanceLine)
	{
		return DanceLineCenter;
	}
	const ARRRandomCharacter* Hero = MVP.Get();
	return Hero ? Hero->GetActorLocation() + FVector(0.f, 0.f, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * AimLift) : GetActorLocation();
}

void ARRRandomResultsDirector::UpdateCamera()
{
	const ARRRandomCharacter* Hero = MVP.Get();
	if (!Hero)
	{
		return;
	}
	const float Scale = Hero->GetActorScale3D().Z;
	const FVector Target = GetTarget();
	const FRotator View(Pitch, StartYaw + SwayAngle * FMath::Sin(2.f * PI * Elapsed / FMath::Max(SwayPeriod, 1.f)), 0.f);
	// Don't end up inside cover or a hill: stop in front of whatever is in the way (and in front of the brawler that no
	// angle could avoid, see Setup)
	const float Reach = GetClearReach(View, FMath::Min(Distance * Scale, BrawlerReach));
	const float Blend = BlendTime > 0.f ? FMath::Clamp(Elapsed / BlendTime, 0.f, 1.f) : 1.f;
	if (Blend >= 1.f)
	{
		Camera->SetFieldOfView(FieldOfView);
		Camera->PostProcessBlendWeight = 0.f;
		SetActorLocationAndRotation(Target - View.Vector() * Reach, View);
		return;
	}

	// Moving in from the player's view: round the MVP from above (down to the final height, never below it, so the view
	// doesn't swing up to the sky) while the aim pans from where the player's camera looked onto the MVP
	const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, Blend, 2.f);
	const FVector FromOffset = FromView.Location - Target;
	const FRotator FromOrbit = (-FromOffset).Rotation();
	const FRotator Orbit(FMath::Lerp(FromOrbit.Pitch, View.Pitch, Alpha), FromOrbit.Yaw + FMath::FindDeltaAngleDegrees(FromOrbit.Yaw, View.Yaw) * Alpha, 0.f);
	const float OrbitReach = FMath::Lerp((float)FromOffset.Size(), Reach, Alpha);
	// Kept clear of cover more and more on the way, so the first frame is exactly the player's view
	const FVector Location = Target - Orbit.Vector() * FMath::Lerp(OrbitReach, GetClearReach(Orbit, OrbitReach), Alpha);
	const FVector FromAim = FromView.Rotation.Vector();
	const FVector FromFocus = FromView.Location + FromAim * FMath::Max(0.f, (float)FVector::DotProduct(Target - FromView.Location, FromAim));
	Camera->SetFieldOfView(FMath::Lerp(FromView.FOV, FieldOfView, Alpha));
	Camera->PostProcessBlendWeight = FromView.PostProcessBlendWeight * (1.f - Alpha);
	SetActorLocationAndRotation(Location, (FMath::Lerp(FromFocus, Target, Alpha) - Location).Rotation());
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
	// Another brawler in the frame between the camera and the MVP: the nearer the camera (the bigger on screen), the worse
	return FMath::Min(Open / UE_ARRAY_COUNT(ViewSideLines), GetBrawlerReach(View, Distance) / Distance);
}

float ARRRandomResultsDirector::GetBrawlerReach(const FRotator& View, float Reach) const
{
	const ARRRandomCharacter* Hero = MVP.Get();
	if (!Hero)
	{
		return Reach;
	}
	// The other brawlers standing (or dancing) where they are when the match ends, each a widened upright cylinder, in
	// the frame of a camera Reach back from the MVP: right in front of the MVP (hiding it), or anywhere in the frame
	// well nearer the camera (big on screen). Those beside the MVP are in the shot with it, not in the way, and so are
	// those headed for the dance line (wherever they are now).
	const FVector Target = GetTarget();
	const FRotationMatrix Axes(View);
	const FVector Back = -Axes.GetUnitAxis(EAxis::X);
	const FVector Right = Axes.GetUnitAxis(EAxis::Y);
	const FVector Up = Axes.GetUnitAxis(EAxis::Z);
	const float HalfWidth = FMath::Tan(FMath::DegreesToRadians(FieldOfView * 0.5f));
	const float HalfHeight = HalfWidth / FrameAspect;
	const float MinAlong = MinBrawlerPull * Hero->GetActorScale3D().Z;
	float Clear = Reach;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		// Knocked-out brawlers lie below the view
		if (*It == Hero || !It->IsAlive() || (bDanceLine && It->HasFormation()))
		{
			continue;
		}
		const UCapsuleComponent* Capsule = It->GetCapsuleComponent();
		const FVector Center = It->GetActorLocation();
		const float Radius = Capsule->GetScaledCapsuleRadius() + BrawlerClearance * It->GetActorScale3D().Z + ViewRadius;
		const float Tall = Capsule->GetScaledCapsuleHalfHeight() + ViewRadius;
		const FVector Offset = Center - Target;
		const float Along = FVector::DotProduct(Offset, Back);
		const float Depth = Reach - Along;
		const float Across = FMath::Abs(FVector::DotProduct(Offset, Right));
		const float Above = FMath::Abs(FVector::DotProduct(Offset, Up));
		const bool bHiding = Along > 0.f && Across < Radius && Above < Tall;
		const bool bInFrame = Along > MinAlong && Depth > 0.f && Across - Radius < Depth * HalfWidth && Above - Tall < Depth * HalfHeight;
		if ((bHiding || bInFrame) && Along - Radius < Clear)
		{
			Clear = FMath::Max(Along - Radius, 0.f);
		}
	}
	return Clear;
}

float ARRRandomResultsDirector::GetClearReach(const FRotator& View, float Reach) const
{
	const ARRRandomCharacter* Hero = MVP.Get();
	if (!Hero)
	{
		return Reach;
	}
	const FVector Target = GetTarget();
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
	const FVector Center = GetTarget();
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
	// Behind the MVP as the camera sees it (once moved in), spread across the frame
	const FVector Forward = FRotator(0.f, StartYaw, 0.f).Vector();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward);
	const FVector Feet = GetTarget() - FVector(0.f, 0.f, Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	// Where the burst can be seen from the camera, not behind the centerpiece (or a wall): a few tries, then wherever
	FVector Ground;
	float Height = 0.f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomFireworkView), false);
	for (int32 Try = 0; Try < 6; ++Try)
	{
		Ground = Feet + Forward * Stream.FRandRange(FireworkDepth.X, FireworkDepth.Y) + Right * Stream.FRandRange(-FireworkSpread, FireworkSpread);
		Height = Stream.FRandRange(FireworkHeight.X, FireworkHeight.Y);
		if (!GetWorld()->LineTraceTestByObjectType(ShotPoint, Ground + FVector(0.f, 0.f, Height), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			break;
		}
	}

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
		Firework->Launch(Ground, Height,
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
