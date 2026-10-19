#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "Math/RandomStream.h"
#include "RRRandomMeteorRush.generated.h"

class ARRRandomGameMode;
class ARRRandomMeteor;

/**
 * Runs a Meteor Rush match on the server (spawned by the game mode when the rule is picked):
 * - drops a meteor FirstMeteorTime seconds into play and every MeteorInterval after, each where both teams have about as
 *   far to go (near the line halfway between their starts) and away from the last one;
 * - keeps the team scores at the ore each team carries, and counts HoldSeconds down for a team holding ScoreToWin or more
 *   and more than the other (the count starts over whenever that stops being true);
 * - at MatchLength, the team with more wins; a tie brings overtime: a golden meteor in the middle, and holding its
 *   crystal for GoldenHoldSeconds wins.
 * Not replicated: everything players see goes through the game state, the meteors, the ore and the brawlers.
 */
UCLASS()
class RRRANDOM_API ARRRandomMeteorRushDirector : public AInfo
{
	GENERATED_BODY()

public:
	ARRRandomMeteorRushDirector();

	virtual void Tick(float DeltaSeconds) override;

	static constexpr int32 ScoreToWin = 77;
	static constexpr float HoldSeconds = 12.f;
	static constexpr float MatchLength = 900.f;
	static constexpr float FirstMeteorTime = 10.f;
	static constexpr float MeteorInterval = 60.f;
	static constexpr float GoldenHoldSeconds = 5.f;
	/** Between meteors, a single piece of ore falls like a shooting star this often (seconds, picked anew each time). */
	static constexpr float FallingStarMin = 6.f;
	static constexpr float FallingStarMax = 10.f;

protected:
	virtual void BeginPlay() override;

private:
	void SpawnMeteor(bool bGolden);
	/** A shard (mostly), a lump or now and then a big crystal falling like a shooting star somewhere on the battlefield. */
	void DropFallingStar();
	/** Open ground for a falling star anywhere between the teams' starts. */
	bool PickStarSpot(FVector& OutSpot);
	float NextStarTime = FirstMeteorTime - 2.f;
	/** A spot on the floor for the next meteor (see the class comment); bMiddle: as near the arena's middle as it can. */
	bool PickSpot(FVector& OutSpot, bool bMiddle);
	/** Finds GroundZ the first time it is needed. */
	void EnsureGroundZ();
	/** The ground the teams start on (meteors land level with it, off maps with walking routes). */
	float GroundZ = 0.f;
	bool bHasGroundZ = false;
	void UpdateHold(int32 Leader, float Seconds);

	FRandomStream Stream;
	float NextMeteorTime = FirstMeteorTime;
	bool bOvertime = false;
	bool bHasLastSpot = false;
	FVector LastSpot = FVector::ZeroVector;
};
