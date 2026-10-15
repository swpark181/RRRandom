#include "RRRandomGameMode.h"
#include "RRRandomSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "RRRandomAIController.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceLand.h"
#include "RRRandomDiceTown.h"
#include "RRRandomDummy.h"
#include "RRRandomGrassFloor.h"
#include "RRRandomIsland.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "RRRandomPlayerController.h"
#include "RRRandomVehicle.h"
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

	// In the field between the two village roads (the teams start TeamSpawnDistance apart, either side of the middle),
	// off the main road and the ring road (the island's map: main road |Y| < 180, west lane Y -1980..-1800, east lane
	// Y 1620..1800) and clear of the field's low hills and solid props. 160 tall: above the height shots fly at, low
	// enough to see brawlers behind it from the tilted camera.
	CoverClass = ARRRandomCover::StaticClass();
	auto AddCover = [this](float X, float Y, float SizeX, float SizeY)
	{
		FRRCoverPlacement& Placement = CoverLayout.AddDefaulted_GetRef();
		Placement.Offset = FVector2D(X, Y);
		Placement.Size = FVector(SizeX, SizeY, 160.f);
	};
	// Middle line: a wall on each side of the main road, and one further out on each side
	AddCover(0.f, -650.f, 120.f, 360.f);
	AddCover(0.f, 650.f, 120.f, 360.f);
	AddCover(0.f, -1500.f, 120.f, 320.f);
	AddCover(0.f, 1350.f, 120.f, 320.f);
	// A pair of pillars in front of each team
	AddCover(750.f, -400.f, 160.f, 160.f);
	AddCover(750.f, 400.f, 160.f, 160.f);
	AddCover(-750.f, -400.f, 160.f, 160.f);
	AddCover(-750.f, 400.f, 160.f, 160.f);
	// Long walls across the field, one on each team's side (each opposite a low hill)
	AddCover(800.f, 1100.f, 360.f, 120.f);
	AddCover(-800.f, -1250.f, 360.f, 120.f);
}

void ARRRandomGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Dice Town (the third person map picked on the title) is big: the teams start far apart, at their bases. Set before
	// anyone logs in, since the host's own brawler is placed as it does (on the level's floor, level with the town's)
	const URRRandomSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>() : nullptr;
	bDiceTown = bSpawnDiceLand && Sessions && Sessions->IsThirdPersonMode() && Sessions->GetThirdPersonMap() == ERRThirdPersonMap::DiceTown;
	if (bDiceTown)
	{
		TeamSpawnDistance = ARRRandomDiceTown::TeamSpawnDistance;
	}
}

void ARRRandomGameMode::StartPlay()
{
	// Dice Town goes up as play begins, before the bots, whose floor traces land on it
	if (bDiceTown)
	{
		SpawnDiceTownIfNoneExists();
	}

	Super::StartPlay();

	// -RRScoreToWin=N on the command line, for quick tests of the end of a match
	FParse::Value(FCommandLine::Get(), TEXT("RRScoreToWin="), ScoreToWin);
	ScoreToWin = FMath::Max(1, ScoreToWin);
	if (ARRRandomGameState* State = GetGameState<ARRRandomGameState>())
	{
		State->SetScoreToWin(ScoreToWin);
		// Third person or the top view, as picked on this machine's title (or -RRThirdPerson)
		const URRRandomSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>() : nullptr;
		State->SetThirdPersonMode(Sessions && Sessions->IsThirdPersonMode());
		State->SetPlayStartTime(GetWorld()->GetTimeSeconds() + StartCountdown);
	}

	// Found on the bare level, before anything is built on its floor
	GetArenaCenter();
	if (bSpawnDiceLand && ARRRandomGameState::IsThirdPersonMode(this))
	{
		// The third person map brings its own ground and cover; it goes up before the bots, whose floor traces land on it
		if (!bDiceTown)
		{
			SpawnDiceLandIfNoneExists();
		}
	}
	else
	{
		// The grass first: it looks for the level's floor under the middle, where the island puts a road
		if (bSpawnGrassFloor)
		{
			SpawnGrassFloorIfNoneExists();
		}
		// The island's hills and walls go up before the cover and the bots, whose floor traces must land beside them
		if (bSpawnIsland)
		{
			SpawnIslandIfNoneExists();
		}
		if (bSpawnCover)
		{
			SpawnCoverIfNoneExists();
		}
	}
	if (bSpawnVehicles)
	{
		SpawnVehiclesIfNoneExist();
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
	ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	if (State && State->IsMatchOver())
	{
		return;
	}
	// A knockout without an opposing attacker still goes to the other team
	const int32 ScoringTeam = Attacker && Attacker->GetTeam() != Victim->GetTeam() ? Attacker->GetTeam() : 1 - Victim->GetTeam();
	if (Attacker && Attacker->GetTeam() != Victim->GetTeam())
	{
		Attacker->AddKill();
	}
	if (State)
	{
		State->AddTeamScore(ScoringTeam);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) knocked out by %s; point to team %d."), *Victim->GetName(), Victim->GetTeam(),
		Attacker ? *Attacker->GetName() : TEXT("a fall"), ScoringTeam);
	if (State && State->GetTeamScore(ScoringTeam) >= ScoreToWin)
	{
		EndMatch(ScoringTeam);
	}
}

