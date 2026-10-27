#include "RRRandomSessionSubsystem.h"
#include "RRRandomSaveGame.h"
#include "RRRandomSpecies.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemNames.h"
#include "OnlineSubsystemUtils.h"
#include "TimerManager.h"

namespace
{
	// Advertised with every session and searched for, so only RRRandom games show up
	const FName GameKey(TEXT("RRGAME"));
	const FString GameValue(TEXT("RRRandom"));
	// What the room list shows of a game: its rule (ERRMatchRule) and defense stage
	const FName RuleKey(TEXT("RRRULE"));
	const FName StageKey(TEXT("RRSTAGE"));
	constexpr int32 MaxSearchResults = 20;
	constexpr int32 LocalUser = 0;
}

void URRRandomSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &URRRandomSessionSubsystem::OnNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &URRRandomSessionSubsystem::OnTravelFailure);
	}
}

void URRRandomSessionSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	if (bDelegatesBound)
	{
		if (IOnlineSubsystem* Online = GetOnline())
		{
			if (IOnlineIdentityPtr Identity = Online->GetIdentityInterface())
			{
				Identity->ClearOnLoginCompleteDelegate_Handle(LocalUser, LoginHandle);
			}
		}
		if (IOnlineSessionPtr Sessions = GetSessions())
		{
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
			Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroyHandle);
		}
	}
	Super::Deinitialize();
}

IOnlineSubsystem* URRRandomSessionSubsystem::GetOnline() const
{
	// The default service is EOS; the engine falls back to Null by itself when EOS can't start (no credentials)
	const FName Service = FParse::Param(FCommandLine::Get(), TEXT("RRLan")) ? NULL_SUBSYSTEM : NAME_None;
	return Online::GetSubsystem(GetGameInstance()->GetWorld(), Service);
}

IOnlineSessionPtr URRRandomSessionSubsystem::GetSessions() const
{
	IOnlineSubsystem* Online = GetOnline();
	return Online ? Online->GetSessionInterface() : nullptr;
}

bool URRRandomSessionSubsystem::IsUsingEOS() const
{
	const IOnlineSubsystem* Online = GetOnline();
	return Online && Online->GetSubsystemName() == EOS_SUBSYSTEM;
}

void URRRandomSessionSubsystem::BindOnlineDelegates()
{
	if (bDelegatesBound)
	{
		return;
	}
	IOnlineSubsystem* Online = GetOnline();
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Online || !Sessions)
	{
		return;
	}
	if (IOnlineIdentityPtr Identity = Online->GetIdentityInterface())
	{
		LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(LocalUser, FOnLoginCompleteDelegate::CreateUObject(this, &URRRandomSessionSubsystem::OnLoginComplete));
	}
	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateUObject(this, &URRRandomSessionSubsystem::OnCreateSessionComplete));
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateUObject(this, &URRRandomSessionSubsystem::OnFindSessionsComplete));
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateUObject(this, &URRRandomSessionSubsystem::OnJoinSessionComplete));
	DestroyHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateUObject(this, &URRRandomSessionSubsystem::OnDestroySessionComplete));
	bDelegatesBound = true;
}

void URRRandomSessionSubsystem::SetStatus(const FString& NewStatus)
{
	Status = NewStatus;
	UE_LOG(LogTemp, Log, TEXT("RRRandom online: %s"), *Status);
}

void URRRandomSessionSubsystem::Fail(const FString& Reason)
{
	Pending = EPendingAction::None;
	JoinRetriesLeft = 0;
	SetStatus(Reason);
}

void URRRandomSessionSubsystem::OnArenaReady()
{
	const UWorld* World = GetGameInstance()->GetWorld();
	if (World && World->GetNetMode() == NM_Client)
	{
		SetStatus(TEXT("Joined the game. F3 plays alone, Esc goes to the title."));
	}
	RunStartupCommand();
}

void URRRandomSessionSubsystem::OnTitleReady()
{
	RunStartupCommand();
}

