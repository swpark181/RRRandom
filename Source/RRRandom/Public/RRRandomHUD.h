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

private:
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
	void DrawAmmoPanel(const ARRRandomCharacter& Brawler);
	/** "[F] RIDE MOTORBIKE" under the brawler next to a free vehicle, "[F] GET OFF" while riding. */
	void DrawRidePrompt(const ARRRandomCharacter& Brawler);
	/** Third person (ARRRandomPlayerController::RRThirdPerson): a crosshair in the middle of the screen, tighter in the sights,
	 * spreading for a moment after each shot. */
	void DrawCrosshair(const ARRRandomCharacter& Brawler);
	/** Draws the gun's name chip ending at Right; nothing for the starting pistol. */
	void DrawWeaponChip(const struct FRRWeapon& Weapon, float Right, float CenterY);
	void DrawScore();
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
	float MapRefreshTimer = 0.f;
	bool bMapExpanded = false;
	/** Where the corner map was last drawn (the square around its circle). */
	FBox2D MinimapRect = FBox2D(ForceInit);
};
