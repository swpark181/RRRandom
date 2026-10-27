#include "RRRandomTitlePlayerController.h"
#include "RRRandomSessionSubsystem.h"
#include "RRRandomSpecies.h"
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
	if (bSpeciesPage)
	{
		return { ERRTitleChoice::Fox, ERRTitleChoice::Rabbit, ERRTitleChoice::Squirrel, ERRTitleChoice::Tiger, ERRTitleChoice::Dragon, ERRTitleChoice::RedFox, ERRTitleChoice::Back };
	}
	if (bRulePage)
	{
		return { ERRTitleChoice::Knockout, ERRTitleChoice::MeteorRush, ERRTitleChoice::Back };
	}
	if (bStagePage)
	{
		return { ERRTitleChoice::Stage1, ERRTitleChoice::Stage2, ERRTitleChoice::Stage3, ERRTitleChoice::Back };
	}
	if (bRoomPage)
	{
		// Every game found, then searching again and back
		TArray<ERRTitleChoice> Choices;
		const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
		for (int32 Room = 0; Sessions && Room < Sessions->GetRooms().Num(); ++Room)
		{
			Choices.Add(ERRTitleChoice::Room);
		}
		Choices.Add(ERRTitleChoice::Refresh);
		Choices.Add(ERRTitleChoice::Back);
		return Choices;
	}
	if (bMapPage)
	{
		return { ERRTitleChoice::DiceTown, ERRTitleChoice::DiceLand, ERRTitleChoice::Back };
	}
	if (bViewPage)
	{
		// Round 90: the top view and Dice Land are put away (still there for -RRMap and the code); a battle is third person in
		// Dice Town, or the defense
		return { ERRTitleChoice::Battle, ERRTitleChoice::Defense, ERRTitleChoice::Back };
	}
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
	case ERRTitleChoice::TopView: return TEXT("TOP VIEW");
	case ERRTitleChoice::ThirdPerson: return TEXT("THIRD PERSON");
	case ERRTitleChoice::Quit: return TEXT("QUIT");
	case ERRTitleChoice::Fox:
	case ERRTitleChoice::Rabbit:
	case ERRTitleChoice::Squirrel:
	case ERRTitleChoice::Tiger:
	case ERRTitleChoice::Dragon:
	case ERRTitleChoice::RedFox: return RRSpecies::GetDisplayName(GetSpeciesOf(Choice));
	case ERRTitleChoice::DiceTown: return TEXT("DICE TOWN");
	case ERRTitleChoice::DiceLand: return TEXT("DICE LAND");
	case ERRTitleChoice::Knockout: return TEXT("KNOCKOUT");
	case ERRTitleChoice::MeteorRush: return TEXT("METEOR RUSH");
	case ERRTitleChoice::Battle: return TEXT("BATTLE");
	case ERRTitleChoice::Defense: return TEXT("DEFENSE");
	case ERRTitleChoice::Stage1:
	case ERRTitleChoice::Stage2:
	case ERRTitleChoice::Stage3: return FString::Printf(TEXT("STAGE %d"), GetStageOf(Choice));
	case ERRTitleChoice::Room: return TEXT("GAME");
	case ERRTitleChoice::Refresh: return TEXT("REFRESH");
	}
	return FString();
}

int32 ARRRandomTitlePlayerController::GetSpeciesOf(ERRTitleChoice Choice)
{
	return Choice >= ERRTitleChoice::Fox && Choice <= ERRTitleChoice::RedFox ? static_cast<int32>(Choice) - static_cast<int32>(ERRTitleChoice::Fox) : INDEX_NONE;
}

int32 ARRRandomTitlePlayerController::GetStageOf(ERRTitleChoice Choice)
{
	return Choice >= ERRTitleChoice::Stage1 && Choice <= ERRTitleChoice::Stage3 ? static_cast<int32>(Choice) - static_cast<int32>(ERRTitleChoice::Stage1) + 1 : 0;
}