void URRRandomSessionSubsystem::RunStartupCommand()
{
	if (bStartupCommandDone)
	{
		return;
	}
	bStartupCommandDone = true;
	if (FParse::Param(FCommandLine::Get(), TEXT("RRThirdPerson")))
	{
		bThirdPersonMode = true;
	}
	FString MapName;
	if (FParse::Value(FCommandLine::Get(), TEXT("RRMap="), MapName))
	{
		ThirdPersonMap = MapName.Equals(TEXT("DiceLand"), ESearchCase::IgnoreCase) ? ERRThirdPersonMap::DiceLand
			: MapName.Equals(TEXT("Studio"), ESearchCase::IgnoreCase) ? ERRThirdPersonMap::Studio : ERRThirdPersonMap::DiceTown;
		// The studio is a third person map only
		bThirdPersonMode |= ThirdPersonMap == ERRThirdPersonMap::Studio;
	}
	FString RuleName;
	if (FParse::Value(FCommandLine::Get(), TEXT("RRRule="), RuleName))
	{
		MatchRule = RuleName.Equals(TEXT("MeteorRush"), ESearchCase::IgnoreCase) ? ERRMatchRule::MeteorRush
			: RuleName.Equals(TEXT("Defense"), ESearchCase::IgnoreCase) ? ERRMatchRule::Defense : ERRMatchRule::Knockout;
	}
	// -RRStage=N: that defense stage, open or not (tests)
	int32 StageNumber = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("RRStage="), StageNumber))
	{
		SetDefenseStage(StageNumber);
	}
	// Defense is played third person, on its own stages
	bThirdPersonMode |= MatchRule == ERRMatchRule::Defense;
	FString SpeciesName;
	if (FParse::Value(FCommandLine::Get(), TEXT("RRSpecies="), SpeciesName) && RRSpecies::FromName(SpeciesName) != INDEX_NONE)
	{
		Species = RRSpecies::FromName(SpeciesName);
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("RRHost")))
	{
		HostGame();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("RRJoin")))
	{
		JoinGame(StartupJoinRetries);
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("RRSolo")) && IsOnTitle())
	{
		PlaySolo();
	}
}

bool URRRandomSessionSubsystem::IsOnTitle() const
{
	const UWorld* World = GetGameInstance()->GetWorld();
	return World && UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == TitleMap;
}

void URRRandomSessionSubsystem::HostGame()
{
	if (IsBusy())
	{
		return;
	}
	Pending = EPendingAction::Host;
	Continue();
}

void URRRandomSessionSubsystem::JoinGame(int32 Retries)
{
	if (IsBusy())
	{
		return;
	}
	Pending = EPendingAction::Join;
	JoinRetriesLeft = Retries;
	RoomToJoin = INDEX_NONE;
	bJoinFromRooms = false;
	Continue();
}

void URRRandomSessionSubsystem::FindRooms()
{
	if (IsBusy())
	{
		return;
	}
	Pending = EPendingAction::ListRooms;
	bRoomsFound = false;
	Continue();
}

void URRRandomSessionSubsystem::JoinRoom(int32 Index)
{
	if (IsBusy() || !Rooms.IsValidIndex(Index) || !Search.IsValid() || !Search->SearchResults.IsValidIndex(Rooms[Index].SearchIndex))
	{
		Fail(TEXT("That game is gone. Refresh the list."));
		return;
	}
	Pending = EPendingAction::Join;
	JoinRetriesLeft = 0;
	RoomToJoin = Rooms[Index].SearchIndex;
	bJoinFromRooms = true;
	Continue();
}

int32 URRRandomSessionSubsystem::GetClearedStage() const
{
	if (ClearedStage == INDEX_NONE)
	{
		const URRRandomSaveGame* Saved = Cast<URRRandomSaveGame>(UGameplayStatics::LoadGameFromSlot(URRRandomSaveGame::SlotName, 0));
		ClearedStage = Saved ? FMath::Clamp(Saved->ClearedStage, 0, DefenseStageCount) : 0;
	}
	return ClearedStage;
}

bool URRRandomSessionSubsystem::RecordStageCleared(int32 Stage)
{
	// Joining someone's game, a stage counts only when this PC had it open (it cleared the one before)
	if (!IsStageUnlocked(Stage) || Stage <= GetClearedStage())
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: defense stage %d cleared; progress stays at %d."), Stage, GetClearedStage());
		return false;
	}
	ClearedStage = Stage;
	SaveProgress();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: defense stage %d cleared and saved; stage %d is open."), Stage, FMath::Min(Stage + 1, DefenseStageCount));
	return Stage < DefenseStageCount;
}

