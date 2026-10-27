#include "RRRandomCannon.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceTown.h"
#include "RRRandomEffects.h"
#include "RRRandomHUD.h"
#include "RRRandomUIStyle.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The barrel's mouth on SM_Cannon (300 long, 286 high), from its pivot (the middle of its bottom), the barrel rising
	// toward +X about 25 degrees
	const FVector MuzzleOffset(110.f, 0.f, 230.f);
	// Without the models: an engine cylinder (100 across, 100 high, pivot in its middle) for the cannon and a flat one for the pad
	const FVector FallbackBodyScale(1.4f, 1.4f, 1.6f);
	const FVector FallbackPadScale(4.f, 4.f, 0.1f);
	// SM_CannonPad is a puffy cushion 120 high: squashed flat to this share, so it is a step anyone walks off (it stops nothing)
	constexpr float PadSquash = 0.3f;
	// The blocker round the cannon's base
	constexpr float BlockerRadius = 95.f;
	constexpr float BlockerHalfHeight = 90.f;
	// The arc: this fast across the ground (within FlightTimeRange seconds), rising this share of the way's length over the
	// higher end (at least ArcMinRise), and never closer than CeilingMargin under the map's flight ceiling
	constexpr float CrossSpeed = 1800.f;
	const FVector2D FlightTimeRange(2.5f, 5.f);
	constexpr float ArcRiseShare = 0.28f;
	constexpr float ArcMinRise = 900.f;
	constexpr float CeilingMargin = 150.f;
	// Loading: the barrel squashes down this much (and swells this much round), then springs up this much as it fires
	constexpr float LoadSquash = 0.15f;
	constexpr float LoadSwell = 0.06f;
	constexpr float FirePunch = 0.18f;
	constexpr float FirePunchTime = 0.35f;
	// Looking for open ground for a set: rings of candidate spots this far apart (out to 2000, a cannon out to CannonReach
	// for a clear arc), the ground within the clearance flat at floor level (no blocks, stairs or roofs) and nothing over
	// it up to SkyClear
	constexpr float SearchStep = 100.f;
	constexpr float CannonReach = 3500.f;
	constexpr float CannonClear = 300.f;
	constexpr float PadClear = 350.f;
	constexpr float SkyClear = 2600.f;
	constexpr float FloorTolerance = 30.f;
	// Dice Town's sets, from the town's middle (blue's half at -X; red's are these turned half way round): each team's
	// cannons beside its base, their pads out on the far flanks of the other half
	struct FSetSpot
	{
		FVector2D Cannon;
		FVector2D Pad;
	};
	const FSetSpot TownSets[] =
	{
		{ FVector2D(-4300.f, -1500.f), FVector2D(1800.f, -6200.f) },
		{ FVector2D(-4300.f, 1500.f), FVector2D(1800.f, 6200.f) },
	};

}

bool ARRRandomCannon::FindOpenGround(UWorld* World, const ARRRandomDiceTown* Town, const FVector2D& Want, float Clear, FVector& Out, TFunctionRef<bool(const FVector&)> Accept, float MaxReach)
{
	const FBox Area = Town->GetFlightArea();
	// GetFlightArea's floor is 1000 under level 0's top
	const float FloorZ = Area.Min.Z + 1000.f;
	auto IsGround = [Town](const FVector2D& Spot)
	{
		int32 Levels = -1;
		Town->GetMapColor(Spot, Levels);
		return Levels == 0;
	};
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRCannonSpot), false);
	for (float Reach = 0.f; Reach <= MaxReach; Reach += SearchStep)
	{
		const int32 Steps = Reach > 0.f ? FMath::Max(8, FMath::RoundToInt(2.f * PI * Reach / SearchStep)) : 1;
		for (int32 Step = 0; Step < Steps; ++Step)
		{
			const float Angle = 2.f * PI * Step / Steps;
			const FVector2D Spot = Want + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Reach;
			bool bOpen = true;
			for (float DX = -Clear; DX <= Clear && bOpen; DX += SearchStep)
			{
				for (float DY = -Clear; DY <= Clear && bOpen; DY += SearchStep)
				{
					if (DX * DX + DY * DY <= Clear * Clear)
					{
						bOpen = IsGround(Spot + FVector2D(DX, DY));
					}
				}
			}
			if (!bOpen)
			{
				continue;
			}
			// The floor right there at level 0 (not a canal's bed), and the sky clear over it
			FHitResult Floor;
			if (!World->LineTraceSingleByChannel(Floor, FVector(Spot, FloorZ + SkyClear), FVector(Spot, FloorZ - 500.f), ECC_Visibility, Params)
				|| FMath::Abs(Floor.ImpactPoint.Z - FloorZ) > FloorTolerance)
			{
				continue;
			}
			FHitResult Roof;
			if (World->LineTraceSingleByChannel(Roof, FVector(Spot, FloorZ + 50.f), FVector(Spot, FloorZ + SkyClear), ECC_Visibility, Params))
			{
				continue;
			}
			if (!Accept(Floor.ImpactPoint))
			{
				continue;
			}
			Out = Floor.ImpactPoint;
			return true;
		}
	}
	return false;
}

