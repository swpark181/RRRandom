#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "RRRandomSessionSubsystem.generated.h"

class FOnlineSessionSearch;
class IOnlineSubsystem;
class UNetDriver;

/** The third person mode's maps. */
enum class ERRThirdPersonMap : uint8
{
	/** The round pastel island of dice (ARRRandomDiceLand). */
	DiceLand,
	/** The big block town (ARRRandomDiceTown). */
	DiceTown,
	/** A bare floor for animation work (ARRRandomStudio): -RRMap=Studio only, not on the title. */
	Studio,
};

/** How a match is won. */
enum class ERRMatchRule : uint8
{
	/** The first team to ScoreToWin knockouts. */
	Knockout,
	/** Meteors drop dice ore; a team holding 30 points of it for 12 seconds wins (ARRRandomMeteorRushDirector). */
	MeteorRush,
};

/**
 * Online play without a dedicated server: one player hosts (their game becomes a listen server) and others find and join it.
 *
 * With Epic Online Services set up in DefaultEngine.ini, players log in to EOS, the host advertises an EOS session,
 * and everyone connects peer to peer through EOS (relayed when needed, so no port forwarding).
 * Without EOS credentials, or when started with -RRLan, it uses the engine's Null subsystem instead:
 * games are found by LAN broadcast and joined over plain IP, which is enough to test on one PC or a home network.
 *
 * Lives on the game instance so it survives the map reloads that hosting and joining cause.
 * The game starts on the title screen (TitleMap); playing alone or hosting from there opens ArenaMap.
 * Command line: -RRSolo plays alone, -RRHost hosts and -RRJoin joins as soon as the game starts; -RRDevAuth=Name picks the
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

	/** Ends or leaves any online game and plays alone in the arena (the title's SINGLE PLAY, F3 in the arena). */
	void PlaySolo();

	/** Ends or leaves any online game and goes back to the title screen. */
	void ReturnToTitle();

	/** Stops a host or join that is still logging in or searching. */
	void CancelPending();

	/** Called by the local player controller whenever an arena has loaded: updates the status, and the first time runs -RRSolo, -RRHost or -RRJoin. */
	void OnArenaReady();

	/** Called by the title screen's player controller once it is up; the first time runs -RRSolo, -RRHost or -RRJoin. */
	void OnTitleReady();

	/** True while the title map is loaded. */
	bool IsOnTitle() const;

	/** What the last step did, for the HUD. */
	const FString& GetStatus() const { return Status; }

	bool IsBusy() const { return Pending != EPendingAction::None; }

	/**
	 * How the next games this machine starts (solo or hosted) are played: third person (over the shoulder, no fog of war) or
	 * the top view. Picked on the title; kept across levels, so new games and F1 hosting keep it. -RRThirdPerson on the
	 * command line sets it too. Joiners play the host's way (ARRRandomGameState::IsThirdPersonMode).
	 */
	void SetThirdPersonMode(bool bOn) { bThirdPersonMode = bOn; }
	bool IsThirdPersonMode() const { return bThirdPersonMode; }

	/**
	 * The map the third person games this machine starts are played on, picked on the title after THIRD PERSON and kept
	 * across levels. -RRMap=DiceLand or -RRMap=DiceTown on the command line sets it too. Joiners get the host's (its map
	 * actor replicates).
	 */
	void SetThirdPersonMap(ERRThirdPersonMap InMap) { ThirdPersonMap = InMap; }
	ERRThirdPersonMap GetThirdPersonMap() const { return ThirdPersonMap; }

	/**
	 * The rule the games this machine starts are played by, picked on the title after the view (and map) and kept across
	 * levels. -RRRule=MeteorRush on the command line sets it too. Joiners play the host's (ARRRandomGameState::IsMeteorRush).
	 */
	void SetMatchRule(ERRMatchRule InRule) { MatchRule = InRule; }
	ERRMatchRule GetMatchRule() const { return MatchRule; }

	/**
	 * The brawler this machine's player plays (RRSpecies: fox, rabbit, squirrel, tiger), picked on the title and kept across
	 * levels. -RRSpecies=<name> on the command line sets it too. Sent to the host by ARRRandomPlayerController.
	 */
	void SetSpecies(int32 InSpecies) { Species = InSpecies; }
	int32 GetSpecies() const { return Species; }

	/** True when online play goes through Epic Online Services, false for LAN. */
	bool IsUsingEOS() const;

protected:
	/** The title screen's map. Its name picks the title game mode (GameModeMapPrefixes in DefaultEngine.ini). */
	UPROPERTY(Config)
	FString TitleMap = TEXT("/Engine/Maps/Entry");

	/** The map played on when starting from the title. Hosting from inside an arena keeps that arena. */
	UPROPERTY(Config)
	FString ArenaMap = TEXT("/Engine/Maps/Templates/Template_Default");

	/**
	 * The third person maps' levels (char-pipeline/scripts/ue_make_tps_levels.py): each has its map actor placed and a
	 * NavMeshBoundsVolume for the bots' routes. One that doesn't exist falls back to ArenaMap, where the game mode builds the
	 * map in code as before.
	 */
	UPROPERTY(Config)
	FString DiceTownMap = TEXT("/Game/Maps/DiceTown");
	UPROPERTY(Config)
	FString DiceLandMap = TEXT("/Game/Maps/DiceLand");
	UPROPERTY(Config)
	FString StudioMap = TEXT("/Game/Maps/Studio");

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

	/** Where to go once the current session is gone. */
	enum class EAfterLeave : uint8
	{
		Nothing,
		PlaySolo,
		Title,
	};

	IOnlineSubsystem* GetOnline() const;
	TSharedPtr<IOnlineSession, ESPMode::ThreadSafe> GetSessions() const;
	void BindOnlineDelegates();
	void SetStatus(const FString& NewStatus);
	void Fail(const FString& Reason);
	/** Forgets a pending host or join without a status message. */
	void StopPending();
	/** Runs -RRSolo, -RRHost or -RRJoin the first time a map is ready. */
	void RunStartupCommand();
	/** Leaves the session (if any), then goes on to Next. */
	void LeaveThen(EAfterLeave Next);
	void FinishLeaving(EAfterLeave Next);

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

	/** Opens (or reloads) the arena map, as a listen server when hosting. */
	void OpenArena(bool bListen);
	void OpenTitle();
	/** The arena currently loaded, or when on the title the one picked there (GetStartMap). */
	FString GetArenaMap() const;
	/** The level a game started from the title opens: the picked third person map's level, else ArenaMap. */
	FString GetStartMap() const;

	EPendingAction Pending = EPendingAction::None;
	/** Set by PlaySolo/ReturnToTitle while the session is being destroyed. */
	EAfterLeave AfterLeave = EAfterLeave::Nothing;
	bool bDelegatesBound = false;
	bool bStartupCommandDone = false;
	bool bThirdPersonMode = false;
	ERRThirdPersonMap ThirdPersonMap = ERRThirdPersonMap::DiceTown;
	ERRMatchRule MatchRule = ERRMatchRule::Knockout;
	int32 Species = 0;
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