void URRRandomSessionSubsystem::UnlockAllStages()
{
	ClearedStage = DefenseStageCount;
	SaveProgress();
}

void URRRandomSessionSubsystem::SaveProgress() const
{
	URRRandomSaveGame* Save = Cast<URRRandomSaveGame>(UGameplayStatics::CreateSaveGameObject(URRRandomSaveGame::StaticClass()));
	Save->ClearedStage = GetClearedStage();
	UGameplayStatics::SaveGameToSlot(Save, URRRandomSaveGame::SlotName, 0);
}

void URRRandomSessionSubsystem::PlaySolo()
{
	LeaveThen(EAfterLeave::PlaySolo);
}

void URRRandomSessionSubsystem::ReturnToTitle()
{
	LeaveThen(EAfterLeave::Title);
}

void URRRandomSessionSubsystem::CancelPending()
{
	if (!IsBusy())
	{
		return;
	}
	// Callbacks still on their way are ignored once nothing is pending; a session they leave behind is destroyed by the next host or join
	StopPending();
	SetStatus(TEXT("Cancelled"));
}

void URRRandomSessionSubsystem::StopPending()
{
	Pending = EPendingAction::None;
	JoinRetriesLeft = 0;
	GetGameInstance()->GetTimerManager().ClearTimer(RetryTimer);
}

void URRRandomSessionSubsystem::LeaveThen(EAfterLeave Next)
{
	StopPending();

	BindOnlineDelegates();
	IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions && Sessions->GetNamedSession(NAME_GameSession))
	{
		SetStatus(TEXT("Leaving the game..."));
		AfterLeave = Next;
		Sessions->DestroySession(NAME_GameSession);
		return;
	}
	FinishLeaving(Next);
}

void URRRandomSessionSubsystem::FinishLeaving(EAfterLeave Next)
{
	if (Next == EAfterLeave::PlaySolo)
	{
		SetStatus(TEXT("Playing alone"));
		OpenArena(false);
	}
	else if (Next == EAfterLeave::Title)
	{
		Status.Reset();
		OpenTitle();
	}
}

void URRRandomSessionSubsystem::Continue()
{
	BindOnlineDelegates();
	IOnlineSubsystem* Online = GetOnline();
	IOnlineSessionPtr Sessions = GetSessions();
	if (!Online || !Sessions)
	{
		Fail(TEXT("No online subsystem available"));
		return;
	}

	// One game at a time: leave the old session first, then come back here
	if (Sessions->GetNamedSession(NAME_GameSession))
	{
		SetStatus(TEXT("Leaving the previous game..."));
		AfterLeave = EAfterLeave::Nothing;
		Sessions->DestroySession(NAME_GameSession);
		return;
	}

	// EOS needs a logged-in user to host, search and connect; LAN doesn't
	if (IsUsingEOS())
	{
		IOnlineIdentityPtr Identity = Online->GetIdentityInterface();
		if (Identity && Identity->GetLoginStatus(LocalUser) != ELoginStatus::LoggedIn)
		{
			Login();
			return;
		}
	}

	if (Pending == EPendingAction::Host)
	{
		CreateSession();
	}
	else if (Pending == EPendingAction::Join && bJoinFromRooms)
	{
		// A game picked from the room list: straight in
		const FOnlineSessionSearchResult& Result = Search->SearchResults[RoomToJoin];
		SetStatus(FString::Printf(TEXT("Joining %s's game..."), *Result.Session.OwningUserName));
		GetSessions()->JoinSession(LocalUser, NAME_GameSession, Result);
	}
	else if (Pending == EPendingAction::Join || Pending == EPendingAction::ListRooms)
	{
		FindSessions();
	}
}

void URRRandomSessionSubsystem::Login()
{
	IOnlineIdentityPtr Identity = GetOnline()->GetIdentityInterface();
	SetStatus(TEXT("Logging in to EOS..."));

	// Credentials given on the command line (-AUTH_TYPE=...) win
	FString CommandLineType;
	if (FParse::Value(FCommandLine::Get(), TEXT("AUTH_TYPE="), CommandLineType))
	{
		Identity->AutoLogin(LocalUser);
		return;
	}

	FOnlineAccountCredentials Credentials;
	Credentials.Type = LoginType;
	if (LoginType == TEXT("developer"))
	{
		FString Credential = DevAuthCredential;
		FParse::Value(FCommandLine::Get(), TEXT("RRDevAuth="), Credential);
		Credentials.Id = DevAuthHost;
		Credentials.Token = Credential;
	}
	Identity->Login(LocalUser, Credentials);
}

