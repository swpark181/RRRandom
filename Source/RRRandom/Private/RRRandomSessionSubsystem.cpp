#include "RRRandomSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
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
		SetStatus(TEXT("Joined the game. F3 leaves it."));
	}

	if (bStartupCommandDone)
	{
		return;
	}
	bStartupCommandDone = true;

	if (FParse::Param(FCommandLine::Get(), TEXT("RRHost")))
	{
		HostGame();
	}
	else if (FParse::Param(FCommandLine::Get(), TEXT("RRJoin")))
	{
		JoinGame(StartupJoinRetries);
	}
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
	Continue();
}

void URRRandomSessionSubsystem::LeaveGame()
{
	Pending = EPendingAction::None;
	JoinRetriesLeft = 0;
	GetGameInstance()->GetTimerManager().ClearTimer(RetryTimer);

	BindOnlineDelegates();
	IOnlineSessionPtr Sessions = GetSessions();
	if (Sessions && Sessions->GetNamedSession(NAME_GameSession))
	{
		SetStatus(TEXT("Leaving the game..."));
		bReturnToSoloAfterDestroy = true;
		Sessions->DestroySession(NAME_GameSession);
		return;
	}
	SetStatus(TEXT("Playing alone"));
	OpenArena(false);
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
		bReturnToSoloAfterDestroy = false;
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
	else if (Pending == EPendingAction::Join)
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
	Settings.NumPublicConnections = MaxPlayers;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bIsLANMatch = !IsUsingEOS();
	// A plain EOS session found by search; no presence or lobby needed
	Settings.bUsesPresence = false;
	Settings.bUseLobbiesIfAvailable = false;
	Settings.bAllowJoinViaPresence = false;
	Settings.Set(SETTING_MAPNAME, GetArenaMap(), EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(GameKey, GameValue, EOnlineDataAdvertisementType::ViaOnlineService);

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
	SetStatus(FString::Printf(TEXT("Hosting (%s). Others can join with F2."), IsUsingEOS() ? TEXT("EOS") : TEXT("LAN")));
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
	Fail(bWasSuccessful ? TEXT("No game found. Has someone pressed F1 to host?") : TEXT("Searching for games failed"));
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
	if (bReturnToSoloAfterDestroy)
	{
		bReturnToSoloAfterDestroy = false;
		SetStatus(TEXT("Playing alone"));
		OpenArena(false);
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
	return World ? UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) : FString();
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
