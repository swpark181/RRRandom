#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RRRandomCover.h"
#include "RRRandomGameMode.generated.h"

class ARRRandomCharacter;
class ARRRandomDummy;

/**
 * Team brawl: the player's team (0) starts at the player start, the opponents (1) further up the screen.
 * Bots fill both teams, and every knockout scores a point for the other team.
 */
UCLASS()
class RRRANDOM_API ARRRandomGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARRRandomGameMode();

	virtual void StartPlay() override;

	/** Called by a brawler when it is knocked out. Attacker may be null. */
	void OnBrawlerKnockedOut(ARRRandomCharacter* Victim, ARRRandomCharacter* Attacker);

	int32 GetTeamScore(int32 Team) const { return Team == 0 || Team == 1 ? TeamScores[Team] : 0; }

protected:
	UPROPERTY(EditAnywhere, Category = "Bots")
	TSubclassOf<ARRRandomCharacter> BotClass;

	/** Bots fighting alongside the player. */
	UPROPERTY(EditAnywhere, Category = "Bots")
	int32 AllyBotCount = 2;

	UPROPERTY(EditAnywhere, Category = "Bots")
	int32 EnemyBotCount = 3;

	/** How far up the screen (+X) from the player start the opponents begin. */
	UPROPERTY(EditAnywhere, Category = "Bots")
	float TeamSpawnDistance = 2000.f;

	/** Sideways gap between teammates at the start. */
	UPROPERTY(EditAnywhere, Category = "Bots")
	float SpawnSpacing = 300.f;

	/** Lays out CoverLayout when the level has no cover of its own. */
	UPROPERTY(EditAnywhere, Category = "Cover")
	bool bSpawnCover = true;

	UPROPERTY(EditAnywhere, Category = "Cover")
	TSubclassOf<ARRRandomCover> CoverClass;

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
	void SpawnBots();
	void SpawnBot(int32 Team, const FVector& Spot, float Yaw, const FVector& ArenaCenter);
	void SpawnDummiesIfNoneExist();
	void SpawnCoverIfNoneExists();
	FVector GetPlayerStartLocation() const;

	/** Places a point on the floor beneath it, or returns false if there is no floor there. */
	bool FindFloorSpot(const FVector& Spot, float HalfHeight, FVector& OutLocation) const;

	int32 TeamScores[2] = { 0, 0 };
};