void URRRandomSessionSubsystem::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (Pending == EPendingAction::None)
	{
		return;
	}
	if (!bWasSuccessful)
	{
		Fail(FString::Printf(TEXT("EOS login failed (%s). Is the Dev Auth Tool running with credential '%s'?"), *Error, *DevAuthCredential));
		return;
	}
	SetStatus(TEXT("Logged in to EOS"));
	Continue();
}

void URRRandomSessionSubsystem::CreateSession()
{
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = GetMaxPlayersFor(MatchRule);
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = !IsUsingEOS();
	// A plain EOS session found by search; no presence or lobby needed
	Settings.bUsesPresence = false;
	Settings.bUseLobbiesIfAvailable = false;
	Settings.bAllowJoinViaPresence = false;
	Settings.Set(SETTING_MAPNAME, GetArenaMap(), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(GameKey, GameValue, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(RuleKey, static_cast<int32>(MatchRule), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(StageKey, MatchRule == ERRMatchRule::Defense ? DefenseStage : 0, EOnlineDataAdvertisementType::ViaOnlineService);

	SetStatus(TEXT("Creating a game..."));
	if (!GetSessions()->CreateSession(LocalUser, NAME_GameSession, Settings))
	{
		Fail(TEXT("Could not create a game"));
	}
}

void URRRandomSessionSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (Pending != EPendingAction::Host)
	{
		return;
	}
	if (!bWasSuccessful)
	{
		Fail(TEXT("Could not create a game"));
		return;
	}
	Pending = EPendingAction::None;
	SetStatus(FString::Printf(TEXT("Hosting (%s). Others can join now."), IsUsingEOS() ? TEXT("EOS") : TEXT("LAN")));
	OpenArena(true);
}

void URRRandomSessionSubsystem::FindSessions()
{
	Search = MakeShared<FOnlineSessionSearch>();
	Search->MaxSearchResults = MaxSearchResults;
	Search->bIsLanQuery = !IsUsingEOS();
	Search->QuerySettings.Set(GameKey, GameValue, EOnlineComparisonOp::Equals);

	SetStatus(TEXT("Looking for a game..."));
	if (!GetSessions()->FindSessions(LocalUser, Search.ToSharedRef()))
	{
		Fail(TEXT("Could not search for games"));
	}
}

void URRRandomSessionSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (Pending == EPendingAction::ListRooms && Search.IsValid())
	{
		// The room list: every RRRandom game found, open or full, with what it plays
		Rooms.Reset();
		for (int32 Index = 0; Index < Search->SearchResults.Num(); ++Index)
		{
			const FOnlineSessionSearchResult& Result = Search->SearchResults[Index];
			if (!Result.IsValid())
			{
				continue;
			}
			FRRRoomInfo& Room = Rooms.AddDefaulted_GetRef();
			Room.HostName = Result.Session.OwningUserName;
			Room.MaxPlayers = Result.Session.SessionSettings.NumPublicConnections;
			Room.Players = FMath::Max(1, Room.MaxPlayers - Result.Session.NumOpenPublicConnections);
			int32 Rule = 0;
			Result.Session.SessionSettings.Get(RuleKey, Rule);
			Room.Rule = static_cast<ERRMatchRule>(FMath::Clamp(Rule, 0, static_cast<int32>(ERRMatchRule::Defense)));
			Result.Session.SessionSettings.Get(StageKey, Room.Stage);
			Room.SearchIndex = Index;
		}
		Pending = EPendingAction::None;
		bRoomsFound = true;
		SetStatus(Rooms.Num() > 0 ? FString::Printf(TEXT("%d game%s found"), Rooms.Num(), Rooms.Num() == 1 ? TEXT("") : TEXT("s"))
			: bWasSuccessful ? TEXT("No game found. Has someone hosted one?") : TEXT("Searching for games failed"));
		return;
	}
	if (Pending != EPendingAction::Join || !Search.IsValid())
	{
		return;
	}

	for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
	{
		if (Result.IsValid() && Result.Session.NumOpenPublicConnections > 0)
		{
			SetStatus(FString::Printf(TEXT("Joining %s's game..."), *Result.Session.OwningUserName));
			GetSessions()->JoinSession(LocalUser, NAME_GameSession, Result);
			return;
		}
	}

	// The host may still be starting up
	if (JoinRetriesLeft > 0)
	{
		--JoinRetriesLeft;
		SetStatus(TEXT("No game found yet, looking again..."));
		GetGameInstance()->GetTimerManager().SetTimer(RetryTimer, FTimerDelegate::CreateUObject(this, &URRRandomSessionSubsystem::FindSessions), JoinRetryDelay, false);
		return;
	}
	Fail(bWasSuccessful ? TEXT("No game found. Has someone hosted one?") : TEXT("Searching for games failed"));
}

void URRRandomSessionSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (Pending != EPendingAction::Join)
	{
		return;
	}
	FString Url;
	if (Result != EOnJoinSessionCompleteResult::Success || !GetSessions()->GetResolvedConnectString(NAME_GameSession, Url))
	{
		Fail(FString::Printf(TEXT("Could not join (%s)"), LexToString(Result)));
		return;
	}

	Pending = EPendingAction::None;
	SetStatus(FString::Printf(TEXT("Connecting to %s"), *Url));
	if (APlayerController* Controller = GetGameInstance()->GetFirstLocalPlayerController())
	{
		Controller->ClientTravel(Url, TRAVEL_Absolute);
	}
}

void URRRandomSessionSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (AfterLeave != EAfterLeave::Nothing)
	{
		const EAfterLeave Next = AfterLeave;
		AfterLeave = EAfterLeave::Nothing;
		FinishLeaving(Next);
		return;
	}
	if (Pending != EPendingAction::None)
	{
		Continue();
	}
}

void URRRandomSessionSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error)
{
	// The host losing one player isn't a failure of its game
	if (NetDriver && !NetDriver->ServerConnection)
	{
		return;
	}

	// The engine goes back to the default map by itself; forget the session so the next host or join starts clean
	Fail(FString::Printf(TEXT("Disconnected: %s"), *Error));
	if (IOnlineSessionPtr Sessions = GetSessions(); Sessions && Sessions->GetNamedSession(NAME_GameSession))
	{
		Sessions->DestroySession(NAME_GameSession);
	}
}

void URRRandomSessionSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	Fail(FString::Printf(TEXT("Could not connect: %s"), *Error));
	if (IOnlineSessionPtr Sessions = GetSessions(); Sessions && Sessions->GetNamedSession(NAME_GameSession))
	{
		Sessions->DestroySession(NAME_GameSession);
	}
}

FString URRRandomSessionSubsystem::GetArenaMap() const
{
	const UWorld* World = GetGameInstance()->GetWorld();
	return World && !IsOnTitle() ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : GetStartMap();
}

FString URRRandomSessionSubsystem::GetStartMap() const
{
	// A defense stage is built in code on the bare arena (ARRRandomDiceTown with its stage, from the game mode)
	if (MatchRule == ERRMatchRule::Defense)
	{
		return ArenaMap;
	}
	if (bThirdPersonMode)
	{
		const FString& Level = ThirdPersonMap == ERRThirdPersonMap::DiceLand ? DiceLandMap
			: ThirdPersonMap == ERRThirdPersonMap::Studio ? StudioMap : DiceTownMap;
		if (!Level.IsEmpty() && FPackageName::DoesPackageExist(Level))
		{
			return Level;
		}
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no level %s, the map is built on %s instead."), *Level, *ArenaMap);
	}
	return ArenaMap;
}

void URRRandomSessionSubsystem::OpenTitle()
{
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*TitleMap));
}

void URRRandomSessionSubsystem::OpenArena(bool bListen)
{
	// EOS listens on its peer-to-peer sockets; LAN on a normal IP port
	FString Options;
	if (bListen)
	{
		Options = IsUsingEOS() ? TEXT("listen") : TEXT("listen?bUseIPSockets");
	}
	UGameplayStatics::OpenLevel(GetGameInstance(), FName(*GetArenaMap()), true, Options);
}
