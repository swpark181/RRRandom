#include "RRRandomGameMode.h"
#include "RRRandomCharacter.h"
#include "RRRandomPlayerController.h"

ARRRandomGameMode::ARRRandomGameMode()
{
	DefaultPawnClass = ARRRandomCharacter::StaticClass();
	PlayerControllerClass = ARRRandomPlayerController::StaticClass();
}
