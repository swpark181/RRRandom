#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RRRandomCover.h"
#include "RRRandomGameMode.generated.h"

class ARRRandomCharacter;
class ARRRandomDummy;

/**
 * Team brawl: blue (team 0) starts at the player start, red (team 1) further up the screen.
 * Each team has TeamSize spots. Players take spots first (alternating teams, or all on blue), bots fill the rest,
 * a player joining mid-game takes a bot's spot and a bot takes over when a player leaves.
 * Every knockout scores a point for the other team; the first team to ScoreToWin ends the match (the results screen:
 * winner, MVP, a new game or back to the title).
 * Exists only on the server (the host); clients see the result through replication.
 */
UCLASS()
class RRRANDOM_API ARRRandomGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	/**
	 * The middle of the arena: the middle of the level's floor under the player start (the island is built around it
	 * and both teams start either side of it), at the player start's height. Worked out once, before anything is spawned.
	 */
	FVector GetArenaCenter() const;

	/** This game is on the studio, the bare test floor (-RRMap=Studio): the other team's bots stand still there. */
	bool IsStudio() const { return bStudio; }

	ARRRandomGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual void Logout(AController* Exiting) override;
	/** Drops its timers: the timer manager is the game instance's and outlives this map (a quit or a map change). */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Called by a brawler when it is knocked out. Attacker may be null; bFell when it fell off the arena. */
	void OnBrawlerKnockedOut(ARRRandomCharacter* Victim, ARRRandomCharacter* Attacker, bool bFell = false);

	/** Ends the match with Winner's win (Meteor Rush's director decides when). */
	void EndMatchFor(int32 Winner) { EndMatch(Winner); }

	/** How far apart the two teams start, along X either side of the arena's center. */
	float GetTeamSpawnDistance() const { return TeamSpawnDistance; }

	/** Starts a fresh match on the same map, taking everyone connected along. */
	void RestartMatch();

	/** Cheat: the team wins now with a full score (straight to the results screen). */
	void CheatWin(int32 Team);

	/**
	 * Defense (round 90, ERRMatchRule::Defense): this game is played by the defense rule on a defense stage. EndDefense ends it:
	 * won (the seventh wave beaten: the winners dance, every player's PC opens the next stage) or lost (the tower fell).
	 */
	bool IsDefense() const { return bDefense; }
	void EndDefense(bool bWon);

	/**
	 * The battle royale (round 92, ERRMatchRule::BattleRoyale): up to ten teams of three on the battle map, bots in every free
	 * spot, nobody getting back up, the ring closing; the last team standing wins.
	 */
	bool IsBattleRoyale() const { return bBattleRoyale; }
	/** Teams in this game (two, or the battle royale's). */
	int32 GetTeamCount() const { return bBattleRoyale ? BattleTeamCount : 2; }
	/** Cheat (RRZone): the ring skips ahead. */
	void CheatZone();
	/** The battle royale's get-ready time (round 97): a player's team wants to start at this spot (ARRRandomGameState::GetStartSpots). */
	void PickTeamStart(AController* Who, int32 Spot);
	/** The get-ready time, in seconds (-RRBRPrep=N; 0 skips it and starts as before round 97). */
	static constexpr float DefaultPrepTime = 180.f;

