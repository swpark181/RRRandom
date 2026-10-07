#include "RRRandomGameMode.h"
#include "RRRandomAIController.h"
#include "RRRandomCharacter.h"
#include "RRRandomDummy.h"
#include "RRRandomHUD.h"
#include "RRRandomPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"

ARRRandomGameMode::ARRRandomGameMode()
{
	DefaultPawnClass = ARRRandomCharacter::StaticClass();
	PlayerControllerClass = ARRRandomPlayerController::StaticClass();
	HUDClass = ARRRandomHUD::StaticClass();

	BotClass = ARRRandomCharacter::StaticClass();
	DummyClass = ARRRandomDummy::StaticClass();
	// A row in front of the player (screen up is +X)
	DummyOffsets = { FVector(500.f, -300.f, 0.f), FVector(600.f, 0.f, 0.f), FVector(500.f, 300.f, 0.f) };

	// Between the two teams (the opponents start TeamSpawnDistance up the screen), clear of both starting rows.
	// 160 tall: above the height shots fly at, low enough to see brawlers behind it from the tilted camera.
	CoverClass = ARRRandomCover::StaticClass();
	auto AddCover = [this](float X, float Y, float SizeX, float SizeY)
	{
		FRRCoverPlacement& Placement = CoverLayout.AddDefaulted_GetRef();
		Placement.Offset = FVector2D(X, Y);
		Placement.Size = FVector(SizeX, SizeY, 160.f);
	};
	// Middle line: a wall across the center lane and one on each side
	AddCover(1000.f, 0.f, 120.f, 400.f);
	AddCover(1000.f, -850.f, 120.f, 320.f);
	AddCover(1000.f, 850.f, 120.f, 320.f);
	// A pair of pillars in front of each team
	AddCover(550.f, -450.f, 160.f, 160.f);
	AddCover(550.f, 450.f, 160.f, 160.f);
	AddCover(1450.f, -450.f, 160.f, 160.f);
	AddCover(1450.f, 450.f, 160.f, 160.f);
	// Long walls along the flanks
	AddCover(1000.f, -1400.f, 500.f, 120.f);
	AddCover(1000.f, 1400.f, 500.f, 120.f);
}

void ARRRandomGameMode::StartPlay()
{
	Super::StartPlay();

	// Before the bots, whose floor traces must not land on top of a block
	if (bSpawnCover)
	{
		SpawnCoverIfNoneExists();
	}
	SpawnBots();
	if (bSpawnDummies)
	{
		SpawnDummiesIfNoneExist();
	}
}

void ARRRandomGameMode::OnBrawlerKnockedOut(ARRRandomCharacter* Victim, ARRRandomCharacter* Attacker)
{
	// A knockout without an opposing attacker still goes to the other team
	const int32 ScoringTeam = Attacker && Attacker->GetTeam() != Victim->GetTeam() ? Attacker->GetTeam() : 1 - Victim->GetTeam();
	if (ScoringTeam == 0 || ScoringTeam == 1)
	{
		++TeamScores[ScoringTeam];
	}
}

FVector ARRRandomGameMode::GetPlayerStartLocation() const
{
	TActorIterator<APlayerStart> PlayerStart(GetWorld());
	return PlayerStart ? PlayerStart->GetActorLocation() : FVector::ZeroVector;
}

bool ARRRandomGameMode::FindFloorSpot(const FVector& Spot, float HalfHeight, FVector& OutLocation) const
{
	FHitResult Floor;
	if (!GetWorld()->LineTraceSingleByChannel(Floor, Spot + FVector(0.f, 0.f, 500.f), Spot - FVector(0.f, 0.f, 2000.f), ECC_Visibility))
	{
		return false;
	}
	OutLocation = FVector(Spot.X, Spot.Y, Floor.ImpactPoint.Z + HalfHeight + 2.f);
	return true;
}