ARRRandomCannon::ARRRandomCannon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CannonMesh(TEXT("/Game/Props/Cannon/SM_Cannon.SM_Cannon"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PadMesh(TEXT("/Game/Props/Cannon/SM_CannonPad.SM_CannonPad"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	if (CannonMesh.Succeeded())
	{
		Body->SetStaticMesh(CannonMesh.Object);
	}
	else if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
		Body->SetRelativeLocation(FVector(0.f, 0.f, 50.f * FallbackBodyScale.Z));
		Body->SetRelativeScale3D(FallbackBodyScale);
	}

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	Pad->SetupAttachment(RootComponent);
	Pad->SetUsingAbsoluteLocation(true);
	Pad->SetUsingAbsoluteRotation(true);
	// Flat on the floor: walked over, not in the way of anything
	Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pad->SetCanEverAffectNavigation(false);
	if (PadMesh.Succeeded())
	{
		Pad->SetStaticMesh(PadMesh.Object);
		Pad->SetRelativeScale3D(FVector(1.f, 1.f, PadSquash));
	}
	else if (Cylinder.Succeeded())
	{
		Pad->SetStaticMesh(Cylinder.Object);
		Pad->SetRelativeScale3D(FallbackPadScale);
	}

	Blocker = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Blocker"));
	Blocker->SetupAttachment(RootComponent);
	Blocker->InitCapsuleSize(BlockerRadius, BlockerHalfHeight);
	Blocker->SetRelativeLocation(FVector(0.f, 0.f, BlockerHalfHeight));
	Blocker->SetCollisionProfileName(TEXT("BlockAll"));
	Blocker->SetCanEverAffectNavigation(true);
	Blocker->SetHiddenInGame(true);
}

void ARRRandomCannon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomCannon, PadLocation);
	DOREPLIFETIME(ARRRandomCannon, FireCount);
	DOREPLIFETIME(ARRRandomCannon, Index);
}

void ARRRandomCannon::BeginPlay()
{
	Super::BeginPlay();
	BodyScale = Body->GetRelativeScale3D();
	PlacePad();
}

void ARRRandomCannon::SetPad(const FVector& Where)
{
	PadLocation = Where;
	PlacePad();
}

void ARRRandomCannon::OnRep_PadLocation()
{
	PlacePad();
}

void ARRRandomCannon::PlacePad()
{
	// The fallback cylinder's pivot is its middle: lift it by half its thickness
	const bool bFallback = !Pad->GetStaticMesh() || Pad->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Engine"));
	Pad->SetWorldLocation(FVector(PadLocation) + FVector(0.f, 0.f, bFallback ? 50.f * FallbackPadScale.Z : 0.f));
	Pad->SetWorldRotation(FRotator(0.f, (FVector(PadLocation) - GetActorLocation()).Rotation().Yaw, 0.f));
}

