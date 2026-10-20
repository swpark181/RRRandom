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

	/** On the server: the species (RRSpecies) this player picked on its title, or INDEX_NONE before it has said. */
	int32 GetChosenSpecies() const { return ChosenSpecies; }

	/** StarCraft-style fog of war on this player's screen (ARRRandomFogOfWar). */
	UPROPERTY(EditAnywhere, Category = "Fog")
	bool bFogOfWar = true;

	/** The capsule-center point a shot along this cursor ray should go for. */
	FVector GetAimTarget(const FVector& CursorOrigin, const FVector& CursorDirection) const;

	/** Hosts an online game (F1). */
	/** Testing (not in Shipping): holds Shift's sprint on (1) or lets go (0), for scripts that can't hold a key. */
	UFUNCTION(Exec)
	void RRSprint(int32 bOn);

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

	/** Cheat: my brawler takes no damage and can't fall out of the arena, until toggled off (F10). Not in shipping builds. */
	UFUNCTION(Exec)
	void RRGod();

	/**
	 * Cheat: my brawler stores an ultimate die, to roll with E (U or F11); one that isn't the tiger turns into the tiger first.
	 * Not in shipping builds.
	 */
	UFUNCTION(Exec)
	void RRUltimate();

	/** Cheat: a dice meteor comes down 700 in front of my brawler, after its usual warning (console only). Not in shipping builds. */
	UFUNCTION(Exec)
	void RRMeteor();

	/** Cheat: one piece of each dice ore (shard, ore, crystal, golden) in a row in front of my brawler (console only). Not in shipping builds. */
	UFUNCTION(Exec)
	void RROre();

	/** Cheat: a piece of dice ore falls like a shooting star 500 in front of my brawler (console only). Not in shipping builds. */
	UFUNCTION(Exec)
	void RRStar();

	/** Cheat: my brawler takes this much damage, as from a fall (console only). Not in shipping builds. */
	UFUNCTION(Exec)
	void RRHurt(float Amount = 70.f);

	/**
	 * Cheat: my brawler's own die lands on this face right now (the fox's invisibility, the rabbit's super hops, the
	 * squirrel's shield, the tiger's giant, the dragon's flight; console only); 1 after the face: every brawler's, bots too. Not in shipping builds.
	 */
	UFUNCTION(Exec)
	void RROwnDie(int32 Face = 6, int32 bEveryone = 0);

	/** Rolls one stored die, as E does (console only, for scripted tests). */
	UFUNCTION(Exec)
	void RRRoll();

	/** Presses F: rides, gets off, combines the star robot, gets in or out of it (console only, for scripted tests). */
	UFUNCTION(Exec)
	void RRRide();

	/** Fires once at the nearest opponent, or straight ahead with none about (console only, for scripted tests). */
	UFUNCTION(Exec)
	void RRShoot();

	/**
	 * Cheat: the star team's three cars park in a ring in front of my animal friend, ready to combine (Ctrl + 0; anyone
	 * riding them is put off, and a robot already about comes apart first). Not in shipping builds.
	 */
	UFUNCTION(Exec)
	void RRStarCars();

	/**
	 * Cheat: smashes the structures round the spot the middle of my view points at (ARRRandomWreckage; console only): a ball
	 * this big, or with 1 after it a column through every height. Not in shipping builds.
	 */
	UFUNCTION(Exec)
	void RRWreck(float Radius = 300.f, int32 bColumn = 0);

	/**
	 * For animation work (the studio, -RRMap=Studio, though it works anywhere; console only): this screen watches my
	 * animal friend (the star robot while I'm aboard it) from a fixed side, following it: Yaw round from its front (0
	 * its face, 90 its right side, 180 its back, -90 its left), this far away and this high over its middle (0: a distance
	 * and height to suit it). The controls stay as they were. RRStudioCamOff goes back to the usual view.
	 */
	UFUNCTION(Exec)
	void RRStudioCam(float Yaw = 90.f, float Distance = 0.f, float Height = 0.f);
	UFUNCTION(Exec)
	void RRStudioCamOff();

	/** Cheat: the whole game runs at this speed (0.25 a quarter, 1 back to normal; console only). Not in shipping builds. */
	UFUNCTION(Exec)
	void RRSlomo(float Rate = 0.25f);

	/**
	 * Cheat: the star robot I'm aboard fires a rocket fist this far straight ahead of its upper body, as its ATTACK seat
	 * would (it needs someone in that seat; console only, for animation work). Not in shipping builds.
	 */
	UFUNCTION(Exec)
	void RRRobotFire(float Distance = 1500.f);

	/** The 0 key: with Ctrl held, RRStarCars. */
	void OnCheatStarCarsKey();

	/** While my tiger's spirit is out: the floor under the cursor, where a strike would land (false off the arena). */
	bool GetUltimateSpot(FVector& OutSpot) const;

	/**
	 * Cheat: third person, like a battle royale shooter (F9, again to go back): the camera over my brawler's right shoulder
	 * follows the mouse (the cursor hides), WASD moves the way the camera looks, the left button shoots at the crosshair in
	 * the middle of the screen, and holding the right button aims down the sights (closer, narrower view, slower mouse).
	 * The fog's shading is hidden meanwhile (it is drawn for the view from above); who can be seen stays the same.
	 */
	UFUNCTION(Exec)
	void RRThirdPerson();

	bool IsThirdPerson() const { return bThirdPerson; }

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

	/** F: get on or off a vehicle. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> RideAction;

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
	TObjectPtr<UInputAction> CheatGodAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CheatUltimateAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CheatThirdPersonAction;

	/** Third person: the mouse turns the camera, the right button aims down the sights. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	/** Held (Shift): run, while not shooting or in the sights. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	/** Third person: held, the cursor comes back and the mouse stops turning the camera. */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FreeCursorAction;

	/** Mouse turn per count, and the share of it left while aiming down the sights. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float LookSensitivity = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Input")
	float SightsSensitivity = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CheatWinAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> CheatStarCarsAction;

	/** One per gun, in RRGun's numbering (keys 1 to 6). */
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TArray<TObjectPtr<UInputAction>> CheatGunActions;

