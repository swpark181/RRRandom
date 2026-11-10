#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "RRRandomVillainAI.generated.h"

class ARRRandomCharacter;
class ARRRandomDefenseWall;
class ARRRandomDiceTown;

/**
 * A defense villain's mind (round 90, ARRRandomCharacter::IsVillain): it walks the stage's routes (ARRRandomDiceTown::FindPath)
 * to the giant dice tower and bites it. On the way an animal friend who comes close is bitten first (the cat chases them
 * from much further), and a wall standing between it and the tower is bitten down. The cat boss also slams the ground when
 * friends crowd round it. Villains never jump, climb or ride (round 90: the level's devices are for later).
 */
UCLASS()
class RRRANDOM_API ARRRandomVillainAI : public AAIController
{
	GENERATED_BODY()

public:
	ARRRandomVillainAI();

	virtual void Tick(float DeltaSeconds) override;

	/** The gap from a villain's capsule edge to the nearest point of an actor's collision (a brawler's capsule edge), in 2D. */
	static float GetGap(const ARRRandomCharacter* Self, const AActor* Target);

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	/** The nearest animal friend it would go for now, or null. */
	ARRRandomCharacter* FindFriend(const ARRRandomCharacter* Self) const;
	/** A standing wall between it and the tower, within reach. */
	ARRRandomDefenseWall* FindBlockingWall(const ARRRandomCharacter* Self, const FVector& TowerAt) const;
	FVector FollowRoute(ARRRandomCharacter* Self, const FVector& Goal, float DeltaSeconds);

	TWeakObjectPtr<ARRRandomCharacter> Friend;
	TWeakObjectPtr<ARRRandomDiceTown> Town;
	TArray<FVector> Route;
	int32 RouteIndex = 0;
	FVector RouteGoal = FVector::ZeroVector;
	float RouteTimer = 0.f;
	float RetargetTimer = 0.f;
	float StuckTimer = 0.f;
	float SidestepTimer = 0.f;
	/** Every few seconds: how far it got since (to tell a villain stuck in a crater), and how many checks in a row it was stuck. */
	float ProgressTimer = 0.f;
	FVector ProgressFrom = FVector::ZeroVector;
	int32 StuckChecks = 0;
	bool bBitLately = false;
	FVector Sidestep = FVector::ZeroVector;
	FRandomStream Stream;
};
