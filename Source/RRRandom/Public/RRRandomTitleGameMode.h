#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RRRandomTitleGameMode.generated.h"

/**
 * Title screen: no pawn, only the menu drawn by ARRRandomTitleHUD.
 * Picked for the title map by its name (GameModeMapPrefixes in DefaultEngine.ini), so no map asset is needed.
 */
UCLASS()
class RRRANDOM_API ARRRandomTitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARRRandomTitleGameMode();

	virtual bool PlayerCanRestart_Implementation(APlayerController* Player) override;
};
