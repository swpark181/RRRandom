#include "RRRandomGameState.h"
#include "RRRandomCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ARRRandomGameState::ARRRandomGameState()
{
	TeamScores[0] = 0;
	TeamScores[1] = 0;
	RoundWins[0] = 0;
	RoundWins[1] = 0;
	TiebreakDice[0] = 0;
	TiebreakDice[1] = 0;
}

bool ARRRandomGameState::IsRounds(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
	return State && State->bRounds;
}

void ARRRandomGameState::StartRound(int32 InRound, ERRRoundRule InRule, int32 InFace)
{
	if (!HasAuthority())
	{
		return;
	}
	Round = InRound;
	RoundRule = InRule;
	RuleFace = InFace;
	PlayedRules |= 1 << static_cast<int32>(InRule);
	RoundIntroTime = GetServerWorldTimeSeconds();
	RoundWinner = INDEX_NONE;
	TiebreakDice[0] = 0;
	TiebreakDice[1] = 0;
	TeamScores[0] = 0;
	TeamScores[1] = 0;
	bOvertime = false;
	HoldTeam = INDEX_NONE;
	HoldEndTime = 0.f;
	HoldLength = 0.f;
}

void ARRRandomGameState::EndRound(int32 Winner, int32 DieBlue, int32 DieRed)
{
	if (!HasAuthority() || RoundWinner != INDEX_NONE || (Winner != 0 && Winner != 1))
	{
		return;
	}
	RoundWinner = Winner;
	RoundOverTime = GetServerWorldTimeSeconds();
	++RoundWins[Winner];
	TiebreakDice[0] = DieBlue;
	TiebreakDice[1] = DieRed;
	HoldTeam = INDEX_NONE;
}

void ARRRandomGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARRRandomGameState, TeamScores);
	DOREPLIFETIME(ARRRandomGameState, ScoreToWin);
	DOREPLIFETIME(ARRRandomGameState, PlayStartTime);
	DOREPLIFETIME(ARRRandomGameState, bMatchOver);
	DOREPLIFETIME(ARRRandomGameState, bThirdPersonMode);
	DOREPLIFETIME(ARRRandomGameState, WinningTeam);
	DOREPLIFETIME(ARRRandomGameState, MVP);
	DOREPLIFETIME(ARRRandomGameState, bHasDanceLine);
	DOREPLIFETIME(ARRRandomGameState, DanceLineCenter);
	DOREPLIFETIME(ARRRandomGameState, DanceLineYaw);
	DOREPLIFETIME(ARRRandomGameState, DanceLineHalfWidth);
	DOREPLIFETIME(ARRRandomGameState, bMeteorRush);
	DOREPLIFETIME(ARRRandomGameState, MatchEndTime);
	DOREPLIFETIME(ARRRandomGameState, bOvertime);
	DOREPLIFETIME(ARRRandomGameState, HoldTeam);
	DOREPLIFETIME(ARRRandomGameState, HoldEndTime);
	DOREPLIFETIME(ARRRandomGameState, HoldLength);
	DOREPLIFETIME(ARRRandomGameState, bDefense);
	DOREPLIFETIME(ARRRandomGameState, DefenseStage);
	DOREPLIFETIME(ARRRandomGameState, Wave);
	DOREPLIFETIME(ARRRandomGameState, bWaveBreak);
	DOREPLIFETIME(ARRRandomGameState, NextWaveTime);
	DOREPLIFETIME(ARRRandomGameState, VillainsLeft);
	DOREPLIFETIME(ARRRandomGameState, WaveVillains);
	DOREPLIFETIME(ARRRandomGameState, bBossOut);
	DOREPLIFETIME(ARRRandomGameState, bBattleRoyale);
	DOREPLIFETIME(ARRRandomGameState, BattleTeamCount);
	DOREPLIFETIME(ARRRandomGameState, AliveFriends);
	DOREPLIFETIME(ARRRandomGameState, AliveTeams);
	DOREPLIFETIME(ARRRandomGameState, TeamPlaces);
	DOREPLIFETIME(ARRRandomGameState, PrepEndTime);
	DOREPLIFETIME(ARRRandomGameState, StartSpots);
	DOREPLIFETIME(ARRRandomGameState, TeamPicks);
	DOREPLIFETIME(ARRRandomGameState, TeamSpots);
	DOREPLIFETIME(ARRRandomGameState, TeamDice);
	DOREPLIFETIME(ARRRandomGameState, TeamOrder);
	DOREPLIFETIME(ARRRandomGameState, bRounds);
	DOREPLIFETIME(ARRRandomGameState, Round);
	DOREPLIFETIME(ARRRandomGameState, RoundRule);
	DOREPLIFETIME(ARRRandomGameState, RuleFace);
	DOREPLIFETIME(ARRRandomGameState, RoundWins);
	DOREPLIFETIME(ARRRandomGameState, RoundIntroTime);
	DOREPLIFETIME(ARRRandomGameState, RoundWinner);
	DOREPLIFETIME(ARRRandomGameState, RoundOverTime);
	DOREPLIFETIME(ARRRandomGameState, TiebreakDice);
	DOREPLIFETIME(ARRRandomGameState, PlayedRules);
}