protected:
	/** Seconds of countdown once the arena has loaded before anybody can move or shoot. */
	UPROPERTY(EditAnywhere, Category = "Match", meta = (ClampMin = "0"))
	float StartCountdown = 5.f;

	/** Knockouts a team needs to win the match (-RRScoreToWin=N on the command line overrides it). */
	UPROPERTY(EditAnywhere, Category = "Match", meta = (ClampMin = "1"))
	int32 ScoreToWin = 10;

	UPROPERTY(EditAnywhere, Category = "Bots")
	TSubclassOf<ARRRandomCharacter> BotClass;

	/** Brawlers per team, players and bots together. */
	UPROPERTY(EditAnywhere, Category = "Teams", meta = (ClampMin = "1"))
	int32 TeamSize = 3;

	/** Players alternate between blue and red. Off puts every player on blue against the bots (until blue is full). */
	UPROPERTY(EditAnywhere, Category = "Teams")
	bool bSplitPlayersAcrossTeams = true;

	/** How far apart (up the screen, +X) the two teams begin, either side of the arena's center (GetArenaCenter): on the island's village roads. */
	UPROPERTY(EditAnywhere, Category = "Teams")
	float TeamSpawnDistance = 3060.f;

	/** Sideways gap between teammates at the start. */
	UPROPERTY(EditAnywhere, Category = "Teams")
	float SpawnSpacing = 300.f;

	/** Lays out CoverLayout when the level has no cover of its own. */
	UPROPERTY(EditAnywhere, Category = "Cover")
	bool bSpawnCover = true;

	/** Parks vehicles to ride (ARRRandomVehicle) at the start: on each team's side a kickboard and a motorbike, and a car on each flank. */
	UPROPERTY(EditAnywhere, Category = "Vehicles")
	bool bSpawnVehicles = true;

	UPROPERTY(EditAnywhere, Category = "Cover")
	TSubclassOf<ARRRandomCover> CoverClass;

	/** Builds the voxel island (cliffs, waterfall, river, bay, hills, paths) around the arena when the level has none (ARRRandomIsland). */
	UPROPERTY(EditAnywhere, Category = "Island")
	bool bSpawnIsland = true;

	/**
	 * In the third person mode, builds the map picked on the title (Dice Town, ARRRandomDiceTown, or Dice Land,
	 * ARRRandomDiceLand) instead of the island, its grass and the cover blocks.
	 */
	UPROPERTY(EditAnywhere, Category = "Island")
	bool bSpawnDiceLand = true;

	/** Covers the floor with grass tiles when the level has none of its own (ARRRandomGrassFloor). */
	UPROPERTY(EditAnywhere, Category = "Grass")
	bool bSpawnGrassFloor = true;

	/** Which random tiles the grass gets; every machine lays the same ones from it. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	int32 GrassSeed = 7;

	/** Blocks of cover relative to the arena's center. The default is point-symmetric so both teams get the same. */
	UPROPERTY(EditAnywhere, Category = "Cover")
	TArray<FRRCoverPlacement> CoverLayout;

	/** Training dummies stand in the middle of the fight, so they're off unless asked for. */
	UPROPERTY(EditAnywhere, Category = "Dummies")
	bool bSpawnDummies = false;

	UPROPERTY(EditAnywhere, Category = "Dummies")
	TSubclassOf<ARRRandomDummy> DummyClass;

	/** Where dummies stand relative to the player start, used only when the level has no dummies of its own. */
	UPROPERTY(EditAnywhere, Category = "Dummies")
	TArray<FVector> DummyOffsets;