FVector ARRRandomCannon::GetMuzzleLocation() const
{
	return GetActorTransform().TransformPosition(MuzzleOffset);
}

bool ARRRandomCannon::IsReady() const
{
	const float Since = GetWorld()->GetTimeSeconds() - (HasAuthority() ? LastLoadTime : LoadStartTime);
	return Since >= LoadTime + ReloadTime;
}

float ARRRandomCannon::GetReloadShare() const
{
	const float Since = GetWorld()->GetTimeSeconds() - (HasAuthority() ? LastLoadTime : LoadStartTime);
	return FMath::Clamp(Since / (LoadTime + ReloadTime), 0.f, 1.f);
}

namespace
{
	/** The arc's two ends and how high its top is (over the line between them), from a muzzle to a pad, for a flier HalfHeight from feet to middle. */
	void ArcEnds(const UWorld* World, const FVector& Muzzle, const FVector& PadSpot, float HalfHeight, FVector& OutStart, FVector& OutEnd, float& OutRise)
	{
		OutStart = Muzzle;
		OutEnd = PadSpot + FVector(0.f, 0.f, HalfHeight + 5.f);
		const float Across = FVector::Dist2D(OutStart, OutEnd);
		float Peak = FMath::Max(OutStart.Z, OutEnd.Z) + FMath::Max(ArcMinRise, Across * ArcRiseShare);
		// Under the map's flight ceiling (the feet's, so the middle's is HalfHeight higher)
		for (TActorIterator<ARRRandomDiceTown> Town(World); Town; ++Town)
		{
			Peak = FMath::Min(Peak, Town->GetFlightArea().Max.Z + HalfHeight - CeilingMargin);
			break;
		}
		OutRise = FMath::Max(0.f, Peak - (OutStart.Z + OutEnd.Z) * 0.5f);
	}

	/** Whether an animal friend shot from a cannon standing at From would fly clear all the way to a pad at To. */
	bool IsArcClear(UWorld* World, const FVector& From, const FVector& To)
	{
		// (An animal friend's capsule with a margin round it; past the barrel's mouth, up to just over the pad)
		constexpr float HalfHeight = 96.f;
		constexpr float Radius = 55.f;
		constexpr int32 Steps = 48;
		const FVector Muzzle = From + FRotator(0.f, (To - From).Rotation().Yaw, 0.f).RotateVector(MuzzleOffset);
		FVector Start;
		FVector End;
		float Rise;
		ArcEnds(World, Muzzle, To, HalfHeight, Start, End, Rise);
		auto At = [&](float A) { return FMath::Lerp(Start, End, A) + FVector(0.f, 0.f, 4.f * Rise * A * (1.f - A)); };
		const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight + 10.f);
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(RRCannonArc), false);
		for (int32 Step = 2; Step < Steps - 2; ++Step)
		{
			if (World->SweepTestByChannel(At(float(Step) / Steps), At(float(Step + 1) / Steps), FQuat::Identity, ECC_Pawn, Shape, Params))
			{
				return false;
			}
		}
		return true;
	}
}

float ARRRandomCannon::GetFlightTime() const
{
	return FMath::Clamp(FVector::Dist2D(GetMuzzleLocation(), FVector(PadLocation)) / CrossSpeed, FlightTimeRange.X, FlightTimeRange.Y);
}

FVector ARRRandomCannon::GetArcPoint(float Alpha, float HalfHeight) const
{
	FVector Start;
	FVector End;
	float Rise;
	ArcEnds(GetWorld(), GetMuzzleLocation(), PadLocation, HalfHeight, Start, End, Rise);
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	// Straight across, with a parabola laid over the line between the ends: Rise up at the middle
	return FMath::Lerp(Start, End, A) + FVector(0.f, 0.f, 4.f * Rise * A * (1.f - A));
}