void ARRRandomGameMode::EndMatch(int32 Winner)
{
	// MVP: the winning team's one with the most knockouts (the results show the winners dancing round it), then whoever
	// is still standing; anyone's only if the winners have nobody
	ARRRandomCharacter* MVP = nullptr;
	auto Rank = [Winner](const ARRRandomCharacter* Brawler)
	{
		return (Brawler->GetTeam() == Winner ? 100000 : 0) + Brawler->GetKills() * 4 + (Brawler->IsAlive() ? 1 : 0);
	};
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		// (a fox's clones are no one)
		if (It->IsDecoy())
		{
			continue;
		}
		if (!MVP || Rank(*It) > Rank(MVP))
		{
			MVP = *It;
		}
	}
	// The camera closes in on the MVP, so it stands up for it; everyone else comes to dance behind it
	if (MVP)
	{
		MVP->ReviveNow();
		ArrangeVictoryFormation(MVP, Winner);
	}
	if (ARRRandomGameState* State = GetGameState<ARRRandomGameState>())
	{
		State->EndMatch(Winner, MVP);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: match over, team %d wins; MVP %s with %d knockouts."), Winner, MVP ? *MVP->GetName() : TEXT("nobody"), MVP ? MVP->GetKills() : 0);
}

void ARRRandomGameMode::ArrangeVictoryFormation(ARRRandomCharacter* MVP, int32 Winner)
{
	// A V of the winning team behind the MVP, like backup dancers: the results camera looks at the MVP's face, so each pair
	// stands a step further back and further out to the sides, all in view. The losers stay where they are and slump.
	constexpr float RowGap = 130.f;
	constexpr float SideGap = 150.f;
	MVP->SetFormationLeader();
	// A different dance (or slump) each, starting somewhere random, so the line doesn't move in step
	int32 Pick = FMath::RandRange(0, 255);
	TArray<ARRRandomCharacter*> Dancers;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsDecoy())
		{
			continue;
		}
		It->SetCelebrationPick(Pick++);
		if (*It != MVP && It->GetTeam() == Winner)
		{
			Dancers.Add(*It);
		}
	}
	const float Yaw = MVP->GetActorRotation().Yaw;
	const FVector Front = FRotator(0.f, Yaw, 0.f).Vector();
	const FVector Right = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
	const float FloorZ = MVP->GetActorLocation().Z - MVP->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TArray<FVector> Taken = { MVP->GetActorLocation() };
	for (int32 Index = 0; Index < Dancers.Num(); ++Index)
	{
		ARRRandomCharacter* Dancer = Dancers[Index];
		// In pairs, right then left: the first pair one step back and out, the next a step further, and so on
		const int32 Step = Index / 2 + 1;
		const float Side = Index % 2 == 0 ? 1.f : -1.f;
		const FVector Wanted = MVP->GetActorLocation() - Front * RowGap * Step + Right * Side * SideGap * Step;
		Dancer->ReviveNow();
		FVector Spot;
		if (!FindFormationSpot(Wanted, FloorZ, Dancer, Taken, Spot))
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: no room in the dance line for %s."), *Dancer->GetName());
			continue;
		}
		Taken.Add(Spot);
		Dancer->SetFormationSpot(Spot);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %d winners line up behind %s to dance."), Taken.Num() - 1, *MVP->GetName());
}

