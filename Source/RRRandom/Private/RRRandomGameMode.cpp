#include "RRRandomGameMode.h"
#include "RRRandomBattleRoyale.h"
#include "RRRandomSessionSubsystem.h"
#include "Engine/GameInstance.h"
#include "RRRandomAIController.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceLand.h"
#include "RRRandomCannon.h"
#include "RRRandomDiceTown.h"
#include "RRRandomDefense.h"
#include "RRRandomPlaneStation.h"
#include "RRRandomDummy.h"
#include "RRRandomGrassFloor.h"
#include "RRRandomIsland.h"
#include "RRRandomGameState.h"
#include "RRRandomLoadout.h"
#include "RRRandomUltimateMeteor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RRRandomMeteorRush.h"
#include "RRRandomOre.h"
#include "RRRandomHUD.h"
#include "RRRandomPlayerController.h"
#include "RRRandomStudio.h"
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
	// A third person map's level (/Game/Maps, URRRandomSessionSubsystem::GetStartMap) has its map actor placed: that map,
	// played third person whatever was picked (so the level also plays straight from the editor)
	UWorld* World = GetWorld();
	const bool bPlacedTown = static_cast<bool>(TActorIterator<ARRRandomDiceTown>(World));
	const bool bPlacedStudio = static_cast<bool>(TActorIterator<ARRRandomStudio>(World));
	bPlacedThirdPersonMap = bPlacedTown || bPlacedStudio || static_cast<bool>(TActorIterator<ARRRandomDiceLand>(World));
	const bool bPicked = !bPlacedThirdPersonMap && bSpawnDiceLand && Sessions && Sessions->IsThirdPersonMode();
	bDiceTown = bPlacedTown || (bPicked && Sessions->GetThirdPersonMap() == ERRThirdPersonMap::DiceTown);
	if (bDiceTown)
	{
		TeamSpawnDistance = ARRRandomDiceTown::TeamSpawnDistance;
	}
	bStudio = bPlacedStudio || (bPicked && Sessions->GetThirdPersonMap() == ERRThirdPersonMap::Studio);
	if (bStudio)
	{
		TeamSpawnDistance = ARRRandomStudio::TeamSpawnDistance;
	}
	// Defense (round 90): a defense stage, built in code on the bare arena (URRRandomSessionSubsystem::GetStartMap), the animal
	// friends all on blue round the tower in its middle
	bDefense = !bPlacedThirdPersonMap && Sessions && Sessions->GetMatchRule() == ERRMatchRule::Defense;
	if (bDefense)
	{
		DefenseStage = Sessions->GetDefenseStage();
		bDiceTown = true;
		bStudio = false;
		bSplitPlayersAcrossTeams = false;
		TeamSpawnDistance = RRDefense::FriendStartOut * 2.f;
	}
	// The battle royale (round 92): its level has the battle map placed (or it is built on the bare arena); ten teams of three
	// (the test map four), every player on the first team with room, the teams' starts spread round the island
	const bool bPlacedBattle = static_cast<bool>(TActorIterator<ARRRandomBattleMap>(World));
	bBattleRoyale = bPlacedBattle || (Sessions && Sessions->GetMatchRule() == ERRMatchRule::BattleRoyale);
	if (bBattleRoyale)
	{
		bBattleRoyaleTest = Sessions && Sessions->IsBattleRoyaleTest();
		if (bPlacedBattle)
		{
			bBattleRoyaleTest = TActorIterator<ARRRandomBattleMap>(World)->GetReach() <= RRBattle::TestTileReach;
		}
		BattleTeamCount = bBattleRoyaleTest ? RRBattle::TestTeamCount : RRBattle::TeamCount;
		FParse::Value(FCommandLine::Get(), TEXT("RRBRTeams="), BattleTeamCount);
		BattleTeamCount = FMath::Clamp(BattleTeamCount, 2, RRBattle::TeamCount);
		TeamSize = RRBattle::TeamSize;
		bPlacedThirdPersonMap = true;
		bDiceTown = false;
		bDefense = false;
		bStudio = false;
		bSplitPlayersAcrossTeams = false;
	}
	TeamSlots.SetNum(GetTeamCount());
}

