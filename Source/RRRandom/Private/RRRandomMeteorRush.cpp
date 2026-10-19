#include "RRRandomMeteorRush.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceTown.h"
#include "RRRandomGameMode.h"
#include "RRRandomGameState.h"
#include "RRRandomMeteor.h"
#include "RRRandomOre.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ARRRandomMeteorRushDirector::ARRRandomMeteorRushDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
}

void ARRRandomMeteorRushDirector::BeginPlay()
{
	Super::BeginPlay();
	Stream.GenerateNewSeed();
}

void ARRRandomMeteorRushDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ARRRandomGameMode* GameMode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>();
	ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (!GameMode || !State || State->IsMatchOver() || !State->HasPlayStarted())
	{
		return;
	}

	// Meteors on the clock (none in overtime but the golden one)
	const float Since = State->GetSincePlayStart();
	if (!bOvertime && Since >= NextMeteorTime - ARRRandomMeteor::WarnTime)
	{
		SpawnMeteor(false);
		NextMeteorTime += MeteorInterval;
	}
	// And now and then a falling star (not too near a meteor's landing)
	if (!bOvertime && Since >= NextStarTime)
	{
		if (FMath::Abs(Since - (NextMeteorTime - MeteorInterval)) > 2.f && FMath::Abs(Since - NextMeteorTime) > 2.f)
		{
			DropFallingStar();
		}
		NextStarTime = Since + Stream.FRandRange(FallingStarMin, FallingStarMax);
	}

	// The scores are what each team carries; overtime's holder is whoever has the golden crystal
	int32 Carried[2] = { 0, 0 };
	int32 GoldenTeam = INDEX_NONE;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		const URRRandomOreCarryComponent* Carry = It->GetOreCarry();
		if (!Carry || It->IsDecoy() || (It->GetTeam() != 0 && It->GetTeam() != 1))
		{
			continue;
		}
		Carried[It->GetTeam()] += Carry->GetPoints();
		if (Carry->HasGolden() && It->IsAlive())
		{
			GoldenTeam = It->GetTeam();
		}
	}
	State->SetTeamScore(0, Carried[0]);
	State->SetTeamScore(1, Carried[1]);

	if (bOvertime)
	{
		// The golden crystal gone (off the edge): another golden meteor
		if (GoldenTeam == INDEX_NONE)
		{
			bool bGoldenAround = false;
			for (TActorIterator<ARRRandomOre> It(GetWorld()); It && !bGoldenAround; ++It)
			{
				bGoldenAround = It->GetKind() == ERROreKind::Golden;
			}
			for (TActorIterator<ARRRandomMeteor> It(GetWorld()); It && !bGoldenAround; ++It)
			{
				bGoldenAround = It->IsGolden() && It->IsActive();
			}
			if (!bGoldenAround)
			{
				SpawnMeteor(true);
			}
		}
		UpdateHold(GoldenTeam, GoldenHoldSeconds);
		return;
	}

	// Holding the goal, and more than the other team: counting down
	const int32 Goal = State->GetScoreToWin();
	const int32 Leader = Carried[0] >= Goal && Carried[0] > Carried[1] ? 0 : Carried[1] >= Goal && Carried[1] > Carried[0] ? 1 : INDEX_NONE;
	UpdateHold(Leader, HoldSeconds);
	if (State->IsMatchOver())
	{
		return;
	}

	// Time: the team carrying more wins; a tie goes to overtime and the golden meteor
	if (State->GetMatchTimeRemaining() <= 0.f)
	{
		if (Carried[0] != Carried[1])
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: time is up, %d : %d."), Carried[0], Carried[1]);
			GameMode->EndMatchFor(Carried[0] > Carried[1] ? 0 : 1);
			return;
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: time is up at %d : %d, overtime: the golden meteor."), Carried[0], Carried[1]);
		bOvertime = true;
		State->SetOvertime(true);
		State->SetHold(INDEX_NONE, 0.f);
		SpawnMeteor(true);
	}
}

void ARRRandomMeteorRushDirector::UpdateHold(int32 Leader, float Seconds)
{
	ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (Leader != State->GetHoldTeam())
	{
		State->SetHold(Leader, Seconds);
		if (Leader != INDEX_NONE)
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: team %d holds %d : %d, %.0f seconds to win."), Leader, State->GetTeamScore(0), State->GetTeamScore(1), Seconds);
		}
		return;
	}
	if (Leader != INDEX_NONE && State->GetHoldRemaining() <= 0.f)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: team %d held on and wins."), Leader);
		if (ARRRandomGameMode* GameMode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>())
		{
			GameMode->EndMatchFor(Leader);
		}
	}
}

void ARRRandomMeteorRushDirector::SpawnMeteor(bool bGolden)
{
	const ARRRandomGameMode* GameMode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>();
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	FVector Spot;
	if (!PickSpot(Spot, bGolden))
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no floor for a meteor."));
		return;
	}
	const float ImpactTime = State->GetServerWorldTimeSeconds() + ARRRandomMeteor::WarnTime;
	const FTransform Transform(Spot);
	if (ARRRandomMeteor* Meteor = GetWorld()->SpawnActorDeferred<ARRRandomMeteor>(ARRRandomMeteor::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Meteor->Setup(ImpactTime, bGolden);
		Meteor->FinishSpawning(Transform);
	}
	LastSpot = Spot;
	bHasLastSpot = true;
	UE_LOG(LogTemp, Log, TEXT("RRRandom: a %smeteor is coming down at %s."), bGolden ? TEXT("golden ") : TEXT(""), *Spot.ToString());
}