private:
	/** One starting spot on a team and who holds it. */
	struct FTeamSlot
	{
		TWeakObjectPtr<ARRRandomCharacter> Brawler;
		/** Set while a player holds the spot; bots leave it empty. */
		TWeakObjectPtr<AController> Player;
	};

	void EndMatch(int32 Winner);
	/** Lines the winners up to dance: in a row in front of the map's centerpiece (FindDanceLine), or a V behind the MVP. */
	void ArrangeVictoryFormation(ARRRandomCharacter* MVP, int32 Winner);
	/** A clear spot for a brawler near Wanted (on the floor, not in cover, apart from Taken), or false if none nearby. */
	bool FindFormationSpot(const FVector& Wanted, float FloorZ, const ARRRandomCharacter* Brawler, const TArray<FVector>& Taken, FVector& OutSpot) const;
	/**
	 * A straight line of spots for these brawlers (the first in the middle, the rest alternating right and left of it) in
	 * front of the map's centerpiece, facing out from it: on the ground the winners start on, as close to the middle as
	 * there's room, open in front for the results camera. Tries the winners' side of the middle first, then the sides,
	 * then the far side. False if there's no room anywhere.
	 */
	bool FindDanceLine(const TArray<ARRRandomCharacter*>& Line, int32 Winner, TArray<FVector>& OutSpots, float& OutYaw) const;
	/** One spot in the dance line: on the ground at GroundZ, with room to dance and nothing solid just in front. */
	bool FindDanceSpot(const FVector& Wanted, float GroundZ, const FVector& Front, const ARRRandomCharacter* Brawler, FVector& OutSpot) const;
	void EnsureSlots();
	/** The team a new player joins, or INDEX_NONE when both are full of players. */
	int32 ChooseTeamForPlayer() const;
	/** The first spot on the team not held by a player, or INDEX_NONE. */
	int32 FindSlotForPlayer(int32 Team) const;
	/** Where a spot is and which way it faces; false if there is no floor there. */
	bool GetSlotTransform(int32 Team, int32 Slot, float HalfHeight, FTransform& OutTransform) const;
	/** Spawns a brawler of a species (RRSpecies) on a team, standing on one of its spots. */
	ARRRandomCharacter* SpawnBrawler(TSubclassOf<ARRRandomCharacter> BrawlerClass, int32 Team, int32 Slot, int32 Species);
	void RemoveBot(int32 Team, int32 Slot);
	/** Puts a bot on every spot that has nobody. */
	void FillEmptySlotsWithBots();
	void SpawnDummiesIfNoneExist();
	void SpawnCoverIfNoneExists();
	void SpawnVehiclesIfNoneExist();
	void SpawnGrassFloorIfNoneExists();
	void SpawnIslandIfNoneExists();
	void SpawnDiceLandIfNoneExists();
	void SpawnDiceTownIfNoneExists();
	void SpawnStudioIfNoneExists();
	/** This game is on Dice Town (decided in InitGame: its level, or this machine's title picks). */
	bool bDiceTown = false;
	/** The level has a third person map placed in it (Dice Town, Dice Land or the studio): one of the /Game/Maps levels. */
	bool bPlacedThirdPersonMap = false;
	/** This game is on the studio (-RRMap=Studio). */
	bool bStudio = false;
	/** This game is played by Meteor Rush's rule (ERRMatchRule, picked on this machine's title). */
	bool bMeteorRush = false;
	/** Defense (decided in InitGame): its stage, and the spots on the animal friends' team (players up to 5, bots fill up to 3). */
	bool bDefense = false;
	int32 DefenseStage = 1;
	static constexpr int32 DefenseTeamSize = 5;
	static constexpr int32 DefenseBotFill = 3;
	/** The spots a team has: TeamSize each, or in defense the friends' DefenseTeamSize and none for the villains (the director's). */
	int32 GetTeamSize(int32 InTeam) const;
	/** Defense: the tower, the wall ring and the director round the stage's middle, and the stage's rides. */
	void SetUpDefense();
	void SpawnDefenseRides(const class ARRRandomDiceTown* Town);
	FVector GetPlayerStartLocation() const;

	/** Places a point on the floor beneath it, or returns false if there is no floor there. */
	bool FindFloorSpot(const FVector& Spot, float HalfHeight, FVector& OutLocation) const;

	/** Each team's spots (GetTeamCount teams). */
	TArray<TArray<FTeamSlot>> TeamSlots;

	// Round 92: the battle royale
	bool bBattleRoyale = false;
	/** Played on the test map (fewer teams, 3 x 3 tiles, a quicker ring). */
	bool bBattleRoyaleTest = false;
	int32 BattleTeamCount = 0;
	/** Where each team starts (feet), once the battle map is built; empty before. */
	TArray<FVector> TeamStarts;
	/** The battle map (placed in the level, or built on the arena), its rides and the ring, then everyone to their team's start. */
	void SetUpBattleRoyale();
	/** Moves every brawler already in (the host's own, placed as it logged in) to its team's start. */
	void PlaceTeamsAtStarts();
	/**
	 * Round 97, the get-ready time: everyone starts on the middle plaza (TeamStarts round it), kept there (KeepOnPlaza); when it
	 * ends the teams roll (RollTeamStarts: three dice each, the highest sum gets its pick first, the rest the nearest free spot
	 * to theirs) and at play start every team appears at its spot (DeployTeams).
	 */
	void KeepOnPlaza();
	void RollTeamStarts();
	void DeployTeams();
	/** Where the plaza is (the map's middle), how far from it the get-ready time lets anyone go, and how far apart two teams start. */
	FVector PlazaCenter = FVector::ZeroVector;
	static constexpr float PlazaReach = 2300.f;
	static constexpr float StartSpotGap = 3000.f;
	FTimerHandle PrepTimer;
	FTimerHandle RollTimer;
	FTimerHandle DeployTimer;
	FRandomStream PrepStream;
	/** Round 97: the ultimate dice falling one at a time (ARRRandomUltimateMeteor::UpdateSky), and when the next is due. */
	FTimerHandle SkyTimer;
	float NextUltimateDrop = 0.f;
	/** Counts who is still standing; a team with nobody left is placed; one team left wins. */
	void UpdateBattleStanding(ARRRandomCharacter* Victim);
	/** A player joining a battle royale under way takes a standing bot's place (where it stands), or watches. */
	APawn* TakeOverBot(AController* NewPlayer, int32 Species);

	mutable TOptional<FVector> ArenaCenter;
};