FString ARRRandomTitlePlayerController::GetDescription(ERRTitleChoice Choice)
{
	switch (Choice)
	{
	case ERRTitleChoice::Single: return TEXT("Play alone, the bots on your side and against you.");
	case ERRTitleChoice::Network: return TEXT("Play with others, no server needed.");
	case ERRTitleChoice::Host: return TEXT("Start a game others can join. Bots fill the empty spots.");
	case ERRTitleChoice::Join: return TEXT("Pick a game from the list of games hosted.");
	case ERRTitleChoice::Back: return TEXT("Back.");
	case ERRTitleChoice::TopView: return TEXT("The camera high above the arena. Fog of war: you only see what your team sees.");
	case ERRTitleChoice::ThirdPerson: return TEXT("Over the shoulder: the mouse looks, right button aims. No fog of war.");
	case ERRTitleChoice::Quit: return TEXT("Close the game.");
	case ERRTitleChoice::Fox: return TEXT("Quick-witted fox with a bushy tail.");
	case ERRTitleChoice::Rabbit: return TEXT("Bouncy rabbit with tall ears.");
	case ERRTitleChoice::Squirrel: return TEXT("Cheeky squirrel with a big curly tail.");
	case ERRTitleChoice::Tiger: return TEXT("Bold little tiger in stripes.");
	case ERRTitleChoice::Dragon: return TEXT("Little red dragon. Its true form breathes fire.");
	case ERRTitleChoice::RedFox: return TEXT("Red fennec fox girl with a bell on her ear. Fox tricks.");
	case ERRTitleChoice::DiceTown: return TEXT("Big block town: climb houses and towers, fight on rooftops, by canals and in the woods.");
	case ERRTitleChoice::DiceLand: return TEXT("Small round island of candy dice. Quick fights in the open.");
	case ERRTitleChoice::Knockout: return TEXT("The first team to 10 knockouts wins.");
	case ERRTitleChoice::MeteorRush: return TEXT("Grab the dice ore meteors drop. Hold 30 points for 12 seconds to win.");
	case ERRTitleChoice::Battle: return TEXT("Team against team in Dice Town, over the shoulder.");
	case ERRTitleChoice::Defense: return TEXT("Together, hold the giant dice tower against 7 waves of rats and their cat boss.");
	case ERRTitleChoice::Stage1: return TEXT("A small island. Kickboards and motorbikes to ride.");
	case ERRTitleChoice::Stage2: return TEXT("A bigger village. Cars and human cannons join the rides.");
	case ERRTitleChoice::Stage3: return TEXT("The whole town. Plane stations too, and the toughest cats.");
	case ERRTitleChoice::Room: return TEXT("Join this game.");
	case ERRTitleChoice::Refresh: return TEXT("Look for games again.");
	}
	return FString();
}

namespace
{
	FString DescribeRoom(const FRRRoomInfo& Room)
	{
		const FString What = Room.Rule == ERRMatchRule::Defense ? FString::Printf(TEXT("DEFENSE STAGE %d"), FMath::Max(1, Room.Stage))
			: Room.Rule == ERRMatchRule::MeteorRush ? TEXT("METEOR RUSH") : TEXT("KNOCKOUT");
		return FString::Printf(TEXT("%s  ·  %s  ·  %d/%d"), *Room.HostName.Left(14).ToUpper(), *What, Room.Players, Room.MaxPlayers);
	}
}

int32 ARRRandomTitlePlayerController::GetRoomOf(int32 Index) const
{
	const TArray<ERRTitleChoice> Choices = GetChoices();
	return bRoomPage && Choices.IsValidIndex(Index) && Choices[Index] == ERRTitleChoice::Room ? Index : INDEX_NONE;
}

FString ARRRandomTitlePlayerController::GetButtonLabel(int32 Index) const
{
	const TArray<ERRTitleChoice> Choices = GetChoices();
	if (!Choices.IsValidIndex(Index))
	{
		return FString();
	}
	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	const int32 Room = GetRoomOf(Index);
	if (Room != INDEX_NONE && Sessions && Sessions->GetRooms().IsValidIndex(Room))
	{
		return DescribeRoom(Sessions->GetRooms()[Room]);
	}
	const int32 Stage = GetStageOf(Choices[Index]);
	if (Stage > 0 && Sessions && !Sessions->IsStageUnlocked(Stage))
	{
		return FString::Printf(TEXT("STAGE %d  ·  LOCKED"), Stage);
	}
	if (Stage > 0 && Sessions && Stage <= Sessions->GetClearedStage())
	{
		return FString::Printf(TEXT("STAGE %d  ·  CLEARED"), Stage);
	}
	return GetLabel(Choices[Index]);
}

