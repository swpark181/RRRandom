#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RRRandomHUD.generated.h"

/** The results screen's and the pause menu's buttons. */
enum class ERRResultsButton : uint8
{
	None,
	NewGame,
	Title,
	Resume,
};

class ARRRandomCharacter;
class ARRRandomGameState;
class UFont;
class URRRandomDiceBuffComponent;
class UTexture2D;

/**
 * Canvas HUD in the pastel style of RRRandomUIStyle (airy text with a white halo and thin rounded bars over the game;
 * frosted rounded panels on the results and pause screens). Over every brawler: health bar, stored dice count, dice gauge, active buffs,
 * the latest dice roll, damage numbers and a friend-or-foe marker (plus ammo over the player's own brawler).
 * On screen: team score, the player's dice gauge and buff list, magazine count, a respawn countdown, the online status
 * with its keys under the minimap, and:
 * - a 5-4-3-2-1-GO countdown in the middle until play starts,
 * - top left, a minimap of the island (terrain, cover, woods) darkened where the team has never looked or doesn't look
 *   now, with markers for the player (an arrow), teammates and the opponents the team can see; a click on it (or M)
 *   shows it full screen,
 * - bottom right over the ammo, a card per teammate: portrait in the team color, name, health and what it has going
 *   (dice, giant, flight, gun, reloading; greyed out while knocked out).
 * Once the match is over, only the results screen: VICTORY or DEFEAT, the final score, each team's brawlers by
 * knockouts, the MVP, and NEW GAME / TITLE buttons, over letterbox bars (the 3D show is ARRRandomResultsDirector's).
 * With the pause menu open (Esc): a dimmed screen with RESUME / TITLE on top of the game.
 */
UCLASS()
class RRRANDOM_API ARRRandomHUD : public AHUD
{
	GENERATED_BODY()

public:
	ARRRandomHUD();

	virtual void DrawHUD() override;

	/** A click on the map: opens it full screen from the corner, or closes it. False when the click wasn't the map's. */
	bool HandleMapClick(const FVector2D& ScreenPoint);
	/** Whether a screen point belongs to the map (on the corner map, or anywhere while it fills the screen). */
	bool IsPointOnMap(const FVector2D& ScreenPoint) const;
	void ToggleMap() { bMapExpanded = !bMapExpanded; }
	bool IsMapExpanded() const { return bMapExpanded; }

	/** The corner map's diameter and its distance from the screen edges, on a 720 pixel tall screen (scaled with it). */
	static constexpr float MinimapSize = 190.f;
	static constexpr float MinimapMargin = 20.f;

	/** The enabled results or pause menu button under a screen point (None when there is none). */
	ERRResultsButton GetResultsButtonAt(const FVector2D& ScreenPoint) const;

