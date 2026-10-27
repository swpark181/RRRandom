#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RRRandomTitlePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/** A button on the title menu. */
enum class ERRTitleChoice : uint8
{
	Single,
	Network,
	Host,
	Join,
	Back,
	/** The two ways to play, picked after SINGLE PLAY or HOST GAME (joiners play the host's way). */
	TopView,
	ThirdPerson,
	Quit,
	/** The brawlers (RRSpecies order), picked last before the game starts. */
	Fox,
	Rabbit,
	Squirrel,
	Tiger,
	Dragon,
	RedFox,
	/** The third person maps, picked after THIRD PERSON (ERRThirdPersonMap). */
	DiceTown,
	DiceLand,
	/** The rules (ERRMatchRule), picked after the view (and map), before the brawler. */
	Knockout,
	MeteorRush,
	/** Round 90: what to play (in place of the view page: the top view and Dice Land are put away), the defense stages, the room list. */
	Battle,
	Defense,
	Stage1,
	Stage2,
	Stage3,
	/** One of the games found (the room list: GetRoomOf), and searching again. */
	Room,
	Refresh,
};

/**
 * Title menu: SINGLE PLAY or NETWORK (then HOST GAME / JOIN GAME), and QUIT. SINGLE PLAY and HOST GAME then ask how to play:
 * TOP VIEW (the camera above, fog of war) or THIRD PERSON (over the shoulder, aiming, no fog); joining takes the host's.
 * THIRD PERSON then asks which map: DICE TOWN (the big block town) or DICE LAND (the small island of dice). Then the rule:
 * KNOCKOUT (first to 10 knockouts) or METEOR RUSH (grab the ore meteors drop, hold 30). Last, every way in asks which brawler to play: FOX, RABBIT, SQUIRREL, TIGER or DRAGON (the bots get theirs at random).
 * W/S or the arrow keys pick, Enter or Space presses, Esc goes back (or cancels a host/join in progress); the mouse works too.
 * Starting a game goes through URRRandomSessionSubsystem, which opens the arena or joins a host.
 */
UCLASS()
class RRRANDOM_API ARRRandomTitlePlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARRRandomTitlePlayerController();

	/** The buttons on the page being shown, top to bottom. */
	TArray<ERRTitleChoice> GetChoices() const;

	int32 GetSelected() const { return Selected; }
	void SetSelected(int32 Index);

	bool IsOnNetworkPage() const { return bNetworkPage; }
	bool IsOnViewPage() const { return bViewPage; }
	bool IsOnSpeciesPage() const { return bSpeciesPage; }
	bool IsOnMapPage() const { return bMapPage; }
	bool IsOnRulePage() const { return bRulePage; }
	bool IsOnStagePage() const { return bStagePage; }
	bool IsOnRoomPage() const { return bRoomPage; }
	/** A button's words and its line under the buttons, as they stand (the rooms found, a locked stage). */
	FString GetButtonLabel(int32 Index) const;
	FString GetButtonDescription(int32 Index) const;
	/** A button that does nothing now (a locked stage, a full room). */
	bool IsButtonEnabled(int32 Index) const;
	/** The room (URRRandomSessionSubsystem::GetRooms) a button joins, or INDEX_NONE. */
	int32 GetRoomOf(int32 Index) const;
	/** The defense stage (1..3) a button picks, or 0. */
	static int32 GetStageOf(ERRTitleChoice Choice);
	/** The species a button picks (RRSpecies), or INDEX_NONE for the other buttons. */
	static int32 GetSpeciesOf(ERRTitleChoice Choice);

	/** True while a host or join is under way; the buttons wait for it. */
	bool IsBusy() const;

	void Choose(ERRTitleChoice Choice);

	static FString GetLabel(ERRTitleChoice Choice);
	static FString GetDescription(ERRTitleChoice Choice);

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	void CreateInputAssets();

	void OnUp();
	void OnDown();
	void OnConfirm();
	void OnBack();
	void OnClick();

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> UpAction;

	UPROPERTY()
	TObjectPtr<UInputAction> DownAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ConfirmAction;

	UPROPERTY()
	TObjectPtr<UInputAction> BackAction;

	UPROPERTY()
	TObjectPtr<UInputAction> ClickAction;

	bool bNetworkPage = false;
	/** The view page, and whether the game it starts is a host's (or a solo one). */
	bool bViewPage = false;
	bool bViewForHost = false;
	/** The brawler page, and what it starts: a solo game, a hosted one, or joining. */
	bool bSpeciesPage = false;
	/** The third person map page, between the view page and the brawler page. */
	bool bMapPage = false;
	/** Opens the map page with the last pick selected. */
	void OpenMapPage();
	/** The rule page, between the view (or map) page and the brawler page. */
	bool bRulePage = false;
	/** Opens the rule page with the last pick selected. */
	void OpenRulePage();
	/** Round 90: the defense stage page (after DEFENSE) and the room list (after JOIN GAME), and the room picked there. */
	bool bStagePage = false;
	bool bRoomPage = false;
	int32 PickedRoom = INDEX_NONE;
	void OpenStagePage();
	void OpenRoomPage();
	enum class EStart : uint8 { Solo, Host, Join } Start = EStart::Solo;
	/** Opens the brawler page with the last pick selected. */
	void OpenSpeciesPage(EStart InStart);
	int32 Selected = 0;
};
