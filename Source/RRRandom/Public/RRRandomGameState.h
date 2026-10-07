#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "RRRandomGameState.generated.h"

/** Match state every machine can see: the team score. The game mode (server only) keeps it up to date. */
UCLASS()
class RRRANDOM_API ARRRandomGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ARRRandomGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	int32 GetTeamScore(int32 Team) const { return Team == 0 || Team == 1 ? TeamScores[Team] : 0; }

	/** Server only. */
	void AddTeamScore(int32 Team);

private:
	UPROPERTY(Replicated)
	int32 TeamScores[2];
};