bool ARRRandomGameMode::FindFormationSpot(const FVector& Wanted, float FloorZ, const ARRRandomCharacter* Brawler, const TArray<FVector>& Taken, FVector& OutSpot) const
{
	constexpr float MinApart = 120.f;
	const UCapsuleComponent* Capsule = Brawler->GetCapsuleComponent();
	const float Radius = Capsule->GetUnscaledCapsuleRadius();
	const float HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	// The wanted spot, then rings round it further and further out
	for (int32 Ring = 0; Ring <= 5; ++Ring)
	{
		const int32 Tries = Ring == 0 ? 1 : 8;
		for (int32 Try = 0; Try < Tries; ++Try)
		{
			const FVector Probe = Wanted + FVector(Ring * 70.f, 0.f, 0.f).RotateAngleAxis(Try * 45.f + Ring * 20.f, FVector::UpVector);
			FVector Location;
			// On the ground the MVP stands on (not the top of a cover block), out of everything solid, apart from the others
			if (!FindFloorSpot(Probe, HalfHeight, Location) || FMath::Abs(Location.Z - HalfHeight - FloorZ) > 60.f)
			{
				continue;
			}
			FCollisionObjectQueryParams Solid;
			Solid.AddObjectTypesToQuery(ECC_WorldStatic);
			Solid.AddObjectTypesToQuery(ECC_WorldDynamic);
			FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomFormation), false, Brawler);
			if (GetWorld()->OverlapAnyTestByObjectType(Location, FQuat::Identity, Solid, FCollisionShape::MakeCapsule(Radius, HalfHeight - 5.f), Params)
				|| Taken.ContainsByPredicate([&Location](const FVector& Other) { return FVector::Dist2D(Other, Location) < MinApart; }))
			{
				continue;
			}
			OutSpot = Location;
			return true;
		}
	}
	return false;
}

void ARRRandomGameMode::CheatWin(int32 Team)
{
	ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	if (State && !State->IsMatchOver() && (Team == 0 || Team == 1))
	{
		State->SetTeamScore(Team, FMath::Max(State->GetTeamScore(Team), ScoreToWin));
		UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, team %d wins."), Team);
		EndMatch(Team);
	}
}

