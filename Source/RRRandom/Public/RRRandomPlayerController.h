#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RRRandomPlayerController.generated.h"

class ARRRandomFogOfWar;
class ARRRandomResultsDirector;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Top-down controls: WASD to walk, left mouse to shoot toward the cursor (hold for full auto), R to reload,
 * Space to jump (climbing onto cover when facing it), E to roll the stored buff dice.
 * Online play: F1 hosts a game, F2 joins one, F3 leaves (also the console commands RRHost, RRJoin, RRLeave).
 * M (or a click on the minimap) shows the map full screen, M, a click or Esc closes it; clicks on the map never shoot.
 * Before the start countdown ends (and after the match) nobody moves or shoots.
 * Esc opens the pause menu (RESUME / TITLE; a solo game stops meanwhile, an online one keeps running), Esc again resumes,
 * Enter goes to the title (RRTitle goes straight there).
 * When the match ends: the view blends to the results show (ARRRandomResultsDirector), gameplay input stops, and the
 * results screen's buttons take clicks: NEW GAME (or Enter, RRNewGame; the host only) and TITLE (or Esc, straight away).
 * Cheats outside shipping builds: F7 giant, F8 flight (F5 is the engine's shader complexity view), 1 to 6 a gun, F6 wins the
 * match for my team right away (also RRGiant, RRFly, RRGun <name or number>, RRWin).
 * Input assets are built in code when none are assigned, so the project runs without .uasset files.
 */
UCLASS()
class RRRANDOM_API ARRRandomPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARRRandomPlayerController();

	/** StarCraft-style fog of war on this player's screen (ARRRandomFogOfWar). */
	UPROPERTY(EditAnywhere, Category = "Fog")
	bool bFogOfWar = true;

	/** The capsule-center point a shot along this cursor ray should go for. */
	FVector GetAimTarget(const FVector& CursorOrigin, const FVector& CursorDirection) const;

	/** Hosts an online game (F1). */
	UFUNCTION(Exec)
	void RRHost();

	/** Joins an online game (F2). */
	UFUNCTION(Exec)
	void RRJoin();

	/** Leaves the online game and plays alone again (F3). */
	UFUNCTION(Exec)
	void RRLeave();

	/** Leaves any online game and goes back to the title screen (Esc). */
	UFUNCTION(Exec)
	void RRTitle();

	/** Starts a new match on the same map (Enter on the results screen). Only the host (or a solo game) can. */
	UFUNCTION(Exec)
	void RRNewGame();

	/** Opens or closes the pause menu (Esc). */
	UFUNCTION(Exec)
	void RRPause();

	/** Cheat: my team wins the match now, straight to the results screen (F6). Not in shipping builds. */
	UFUNCTION(Exec)
	void RRWin();

	bool IsPauseMenuOpen() const { return bPauseMenuOpen; }
	/** This screen's fog of war (null on a server's remote players, or with bFogOfWar off). */
	ARRRandomFogOfWar* GetFogOfWar() const { return FogOfWar; }

	/** Shows the map full screen or back in its corner (M). */
	UFUNCTION(Exec)
	void RRMap();

	/** Cheat: turns my brawler giant for CheatSeconds (F7). Not in shipping builds. */
	UFUNCTION(Exec)
	void RRGiant();

	/** Cheat: lets my brawler fly for CheatSeconds (F8). Not in shipping builds. */
	UFUNCTION(Exec)
	void RRFly();

	/**
	 * Cheat: hands my brawler a gun by name or number (keys 1 to 6): 1 PISTOL, 2 MINIGUN, 3 SMG, 4 RIFLE, 5 BLASTER,
	 * 6 CANNON, made as from a CheatGunQuality weapon die with no random spread. Not in shipping builds.
	 */
	UFUNCTION(Exec)
	void RRGun(const FString& Gun);

	/** Seconds of giant or flight a cheat gives. */
	UPROPERTY(EditAnywhere, Category = "Cheats", meta = (ClampMin = "1"))
	float CheatSeconds = 60.f;

	/** Weapon die face the gun cheat makes its guns as (1 to 6). */
	UPROPERTY(EditAnywhere, Category = "Cheats", meta = (ClampMin = "1", ClampMax = "6"))
	int32 CheatGunQuality = 6;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DiceAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> HostAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JoinAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LeaveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> TitleAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MapAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CheatGiantAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CheatFlyAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CheatWinAction;

	/** One per gun, in RRGun's numbering (keys 1 to 6). */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputAction>> CheatGunActions;

private:
	void CreateDefaultInputAssets();

	/** The cheats run on the server, which owns the brawlers' powers and guns. */
	UFUNCTION(Server, Reliable)
	void ServerCheatGiant();
	UFUNCTION(Server, Reliable)
	void ServerCheatFly();
	UFUNCTION(Server, Reliable)
	void ServerCheatGun(int32 Type);
	UFUNCTION(Server, Reliable)
	void ServerCheatWin();
	void OnCheatGun(int32 Type);

	/** Seconds the view takes to blend to the results show. */
	UPROPERTY(EditAnywhere, Category = "Results")
	float ResultsBlendTime = 1.6f;

	UPROPERTY(Transient)
	TObjectPtr<ARRRandomResultsDirector> ResultsDirector;

	UPROPERTY(Transient)
	TObjectPtr<ARRRandomFogOfWar> FogOfWar;

	void OnFire();
	/** A click on the results screen's buttons. */
	void OnClick();
	/** Esc: straight to the title on the results screen, the pause menu otherwise. */
	void OnEscape();
	/** Enter: the title from the pause menu, a new game on the results screen. */
	void OnConfirm();
	/** The pause menu is open or the match is over: no moving, shooting, dice or reloading. */
	bool IsGameplayInputBlocked() const;
	void ClosePauseMenu();

	bool bPauseMenuOpen = false;
	void OnMove(const FInputActionValue& Value);
	void OnJump();
	void OnStopJumping();
	void OnRollDice();
	void OnReload();
};