void ARRRandomMeteorRushDirector::EnsureGroundZ()
{
	if (bHasGroundZ)
	{
		return;
	}
	bHasGroundZ = true;
	GroundZ = GetWorld()->GetAuthGameMode<ARRRandomGameMode>()->GetArenaCenter().Z - 90.f;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		GroundZ = It->GetHomeLocation().Z - It->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
		break;
	}
}

void ARRRandomMeteorRushDirector::DropFallingStar()
{
	FVector Spot;
	if (!PickStarSpot(Spot))
	{
		return;
	}
	const float Roll = Stream.FRand();
	const ERROreKind Kind = Roll < 0.6f ? ERROreKind::Shard : Roll < 0.9f ? ERROreKind::Ore : ERROreKind::Big;
	// From high up and off to one side, so it streaks down at a slant (its tail shows from the top-down camera too)
	const FVector From = Spot + FVector(1500.f, 0.f, 0.f).RotateAngleAxis(Stream.FRandRange(0.f, 360.f), FVector::UpVector) + FVector(0.f, 0.f, 2800.f);
	const FTransform Transform(From);
	if (ARRRandomOre* Star = GetWorld()->SpawnActorDeferred<ARRRandomOre>(ARRRandomOre::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Star->SetupFallingStar(Kind, Spot);
		Star->FinishSpawning(Transform);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: a falling star (%s) comes down at %s."), *StaticEnum<ERROreKind>()->GetNameStringByValue(int64(Kind)), *Spot.ToString());
	}
}

bool ARRRandomMeteorRushDirector::PickStarSpot(FVector& OutSpot)
{
	const ARRRandomGameMode* GameMode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>();
	const FVector Center = GameMode->GetArenaCenter();
	const float Distance = GameMode->GetTeamSpawnDistance();
	TActorIterator<ARRRandomDiceTown> Town(GetWorld());
	if (Town && Town->IsBuilt())
	{
		for (int32 Try = 0; Try < 60; ++Try)
		{
			const FVector Spot = Town->GetRandomWalkSpot(Stream, Distance * 0.5f);
			if (FMath::Abs(Spot.X - Center.X) <= Distance * 0.35f)
			{
				OutSpot = Spot;
				return true;
			}
		}
	}
	EnsureGroundZ();
	for (int32 Try = 0; Try < 60; ++Try)
	{
		const FVector Probe = Center + FVector(Stream.FRandRange(-0.35f, 0.35f) * Distance, Stream.FRandRange(-0.45f, 0.45f) * Distance, 0.f);
		FHitResult Floor;
		if (GetWorld()->LineTraceSingleByObjectType(Floor, Probe + FVector(0.f, 0.f, 4000.f), Probe - FVector(0.f, 0.f, 4000.f), FCollisionObjectQueryParams(ECC_WorldStatic))
			&& Floor.ImpactNormal.Z >= 0.85f && FMath::Abs(Floor.ImpactPoint.Z - GroundZ) <= 40.f)
		{
			OutSpot = Floor.ImpactPoint;
			return true;
		}
	}
	return false;
}

bool ARRRandomMeteorRushDirector::PickSpot(FVector& OutSpot, bool bMiddle)
{
	const ARRRandomGameMode* GameMode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>();
	const FVector Center = GameMode->GetArenaCenter();
	// The teams start TeamSpawnDistance apart along X: spots near X = the middle are as far from both. The golden meteor
	// goes as near the very middle as it can.
	const float Distance = GameMode->GetTeamSpawnDistance();
	const float MaxAcross = bMiddle ? 450.f : Distance * 0.15f;
	const float MaxAlong = bMiddle ? 450.f : Distance * 0.45f;
	const float MinApart = bMiddle ? 0.f : Distance * 0.28f;
	auto FarFromLast = [&](const FVector& Spot) { return !bHasLastSpot || FVector::Dist2D(Spot, LastSpot) >= MinApart; };

	// A map with walking routes: a spot anyone can walk to
	TActorIterator<ARRRandomDiceTown> Town(GetWorld());
	if (Town && Town->IsBuilt())
	{
		for (int32 Try = 0; Try < 200; ++Try)
		{
			const FVector Spot = Town->GetRandomWalkSpot(Stream, MaxAlong);
			if (FMath::Abs(Spot.X - Center.X) <= MaxAcross && FarFromLast(Spot))
			{
				OutSpot = Spot;
				return true;
			}
		}
	}

	// Elsewhere: open, flat ground, level with where the teams start (not up on a hill, a bridge or a block of cover,
	// where bots can't climb)
	EnsureGroundZ();
	// The first good spot far enough from the last one; failing that, the good spot furthest from it
	float BestApart = -1.f;
	for (int32 Try = 0; Try < 160; ++Try)
	{
		const FVector Probe = Center + FVector(Stream.FRandRange(-MaxAcross, MaxAcross), Stream.FRandRange(-MaxAlong, MaxAlong), 0.f);
		FHitResult Floor;
		if (!GetWorld()->LineTraceSingleByObjectType(Floor, Probe + FVector(0.f, 0.f, 4000.f), Probe - FVector(0.f, 0.f, 4000.f), FCollisionObjectQueryParams(ECC_WorldStatic))
			|| Floor.ImpactNormal.Z < 0.85f || FMath::Abs(Floor.ImpactPoint.Z - GroundZ) > 40.f)
		{
			continue;
		}
		if (FarFromLast(Floor.ImpactPoint))
		{
			OutSpot = Floor.ImpactPoint;
			return true;
		}
		const float Apart = FVector::Dist2D(Floor.ImpactPoint, LastSpot);
		if (Apart > BestApart)
		{
			BestApart = Apart;
			OutSpot = Floor.ImpactPoint;
		}
	}
	return BestApart >= 0.f;
}