void ARRRandomGameMode::StartPlay()
{
	// Dice Town goes up as play begins, before the bots, whose floor traces land on it
	if (bDiceTown)
	{
		SpawnDiceTownIfNoneExists();
	}
	// The battle map's tiles: spawned now, each built as play begins (before the bots, whose floor traces land on them)
	const double BattleBuildStart = FPlatformTime::Seconds();
	if (bBattleRoyale)
	{
		ARRRandomBattleMap* Battle = ARRRandomBattleMap::Get(this);
		if (!Battle)
		{
			const FTransform Where(GetArenaCenter());
			Battle = GetWorld()->SpawnActorDeferred<ARRRandomBattleMap>(ARRRandomBattleMap::StaticClass(), Where);
			if (Battle)
			{
				Battle->SetReach(bBattleRoyaleTest ? RRBattle::TestTileReach : RRBattle::TileReach);
				Battle->FinishSpawning(Where);
			}
		}
		if (Battle)
		{
			Battle->SpawnTiles();
		}
	}

	Super::StartPlay();
	if (bBattleRoyale)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: battle map built in %.2f s."), FPlatformTime::Seconds() - BattleBuildStart);
	}

	// The rule picked on this machine's title (or -RRRule=MeteorRush): Meteor Rush holds ore points instead of counting
	// knockouts, and has a clock
	const URRRandomSessionSubsystem* Sessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>() : nullptr;
	bMeteorRush = Sessions && Sessions->GetMatchRule() == ERRMatchRule::MeteorRush;
	if (bMeteorRush)
	{
		ScoreToWin = ARRRandomMeteorRushDirector::ScoreToWin;
	}
	// -RRScoreToWin=N on the command line, for quick tests of the end of a match
	FParse::Value(FCommandLine::Get(), TEXT("RRScoreToWin="), ScoreToWin);
	ScoreToWin = FMath::Max(1, ScoreToWin);
	if (ARRRandomGameState* State = GetGameState<ARRRandomGameState>())
	{
		State->SetScoreToWin(ScoreToWin);
		// Third person or the top view, as picked on this machine's title (or -RRThirdPerson); a third person map's level is
		// always third person
		State->SetThirdPersonMode(bPlacedThirdPersonMap || bDefense || (Sessions && Sessions->IsThirdPersonMode()));
		if (bDefense)
		{
			State->SetDefense(DefenseStage);
		}
		if (bBattleRoyale)
		{
			State->SetBattleRoyale(BattleTeamCount);
		}
		const float PlayStart = GetWorld()->GetTimeSeconds() + StartCountdown;
		State->SetPlayStartTime(PlayStart);
		if (bMeteorRush)
		{
			// -RRRushLength=N (seconds) shortens the clock for tests
			float Length = ARRRandomMeteorRushDirector::MatchLength;
			FParse::Value(FCommandLine::Get(), TEXT("RRRushLength="), Length);
			State->SetMeteorRush(true);
			State->SetMatchEndTime(PlayStart + FMath::Max(10.f, Length));
			GetWorld()->SpawnActor<ARRRandomMeteorRushDirector>();
			UE_LOG(LogTemp, Log, TEXT("RRRandom: Meteor Rush, hold %d for %.0f seconds, %.0f second clock, a meteor every %.0f seconds."), ScoreToWin,
				ARRRandomMeteorRushDirector::HoldSeconds, Length, ARRRandomMeteorRushDirector::MeteorInterval);
		}
	}

	// Found on the bare level, before anything is built on its floor
	GetArenaCenter();
	if ((bSpawnDiceLand || bPlacedThirdPersonMap) && ARRRandomGameState::IsThirdPersonMode(this))
	{
		// The third person map brings its own ground and cover; it goes up before the bots, whose floor traces land on it
		if (bStudio)
		{
			SpawnStudioIfNoneExists();
		}
		else if (!bDiceTown && !bBattleRoyale)
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
	if (bSpawnVehicles && !bDefense && !bBattleRoyale)
	{
		SpawnVehiclesIfNoneExist();
	}
	// The battle royale: the battle map's rides, the ring, and everyone to their team's start
	if (bBattleRoyale)
	{
		SetUpBattleRoyale();
	}
	// Dice Town's human cannons and plane stations (round 88), on open ground by each team's base and out on the flanks
	if (bDiceTown && !bDefense)
	{
		if (const ARRRandomDiceTown* Town = TActorIterator<ARRRandomDiceTown>(GetWorld()) ? *TActorIterator<ARRRandomDiceTown>(GetWorld()) : nullptr; Town && Town->IsBuilt())
		{
			ARRRandomCannon::SpawnDiceTownSets(GetWorld(), Town);
			ARRRandomPlaneStation::SpawnDiceTownStations(GetWorld(), Town);
		}
	}
	// Defense: the tower, the walls, the waves and the stage's rides
	if (bDefense)
	{
		SetUpDefense();
	}
	// The host's own brawler may already hold a spot; bots take the rest
	FillEmptySlotsWithBots();
	// (the battle royale: everyone counted standing from the start)
	if (bBattleRoyale)
	{
		UpdateBattleStanding(nullptr);
	}
	if (bSpawnDummies)
	{
		SpawnDummiesIfNoneExist();
	}
}

void ARRRandomGameMode::OnBrawlerKnockedOut(ARRRandomCharacter* Victim, ARRRandomCharacter* Attacker, bool bFell)
{
	ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	if (State && State->IsMatchOver())
	{
		return;
	}
	// Defense: no score; a villain down is the director's (its killer's knockout, a die maybe), a friend just gets back up
	if (bDefense)
	{
		if (Victim->IsVillain())
		{
			if (ARRRandomDefenseDirector* Director = ARRRandomDefenseDirector::Get(this))
			{
				Director->OnVillainDown(Victim, Attacker);
			}
		}
		else
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s knocked out by %s; back up in a moment."), *Victim->GetName(), Attacker ? *Attacker->GetName() : TEXT("a fall"));
		}
		return;
	}
	// The battle royale: no score, nobody gets back up; a team with nobody left is out (placed), the last one wins
	if (bBattleRoyale)
	{
		if (Attacker && Attacker->GetTeam() != Victim->GetTeam())
		{
			Attacker->AddKill();
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) is out, knocked out by %s."), *Victim->GetName(), Victim->GetTeam(), Attacker ? *Attacker->GetName() : bFell ? TEXT("a fall") : TEXT("the ring"));
		UpdateBattleStanding(Victim);
		return;
	}
	if (bMeteorRush)
	{
		// No points for knockouts: the victim's ore flies out for anyone to grab (lost with it off the edge)
		if (Attacker && Attacker->GetTeam() != Victim->GetTeam())
		{
			Attacker->AddKill();
		}
		if (URRRandomOreCarryComponent* Carry = Victim->GetOreCarry())
		{
			Carry->DropAll(bFell);
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) knocked out by %s (r77 film remakes)."), *Victim->GetName(), Victim->GetTeam(), Attacker ? *Attacker->GetName() : TEXT("a fall"));
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
	// (Meteor Rush: the most ore picked up, then knockouts)
	ARRRandomCharacter* MVP = nullptr;
	auto Rank = [Winner, this](const ARRRandomCharacter* Brawler)
	{
		const int32 Ore = bMeteorRush && Brawler->GetOreCarry() ? Brawler->GetOreCarry()->GetCollected() : 0;
		return (Brawler->GetTeam() == Winner ? 100000 : 0) + Ore * 400 + Brawler->GetKills() * 4 + (Brawler->IsAlive() ? 1 : 0);
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
			It->ReviveNow();
			Dancers.Add(*It);
		}
	}

	// A row in front of the map's centerpiece, the MVP in the middle, all dashing there: the results camera looks at the
	// row from the open side, with the centerpiece behind it instead of in the way. The losers stay where they are and slump.
	TArray<ARRRandomCharacter*> Line = { MVP };
	Line.Append(Dancers);
	TArray<FVector> Spots;
	float LineYaw = 0.f;
	// (the battle royale ends anywhere on its big map: the winners dance round the MVP where it stands)
	if (!bBattleRoyale && FindDanceLine(Line, Winner, Spots, LineYaw))
	{
		float HalfWidth = 0.f;
		for (int32 Index = 0; Index < Line.Num(); ++Index)
		{
			Line[Index]->SetFormationSpot(Spots[Index]);
			HalfWidth = FMath::Max(HalfWidth, (float)FVector::Dist2D(Spots[Index], Spots[0]));
		}
		if (ARRRandomGameState* State = GetGameState<ARRRandomGameState>())
		{
			State->SetDanceLine(Spots[0], LineYaw, HalfWidth);
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %d winners line up to dance at %s, %.0f from the middle, facing %.0f."), Line.Num(), *Spots[0].ToString(),
			FVector::Dist2D(Spots[0], GetArenaCenter()), LineYaw);
		return;
	}

	// No room there: a V of the winning team behind the MVP where it stands, like backup dancers: the results camera looks
	// at the MVP's face, so each pair stands a step further back and further out to the sides, all in view
	constexpr float RowGap = 130.f;
	constexpr float SideGap = 150.f;
	MVP->SetFormationLeader();
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

