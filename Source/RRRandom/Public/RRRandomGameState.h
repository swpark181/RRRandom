#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RRRandomGameState.generated.h"

class ARRRandomCharacter;

/**
 * Match state every machine can see: when play starts (after the start countdown), the team score, the score that wins,
 * and once a team has it, who won and the MVP (the brawler with the most knockouts). The game mode (server only) keeps it up to date.
 */
UCLASS()
class RRRANDOM_API ARRRandomGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARRRandomGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetTeamScore(int32 Team) const { return Team == 0 || Team == 1 ? TeamScores[Team] : 0; }
	int32 GetScoreToWin() const { return ScoreToWin; }
	bool IsMatchOver() const { return bMatchOver; }
	int32 GetWinningTeam() const { return WinningTeam; }
	ARRRandomCharacter* GetMVP() const { return MVP; }

	/** True when the world's game state says the match is over (false without one). */
	static bool IsMatchOver(const UObject* WorldContext);

	/** Seconds until play starts (zero or less once it has), on the server's clock. */
	float GetCountdownRemaining() const { return PlayStartTime - GetServerWorldTimeSeconds(); }
	bool HasPlayStarted() const { return GetCountdownRemaining() <= 0.f; }
	/** Seconds since play started (negative before). */
	float GetSincePlayStart() const { return -GetCountdownRemaining(); }
	/** Nobody moves, shoots or rolls: still counting down to the start, or the match is over (false without a game state). */
	static bool IsPlayLocked(const UObject* WorldContext);

	/** Server only: play starts at this server world time. */
	void SetPlayStartTime(float Time) { PlayStartTime = Time; }

	/** Server only. */
	void AddTeamScore(int32 Team);
	/** Server only. */
	void SetScoreToWin(int32 Score) { ScoreToWin = Score; }
	/** Server only (the win cheat). */
	void SetTeamScore(int32 Team, int32 Score);
	/** Server only: ends the match for everyone. */
	void EndMatch(int32 Winner, ARRRandomCharacter* InMVP);

private:
	UPROPERTY(Replicated)
	int32 TeamScores[2];

	UPROPERTY(Replicated)
	int32 ScoreToWin = 0;

	UPROPERTY(Replicated)
	float PlayStartTime = 0.f;

	UPROPERTY(Replicated)
	bool bMatchOver = false;

	UPROPERTY(Replicated)
	int32 WinningTeam = INDEX_NONE;

	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomCharacter> MVP;
};
