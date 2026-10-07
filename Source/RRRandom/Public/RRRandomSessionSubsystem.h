#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RRRandomSessionSubsystem.generated.h"

class FOnlineSessionSearch;
class IOnlineSubsystem;
class UNetDriver;

/**
 * Online play without a dedicated server: one player hosts (their game becomes a listen server) and others find and join it.
 *
 * With Epic Online Services set up in DefaultEngine.ini, players log in to EOS, the host advertises an EOS session,
 * and everyone connects peer to peer through EOS (relayed when needed, so no port forwarding).
 * Without EOS credentials, or when started with -RRLan, it uses the engine's Null subsystem instead:
 * games are found by LAN broadcast and joined over plain IP, which is enough to test on one PC or a home network.
 *
 * Lives on the game instance so it survives the map reloads that hosting and joining cause.
 * Command line: -RRHost hosts and -RRJoin joins as soon as the arena loads; -RRDevAuth=Name picks the
 * EOS Dev Auth Tool credential, and -AUTH_TYPE/-AUTH_LOGIN/-AUTH_PASSWORD are passed through to EOS.
 */
UCLASS(Config = Game)
class RRRANDOM_API URRRandomSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Logs in if needed, creates a session and reloads the arena as its host. */
	void HostGame();

	/** Logs in if needed, finds a game and joins the first one. Retries a few times when nothing is found yet. */
	void JoinGame(int32 Retries = 0);

	/** Ends or leaves the current game and goes back to playing alone. */
	void LeaveGame();

	/** Called by the local player controller whenever an arena has loaded: updates the status, and the first time runs -RRHost or -RRJoin. */
	void OnArenaReady();

	/** What the last step did, for the HUD. */
	const FString& GetStatus() const { return Status; }

	bool IsBusy() const { return Pending != EPendingAction::None; }

	/** True when online play goes through Epic Online Services, false for LAN. */
	bool IsUsingEOS() const;

protected:
	/**
	 * EOS login: "developer" uses the EOS Dev Auth Tool (DevAuthHost + DevAuthCredential),
	 * "accountportal" opens the Epic account login, "persistentauth" reuses a saved login.
	 */
	UPROPERTY(Config)
	FString LoginType = TEXT("developer");

	UPROPERTY(Config)
	FString DevAuthHost = TEXT("localhost:6547");

	/** Credential name added in the Dev Auth Tool. A second game on the same PC needs another: -RRDevAuth=Name. */
	UPROPERTY(Config)
	FString DevAuthCredential = TEXT("Player1");

	/** Players in one game, the host included. */
	UPROPERTY(Config)
	int32 MaxPlayers = 6;

	/** How many times -RRJoin looks again when no game is found yet (the host may still be starting). */
	UPROPERTY(Config)
	int32 StartupJoinRetries = 10;

	/** Seconds between those searches. */
	UPROPERTY(Config)
	float JoinRetryDelay = 3.f;

private:
	enum class EPendingAction : uint8
	{
		None,
		Host,
		Join,
	};

	IOnlineSubsystem* GetOnline() const;
	TSharedPtr<IOnlineSession, ESPMode::ThreadSafe> GetSessions() const;
	void BindOnlineDelegates();
	void SetStatus(const FString& NewStatus);
	void Fail(const FString& Reason);

	/** Moves the pending host or join along: leaves an old session, logs in, then creates or searches. */
	void Continue();
	void Login();
	void CreateSession();
	void FindSessions();

	void OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error);
	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
	void OnDestroySessionComplete(FName SessionName, bool bWasSuccessful);
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);

	/** Reloads the arena map, as a listen server when hosting. */
	void OpenArena(bool bListen);
	FString GetArenaMap() const;

	EPendingAction Pending = EPendingAction::None;
	/** Set by LeaveGame: once the session is gone, go back to playing alone. */
	bool bReturnToSoloAfterDestroy = false;
	bool bDelegatesBound = false;
	bool bStartupCommandDone = false;
	int32 JoinRetriesLeft = 0;
	FString Status;
	TSharedPtr<FOnlineSessionSearch> Search;
	FTimerHandle RetryTimer;

	FDelegateHandle LoginHandle;
	FDelegateHandle CreateHandle;
	FDelegateHandle FindHandle;
	FDelegateHandle JoinHandle;
	FDelegateHandle DestroyHandle;
	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
