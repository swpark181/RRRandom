#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "RRRandomSaveGame.generated.h"

/**
 * What this PC remembers between games (round 90): how far its player got in the defense mode. Saved in the slot
 * SlotName (Saved/SaveGames) by URRRandomSessionSubsystem.
 */
UCLASS()
class RRRANDOM_API URRRandomSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr const TCHAR* SlotName = TEXT("RRRandomProgress");

	/** The highest defense stage cleared (0 for none): the next one is open. */
	UPROPERTY()
	int32 ClearedStage = 0;
};