bool ARRRandomGameMode::FindDanceLine(const TArray<ARRRandomCharacter*>& Line, int32 Winner, TArray<FVector>& OutSpots, float& OutYaw) const
{
	// Between neighbors (the dance sways the hips about 60 out of the capsule), how far out from the middle to look, and
	// how far in front of the line the camera needs open ground
	constexpr float Gap = 180.f;
	constexpr float MinOut = 0.f;
	constexpr float MaxOut = 3000.f;
	constexpr float OutStep = 50.f;
	constexpr float CameraRoom = 800.f;
	OutSpots.Reset();
	if (Line.Num() == 0)
	{
		return false;
	}
	// The ground the winners started on (not the centerpiece's tiers or steps), and the way out toward their base
	const FVector Center = GetArenaCenter();
	const float HalfHeight = Line[0]->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	FTransform Base;
	if (!GetSlotTransform(Winner == 1 ? 1 : 0, 0, HalfHeight, Base))
	{
		return false;
	}
	const float GroundZ = Base.GetLocation().Z - HalfHeight;
	FVector Toward = (Base.GetLocation() - Center).GetSafeNormal2D();
	if (Toward.IsNearlyZero())
	{
		Toward = FVector::ForwardVector;
	}
	for (const float Turn : { 0.f, 90.f, -90.f, 180.f })
	{
		const FVector Front = Toward.RotateAngleAxis(Turn, FVector::UpVector);
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Front);
		// From the middle outward (on a map with nothing there, right in the middle): the first place the whole line fits,
		// with open ground in front of it
		for (float Out = MinOut; Out <= MaxOut; Out += OutStep)
		{
			const FVector Middle = FVector(Center.X, Center.Y, GroundZ) + Front * Out;
			OutSpots.Reset();
			for (int32 Index = 0; Index < Line.Num(); ++Index)
			{
				const int32 Step = (Index + 1) / 2;
				const float Side = Index % 2 == 1 ? 1.f : -1.f;
				FVector Spot;
				if (!FindDanceSpot(Middle + Right * Side * Step * Gap, GroundZ, Front, Line[Index], Spot))
				{
					break;
				}
				OutSpots.Add(Spot);
			}
			if (OutSpots.Num() < Line.Num())
			{
				continue;
			}
			FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomDanceLine), false);
			if (GetWorld()->SweepTestByObjectType(OutSpots[0], OutSpots[0] + Front * CameraRoom, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(40.f), Params))
			{
				continue;
			}
			OutYaw = Front.Rotation().Yaw;
			return true;
		}
	}
	OutSpots.Reset();
	return false;
}

bool ARRRandomGameMode::FindDanceSpot(const FVector& Wanted, float GroundZ, const FVector& Front, const ARRRandomCharacter* Brawler, FVector& OutSpot) const
{
	// Room round the capsule to dance in, and open ground this far in front (nothing between it and the camera)
	constexpr float DanceRoom = 50.f;
	constexpr float OpenAhead = 500.f;
	const UCapsuleComponent* Capsule = Brawler->GetCapsuleComponent();
	const float Radius = Capsule->GetUnscaledCapsuleRadius();
	const float HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	FVector Location;
	if (!FindFloorSpot(Wanted, HalfHeight, Location) || FMath::Abs(Location.Z - HalfHeight - GroundZ) > 40.f)
	{
		return false;
	}
	// The room: the capsule widened, kept clear of the floor (a wide capsule is rounder, and would dip into the paving)
	const float RoomRadius = Radius + DanceRoom;
	const float RoomHalfHeight = FMath::Max(HalfHeight - 5.f, RoomRadius);
	const FVector RoomCenter(Location.X, Location.Y, Location.Z - HalfHeight + RoomHalfHeight + 10.f);
	FCollisionObjectQueryParams Solid;
	Solid.AddObjectTypesToQuery(ECC_WorldStatic);
	Solid.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomDanceLine), false, Brawler);
	if (GetWorld()->OverlapAnyTestByObjectType(RoomCenter, FQuat::Identity, Solid, FCollisionShape::MakeCapsule(RoomRadius, RoomHalfHeight), Params)
		|| GetWorld()->SweepTestByObjectType(Location, Location + Front * OpenAhead, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(Radius), Params))
	{
		return false;
	}
	OutSpot = Location;
	return true;
}

void ARRRandomGameMode::CheatWin(int32 Team)
{
	ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	// Defense: the friends' cheat clears the stage
	if (bDefense)
	{
		EndDefense(Team == 0);
		return;
	}
	// The battle royale: that team is the last standing
	if (bBattleRoyale)
	{
		if (State && !State->IsMatchOver() && Team >= 0 && Team < GetTeamCount())
		{
			State->SetTeamPlace(Team, 1);
			UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, team %d wins the battle royale."), Team);
			EndMatch(Team);
		}
		return;
	}
	if (State && !State->IsMatchOver() && (Team == 0 || Team == 1))
	{
		State->SetTeamScore(Team, FMath::Max(State->GetTeamScore(Team), ScoreToWin));
		UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, team %d wins."), Team);
		EndMatch(Team);
	}
}

void ARRRandomGameMode::EndDefense(bool bWon)
{
	ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	if (!State || State->IsMatchOver())
	{
		return;
	}
	// The MVP: the animal friend with the most villains down (won, it leads the dance; lost, nobody dances)
	ARRRandomCharacter* MVP = nullptr;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDecoy() && !It->IsVillain() && It->GetTeam() == 0 && (!MVP || It->GetKills() > MVP->GetKills()))
		{
			MVP = *It;
		}
	}
	if (bWon && MVP)
	{
		MVP->ReviveNow();
		ArrangeVictoryFormation(MVP, 0);
	}
	State->EndMatch(bWon ? 0 : 1, MVP);
	// Every player's own PC keeps its progress: told here, each counts it only if it had the stage open
	if (bWon)
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ARRRandomPlayerController* Player = Cast<ARRRandomPlayerController>(It->Get()))
			{
				Player->ClientDefenseCleared(DefenseStage);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: defense stage %d %s; MVP %s with %d villains down."), DefenseStage, bWon ? TEXT("cleared") : TEXT("lost (the tower fell)"),
		MVP ? *MVP->GetName() : TEXT("nobody"), MVP ? MVP->GetKills() : 0);
}

