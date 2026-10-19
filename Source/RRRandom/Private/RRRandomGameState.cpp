#include "RRRandomGameState.h"
#include "RRRandomCharacter.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

ARRRandomGameState::ARRRandomGameState()
{
	TeamScores[0] = 0;
	TeamScores[1] = 0;
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
	return State && (State->IsMatchOver() || !State->HasPlayStarted());
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