FString ARRRandomTitlePlayerController::GetButtonDescription(int32 Index) const
{
	const TArray<ERRTitleChoice> Choices = GetChoices();
	if (!Choices.IsValidIndex(Index))
	{
		return FString();
	}
	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	const int32 Room = GetRoomOf(Index);
	if (Room != INDEX_NONE && Sessions && Sessions->GetRooms().IsValidIndex(Room))
	{
		const FRRRoomInfo& Info = Sessions->GetRooms()[Room];
		return Info.Players >= Info.MaxPlayers ? TEXT("This game is full.") : FString::Printf(TEXT("Join %s's game."), *Info.HostName);
	}
	const int32 Stage = GetStageOf(Choices[Index]);
	if (Stage > 0 && Sessions && !Sessions->IsStageUnlocked(Stage))
	{
		return FString::Printf(TEXT("Clear stage %d to open it."), Stage - 1);
	}
	return GetDescription(Choices[Index]);
}

bool ARRRandomTitlePlayerController::IsButtonEnabled(int32 Index) const
{
	const TArray<ERRTitleChoice> Choices = GetChoices();
	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	if (!Choices.IsValidIndex(Index) || !Sessions)
	{
		return Choices.IsValidIndex(Index);
	}
	const int32 Stage = GetStageOf(Choices[Index]);
	if (Stage > 0)
	{
		return Sessions->IsStageUnlocked(Stage);
	}
	const int32 Room = GetRoomOf(Index);
	if (Room != INDEX_NONE)
	{
		return Sessions->GetRooms().IsValidIndex(Room) && Sessions->GetRooms()[Room].Players < Sessions->GetRooms()[Room].MaxPlayers;
	}
	return true;
}

void ARRRandomTitlePlayerController::Choose(ERRTitleChoice Choice)
{
	URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	// (a locked stage or a full room does nothing)
	if (!Sessions || Sessions->IsBusy() || (!IsButtonEnabled(Selected) && GetChoices().IsValidIndex(Selected) && GetChoices()[Selected] == Choice))
	{
		return;
	}

	switch (Choice)
	{
	case ERRTitleChoice::Single:
		bViewPage = true;
		bViewForHost = false;
		Selected = 0;
		break;
	case ERRTitleChoice::Network:
		bNetworkPage = true;
		Selected = 0;
		break;
	case ERRTitleChoice::Host:
		bViewPage = true;
		bViewForHost = true;
		Selected = 0;
		break;
	case ERRTitleChoice::TopView:
		Sessions->SetThirdPersonMode(false);
		OpenRulePage();
		break;
	case ERRTitleChoice::ThirdPerson:
		// Which map first
		Sessions->SetThirdPersonMode(true);
		OpenMapPage();
		break;
	case ERRTitleChoice::DiceTown:
	case ERRTitleChoice::DiceLand:
		Sessions->SetThirdPersonMap(Choice == ERRTitleChoice::DiceLand ? ERRThirdPersonMap::DiceLand : ERRThirdPersonMap::DiceTown);
		bMapPage = false;
		OpenRulePage();
		break;
	case ERRTitleChoice::Battle:
		// Third person in Dice Town, then the rule
		Sessions->SetThirdPersonMode(true);
		Sessions->SetThirdPersonMap(ERRThirdPersonMap::DiceTown);
		if (Sessions->GetMatchRule() == ERRMatchRule::Defense)
		{
			Sessions->SetMatchRule(ERRMatchRule::Knockout);
		}
		bViewPage = false;
		OpenRulePage();
		break;
	case ERRTitleChoice::Defense:
		Sessions->SetThirdPersonMode(true);
		Sessions->SetMatchRule(ERRMatchRule::Defense);
		bViewPage = false;
		OpenStagePage();
		break;
	case ERRTitleChoice::Stage1:
	case ERRTitleChoice::Stage2:
	case ERRTitleChoice::Stage3:
		// Only the stages this PC has open (the one after the last it cleared)
		if (!Sessions->IsStageUnlocked(GetStageOf(Choice)))
		{
			break;
		}
		Sessions->SetDefenseStage(GetStageOf(Choice));
		bStagePage = false;
		OpenSpeciesPage(bViewForHost ? EStart::Host : EStart::Solo);
		break;
	case ERRTitleChoice::Knockout:
	case ERRTitleChoice::MeteorRush:
		Sessions->SetMatchRule(Choice == ERRTitleChoice::MeteorRush ? ERRMatchRule::MeteorRush : ERRMatchRule::Knockout);
		bRulePage = false;
		OpenSpeciesPage(bViewForHost ? EStart::Host : EStart::Solo);
		break;
	case ERRTitleChoice::Join:
		OpenRoomPage();
		break;
	case ERRTitleChoice::Refresh:
		Sessions->FindRooms();
		break;
	case ERRTitleChoice::Room:
		PickedRoom = GetRoomOf(Selected);
		if (PickedRoom == INDEX_NONE || !IsButtonEnabled(Selected))
		{
			break;
		}
		bRoomPage = false;
		OpenSpeciesPage(EStart::Join);
		break;
	case ERRTitleChoice::Fox:
	case ERRTitleChoice::Rabbit:
	case ERRTitleChoice::Squirrel:
	case ERRTitleChoice::Tiger:
	case ERRTitleChoice::Dragon:
	case ERRTitleChoice::RedFox:
		Sessions->SetSpecies(GetSpeciesOf(Choice));
		Start == EStart::Join ? Sessions->JoinRoom(PickedRoom) : Start == EStart::Host ? Sessions->HostGame() : Sessions->PlaySolo();
		break;
	case ERRTitleChoice::Back:
		if (bSpeciesPage)
		{
			// Back to the room list for a join, the stage for a defense, the rule for a battle
			bSpeciesPage = false;
			if (Start == EStart::Join)
			{
				OpenRoomPage();
				break;
			}
			if (Sessions->GetMatchRule() == ERRMatchRule::Defense)
			{
				OpenStagePage();
				break;
			}
			OpenRulePage();
			break;
		}
		if (bRulePage || bStagePage)
		{
			// Back to what to play
			const ERRTitleChoice Was = bStagePage ? ERRTitleChoice::Defense : ERRTitleChoice::Battle;
			bRulePage = false;
			bStagePage = false;
			bViewPage = true;
			Selected = GetChoices().IndexOfByKey(Was);
			break;
		}
		if (bRoomPage)
		{
			bRoomPage = false;
			Selected = GetChoices().IndexOfByKey(ERRTitleChoice::Join);
			break;
		}
		if (bMapPage)
		{
			// Back to the way to play
			bMapPage = false;
			Selected = GetChoices().IndexOfByKey(ERRTitleChoice::ThirdPerson);
			break;
		}
		if (bViewPage)
		{
			// Back to the page the game was started from
			bViewPage = false;
			Selected = GetChoices().IndexOfByKey(bViewForHost ? ERRTitleChoice::Host : ERRTitleChoice::Single);
			break;
		}
		bNetworkPage = false;
		Selected = GetChoices().IndexOfByKey(ERRTitleChoice::Network);
		break;
	case ERRTitleChoice::Quit:
		ConsoleCommand(TEXT("quit"));
		break;
	}
}