void ARRRandomGameMode::SetUpDefense()
{
	UWorld* World = GetWorld();
	TActorIterator<ARRRandomDiceTown> TownIt(World);
	ARRRandomDiceTown* Town = TownIt ? *TownIt : nullptr;
	if (!Town || !Town->IsBuilt())
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no defense stage built; no waves."));
		return;
	}
	// The tower on the floor in the middle, tougher on later stages
	const FVector Center = GetArenaCenter();
	const FVector Floor(Center.X, Center.Y, Town->GetFloorZ());
	const float Toughness = 1.f + 0.25f * (DefenseStage - 1);
	const FTransform TowerAt(Floor);
	ARRRandomDiceTower* Tower = World->SpawnActorDeferred<ARRRandomDiceTower>(ARRRandomDiceTower::StaticClass(), TowerAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Tower)
	{
		Tower->SetMaxHealth(3000.f * Toughness);
		Tower->FinishSpawning(TowerAt);
	}
	// The castle's two gate doors (the wall itself is the stage's, unbreakable): at the outer mouth of the east and west
	// gateways, the width of the road, facing out, so the villains biting them stand in the open under the battlements
	TArray<ARRRandomDefenseWall*> Walls;
	const float Out = (RRDefense::WallRingCells + RRDefense::WallThickCells) * 100.f - 70.f;
	for (const float Yaw : { 0.f, 180.f })
	{
		const FTransform DoorAt(FRotator(0.f, Yaw, 0.f), Floor + FRotator(0.f, Yaw, 0.f).Vector() * Out);
		if (ARRRandomDefenseWall* Door = World->SpawnActorDeferred<ARRRandomDefenseWall>(ARRRandomDefenseWall::StaticClass(), DoorAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Door->Setup(RRDefense::GateHalfCells * 2 * 100.f, 3000.f * Toughness, FColor(255, 196, 40));
			Door->FinishSpawning(DoorAt);
			Walls.Add(Door);
		}
	}
	SpawnDefenseRides(Town);
	if (ARRRandomDefenseDirector* Director = World->SpawnActor<ARRRandomDefenseDirector>())
	{
		Director->Begin(DefenseStage, Town, Tower, Walls);
	}
}