	/**
	 * A comic word popping up over a spot in the world on this machine's screen (BOOM!, THUD!, POOF!), drawn like the
	 * meteor's: big, settling, drifting up and fading in a white starburst. Cosmetic: every machine pops its own.
	 */
	static void PopComicWord(const UObject* WorldContext, const FString& Text, const FVector& Where, const FLinearColor& Color, float Size = 1.f);

private:
	/** One comic word: on its burst at the time it popped (this machine's clock), its color, size and the burst's tilt. */
	void DrawComicWord(const FString& Text, const FVector& Where, float Start, const FLinearColor& Color, float Size, float Tilt);
	struct FComicWord
	{
		FString Text;
		FVector Where = FVector::ZeroVector;
		float Start = 0.f;
		FLinearColor Color = FLinearColor::White;
		float Size = 1.f;
		float Tilt = 0.f;
	};
	TArray<FComicWord> ComicWords;
	void DrawComicWords();
	/**
	 * The human cannons and plane stations (round 88): a pulsing ring round each cannon's pad where it can be seen, and the
	 * station's state over it (charging up, its plane out).
	 */
	void DrawCannonsAndStations(const ARRRandomCharacter* Viewer);
	/** Flying a plane: its name and hull over the ammo panel, and the keys for up, down and out. */
	void DrawPlanePanel(const ARRRandomCharacter& Viewer);
	void DrawOverhead(const ARRRandomCharacter& Brawler, const ARRRandomCharacter* Viewer);
	void DrawAmmoBar(const ARRRandomCharacter& Brawler, float Left, float Top, float Width, float Height);
	/** Draws the buff chips ending just above Bottom and returns the new top edge. */
	float DrawBuffChips(const URRRandomDiceBuffComponent& Dice, float CenterX, float Bottom);
	/** Draws power chips (GIANT, FLY) side by side ending just above Bottom and returns the new top edge. */
	float DrawPowerChips(TArrayView<const FString> Labels, TArrayView<const FLinearColor> Colors, TArrayView<const float> Remaining, float CenterX, float Bottom);
	/** The latest roll's result under its 3D die, ending just above Bottom; returns the new top edge (where the die goes). */
	float DrawRollPopup(const URRRandomDiceBuffComponent& Dice, float CenterX, float Bottom);
	void DrawDamagePopups(const ARRRandomCharacter& Brawler, const ARRRandomCharacter* Viewer);
	void DrawDicePanel(const ARRRandomCharacter& Viewer);
	/**
	 * Meteor Rush, bottom left over the dice: the ore I carry (big, bouncing when it grows, blinking pink when I'm hurt
	 * enough to lose it), how many of each kind, and a bar of my team's ore toward the goal with my share in it. Its bottom
	 * edge at Bottom; returns its top edge.
	 */
	float DrawOrePanel(const ARRRandomCharacter& Viewer, float Left, float Bottom);
	/** Meteor Rush: "+3" popping over my head and flying into the ore panel as I pick ore up; "-9 ORE DROPPED" when I lose it. */
	void UpdateOreFeedback(const ARRRandomCharacter* Viewer);
	void DrawOreFeedback(const ARRRandomCharacter* Viewer);
	void DrawAmmoPanel(const ARRRandomCharacter& Brawler);
	/** "[F] RIDE MOTORBIKE" under the brawler next to a free vehicle, "[F] GET OFF" while riding. */
	void DrawRidePrompt(const ARRRandomCharacter& Brawler);
	/**
	 * The star robot (ARRRandomStarRobot): over its gathered cars a pulsing combine sign ("[F]
	 * COMBINE!" when close enough), over the robot coming together its gauge, over the finished one its health, shield and
	 * three seats ("[F] GET IN" next to it), a banner when it is done, and for its crew which seat they have.
	 */
	void DrawStarRobot(const ARRRandomCharacter* Viewer, const class ARRRandomStarRobot* Robot);
	/** Every team's robot (DrawStarRobot: the viewer's team's, or its combine sign; then the rival's), and the duel. */
	void DrawStarRobots(const ARRRandomCharacter* Viewer);
	/**
	 * Robot against robot: the comic words over them (PARRY!, GUARD BREAK!, CLASH!, DOWN!, FINISH! ...); for the crew a
	 * warning when the rival winds up a blow or its missiles come in; in a clash, the crew's row of keys to press, both
	 * sides' progress, the time left and how it ended.
	 */
	void DrawRobotDuel(const ARRRandomCharacter* Viewer);
	/** Third person (ARRRandomPlayerController::RRThirdPerson): a crosshair in the middle of the screen, tighter in the sights,
	 * spreading for a moment after each shot. */
	void DrawCrosshair(const ARRRandomCharacter& Brawler);
	/** Draws the gun's name chip ending at Right; nothing for the starting pistol. */
	void DrawWeaponChip(const struct FRRWeapon& Weapon, float Right, float CenterY);
	void DrawScore();
	/** Defense (round 90): the stage, the wave and what is left of it (or the break's countdown), the tower's health, a boss warning. */
	void DrawDefenseStatus(const ARRRandomGameState& State);
	/**
	 * Meteor Rush: over each meteor's landing spot a marker counting down (an arrow at the screen's edge pointing to it when
	 * it is off the screen), then ORE! while it throws its ore.
	 */
	void DrawMeteorWarnings();
	/** Meteor Rush's cartoon touches over each meteor: comic words in starbursts, speed lines while it spins, dizzy stars. */
	void DrawMeteorCartoons();
	/** Online role (solo, hosting, joined), the host/join/leave keys and the latest online status. */
	void DrawOnlinePanel();
	void DrawRespawnCountdown(const ARRRandomCharacter& Viewer);
	void DrawResults(const ARRRandomGameState& State, const ARRRandomCharacter* Viewer);
	void DrawPauseMenu();
	void DrawCountdown(const ARRRandomGameState& State);
	/** The teammates' cards, stacked up from over the ammo panel. */
	void DrawTeamPanel(const ARRRandomCharacter& Viewer);
	void DrawTeamCard(const ARRRandomCharacter& Mate, const ARRRandomCharacter& Viewer, float Left, float Top);
	void DrawMinimap(const ARRRandomCharacter* Viewer);
	void DrawMapMarkers(const FBox2D& Rect, const ARRRandomCharacter* Viewer);
	/** The map's terrain colors, once (again when cover turns up later). False while there is no island. */
	bool BuildMinimapBase();
	/** The terrain colors shaded by the fog of war, into MapTexture. */
	void RefreshMinimap();
	FVector2D MapPixelToWorld(int32 X, int32 Y) const;
	/** One team's brawlers by knockouts on a frosted panel, from Top; returns the bottom edge. */
	float DrawTeamBoard(int32 Team, const ARRRandomCharacter* Viewer, const ARRRandomCharacter* MVP, float X, float Top, float Width, float Alpha, float Scale);
	/** Whether a screen point is on the round corner map. */
	bool IsOnCornerMap(const FVector2D& ScreenPoint) const;

