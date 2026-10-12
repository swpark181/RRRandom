#include "RRRandomGameMode.h"
#include "RRRandomAIController.h"
#include "RRRandomCharacter.h"
#include "RRRandomDummy.h"
#include "RRRandomGrassFloor.h"
#include "RRRandomIsland.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "RRRandomPlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"

ARRRandomGameMode::ARRRandomGameMode()
{
	DefaultPawnClass = ARRRandomCharacter::StaticClass();
	PlayerControllerClass = ARRRandomPlayerController::StaticClass();
	HUDClass = ARRRandomHUD::StaticClass();
	GameStateClass = ARRRandomGameState::StaticClass();

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
	// The island's hills and walls go up before the cover and the bots, whose floor traces must land beside them
	if (bSpawnIsland)
	{
		SpawnIslandIfNoneExists();
	}
	if (bSpawnGrassFloor)
	{
		SpawnGrassFloorIfNoneExists();
	}
	if (bSpawnCover)
	{
		SpawnCoverIfNoneExists();
	}
	// The host's own brawler may already hold a spot; bots take the rest
	FillEmptySlotsWithBots();
	if (bSpawnDummies)
	{
		SpawnDummiesIfNoneExist();
	}
}

void ARRRandomGameMode::OnBrawlerKnockedOut(ARRRandomCharacter* Victim, ARRRandomCharacter* Attacker)
{
	// A knockout without an opposing attacker still goes to the other team
	const int32 ScoringTeam = Attacker && Attacker->GetTeam() != Victim->GetTeam() ? Attacker->GetTeam() : 1 - Victim->GetTeam();
	if (ARRRandomGameState* State = GetGameState<ARRRandomGameState>())
	{
		State->AddTeamScore(ScoringTeam);
	}
}

void ARRRandomGameMode::EnsureSlots()
{
	for (TArray<FTeamSlot>& Slots : TeamSlots)
	{
		if (Slots.Num() < TeamSize)
		{
			Slots.SetNum(TeamSize);
		}
	}
}

int32 ARRRandomGameMode::ChooseTeamForPlayer() const
{
	int32 Players[2] = { 0, 0 };
	for (int32 Team = 0; Team < 2; ++Team)
	{
		for (const FTeamSlot& Slot : TeamSlots[Team])
		{
			Players[Team] += Slot.Player.IsValid() ? 1 : 0;
		}
	}

	// The team with fewer players (blue on a tie), or blue until it is full; the other team if that one is full
	const int32 Preferred = bSplitPlayersAcrossTeams && Players[1] < Players[0] ? 1 : 0;
	if (FindSlotForPlayer(Preferred) != INDEX_NONE)
	{
		return Preferred;
	}
	return FindSlotForPlayer(1 - Preferred) != INDEX_NONE ? 1 - Preferred : INDEX_NONE;
}

int32 ARRRandomGameMode::FindSlotForPlayer(int32 Team) const
{
	return TeamSlots[Team].IndexOfByPredicate([](const FTeamSlot& Slot) { return !Slot.Player.IsValid(); });
}

bool ARRRandomGameMode::GetSlotTransform(int32 Team, int32 Slot, float HalfHeight, FTransform& OutTransform) const
{
	// Red lines up across from blue, facing down the screen
	const FVector Origin = GetPlayerStartLocation() + (Team == 1 ? FVector(TeamSpawnDistance, 0.f, 0.f) : FVector::ZeroVector);
	// The middle spot first (the first player's), then alternating right and left of it
	const float Side = Slot % 2 == 1 ? 1.f : -1.f;
	const FVector Spot = Origin + FVector(0.f, Side * ((Slot + 1) / 2) * SpawnSpacing, 0.f);

	FVector Location;
	if (!FindFloorSpot(Spot, HalfHeight, Location))
	{
		return false;
	}
	OutTransform = FTransform(FRotator(0.f, Team == 1 ? 180.f : 0.f, 0.f), Location);
	return true;
}

