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
 * Every knockout scores a point for the other team.
 * Exists only on the server (the host); clients see the result through replication.
 */
UCLASS()
class RRRANDOM_API ARRRandomGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARRRandomGameMode();

	virtual void StartPlay() override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	virtual void Logout(AController* Exiting) override;

	/** Called by a brawler when it is knocked out. Attacker may be null. */
	void OnBrawlerKnockedOut(ARRRandomCharacter* Victim, ARRRandomCharacter* Attacker);

protected:
	UPROPERTY(EditAnywhere, Category = "Bots")
	TSubclassOf<ARRRandomCharacter> BotClass;

	/** Brawlers per team, players and bots together. */
	UPROPERTY(EditAnywhere, Category = "Teams", meta = (ClampMin = "1"))
	int32 TeamSize = 3;

	/** Players alternate between blue and red. Off puts every player on blue against the bots (until blue is full). */
	UPROPERTY(EditAnywhere, Category = "Teams")
	bool bSplitPlayersAcrossTeams = true;

	/** How far up the screen (+X) from the player start the red team begins. */
	UPROPERTY(EditAnywhere, Category = "Teams")
	float TeamSpawnDistance = 2000.f;

	/** Sideways gap between teammates at the start. */
	UPROPERTY(EditAnywhere, Category = "Teams")
	float SpawnSpacing = 300.f;

	/** Lays out CoverLayout when the level has no cover of its own. */
	UPROPERTY(EditAnywhere, Category = "Cover")
	bool bSpawnCover = true;

	UPROPERTY(EditAnywhere, Category = "Cover")
	TSubclassOf<ARRRandomCover> CoverClass;

	/** Builds the voxel island (cliffs, waterfall, river, bay, hills, paths) around the arena when the level has none (ARRRandomIsland). */
	UPROPERTY(EditAnywhere, Category = "Island")
	bool bSpawnIsland = true;

	/** Covers the floor with grass tiles when the level has none of its own (ARRRandomGrassFloor). */
	UPROPERTY(EditAnywhere, Category = "Grass")
	bool bSpawnGrassFloor = true;

	/** Which random tiles the grass gets; every machine lays the same ones from it. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	int32 GrassSeed = 7;

	/** Blocks of cover relative to the player start. The default is symmetric so both teams get the same. */
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

	void EnsureSlots();
	/** The team a new player joins, or INDEX_NONE when both are full of players. */
	int32 ChooseTeamForPlayer() const;
	/** The first spot on the team not held by a player, or INDEX_NONE. */
	int32 FindSlotForPlayer(int32 Team) const;
	/** Where a spot is and which way it faces; false if there is no floor there. */
	bool GetSlotTransform(int32 Team, int32 Slot, float HalfHeight, FTransform& OutTransform) const;
	/** Spawns a brawler on a team, standing on one of its spots. */
	ARRRandomCharacter* SpawnBrawler(TSubclassOf<ARRRandomCharacter> BrawlerClass, int32 Team, int32 Slot);
	void RemoveBot(int32 Team, int32 Slot);
	/** Puts a bot on every spot that has nobody. */
	void FillEmptySlotsWithBots();
	void SpawnDummiesIfNoneExist();
	void SpawnCoverIfNoneExists();
	void SpawnGrassFloorIfNoneExists();
	void SpawnIslandIfNoneExists();
	FVector GetPlayerStartLocation() const;

	/** Places a point on the floor beneath it, or returns false if there is no floor there. */
	bool FindFloorSpot(const FVector& Spot, float HalfHeight, FVector& OutLocation) const;

	TArray<FTeamSlot> TeamSlots[2];
};