FVector ARRRandomCannon::GetArcVelocity(float Alpha, float HalfHeight) const
{
	FVector Start;
	FVector End;
	float Rise;
	ArcEnds(GetWorld(), GetMuzzleLocation(), PadLocation, HalfHeight, Start, End, Rise);
	const float A = FMath::Clamp(Alpha, 0.f, 1.f);
	return ((End - Start) + FVector(0.f, 0.f, 4.f * Rise * (1.f - 2.f * A))) / GetFlightTime();
}

bool ARRRandomCannon::Fire(ARRRandomCharacter* Who)
{
	if (!HasAuthority() || !Who || !IsReady())
	{
		return false;
	}
	LastLoadTime = GetWorld()->GetTimeSeconds();
	++FireCount;
	OnRep_FireCount();
	Who->StartCannonRide(this);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s climbs into cannon %d (%.0f to its pad, %.1f s)."), *Who->GetName(), Index,
		FVector::Dist2D(GetActorLocation(), FVector(PadLocation)), GetFlightTime());
	return true;
}

void ARRRandomCannon::OnRep_FireCount()
{
	LoadStartTime = GetWorld()->GetTimeSeconds();
	bBoomShown = false;
}

void ARRRandomCannon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Loading: the barrel squashes down round the one climbing in; firing: it springs up past its size and settles
	const float Since = GetWorld()->GetTimeSeconds() - LoadStartTime;
	FVector Scale = BodyScale;
	if (Since < LoadTime)
	{
		const float Squash = FMath::Sin(FMath::Clamp(Since / LoadTime, 0.f, 1.f) * HALF_PI);
		Scale *= FVector(1.f + LoadSwell * Squash, 1.f + LoadSwell * Squash, 1.f - LoadSquash * Squash);
	}
	else if (Since < LoadTime + FirePunchTime)
	{
		const float Alpha = (Since - LoadTime) / FirePunchTime;
		const float Spring = FMath::Sin(Alpha * PI * 2.f) * (1.f - Alpha);
		Scale *= FVector(1.f - FirePunch * 0.4f * Spring, 1.f - FirePunch * 0.4f * Spring, 1.f + FirePunch * Spring);
	}
	if (!Body->GetRelativeScale3D().Equals(Scale))
	{
		Body->SetRelativeScale3D(Scale);
	}

	// BOOM: a jet of fire and smoke out of the mouth, a pop of stars, the word over it
	if (!bBoomShown && Since >= LoadTime)
	{
		bBoomShown = true;
		const FVector Muzzle = GetMuzzleLocation();
		const FVector Way = (GetArcPoint(0.05f, 0.f) - GetArcPoint(0.f, 0.f)).GetSafeNormal();
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->RocketLaunch(Muzzle, Way, RRUI::Butter);
			Effects->FoxPoof(Muzzle, 1.4f);
			Effects->DiceLandBurst(Muzzle + Way * 40.f, 70.f);
		}
		ARRRandomHUD::PopComicWord(this, TEXT("BOOM!"), Muzzle + FVector(0.f, 0.f, 90.f), RRUI::PinkDeep, 1.4f);
	}
}

ARRRandomCannon* ARRRandomCannon::FindInReach(const ARRRandomCharacter* Who)
{
	if (!Who || !Who->IsAlive() || Who->IsDecoy() || Who->IsClimbing() || Who->GetVehicle() || Who->GetRobot() || Who->IsGiant() || Who->CanFly()
		|| Who->IsUsingUltimate() || Who->IsRidingCannon())
	{
		return nullptr;
	}
	const float Feet = Who->GetActorLocation().Z - Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	ARRRandomCannon* Nearest = nullptr;
	float NearestDistance = UseRange + BlockerRadius;
	for (TActorIterator<ARRRandomCannon> It(Who->GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Who->GetActorLocation());
		if (It->IsReady() && Distance <= NearestDistance && FMath::Abs(It->GetActorLocation().Z - Feet) < 150.f)
		{
			Nearest = *It;
			NearestDistance = Distance;
		}
	}
	return Nearest;
}