bool ARRRandomGameState::IsBattleRoyale(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
	return State && State->bBattleRoyale;
}

bool ARRRandomGameState::IsDefense(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
	return State && State->bDefense;
}

bool ARRRandomGameState::IsMeteorRush(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
	return State && State->bMeteorRush;
}

void ARRRandomGameState::SetHold(int32 Team, float Seconds)
{
	if (!HasAuthority())
	{
		return;
	}
	HoldTeam = Team == 0 || Team == 1 ? Team : INDEX_NONE;
	HoldLength = HoldTeam != INDEX_NONE ? Seconds : 0.f;
	HoldEndTime = GetServerWorldTimeSeconds() + HoldLength;
}

bool ARRRandomGameState::IsThirdPersonMode(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
	return State && State->bThirdPersonMode;
}

bool ARRRandomGameState::IsMatchOver(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
	return State && State->IsMatchOver();
}

bool ARRRandomGameState::IsPlayLocked(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
	// (the battle royale's get-ready time isn't locked: only attacks are, IsAttackLocked; round 98: between rounds is)
	return State && (State->IsMatchOver() || State->IsRoundOver() || (!State->HasPlayStarted() && !State->IsPrep()));
}

void ARRRandomGameState::AddTeamScore(int32 Team)
{
	if (HasAuthority() && (Team == 0 || Team == 1))
	{
		++TeamScores[Team];
	}
}

void ARRRandomGameState::SetTeamScore(int32 Team, int32 Score)
{
	if (HasAuthority() && (Team == 0 || Team == 1))
	{
		TeamScores[Team] = Score;
	}
}

void ARRRandomGameState::EndMatch(int32 Winner, ARRRandomCharacter* InMVP)
{
	if (HasAuthority() && !bMatchOver)
	{
		bMatchOver = true;
		WinningTeam = Winner;
		MVP = InMVP;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The battle royale's get-ready time (round 97)

bool ARRRandomGameState::IsAttackLocked(const UObject* WorldContext)
{
	return IsPlayLocked(WorldContext) || IsPrep(WorldContext);
}

bool ARRRandomGameState::IsPrep(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
	return State && State->IsPrep();
}

void ARRRandomGameState::SetPrep(float InPrepEndTime, const TArray<FVector>& Spots, int32 TeamCount)
{
	PrepEndTime = InPrepEndTime;
	StartSpots.Reset();
	for (const FVector& Spot : Spots)
	{
		StartSpots.Add(Spot);
	}
	TeamPicks.Init(INDEX_NONE, TeamCount);
	TeamSpots.Init(INDEX_NONE, TeamCount);
	TeamDice.Init(0, TeamCount * 3);
	TeamOrder.Init(0, TeamCount);
}

void ARRRandomGameState::SetTeamPick(int32 Team, int32 Spot)
{
	if (TeamPicks.IsValidIndex(Team) && (Spot == INDEX_NONE || StartSpots.IsValidIndex(Spot)))
	{
		TeamPicks[Team] = Spot;
	}
}

void ARRRandomGameState::SetTeamRoll(const TArray<uint8>& Dice, const TArray<int32>& Order, const TArray<int32>& Spots)
{
	TeamDice = Dice;
	TeamOrder = Order;
	TeamSpots = Spots;
}
