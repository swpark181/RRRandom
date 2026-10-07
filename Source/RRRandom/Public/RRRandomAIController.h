#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Math/RandomStream.h"
#include "RRRandomAIController.generated.h"

class ARRRandomCharacter;

/**
 * Careful brawler bot. Goes after the nearest opponent it can see, keeps its own preferred distance while strafing,
 * waits a moment after spotting someone before shooting, fires in short bursts with some aim error,
 * and only shoots when no cover is in the way. Ducks behind cover to reload or heal, reloads early while
 * nobody is in sight, and rolls its stored dice when a fight starts.
 * Steers directly (sliding around cover) instead of pathfinding, so it needs no navmesh.
 */
UCLASS()
class RRRANDOM_API ARRRandomAIController : public AAIController
{
	GENERATED_BODY()

public:
	ARRRandomAIController();

	virtual void Tick(float DeltaSeconds) override;

	/** Where to wait when no opponent is up. */
	void SetArenaCenter(const FVector& Center) { ArenaCenter = Center; bHasArenaCenter = true; }

protected:
	virtual void OnPossess(APawn* InPawn) override;

	/** Each bot picks a preferred fighting distance in this range, as a share of its attack range. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FVector2D PreferredRangeShare = FVector2D(0.6f, 0.85f);

	/** Below this share of health the bot backs off to heal behind cover... */
	UPROPERTY(EditAnywhere, Category = "AI")
	float RetreatHealthShare = 0.45f;

	/** ...and comes back once healed to this share. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float ReturnHealthShare = 0.9f;

	/** Recently hit and below this share of health, the bot looks for cover even with ammo left. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float UnderFireCoverHealthShare = 0.7f;

	/** Seconds after spotting an opponent before the first shot. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FVector2D ReactionTime = FVector2D(0.25f, 0.55f);

	/** Shots are thrown off by up to this many degrees either way. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float AimErrorDegrees = 6.f;

	/** Shots per burst, picked anew for each burst. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FIntPoint BurstSize = FIntPoint(3, 5);

	/** Seconds between bursts. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FVector2D BurstPause = FVector2D(0.4f, 0.9f);

	/** With nobody in sight, the bot reloads once the magazine is down to this share. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float TacticalReloadShare = 0.6f;

	/** Cover further than this from the bot is not considered. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float CoverSearchRadius = 1200.f;

	/** Out of a fight, the bot holds this many dice at most before rolling anyway. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FIntPoint DiceHoldRange = FIntPoint(2, 4);

	/** Teammates closer than this push each other apart so bots don't stack up. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float SeparationDistance = 220.f;

	/** 0 picks a new seed each play session. */
	UPROPERTY(EditAnywhere, Category = "AI")
	int32 Seed = 0;

private:
	ARRRandomCharacter* FindBestOpponent(const ARRRandomCharacter* Self) const;
	FVector ChooseFightDirection(const ARRRandomCharacter* Self, const ARRRandomCharacter* Opponent, float Distance, bool bCanSee) const;
	/** Picks a spot behind cover, hidden from Threat. Returns false if there is none nearby. */
	bool FindCoverSpot(const ARRRandomCharacter* Self, const FVector& Threat, FVector& OutSpot) const;
	/** Turns a wanted direction aside so the bot slides around cover instead of walking into it. */
	FVector SteerAroundObstacles(const ARRRandomCharacter* Self, const FVector& Wanted);
	FVector GetSeparation(const ARRRandomCharacter* Self) const;
	/** Whether a shot from one spot would reach the other without hitting cover. Both are capsule centers. */
	bool HasLineOfFire(const FVector& From, const FVector& To) const;
	void TryShoot(ARRRandomCharacter* Self, const ARRRandomCharacter* Opponent, float Distance);
	void UpdateReload(ARRRandomCharacter* Self, bool bOpponentInSight);
	void UpdateDice(ARRRandomCharacter* Self, float Distance, float DeltaSeconds);

	TWeakObjectPtr<ARRRandomCharacter> Opponent;
	FRandomStream Stream;
	FVector ArenaCenter = FVector::ZeroVector;
	bool bHasArenaCenter = false;
	bool bRetreating = false;
	bool bHadSight = false;
	bool bHasCoverSpot = false;
	FVector CoverSpot = FVector::ZeroVector;
	float PreferredDistance = 900.f;
	float RetargetTimer = 0.f;
	float CoverSearchTimer = 0.f;
	float StrafeTimer = 0.f;
	float StrafeSign = 1.f;
	float AvoidSign = 1.f;
	float ReactionTimer = 0.f;
	float ShotTimer = 0.f;
	int32 BurstRemaining = 0;
	float UnderFireTimer = 0.f;
	float LastHealth = 0.f;
	float DiceTimer = 0.f;
	int32 DiceHoldLimit = 3;
};