ARRRandomCharacter* ARRRandomGameMode::SpawnBrawler(TSubclassOf<ARRRandomCharacter> BrawlerClass, int32 Team, int32 Slot)
{
	const float HalfHeight = BrawlerClass->GetDefaultObject<ARRRandomCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FTransform SpawnTransform;
	if (!GetSlotTransform(Team, Slot, HalfHeight, SpawnTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no floor for spot %d of team %d, leaving it empty."), Slot, Team);
		return nullptr;
	}

	// Deferred so the team (and with it the body color) is set before the brawler begins play
	ARRRandomCharacter* Brawler = GetWorld()->SpawnActorDeferred<ARRRandomCharacter>(BrawlerClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Brawler)
	{
		return nullptr;
	}
	Brawler->SetTeam(Team);
	Brawler->FinishSpawning(SpawnTransform);
	TeamSlots[Team][Slot].Brawler = Brawler;
	return Brawler;
}

APawn* ARRRandomGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	EnsureSlots();

	// A player takes a spot on a team, replacing the bot standing there
	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	const int32 Team = PawnClass && PawnClass->IsChildOf<ARRRandomCharacter>() ? ChooseTeamForPlayer() : INDEX_NONE;
	const int32 Slot = Team != INDEX_NONE ? FindSlotForPlayer(Team) : INDEX_NONE;
	if (Slot == INDEX_NONE)
	{
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}

	RemoveBot(Team, Slot);
	ARRRandomCharacter* Brawler = SpawnBrawler(PawnClass, Team, Slot);
	if (!Brawler)
	{
		return Super::SpawnDefaultPawnFor_Implementation(NewPlayer, StartSpot);
	}
	TeamSlots[Team][Slot].Player = NewPlayer;
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s joins team %d at spot %d."), *GetNameSafe(NewPlayer), Team, Slot);
	return Brawler;
}

void ARRRandomGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	for (TArray<FTeamSlot>& Slots : TeamSlots)
	{
		for (FTeamSlot& Slot : Slots)
		{
			if (Slot.Player.Get() == Exiting)
			{
				// The leaving player's brawler normally goes with them; make sure, then let a bot take over next frame
				if (ARRRandomCharacter* Brawler = Slot.Brawler.Get())
				{
					Brawler->Destroy();
				}
				Slot = FTeamSlot();
				UE_LOG(LogTemp, Log, TEXT("RRRandom: %s left; a bot takes over their spot."), *GetNameSafe(Exiting));
				GetWorldTimerManager().SetTimerForNextTick(this, &ARRRandomGameMode::FillEmptySlotsWithBots);
			}
		}
	}
}

void ARRRandomGameMode::RemoveBot(int32 Team, int32 Slot)
{
	FTeamSlot& Entry = TeamSlots[Team][Slot];
	if (Entry.Player.IsValid())
	{
		return;
	}
	if (ARRRandomCharacter* Bot = Entry.Brawler.Get())
	{
		if (AController* Controller = Bot->GetController())
		{
			Controller->Destroy();
		}
		Bot->Destroy();
	}
	Entry = FTeamSlot();
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

void ARRRandomGameMode::FillEmptySlotsWithBots()
{
	if (!BotClass || !HasActorBegunPlay())
	{
		return;
	}
	EnsureSlots();

	const FVector ArenaCenter = GetPlayerStartLocation() + FVector(TeamSpawnDistance * 0.5f, 0.f, 0.f);
	for (int32 Team = 0; Team < 2; ++Team)
	{
		for (int32 Slot = 0; Slot < TeamSlots[Team].Num(); ++Slot)
		{
			if (TeamSlots[Team][Slot].Brawler.IsValid() || TeamSlots[Team][Slot].Player.IsValid())
			{
				continue;
			}
			ARRRandomCharacter* Bot = SpawnBrawler(BotClass, Team, Slot);
			if (!Bot)
			{
				continue;
			}
			Bot->SpawnDefaultController();
			if (ARRRandomAIController* AI = Cast<ARRRandomAIController>(Bot->GetController()))
			{
				AI->SetArenaCenter(ArenaCenter);
			}
		}
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

void ARRRandomGameMode::SpawnIslandIfNoneExists()
{
	UWorld* World = GetWorld();
	if (TActorIterator<ARRRandomIsland>(World))
	{
		return;
	}
	// Centered halfway between the teams; its map puts each team's start on a sand path
	World->SpawnActor<ARRRandomIsland>(ARRRandomIsland::StaticClass(), FTransform(GetPlayerStartLocation() + FVector(TeamSpawnDistance * 0.5f, 0.f, 0.f)));
}

void ARRRandomGameMode::SpawnGrassFloorIfNoneExists()
{
	UWorld* World = GetWorld();
	if (TActorIterator<ARRRandomGrassFloor>(World))
	{
		return;
	}
	// Over the middle of the arena, halfway between the teams; it finds the floor under itself
	const FTransform SpawnTransform(GetPlayerStartLocation() + FVector(TeamSpawnDistance * 0.5f, 0.f, 0.f));
	if (ARRRandomGrassFloor* Grass = World->SpawnActorDeferred<ARRRandomGrassFloor>(ARRRandomGrassFloor::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
	{
		Grass->Seed = GrassSeed;
		Grass->FinishSpawning(SpawnTransform);
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