void ARRRandomGameMode::SpawnBots()
{
	if (!BotClass)
	{
		return;
	}

	const FVector Origin = GetPlayerStartLocation();
	const FVector EnemyOrigin = Origin + FVector(TeamSpawnDistance, 0.f, 0.f);
	const FVector ArenaCenter = (Origin + EnemyOrigin) * 0.5f;

	// The player stands in the middle of their team, allies alternate right and left of them
	for (int32 Index = 0; Index < AllyBotCount; ++Index)
	{
		const float Side = (Index % 2 == 0) ? 1.f : -1.f;
		SpawnBot(0, Origin + FVector(0.f, Side * SpawnSpacing * (Index / 2 + 1), 0.f), 0.f, ArenaCenter);
	}

	// Opponents in a row centered across from the player, facing down the screen
	for (int32 Index = 0; Index < EnemyBotCount; ++Index)
	{
		const float Offset = (Index - (EnemyBotCount - 1) * 0.5f) * SpawnSpacing;
		SpawnBot(1, EnemyOrigin + FVector(0.f, Offset, 0.f), 180.f, ArenaCenter);
	}
}

void ARRRandomGameMode::SpawnBot(int32 Team, const FVector& Spot, float Yaw, const FVector& ArenaCenter)
{
	UWorld* World = GetWorld();
	const float HalfHeight = BotClass->GetDefaultObject<ARRRandomCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector Location;
	if (!FindFloorSpot(Spot, HalfHeight, Location))
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no floor at %s, skipping a bot for team %d."), *Spot.ToString(), Team);
		return;
	}

	// Deferred so the team (and with it the body color) is set before the bot begins play
	const FTransform SpawnTransform(FRotator(0.f, Yaw, 0.f), Location);
	ARRRandomCharacter* Bot = World->SpawnActorDeferred<ARRRandomCharacter>(BotClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Bot)
	{
		return;
	}
	Bot->SetTeam(Team);
	Bot->FinishSpawning(SpawnTransform);
	Bot->SpawnDefaultController();
	if (ARRRandomAIController* AI = Cast<ARRRandomAIController>(Bot->GetController()))
	{
		AI->SetArenaCenter(ArenaCenter);
	}
}

void ARRRandomGameMode::SpawnCoverIfNoneExists()
{
	UWorld* World = GetWorld();
	if (!CoverClass || TActorIterator<ARRRandomCover>(World))
	{
		return;
	}

	const FVector Origin = GetPlayerStartLocation();
	for (const FRRCoverPlacement& Placement : CoverLayout)
	{
		// The block's pivot is at its bottom, so it goes straight onto the floor; no floor, no block
		FVector Location;
		if (!FindFloorSpot(Origin + FVector(Placement.Offset, 0.f), 0.f, Location))
		{
			continue;
		}

		const FTransform SpawnTransform(FRotator::ZeroRotator, Location);
		if (ARRRandomCover* Cover = World->SpawnActorDeferred<ARRRandomCover>(CoverClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Cover->SetSize(Placement.Size);
			Cover->FinishSpawning(SpawnTransform);
		}
	}
}

void ARRRandomGameMode::SpawnDummiesIfNoneExist()
{
	UWorld* World = GetWorld();
	if (!DummyClass || TActorIterator<ARRRandomDummy>(World))
	{
		return;
	}

	const FVector Origin = GetPlayerStartLocation();
	const float HalfHeight = DummyClass->GetDefaultObject<ARRRandomDummy>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

	for (const FVector& Offset : DummyOffsets)
	{
		// Stand on whatever floor is there; skip spots with no floor so nothing falls forever
		FVector Location;
		if (!FindFloorSpot(Origin + Offset, HalfHeight, Location))
		{
			continue;
		}

		const FRotator FacingPlayer(0.f, (Origin - Location).Rotation().Yaw, 0.f);
		World->SpawnActor<ARRRandomDummy>(DummyClass, Location, FacingPlayer, Params);
	}
}
