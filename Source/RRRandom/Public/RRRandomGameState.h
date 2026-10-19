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
	/** The match is played in third person (over the shoulder, no fog of war) rather than from the top view; the host picks. */
	static bool IsThirdPersonMode(const UObject* WorldContext);

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
	void SetThirdPersonMode(bool bOn) { bThirdPersonMode = bOn; }
	/** Server only (the win cheat). */
	void SetTeamScore(int32 Team, int32 Score);
	/** Server only: ends the match for everyone. */
	void EndMatch(int32 Winner, ARRRandomCharacter* InMVP);

	/**
	 * Where the winners line up to dance once the match is over: the middle of the line (the MVP's spot, at the middle of
	 * its capsule), the way the line faces (yaw), out from the map's centerpiece behind it, and how far its ends are from
	 * the middle. Set before EndMatch, so it replicates with it; false when there is no room for a line there (the winners
	 * dance round the MVP where it stands).
	 */
	bool GetDanceLine(FVector& OutCenter, float& OutYaw, float& OutHalfWidth) const
	{
		OutCenter = DanceLineCenter;
		OutYaw = DanceLineYaw;
		OutHalfWidth = DanceLineHalfWidth;
		return bHasDanceLine;
	}
	/** Server only. */
	void SetDanceLine(const FVector& Center, float Yaw, float HalfWidth)
	{
		DanceLineCenter = Center;
		DanceLineYaw = Yaw;
		DanceLineHalfWidth = HalfWidth;
		bHasDanceLine = true;
	}

	/**
	 * Meteor Rush (ERRMatchRule::MeteorRush): the team scores are the dice ore each team carries, ScoreToWin the points to
	 * hold, and a team holding them (more than the other) counts down to the win. The match also has a clock; a tie when
	 * it runs out goes to overtime, where holding the golden crystal counts down instead.
	 */
	bool IsMeteorRush() const { return bMeteorRush; }
	static bool IsMeteorRush(const UObject* WorldContext);
	/** Seconds of the match clock left (server clock), or a negative number without one. */
	float GetMatchTimeRemaining() const { return MatchEndTime > 0.f ? MatchEndTime - GetServerWorldTimeSeconds() : -1.f; }
	bool IsOvertime() const { return bOvertime; }
	/** The team counting down to a win, or INDEX_NONE. */
	int32 GetHoldTeam() const { return HoldTeam; }
	float GetHoldRemaining() const { return HoldEndTime - GetServerWorldTimeSeconds(); }
	float GetHoldLength() const { return HoldLength; }

	/** Server only. */
	void SetMeteorRush(bool bOn) { bMeteorRush = bOn; }
	void SetMatchEndTime(float Time) { MatchEndTime = Time; }
	void SetOvertime(bool bOn) { bOvertime = bOn; }
	/** Server only: Team starts counting down Seconds to its win (INDEX_NONE stops the count). */
	void SetHold(int32 Team, float Seconds);

private:
	UPROPERTY(Replicated)
	bool bMeteorRush = false;

	UPROPERTY(Replicated)
	float MatchEndTime = 0.f;

	UPROPERTY(Replicated)
	bool bOvertime = false;

	UPROPERTY(Replicated)
	int32 HoldTeam = INDEX_NONE;

	UPROPERTY(Replicated)
	float HoldEndTime = 0.f;

	UPROPERTY(Replicated)
	float HoldLength = 0.f;

	UPROPERTY(Replicated)
	int32 TeamScores[2];

	UPROPERTY(Replicated)
	int32 ScoreToWin = 0;

	UPROPERTY(Replicated)
	float PlayStartTime = 0.f;

	UPROPERTY(Replicated)
	bool bMatchOver = false;

	UPROPERTY(Replicated)
	bool bThirdPersonMode = false;

	UPROPERTY(Replicated)
	int32 WinningTeam = INDEX_NONE;

	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomCharacter> MVP;

	UPROPERTY(Replicated)
	bool bHasDanceLine = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize DanceLineCenter = FVector::ZeroVector;

	UPROPERTY(Replicated)
	float DanceLineYaw = 0.f;

	UPROPERTY(Replicated)
	float DanceLineHalfWidth = 0.f;
};