	/** Seconds the big "TIGER SPIRIT xN!" stays up after my ultimate die lands. */
	static constexpr float UltimateBannerTime = 1.8f;
	/** A downward triangle with a white border, its top edge at Top, centered on CenterX. */
	void DrawFriendOrFoeMarker(float CenterX, float Top, const FLinearColor& Color);

	/** Real time the results screen first showed (negative while playing). */
	float ResultsShownAt = -1.f;
	struct FResultsButtonRect
	{
		ERRResultsButton Button = ERRResultsButton::None;
		FBox2D Rect;
		bool bEnabled = true;
	};
	TArray<FResultsButtonRect> ResultsButtons;
	/** One of ResultsButtons as a pill: mint for the main choice, frosted white otherwise; lifted under the mouse, faded when disabled. */
	void DrawMenuButton(const FResultsButtonRect& Button, const FString& Label, bool bPrimary, float Alpha, float Scale);

	/** This frame's screen scale (720 pixels tall = 1). */
	float UIScale = 1.f;

	/** 0 .. 1: how far the aim shows that the muzzle is buried in a wall (ARRRandomCharacter::GetShotStart), easing in and out. */
	float MuzzleBlockedBlend = 0.f;

	/** The clash: wrong keys seen so far for this viewer's seat, and when the last one flashed. */
	int32 ClashMissesSeen = 0;
	float ClashMissTime = -100.f;

	/** White sprites for rounded shapes and soft shadows (RRRandomUIStyle). */
	UPROPERTY()
	TObjectPtr<UTexture2D> UICircle;

	UPROPERTY()
	TObjectPtr<UTexture2D> UISoft;

	/** Head-and-shoulders picture for the team cards (char-pipeline/scripts/blender_portrait.py). */
	UPROPERTY()
	TObjectPtr<UTexture2D> Portrait;

	/** Cartoon hit stickers, a 4 x 2 atlas in ERRHitEffect order (char-pipeline/scripts/make_hit_effects.py). */
	UPROPERTY()
	TObjectPtr<UTexture2D> HitEffects;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> MapTexture;
	/** The island's area, its terrain colors, how many cover blocks they include, and when the fog shading is next due. */
	FBox2D MapBounds = FBox2D(ForceInit);
	TArray<FColor> MapBase;
	int32 MapCoverCount = INDEX_NONE;
	/** Dice Town's map version the minimap was last built from (holes, what fell, lava; round 89). */
	int32 MapTownVersion = 0;
	float MapRefreshTimer = 0.f;
	bool bMapExpanded = false;
	/** Where the corner map was last drawn (the square around its circle). */
	FBox2D MinimapRect = FBox2D(ForceInit);

	/** Meteor Rush feedback (UpdateOreFeedback): ore gains flying to the panel, the ore last seen, the panel's bounce and the drop notice. */
	struct FOreGain
	{
		int32 Amount = 0;
		FLinearColor Color = FLinearColor::White;
		/** Real time it was picked up. */
		float Start = 0.f;
		/** Where it popped from on the screen (over my head). */
		FVector2D From = FVector2D::ZeroVector;
		bool bArrived = false;
	};
	TArray<FOreGain> OreGains;
	int32 LastOrePoints = 0;
	TWeakObjectPtr<const ARRRandomCharacter> OreViewer;
	float OreBounceAt = -10.f;
	int32 DroppedOre = 0;
	/** What the squirrel's cheek pouch kept of it. */
	int32 KeptOre = 0;
	float DroppedAt = -10.f;
	/** Where the panel's big number was last drawn (the gains fly there). */
	FVector2D OreCountAt = FVector2D::ZeroVector;
};