void ARRRandomGameMode::SpawnDefenseRides(const ARRRandomDiceTown* Town)
{
	// One more kind of ride each stage (round 90): stage 1 the kickboards and motorbikes, stage 2 the cars and human cannons
	// too, stage 3 the plane stations as well. The star robot only comes for the boss waves (ARRRandomDefenseDirector). All
	// just outside the wall ring, between the roads, so riders head straight out to meet the villains.
	UWorld* World = GetWorld();
	const FVector Center = GetArenaCenter();
	struct FParking
	{
		ERRVehicleKind Kind;
		float Angle;
		int32 FromStage;
	};
	const FParking Rides[] =
	{
		{ ERRVehicleKind::Kickboard, 22.5f, 1 },
		{ ERRVehicleKind::Motorbike, 112.5f, 1 },
		{ ERRVehicleKind::Kickboard, 202.5f, 1 },
		{ ERRVehicleKind::Motorbike, 292.5f, 1 },
		{ ERRVehicleKind::Car, 67.5f, 2 },
		{ ERRVehicleKind::Car, 247.5f, 2 },
	};
	// (on the ring road outside the castle wall)
	const float Out = (RRDefense::WallRingCells + RRDefense::WallThickCells + 2.5f) * 100.f;
	for (const FParking& Ride : Rides)
	{
		if (DefenseStage < Ride.FromStage)
		{
			continue;
		}
		const FVector Way = FRotator(0.f, Ride.Angle, 0.f).Vector();
		FVector Location;
		if (!FindFloorSpot(Center + Way * Out, 0.f, Location))
		{
			continue;
		}
		const FTransform SpawnTransform(FRotator(0.f, Ride.Angle, 0.f), Location);
		if (ARRRandomVehicle* Vehicle = World->SpawnActorDeferred<ARRRandomVehicle>(ARRRandomVehicle::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Vehicle->SetKind(Ride.Kind);
			Vehicle->FinishSpawning(SpawnTransform);
		}
	}
	// The jump ramps on the north side, each with a motorbike lined up behind it: outside leaping in over the wall onto the
	// plaza, inside leaping out onto the north road
	struct FRamp
	{
		float X;
		float FootY;
		float LipY;
	};
	const FRamp Ramps[] = { { RRDefense::OuterRampX, RRDefense::OuterRampFootY, RRDefense::OuterRampLipY }, { RRDefense::InnerRampX, RRDefense::InnerRampFootY, RRDefense::InnerRampLipY } };
	for (const FRamp& Ramp : Ramps)
	{
		const float Way = FMath::Sign(Ramp.LipY - Ramp.FootY);
		FVector Foot;
		if (!FindFloorSpot(Center + FVector(Ramp.X, Ramp.FootY, 0.f), 0.f, Foot))
		{
			continue;
		}
		const FTransform RampAt(FRotator(0.f, Way > 0.f ? 90.f : -90.f, 0.f), Foot);
		if (ARRRandomJumpRamp* JumpRamp = World->SpawnActorDeferred<ARRRandomJumpRamp>(ARRRandomJumpRamp::StaticClass(), RampAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			JumpRamp->Setup(FMath::Abs(Ramp.LipY - Ramp.FootY), RRDefense::RampLandDistance);
			JumpRamp->FinishSpawning(RampAt);
		}
		FVector Bike;
		if (FindFloorSpot(Center + FVector(Ramp.X, Ramp.FootY - Way * 280.f, 0.f), 0.f, Bike))
		{
			const FTransform BikeAt(RampAt.GetRotation(), Bike);
			if (ARRRandomVehicle* Vehicle = World->SpawnActorDeferred<ARRRandomVehicle>(ARRRandomVehicle::StaticClass(), BikeAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Vehicle->SetKind(ERRVehicleKind::Motorbike);
				Vehicle->FinishSpawning(BikeAt);
			}
		}
	}
	if (Town && DefenseStage >= 2)
	{
		ARRRandomCannon::SpawnDefenseSets(World, Town, Center);
	}
	if (Town && DefenseStage >= 3)
	{
		ARRRandomPlaneStation::SpawnDefenseStations(World, Town, Center);
	}
}

int32 ARRRandomGameMode::GetTeamSize(int32 InTeam) const
{
	if (bDefense)
	{
		return InTeam == 0 ? DefenseTeamSize : 0;
	}
	return TeamSize;
}

void ARRRandomGameMode::RestartMatch()
{
	// The same map from scratch (a listen server keeps listening and brings its clients along)
	UE_LOG(LogTemp, Log, TEXT("RRRandom: new match."));
	GetWorld()->ServerTravel(TEXT("?Restart"), false);
}

void ARRRandomGameMode::EnsureSlots()
{
	if (TeamSlots.Num() < GetTeamCount())
	{
		TeamSlots.SetNum(GetTeamCount());
	}
	for (int32 Team = 0; Team < GetTeamCount(); ++Team)
	{
		if (TeamSlots[Team].Num() < GetTeamSize(Team))
		{
			TeamSlots[Team].SetNum(GetTeamSize(Team));
		}
	}
}

int32 ARRRandomGameMode::ChooseTeamForPlayer() const
{
	// The battle royale: players make up squads in the order they come, three to a team
	if (bBattleRoyale)
	{
		for (int32 Team = 0; Team < GetTeamCount(); ++Team)
		{
			if (FindSlotForPlayer(Team) != INDEX_NONE)
			{
				return Team;
			}
		}
		return INDEX_NONE;
	}
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
	return TeamSlots.IsValidIndex(Team) ? TeamSlots[Team].IndexOfByPredicate([](const FTeamSlot& Slot) { return !Slot.Player.IsValid(); }) : INDEX_NONE;
}

bool ARRRandomGameMode::GetSlotTransform(int32 Team, int32 Slot, float HalfHeight, FTransform& OutTransform) const
{
	// The battle royale: round the team's start (the team close together, facing out from the island's middle); before
	// the map is built (the host's own player logs in first), by the player start for a moment (PlaceTeamsAtStarts)
	if (bBattleRoyale)
	{
		const FVector Start = TeamStarts.IsValidIndex(Team) ? TeamStarts[Team] : GetPlayerStartLocation() + FVector(0.f, Team * 400.f, 0.f);
		const FVector Out = (Start - GetArenaCenter()).GetSafeNormal2D();
		const FVector Across = FVector::CrossProduct(FVector::UpVector, Out.IsNearlyZero() ? FVector::ForwardVector : Out);
		const float Side = Slot % 2 == 1 ? 1.f : -1.f;
		const int32 Step = (Slot + 1) / 2;
		FVector Location;
		if (!FindFloorSpot(Start + Across * Side * Step * 180.f, HalfHeight, Location))
		{
			Location = Start + FVector(0.f, 0.f, HalfHeight + 2.f);
		}
		OutTransform = FTransform(FRotator(0.f, (Out.IsNearlyZero() ? FVector::ForwardVector : -Out).Rotation().Yaw, 0.f), Location);
		return true;
	}
	// Blue below the middle, red above it across from blue, facing down the screen
	const FVector Origin = GetArenaCenter() + FVector(TeamSpawnDistance * (Team == 1 ? 0.5f : -0.5f), 0.f, 0.f);
	// The middle spot first (the first player's), then alternating right and left of it
	const float Side = Slot % 2 == 1 ? 1.f : -1.f;
	const int32 Step = (Slot + 1) / 2;
	FVector Location;
	if (!FindFloorSpot(Origin + FVector(0.f, Side * Step * SpawnSpacing, 0.f), HalfHeight, Location))
	{
		return false;
	}
	// Level with the middle spot: a spot up on something (a wall or a roof by a base, where bots would be stuck) moves
	// further out to the side, half a gap at a time, until it finds the same ground
	FVector Middle;
	if (Step > 0 && FindFloorSpot(Origin, HalfHeight, Middle) && FMath::Abs(Location.Z - Middle.Z) > 60.f)
	{
		for (int32 Try = 1; Try <= 8; ++Try)
		{
			FVector Further;
			if (FindFloorSpot(Origin + FVector(0.f, Side * (Step + Try * 0.5f) * SpawnSpacing, 0.f), HalfHeight, Further) && FMath::Abs(Further.Z - Middle.Z) <= 60.f)
			{
				Location = Further;
				break;
			}
		}
	}
	// (defense: the friends start by the tower facing out, the way the villains come)
	OutTransform = FTransform(FRotator(0.f, Team == 1 || bDefense ? 180.f : 0.f, 0.f), Location);
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
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s is a %s on team %d (at %s)."), *Brawler->GetName(), RRSpecies::GetAssetName(Species), Team, *SpawnTransform.GetLocation().ToCompactString());
	TeamSlots[Team][Slot].Brawler = Brawler;
	return Brawler;
}

APawn* ARRRandomGameMode::SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot)
{
	EnsureSlots();

	// The battle royale under way: no spot to start from; a standing bot's place, or watching
	const ARRRandomGameState* Match = GetGameState<ARRRandomGameState>();
	if (bBattleRoyale && Match && Match->HasPlayStarted() && TeamStarts.Num() > 0)
	{
		const URRRandomSessionSubsystem* JoinSessions = GetGameInstance() ? GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>() : nullptr;
		const ARRRandomPlayerController* Joiner = Cast<ARRRandomPlayerController>(NewPlayer);
		return TakeOverBot(NewPlayer, Joiner && Joiner->GetChosenSpecies() != INDEX_NONE ? Joiner->GetChosenSpecies()
			: NewPlayer && NewPlayer->IsLocalController() && JoinSessions ? JoinSessions->GetSpecies() : RRSpecies::Fox);
	}

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

void ARRRandomGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The next-tick bot fill and standings recount (Logout) would otherwise run on the next map, with no world here
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
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
				// (the battle royale under way: no bot comes; the spot is out)
				if (bBattleRoyale)
				{
					// (bound to this game mode: a player leaving because the game quits mustn't recount after the map is gone)
					GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { UpdateBattleStanding(nullptr); }));
				}
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
	// The battle royale: bots only fill the spots at the start (a player leaving later leaves the spot empty), and only once
	// the map is up and the teams' starts are known
	const ARRRandomGameState* Match = GetGameState<ARRRandomGameState>();
	if (bBattleRoyale && (TeamStarts.Num() == 0 || (Match && Match->HasPlayStarted())))
	{
		return;
	}
	for (int32 Team = 0; Team < GetTeamCount(); ++Team)
	{
		for (int32 Slot = 0; Slot < TeamSlots[Team].Num(); ++Slot)
		{
			if (TeamSlots[Team][Slot].Brawler.IsValid() || TeamSlots[Team][Slot].Player.IsValid())
			{
				continue;
			}
			// Defense: bots make the team up to 3 (solo, you and two bots); players beyond that bring their own spots
			if (bDefense && Slot >= DefenseBotFill)
			{
				continue;
			}
			// Bots are any of the brawlers (-RRBotSpecies=Fox makes them all one, for tests)
			FString BotSpeciesName;
			const int32 BotSpecies = FParse::Value(FCommandLine::Get(), TEXT("RRBotSpecies="), BotSpeciesName) ? RRSpecies::FromName(BotSpeciesName) : INDEX_NONE;
			ARRRandomCharacter* Bot = SpawnBrawler(BotClass, Team, Slot, BotSpecies != INDEX_NONE ? BotSpecies : FMath::RandRange(0, RRSpecies::Count - 1));
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
		/** A star car: whose team's. */
		int32 Team = 0;
	};
	// From the arena's center (X up the screen, Y right): each team's kickboard and motorbike just ahead of its village
	// road, on opposite sides, facing the field; a car on each side lane of the ring road, halfway, pointing opposite ways
	const float Near = TeamSpawnDistance * 0.5f - 230.f;
	// Star cars parked in a row this far apart (well within ARRRandomStarRobot::GatherRadius of their middle)
	constexpr float StarCarGap = 500.f;
	const FParking Spots[] =
	{
		{ ERRVehicleKind::Kickboard, FVector2D(-Near, -900.f), 0.f },
		{ ERRVehicleKind::Motorbike, FVector2D(-Near, 900.f), 0.f },
		{ ERRVehicleKind::Kickboard, FVector2D(Near, 900.f), 180.f },
		{ ERRVehicleKind::Motorbike, FVector2D(Near, -900.f), 180.f },
		{ ERRVehicleKind::Car, FVector2D(90.f, -1890.f), 0.f },
		{ ERRVehicleKind::Car, FVector2D(-90.f, 1710.f), 180.f },
		// Each team's three star cars (ARRRandomStarRobot), parked nose to tail on the road out of its own side, already
		// gathered: a teammate pressing F by them combines the team's robot (the other team's mirror the first round the middle)
		{ ERRVehicleKind::StarChest, FVector2D(-Near * 0.6f - StarCarGap, 1250.f), 0.f, 0 },
		{ ERRVehicleKind::StarArms, FVector2D(-Near * 0.6f + StarCarGap, 1250.f), 0.f, 0 },
		{ ERRVehicleKind::StarLegs, FVector2D(-Near * 0.6f, 1250.f), 0.f, 0 },
		{ ERRVehicleKind::StarChest, FVector2D(Near * 0.6f + StarCarGap, -1250.f), 180.f, 1 },
		{ ERRVehicleKind::StarArms, FVector2D(Near * 0.6f - StarCarGap, -1250.f), 180.f, 1 },
		{ ERRVehicleKind::StarLegs, FVector2D(Near * 0.6f, -1250.f), 180.f, 1 },
	};
	const FVector Origin = GetArenaCenter();
	// Floor more than this above the arena's isn't the ground
	constexpr float StarCarGroundStep = 40.f;
	for (const FParking& Spot : Spots)
	{
		// The studio keeps its floor bare: only the star team's cars, for RRStarCars to bring
		if (bStudio && Spot.Kind < ERRVehicleKind::StarChest)
		{
			continue;
		}
		// Its pivot is the middle of its bottom, so it goes straight onto the floor; no floor, no vehicle
		FVector Location;
		if (!FindFloorSpot(Origin + FVector(Spot.Offset, 0.f), 0.f, Location))
		{
			continue;
		}
		// The star team's big cars on the ground, not up on a hedge or a crate: the nearest spot that is, along the road
		if (Spot.Kind >= ERRVehicleKind::StarChest)
		{
			for (const float Shift : { 200.f, -200.f, 400.f, -400.f, 600.f, -600.f })
			{
				FVector Shifted;
				if (Location.Z - Origin.Z <= StarCarGroundStep || !FindFloorSpot(Origin + FVector(Spot.Offset + FVector2D(Shift, 0.f), 0.f), 0.f, Shifted))
				{
					continue;
				}
				Location = Shifted.Z - Origin.Z <= StarCarGroundStep ? Shifted : Location;
			}
		}
		const FTransform SpawnTransform(FRotator(0.f, Spot.Yaw, 0.f), Location);
		if (ARRRandomVehicle* Vehicle = World->SpawnActorDeferred<ARRRandomVehicle>(ARRRandomVehicle::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Vehicle->SetKind(Spot.Kind);
			Vehicle->SetStarTeam(Spot.Team);
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
	// Around the arena's center, halfway between the teams; each team starts at its base on the main road. A defense stage
	// is laid out round the tower in its middle instead (its stage replicates, so every machine builds the same)
	const FTransform Where(GetArenaCenter());
	if (ARRRandomDiceTown* Town = World->SpawnActorDeferred<ARRRandomDiceTown>(ARRRandomDiceTown::StaticClass(), Where))
	{
		Town->SetDefenseStage(bDefense ? DefenseStage : 0);
		Town->FinishSpawning(Where);
	}
}

void ARRRandomGameMode::SpawnStudioIfNoneExists()
{
	UWorld* World = GetWorld();
	if (TActorIterator<ARRRandomStudio>(World))
	{
		return;
	}
	World->SpawnActor<ARRRandomStudio>(ARRRandomStudio::StaticClass(), FTransform(GetArenaCenter()));
}

// ---------------------------------------------------------------------------------------------------------------------
// The battle royale (round 92)

void ARRRandomGameMode::SetUpBattleRoyale()
{
	UWorld* World = GetWorld();
	ARRRandomBattleMap* Battle = ARRRandomBattleMap::Get(this);
	if (!Battle || !Battle->IsBuilt())
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no battle map built; the battle royale has no ground."));
		return;
	}
	// Round 97: the get-ready time first (everyone on the plaza, no attacks, thirty weapon dice each, each team picking where
	// to start), then the team roll, then play from the spots; without it (-RRBRPrep=0) the teams' starts round the island
	ARRRandomGameState* PrepState = GetGameState<ARRRandomGameState>();
	float PrepTime = DefaultPrepTime;
	FParse::Value(FCommandLine::Get(), TEXT("RRBRPrep="), PrepTime);
	TArray<FVector> Spots;
	if (PrepTime > 0.f && PrepState)
	{
		Battle->GetStartSpots(Spots);
	}
	PrepStream.GenerateNewSeed();
	if (PrepTime > 0.f && PrepState && Spots.Num() >= GetTeamCount())
	{
		const float Now = PrepState->GetServerWorldTimeSeconds();
		PrepState->SetPlayStartTime(Now + PrepTime + ARRRandomGameState::TeamRollTime);
		PrepState->SetPrep(Now + PrepTime, Spots, GetTeamCount());
		Battle->GetPlazaStarts(GetTeamCount(), TeamStarts);
		PlazaCenter = Battle->GetActorLocation();
		// Every team wants somewhere to begin with (spread out); a player's team picks again on the map
		TArray<int32> Free;
		for (int32 Spot = 0; Spot < Spots.Num(); ++Spot)
		{
			Free.Add(Spot);
		}
		for (int32 Team = 0; Team < GetTeamCount() && Free.Num() > 0; ++Team)
		{
			PrepState->SetTeamPick(Team, Free[PrepStream.RandRange(0, Free.Num() - 1)]);
			Free.Remove(PrepState->GetTeamPick(Team));
		}
		GetWorldTimerManager().SetTimer(PrepTimer, this, &ARRRandomGameMode::KeepOnPlaza, 0.5f, true);
		GetWorldTimerManager().SetTimer(RollTimer, this, &ARRRandomGameMode::RollTeamStarts, PrepTime, false);
		GetWorldTimerManager().SetTimer(DeployTimer, this, &ARRRandomGameMode::DeployTeams, PrepTime + ARRRandomGameState::TeamRollTime, false);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: battle royale get-ready time, %.0f s on the plaza, %d start spots."), PrepTime, Spots.Num());
	}
	else
	{
		// The teams' starts round the island
		Battle->GetTeamStarts(GetTeamCount(), TeamStarts);
	}
	// Round 97: the ultimate dice fall from the sky one at a time once play starts
	NextUltimateDrop = 0.f;
	GetWorldTimerManager().SetTimer(SkyTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		ARRRandomUltimateMeteor::UpdateSky(GetWorld(), NextUltimateDrop);
	}), 1.f, true);
	// The rides spread over the island, the ring round all of it from play start
	Battle->SpawnRides();
	if (ARRRandomBattleZone* Zone = World->SpawnActor<ARRRandomBattleZone>())
	{
		const ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
		const float PlayStart = State ? State->GetServerWorldTimeSeconds() + State->GetCountdownRemaining() : World->GetTimeSeconds();
		const FVector Center = Battle->GetActorLocation();
		Zone->Begin(FVector2D(Center.X, Center.Y), Battle->GetIslandRadius() * 1.05f, PlayStart, Battle->GetFloorZ(), bBattleRoyaleTest);
	}
	PlaceTeamsAtStarts();
	if (ARRRandomGameState* State = GetGameState<ARRRandomGameState>())
	{
		State->SetAlive(0, GetTeamCount());
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: battle royale, %d teams of %d%s."), GetTeamCount(), TeamSize, bBattleRoyaleTest ? TEXT(" (test map)") : TEXT(""));
}

void ARRRandomGameMode::KeepOnPlaza()
{
	const ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	if (!State || !State->IsPrep())
	{
		GetWorldTimerManager().ClearTimer(PrepTimer);
		return;
	}
	// Wandered off the plaza (or flung by a cannon): back to the team's place
	for (int32 Team = 0; Team < TeamSlots.Num(); ++Team)
	{
		for (int32 Slot = 0; Slot < TeamSlots[Team].Num(); ++Slot)
		{
			ARRRandomCharacter* Brawler = TeamSlots[Team][Slot].Brawler.Get();
			FTransform Where;
			if (Brawler && Brawler->IsAlive() && FVector::Dist2D(Brawler->GetActorLocation(), PlazaCenter) > PlazaReach
				&& !Brawler->GetVehicle() && GetSlotTransform(Team, Slot, Brawler->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Where))
			{
				Brawler->TeleportTo(Where.GetLocation(), Where.Rotator(), false, true);
				Brawler->GetCharacterMovement()->StopMovementImmediately();
			}
		}
	}
}

void ARRRandomGameMode::PickTeamStart(AController* Who, int32 Spot)
{
	ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	const ARRRandomCharacter* Brawler = Who ? Cast<ARRRandomCharacter>(Who->GetPawn()) : nullptr;
	if (!State || !State->IsPrep() || !Brawler || !State->GetStartSpots().IsValidIndex(Spot))
	{
		return;
	}
	State->SetTeamPick(Brawler->GetTeam(), Spot);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s picks start spot %d for team %d."), *GetNameSafe(Who), Spot, Brawler->GetTeam());
}

void ARRRandomGameMode::RollTeamStarts()
{
	ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	if (!State)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(PrepTimer);
	const TArray<FVector_NetQuantize>& Spots = State->GetStartSpots();
	const int32 Teams = GetTeamCount();
	// Three dice a team; the highest sum first (a tie by a coin)
	TArray<uint8> Dice;
	TArray<float> Sums;
	for (int32 Team = 0; Team < Teams; ++Team)
	{
		int32 Sum = 0;
		for (int32 Die = 0; Die < 3; ++Die)
		{
			const uint8 Face = static_cast<uint8>(PrepStream.RandRange(1, 6));
			Dice.Add(Face);
			Sum += Face;
		}
		Sums.Add(Sum + PrepStream.FRand() * 0.5f);
	}
	TArray<int32> ByRoll;
	for (int32 Team = 0; Team < Teams; ++Team)
	{
		ByRoll.Add(Team);
	}
	ByRoll.Sort([&Sums](int32 A, int32 B) { return Sums[A] > Sums[B]; });
	// In that order each team gets its pick, or the nearest spot to it still free and clear of the teams placed before
	TArray<int32> Order;
	TArray<int32> SpotOf;
	Order.Init(0, Teams);
	SpotOf.Init(INDEX_NONE, Teams);
	auto IsClear = [&](int32 Spot, float Gap)
	{
		for (const int32 Taken : SpotOf)
		{
			if (Taken == Spot || (Taken != INDEX_NONE && FVector::Dist2D(Spots[Taken], Spots[Spot]) < Gap))
			{
				return false;
			}
		}
		return true;
	};
	for (int32 Rank = 0; Rank < ByRoll.Num(); ++Rank)
	{
		const int32 Team = ByRoll[Rank];
		Order[Team] = Rank + 1;
		const int32 Want = State->GetTeamPick(Team) != INDEX_NONE ? State->GetTeamPick(Team) : PrepStream.RandRange(0, Spots.Num() - 1);
		for (const float Gap : { StartSpotGap, StartSpotGap * 0.5f, 1.f })
		{
			int32 Best = INDEX_NONE;
			for (int32 Spot = 0; Spot < Spots.Num(); ++Spot)
			{
				if (IsClear(Spot, Gap) && (Best == INDEX_NONE || FVector::Dist2D(Spots[Spot], Spots[Want]) < FVector::Dist2D(Spots[Best], Spots[Want])))
				{
					Best = Spot;
				}
			}
			if (Best != INDEX_NONE)
			{
				SpotOf[Team] = Best;
				break;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: team %d rolls %d (%d, %d, %d): #%d, wanted spot %d, gets %d."), Team, FMath::FloorToInt(Sums[Team]),
			Dice[Team * 3], Dice[Team * 3 + 1], Dice[Team * 3 + 2], Rank + 1, Want, SpotOf[Team]);
	}
	State->SetTeamRoll(Dice, Order, SpotOf);
	// The bots put their bags in order for the fight
	for (const TArray<FTeamSlot>& Slots : TeamSlots)
	{
		for (const FTeamSlot& Slot : Slots)
		{
			ARRRandomCharacter* Brawler = Slot.Brawler.Get();
			if (Brawler && !Slot.Player.IsValid() && Brawler->GetLoadout())
			{
				Brawler->GetLoadout()->AutoManage();
			}
		}
	}
}

void ARRRandomGameMode::DeployTeams()
{
	const ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	if (!State)
	{
		return;
	}
	for (int32 Team = 0; Team < TeamStarts.Num(); ++Team)
	{
		const int32 Spot = State->GetTeamSpot(Team);
		if (State->GetStartSpots().IsValidIndex(Spot))
		{
			TeamStarts[Team] = State->GetStartSpots()[Spot];
		}
	}
	PlaceTeamsAtStarts();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the teams are at their spots; play starts."));
}

void ARRRandomGameMode::PlaceTeamsAtStarts()
{
	for (int32 Team = 0; Team < TeamSlots.Num(); ++Team)
	{
		for (int32 Slot = 0; Slot < TeamSlots[Team].Num(); ++Slot)
		{
			ARRRandomCharacter* Brawler = TeamSlots[Team][Slot].Brawler.Get();
			FTransform Where;
			if (!Brawler || !GetSlotTransform(Team, Slot, Brawler->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), Where))
			{
				continue;
			}
			Brawler->TeleportTo(Where.GetLocation(), Where.Rotator(), false, true);
			Brawler->SetHome(Where.GetLocation(), Where.Rotator());
			if (AController* Controller = Brawler->GetController())
			{
				Controller->SetControlRotation(Where.Rotator());
			}
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s starts with team %d at %s."), *Brawler->GetName(), Team, *Where.GetLocation().ToString());
		}
	}
}

void ARRRandomGameMode::UpdateBattleStanding(ARRRandomCharacter* Victim)
{
	ARRRandomGameState* State = GetGameState<ARRRandomGameState>();
	// (nothing to count once the map is going away)
	if (!State || State->IsMatchOver() || !GetWorld() || IsActorBeingDestroyed())
	{
		return;
	}
	// Who is still standing, team by team (a spot nobody holds any more counts as out)
	TArray<int32> Standing;
	Standing.Init(0, GetTeamCount());
	int32 Friends = 0;
	for (int32 Team = 0; Team < TeamSlots.Num(); ++Team)
	{
		for (const FTeamSlot& Slot : TeamSlots[Team])
		{
			const ARRRandomCharacter* Brawler = Slot.Brawler.Get();
			if (Brawler && Brawler->IsAlive() && Brawler != Victim)
			{
				++Standing[Team];
				++Friends;
			}
		}
	}
	int32 Teams = 0;
	int32 LastTeam = INDEX_NONE;
	for (int32 Team = 0; Team < Standing.Num(); ++Team)
	{
		if (Standing[Team] > 0)
		{
			++Teams;
			LastTeam = Team;
		}
	}
	// A team just gone: placed after every team still standing
	for (int32 Team = 0; Team < Standing.Num(); ++Team)
	{
		if (Standing[Team] == 0 && State->GetTeamPlace(Team) == 0)
		{
			State->SetTeamPlace(Team, Teams + 1);
			UE_LOG(LogTemp, Log, TEXT("RRRandom: team %d is out, #%d of %d."), Team, Teams + 1, GetTeamCount());
			for (const FTeamSlot& Slot : TeamSlots[Team])
			{
				if (ARRRandomPlayerController* Player = Cast<ARRRandomPlayerController>(Slot.Player.Get()))
				{
					Player->ClientTeamOut(Teams + 1, GetTeamCount());
				}
			}
		}
	}
	State->SetAlive(Friends, Teams);
	if (Teams <= 1)
	{
		const int32 Winner = LastTeam != INDEX_NONE ? LastTeam : Victim ? Victim->GetTeam() : 0;
		State->SetTeamPlace(Winner, 1);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: team %d is the last standing and wins the battle royale."), Winner);
		EndMatch(Winner);
	}
}

APawn* ARRRandomGameMode::TakeOverBot(AController* NewPlayer, int32 Species)
{
	// A standing bot on the team with the fewest players: the player takes its body over where it stands (its species,
	// health and gun as they are)
	int32 BestTeam = INDEX_NONE;
	int32 BestSlot = INDEX_NONE;
	int32 FewestPlayers = MAX_int32;
	for (int32 Team = 0; Team < TeamSlots.Num(); ++Team)
	{
		int32 Players = 0;
		int32 BotSlot = INDEX_NONE;
		for (int32 Slot = 0; Slot < TeamSlots[Team].Num(); ++Slot)
		{
			const FTeamSlot& Entry = TeamSlots[Team][Slot];
			Players += Entry.Player.IsValid() ? 1 : 0;
			if (!Entry.Player.IsValid() && Entry.Brawler.IsValid() && Entry.Brawler->IsAlive() && BotSlot == INDEX_NONE)
			{
				BotSlot = Slot;
			}
		}
		if (BotSlot != INDEX_NONE && Players < FewestPlayers)
		{
			FewestPlayers = Players;
			BestTeam = Team;
			BestSlot = BotSlot;
		}
	}
	if (BestTeam == INDEX_NONE)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s joins the battle royale under way with no bot left to take over; watching."), *GetNameSafe(NewPlayer));
		return nullptr;
	}
	FTeamSlot& Entry = TeamSlots[BestTeam][BestSlot];
	ARRRandomCharacter* Bot = Entry.Brawler.Get();
	if (AController* BotController = Bot->GetController())
	{
		BotController->UnPossess();
		BotController->Destroy();
	}
	Entry.Player = NewPlayer;
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s takes over %s on team %d (a %s, as it was)."), *GetNameSafe(NewPlayer), *Bot->GetName(), BestTeam, RRSpecies::GetAssetName(Bot->GetSpecies()));
	return Bot;
}

void ARRRandomGameMode::CheatZone()
{
	if (ARRRandomBattleZone* Zone = ARRRandomBattleZone::Get(this))
	{
		Zone->SkipAhead();
	}
}