ARRRandomCannon* ARRRandomCannon::SpawnSet(UWorld* World, const FVector& From, const FVector& To, int32 InIndex)
{
	// Facing its pad, so the barrel points the way it shoots
	const FTransform Transform(FRotator(0.f, (To - From).Rotation().Yaw, 0.f), From);
	ARRRandomCannon* Cannon = World->SpawnActorDeferred<ARRRandomCannon>(ARRRandomCannon::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Cannon)
	{
		Cannon->Index = InIndex;
		Cannon->SetPad(To);
		Cannon->FinishSpawning(Transform);
	}
	return Cannon;
}

void ARRRandomCannon::SpawnDiceTownSets(UWorld* World, const ARRRandomDiceTown* Town)
{
	if (!World || !Town || TActorIterator<ARRRandomCannon>(World))
	{
		return;
	}
	const FVector2D Middle(Town->GetActorLocation().X, Town->GetActorLocation().Y);
	int32 Count = 0;
	for (const FSetSpot& Set : TownSets)
	{
		// Blue's set, then red's: the same turned half way round the middle (the town is the same both ways round), looked
		// for from where blue's ended up, so the two match exactly where they can
		FVector2D WantCannon = Middle + Set.Cannon;
		FVector2D WantPad = Middle + Set.Pad;
		for (const float Turn : { 1.f, -1.f })
		{
			FVector From;
			FVector To;
			if (Turn < 0.f)
			{
				WantCannon = Middle * 2.f - WantCannon;
				WantPad = Middle * 2.f - WantPad;
			}
			// The pad first, then a cannon near its spot whose arc out to the pad is clear of the buildings
			if (!FindOpenGround(World, Town, WantPad, PadClear, To)
				|| !FindOpenGround(World, Town, WantCannon, CannonClear, From, [World, &To](const FVector& Spot) { return IsArcClear(World, Spot, To); }, CannonReach))
			{
				UE_LOG(LogTemp, Warning, TEXT("RRRandom: no open ground for a cannon set near %s -> %s."), *(Set.Cannon * Turn).ToString(), *(Set.Pad * Turn).ToString());
				continue;
			}
			SpawnSet(World, From, To, Count);
			WantCannon = FVector2D(From);
			WantPad = FVector2D(To);
			UE_LOG(LogTemp, Log, TEXT("RRRandom: cannon %d at %s, its pad at %s (%.0f apart)."), Count, *(FVector2D(From) - Middle).ToString(),
				*(FVector2D(To) - Middle).ToString(), FVector::Dist2D(From, To));
			++Count;
		}
	}
}

void ARRRandomCannon::SpawnDefenseSets(UWorld* World, const ARRRandomDiceTown* Town, const FVector& Center)
{
	if (!World || !Town || TActorIterator<ARRRandomCannon>(World))
	{
		return;
	}
	// A defense stage (round 90): four cannons inside the wall ring on the diagonals, each shooting out over the walls to a
	// pad halfway to the rim, between the roads the villains come in by
	const FVector2D Middle(Center.X, Center.Y);
	const float CannonOut = Town->GetDefenseRingRadius() * 0.75f;
	const float PadOut = FMath::Max(Town->GetDefenseRimRadius() * 0.55f, Town->GetDefenseRingRadius() + 1200.f);
	int32 Count = 0;
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const FVector2D Way = FVector2D(FRotator(0.f, 45.f + 90.f * Corner, 0.f).Vector());
		FVector From;
		FVector To;
		if (!FindOpenGround(World, Town, Middle + Way * PadOut, PadClear, To)
			|| !FindOpenGround(World, Town, Middle + Way * CannonOut, CannonClear, From, [World, &To](const FVector& Spot) { return IsArcClear(World, Spot, To); }, 600.f))
		{
			UE_LOG(LogTemp, Warning, TEXT("RRRandom: no open ground for a defense cannon set on corner %d."), Corner);
			continue;
		}
		SpawnSet(World, From, To, Count);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: defense cannon %d at %s, its pad at %s (%.0f apart)."), Count, *(FVector2D(From) - Middle).ToString(),
			*(FVector2D(To) - Middle).ToString(), FVector::Dist2D(From, To));
		++Count;
	}
}