private:
	void CreateDefaultInputAssets();

	/** A joiner tells the host which brawler it picked; its brawler (there already, as the fox) changes to it. */
	UFUNCTION(Server, Reliable)
	void ServerChooseSpecies(uint8 Species);
	int32 ChosenSpecies = INDEX_NONE;

	/** The cheats run on the server, which owns the brawlers' powers and guns. */
	UFUNCTION(Server, Reliable)
	void ServerCheatGiant();
	UFUNCTION(Server, Reliable)
	void ServerCheatFly();
	UFUNCTION(Server, Reliable)
	void ServerCheatGod();
	UFUNCTION(Server, Reliable)
	void ServerCheatUltimate();
	UFUNCTION(Server, Reliable)
	void ServerCheatMeteor();
	UFUNCTION(Server, Reliable)
	void ServerCheatOre();
	UFUNCTION(Server, Reliable)
	void ServerCheatStar();
	UFUNCTION(Server, Reliable)
	void ServerCheatHurt(float Amount);
	UFUNCTION(Server, Reliable)
	void ServerCheatOwnDie(int32 Face, bool bEveryone);
	UFUNCTION(Server, Reliable)
	void ServerCheatStarCars();
	UFUNCTION(Server, Reliable)
	void ServerCheatWreck(FVector_NetQuantize Spot, float Radius, bool bColumn);
	UFUNCTION(Server, Reliable)
	void ServerCheatSlomo(float Rate);
	UFUNCTION(Server, Reliable)
	void ServerCheatRobotFire(float Distance);

	/** RRStudioCam: the camera (this screen's own), whether it's on, its angle, distance and height, and the yaw it follows. */
	void UpdateStudioCam(float DeltaTime);
	UPROPERTY(Transient)
	TObjectPtr<class ACameraActor> StudioCam;
	bool bStudioCam = false;
	float StudioYaw = 90.f;
	float StudioDistance = 0.f;
	float StudioHeight = 0.f;
	float StudioFacing = 0.f;

	/** My tiger's spirit strikes at this spot (if it's ready for one). */
	UFUNCTION(Server, Reliable)
	void ServerUltimateStrike(FVector_NetQuantize Spot);
	/** When this machine last asked, so a held button asks once per strike. */
	float LastStrikeRequestTime = -10.f;
	/** Whether the view is on my tiger's spirit (its overview camera). */
	bool bViewingSpirit = false;
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
	bool bThirdPerson = false;
	/** Over the shoulder (or back to the top view) on this screen: camera, mouse, cursor, the fog's shading. */
	void SetThirdPersonView(bool bOn);
	void OnLook(const FInputActionValue& Value);
	void OnAimStart();
	void OnAimStop();
	void OnSprintStart() { bSprintHeld = true; }
	void OnSprintStop() { bSprintHeld = false; }
	bool bSprintHeld = false;
	/** Every frame: tells my animal friend's movement whether to sprint (Shift held, not shooting or in the sights). */
	void UpdateSprint();
	void OnFreeCursorStart();
	void OnFreeCursorStop();
	/** The game's window is the one in use and its view has the keyboard and mouse. */
	bool IsViewportFocused() const;
	bool bCursorFreed = false;
	/** Third person: hold the mouse inside the game's view (and capture it), or let it go. */
	void LockMouse(bool bLock);
	/** Third person: put the hidden cursor back in the middle of the view if it has drifted. */
	void CenterMouse();
	/** Third person: where the crosshair (the middle of the screen) points in the world. */
	FVector GetCrosshairTarget() const;
	void OnMove(const FInputActionValue& Value);
	void OnJump();
	void OnStopJumping();
	void OnRollDice();
	void OnReload();
	void OnRide();
};
