#include "RRRandomTitlePlayerController.h"
#include "RRRandomSessionSubsystem.h"
#include "RRRandomTitleHUD.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"

ARRRandomTitlePlayerController::ARRRandomTitlePlayerController()
{
	bShowMouseCursor = true;
}

TArray<ERRTitleChoice> ARRRandomTitlePlayerController::GetChoices() const
{
	if (bNetworkPage)
	{
		return { ERRTitleChoice::Host, ERRTitleChoice::Join, ERRTitleChoice::Back };
	}
	return { ERRTitleChoice::Single, ERRTitleChoice::Network, ERRTitleChoice::Quit };
}

void ARRRandomTitlePlayerController::SetSelected(int32 Index)
{
	Selected = FMath::Clamp(Index, 0, GetChoices().Num() - 1);
}

bool ARRRandomTitlePlayerController::IsBusy() const
{
	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	return Sessions && Sessions->IsBusy();
}

FString ARRRandomTitlePlayerController::GetLabel(ERRTitleChoice Choice)
{
	switch (Choice)
	{
	case ERRTitleChoice::Single: return TEXT("SINGLE PLAY");
	case ERRTitleChoice::Network: return TEXT("NETWORK");
	case ERRTitleChoice::Host: return TEXT("HOST GAME");
	case ERRTitleChoice::Join: return TEXT("JOIN GAME");
	case ERRTitleChoice::Back: return TEXT("BACK");
	case ERRTitleChoice::Quit: return TEXT("QUIT");
	}
	return FString();
}

FString ARRRandomTitlePlayerController::GetDescription(ERRTitleChoice Choice)
{
	switch (Choice)
	{
	case ERRTitleChoice::Single: return TEXT("You and 2 bots against 3 bots.");
	case ERRTitleChoice::Network: return TEXT("Play with others, no server needed.");
	case ERRTitleChoice::Host: return TEXT("Start a game others can join. Bots fill the empty spots.");
	case ERRTitleChoice::Join: return TEXT("Join the first game found.");
	case ERRTitleChoice::Back: return TEXT("Back to the main menu.");
	case ERRTitleChoice::Quit: return TEXT("Close the game.");
	}
	return FString();
}

void ARRRandomTitlePlayerController::Choose(ERRTitleChoice Choice)
{
	URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	if (!Sessions || Sessions->IsBusy())
	{
		return;
	}

	switch (Choice)
	{
	case ERRTitleChoice::Single:
		Sessions->PlaySolo();
		break;
	case ERRTitleChoice::Network:
		bNetworkPage = true;
		Selected = 0;
		break;
	case ERRTitleChoice::Host:
		Sessions->HostGame();
		break;
	case ERRTitleChoice::Join:
		Sessions->JoinGame();
		break;
	case ERRTitleChoice::Back:
		bNetworkPage = false;
		Selected = GetChoices().IndexOfByKey(ERRTitleChoice::Network);
		break;
	case ERRTitleChoice::Quit:
		ConsoleCommand(TEXT("quit"));
		break;
	}
}

void ARRRandomTitlePlayerController::CreateInputAssets()
{
	if (MappingContext)
	{
		return;
	}

	auto MakeAction = [this](const TCHAR* Name)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = EInputActionValueType::Boolean;
		return Action;
	};
	UpAction = MakeAction(TEXT("IA_MenuUp"));
	DownAction = MakeAction(TEXT("IA_MenuDown"));
	ConfirmAction = MakeAction(TEXT("IA_MenuConfirm"));
	BackAction = MakeAction(TEXT("IA_MenuBack"));
	ClickAction = MakeAction(TEXT("IA_MenuClick"));

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Title"));
	MappingContext->MapKey(UpAction, EKeys::W);
	MappingContext->MapKey(UpAction, EKeys::Up);
	MappingContext->MapKey(DownAction, EKeys::S);
	MappingContext->MapKey(DownAction, EKeys::Down);
	MappingContext->MapKey(ConfirmAction, EKeys::Enter);
	MappingContext->MapKey(ConfirmAction, EKeys::SpaceBar);
	MappingContext->MapKey(BackAction, EKeys::Escape);
	MappingContext->MapKey(ClickAction, EKeys::LeftMouseButton);
}

void ARRRandomTitlePlayerController::BeginPlay()
{
	Super::BeginPlay();

	CreateInputAssets();
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}

	// -RRSolo / -RRHost / -RRJoin skip the menu
	if (IsLocalController())
	{
		if (URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>())
		{
			Sessions->OnTitleReady();
		}
	}
}

void ARRRandomTitlePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	CreateInputAssets();
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInput)
	{
		UE_LOG(LogTemp, Error, TEXT("RRRandom needs EnhancedInputComponent as the default input component (Config/DefaultInput.ini)."));
		return;
	}
	EnhancedInput->BindAction(UpAction, ETriggerEvent::Started, this, &ARRRandomTitlePlayerController::OnUp);
	EnhancedInput->BindAction(DownAction, ETriggerEvent::Started, this, &ARRRandomTitlePlayerController::OnDown);
	EnhancedInput->BindAction(ConfirmAction, ETriggerEvent::Started, this, &ARRRandomTitlePlayerController::OnConfirm);
	EnhancedInput->BindAction(BackAction, ETriggerEvent::Started, this, &ARRRandomTitlePlayerController::OnBack);
	EnhancedInput->BindAction(ClickAction, ETriggerEvent::Started, this, &ARRRandomTitlePlayerController::OnClick);
}

void ARRRandomTitlePlayerController::OnUp()
{
	const int32 Count = GetChoices().Num();
	Selected = (Selected + Count - 1) % Count;
}

void ARRRandomTitlePlayerController::OnDown()
{
	Selected = (Selected + 1) % GetChoices().Num();
}

void ARRRandomTitlePlayerController::OnConfirm()
{
	const TArray<ERRTitleChoice> Choices = GetChoices();
	if (Choices.IsValidIndex(Selected))
	{
		Choose(Choices[Selected]);
	}
}

void ARRRandomTitlePlayerController::OnBack()
{
	URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	if (Sessions && Sessions->IsBusy())
	{
		Sessions->CancelPending();
	}
	else if (bNetworkPage)
	{
		Choose(ERRTitleChoice::Back);
	}
}

void ARRRandomTitlePlayerController::OnClick()
{
	const ARRRandomTitleHUD* TitleHUD = GetHUD<ARRRandomTitleHUD>();
	float MouseX;
	float MouseY;
	if (!TitleHUD || !GetMousePosition(MouseX, MouseY))
	{
		return;
	}
	const int32 Index = TitleHUD->GetButtonAt(FVector2D(MouseX, MouseY));
	const TArray<ERRTitleChoice> Choices = GetChoices();
	if (Choices.IsValidIndex(Index))
	{
		Selected = Index;
		Choose(Choices[Index]);
	}
}