void ARRRandomTitlePlayerController::OpenMapPage()
{
	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	bMapPage = true;
	Selected = GetChoices().IndexOfByKey(Sessions && Sessions->GetThirdPersonMap() == ERRThirdPersonMap::DiceLand ? ERRTitleChoice::DiceLand : ERRTitleChoice::DiceTown);
}

void ARRRandomTitlePlayerController::OpenRulePage()
{
	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	bRulePage = true;
	Selected = GetChoices().IndexOfByKey(Sessions && Sessions->GetMatchRule() == ERRMatchRule::MeteorRush ? ERRTitleChoice::MeteorRush : ERRTitleChoice::Knockout);
}

void ARRRandomTitlePlayerController::OpenStagePage()
{
	// The last stage played if it is still open, else the furthest one open
	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	bStagePage = true;
	int32 Stage = Sessions ? Sessions->GetDefenseStage() : 1;
	if (Sessions && !Sessions->IsStageUnlocked(Stage))
	{
		Stage = FMath::Min(Sessions->GetClearedStage() + 1, URRRandomSessionSubsystem::DefenseStageCount);
	}
	Selected = FMath::Clamp(Stage - 1, 0, 2);
}

void ARRRandomTitlePlayerController::OpenRoomPage()
{
	// Looks for games as it opens; REFRESH looks again
	bRoomPage = true;
	PickedRoom = INDEX_NONE;
	if (URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>())
	{
		Sessions->FindRooms();
	}
	Selected = 0;
}

void ARRRandomTitlePlayerController::OpenSpeciesPage(EStart InStart)
{
	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	Start = InStart;
	bSpeciesPage = true;
	Selected = Sessions ? FMath::Clamp(Sessions->GetSpecies(), 0, RRSpecies::Count - 1) : 0;
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
	// The brawler page lays its cards out in a row: left and right move along it too
	MappingContext->MapKey(UpAction, EKeys::A);
	MappingContext->MapKey(UpAction, EKeys::Left);
	MappingContext->MapKey(DownAction, EKeys::D);
	MappingContext->MapKey(DownAction, EKeys::Right);
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
	else if (bNetworkPage || bViewPage || bSpeciesPage || bMapPage || bRulePage || bStagePage || bRoomPage)
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