void ARRRandomGameMode::RestartMatch()
{
	// The same map from scratch (a listen server keeps listening and brings its clients along)
	UE_LOG(LogTemp, Log, TEXT("RRRandom: new match."));
	GetWorld()->ServerTravel(TEXT("?Restart"), false);
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
	// Blue below the middle, red above it across from blue, facing down the screen
	const FVector Origin = GetArenaCenter() + FVector(TeamSpawnDistance * (Team == 1 ? 0.5f : -0.5f), 0.f, 0.f);
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

ARRRandomCharacter* ARRRandomGameMode::SpawnBrawler(TSubclassOf<ARRRandomCharacter> BrawlerClass, int32 Team, int32 Slot, int32 Species)
{
	const float HalfHeight = BrawlerClass->GetDefaultObject<ARRRandomCharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FTransform SpawnTransform;
	if (!GetSlotTransform(Team, Slot, HalfHeight, SpawnTransform))
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no floor for spot %d of team %d, leaving it empty."), Slot, Team);
		return nullptr;
	}

	// Deferred so the team (and with it the body color) and the species are set before the brawler begins play
	ARRRandomCharacter* Brawler = GetWorld()->SpawnActorDeferred<ARRRandomCharacter>(BrawlerClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Brawler)
	{
		return nullptr;
	}
	Brawler->SetTeam(Team);
	Brawler->SetSpecies(Species);
	Brawler->FinishSpawning(SpawnTransform);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s is a %s on team %d."), *Brawler->GetName(), RRSpecies::GetAssetName(Species), Team);
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
	// This machine's own player is the one picked on its title; a joiner's pick comes with ServerChooseSpecies
	const URRRandomSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>() : nullptr;
	const ARRRandomPlayerController* Player = Cast<ARRRandomPlayerController>(NewPlayer);
	const int32 Species = Player && Player->GetChosenSpecies() != INDEX_NONE ? Player->GetChosenSpecies()
		: NewPlayer && NewPlayer->IsLocalController() && Sessions ? Sessions->GetSpecies() : RRSpecies::Fox;
	ARRRandomCharacter* Brawler = SpawnBrawler(PawnClass, Team, Slot, Species);
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

FVector ARRRandomGameMode::GetArenaCenter() const
{
	if (!ArenaCenter.IsSet())
	{
		// The level's floor under the player start: its middle, so the island fits on it with sea all round
		const FVector Start = GetPlayerStartLocation();
		FHitResult Floor;
		FVector Center = Start;
		if (GetWorld()->LineTraceSingleByChannel(Floor, Start + FVector(0.f, 0.f, 500.f), Start - FVector(0.f, 0.f, 2000.f), ECC_Visibility) && Floor.GetComponent())
		{
			const FVector Middle = Floor.GetComponent()->Bounds.Origin;
			Center = FVector(Middle.X, Middle.Y, Start.Z);
		}
		ArenaCenter = Center;
		UE_LOG(LogTemp, Log, TEXT("RRRandom: arena center %s (player start %s)."), *Center.ToString(), *Start.ToString());
	}
	return ArenaCenter.GetValue();
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

	const FVector Center = GetArenaCenter();
	for (int32 Team = 0; Team < 2; ++Team)
	{
		for (int32 Slot = 0; Slot < TeamSlots[Team].Num(); ++Slot)
		{
			if (TeamSlots[Team][Slot].Brawler.IsValid() || TeamSlots[Team][Slot].Player.IsValid())
			{
				continue;
			}
			// Bots are any of the brawlers
			ARRRandomCharacter* Bot = SpawnBrawler(BotClass, Team, Slot, FMath::RandRange(0, RRSpecies::Count - 1));
			if (!Bot)
			{
				continue;
			}
			Bot->SpawnDefaultController();
			if (ARRRandomAIController* AI = Cast<ARRRandomAIController>(Bot->GetController()))
			{
				AI->SetArenaCenter(Center);
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

	const FVector Origin = GetArenaCenter();
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

void ARRRandomGameMode::SpawnVehiclesIfNoneExist()
{
	UWorld* World = GetWorld();
	if (TActorIterator<ARRRandomVehicle>(World))
	{
		return;
	}
	struct FParking
	{
		ERRVehicleKind Kind;
		FVector2D Offset;
		float Yaw;
	};
	// From the arena's center (X up the screen, Y right): each team's kickboard and motorbike just ahead of its village
	// road, on opposite sides, facing the field; a car on each side lane of the ring road, halfway, pointing opposite ways
	const float Near = TeamSpawnDistance * 0.5f - 230.f;
	const FParking Spots[] =
	{
		{ ERRVehicleKind::Kickboard, FVector2D(-Near, -900.f), 0.f },
		{ ERRVehicleKind::Motorbike, FVector2D(-Near, 900.f), 0.f },
		{ ERRVehicleKind::Kickboard, FVector2D(Near, 900.f), 180.f },
		{ ERRVehicleKind::Motorbike, FVector2D(Near, -900.f), 180.f },
		{ ERRVehicleKind::Car, FVector2D(90.f, -1890.f), 0.f },
		{ ERRVehicleKind::Car, FVector2D(-90.f, 1710.f), 180.f },
	};
	const FVector Origin = GetArenaCenter();
	for (const FParking& Spot : Spots)
	{
		// Its pivot is the middle of its bottom, so it goes straight onto the floor; no floor, no vehicle
		FVector Location;
		if (!FindFloorSpot(Origin + FVector(Spot.Offset, 0.f), 0.f, Location))
		{
			continue;
		}
		const FTransform SpawnTransform(FRotator(0.f, Spot.Yaw, 0.f), Location);
		if (ARRRandomVehicle* Vehicle = World->SpawnActorDeferred<ARRRandomVehicle>(ARRRandomVehicle::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Vehicle->SetKind(Spot.Kind);
			Vehicle->FinishSpawning(SpawnTransform);
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
	// Around the arena's center, halfway between the teams; its map puts each team's start on a village road
	World->SpawnActor<ARRRandomIsland>(ARRRandomIsland::StaticClass(), FTransform(GetArenaCenter()));
}

void ARRRandomGameMode::SpawnDiceLandIfNoneExists()
{
	UWorld* World = GetWorld();
	if (TActorIterator<ARRRandomDiceLand>(World))
	{
		return;
	}
	// Around the arena's center, halfway between the teams; its roads run through each team's starting row
	World->SpawnActor<ARRRandomDiceLand>(ARRRandomDiceLand::StaticClass(), FTransform(GetArenaCenter()));
}

void ARRRandomGameMode::SpawnGrassFloorIfNoneExists()
{
	UWorld* World = GetWorld();
	if (TActorIterator<ARRRandomGrassFloor>(World))
	{
		return;
	}
	// Over the middle of the arena, halfway between the teams; it finds the floor under itself
	const FTransform SpawnTransform(GetArenaCenter());
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

	// In front of blue's starting row
	const FVector Origin = GetArenaCenter() - FVector(TeamSpawnDistance * 0.5f, 0.f, 0.f);
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

void ARRRandomGameMode::SpawnDiceTownIfNoneExists()
{
	UWorld* World = GetWorld();
	if (TActorIterator<ARRRandomDiceTown>(World))
	{
		return;
	}
	// Around the arena's center, halfway between the teams; each team starts at its base on the main road
	World->SpawnActor<ARRRandomDiceTown>(ARRRandomDiceTown::StaticClass(), FTransform(GetArenaCenter()));
}
