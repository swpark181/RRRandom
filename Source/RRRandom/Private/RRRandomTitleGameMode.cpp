#include "RRRandomTitleGameMode.h"
#include "RRRandomTitleHUD.h"
#include "RRRandomTitlePlayerController.h"

ARRRandomTitleGameMode::ARRRandomTitleGameMode()
{
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ARRRandomTitlePlayerController::StaticClass();
	HUDClass = ARRRandomTitleHUD::StaticClass();
}

bool ARRRandomTitleGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	// Nobody gets a pawn on the title
	return false;
}
