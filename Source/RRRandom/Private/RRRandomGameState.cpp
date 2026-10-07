#include "RRRandomGameState.h"
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
}

void ARRRandomGameState::AddTeamScore(int32 Team)
{
	if (HasAuthority() && (Team == 0 || Team == 1))
	{
		++TeamScores[Team];
	}
}
