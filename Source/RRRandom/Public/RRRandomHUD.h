#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RRRandomHUD.generated.h"

class ARRRandomCharacter;
class UFont;
class URRRandomDiceBuffComponent;

/**
 * Plain canvas HUD. Over every brawler: health bar, stored dice count, dice gauge, active buffs,
 * the latest dice roll, damage numbers and a friend-or-foe marker (plus ammo over the player's own brawler).
 * On screen: team score, the player's dice gauge and buff list, magazine count, a respawn countdown,
 * and the online status with its keys in the top-left corner.
 */
UCLASS()
class RRRANDOM_API ARRRandomHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawOverhead(const ARRRandomCharacter& Brawler, const ARRRandomCharacter* Viewer);
	void DrawAmmoBar(const ARRRandomCharacter& Brawler, float Left, float Top);
	/** Draws the buff chips ending just above Bottom and returns the new top edge. */
	float DrawBuffChips(const URRRandomDiceBuffComponent& Dice, float CenterX, float Bottom, UFont* Font);
	/** Draws power chips (GIANT, FLY) side by side ending just above Bottom and returns the new top edge. */
	float DrawPowerChips(TArrayView<const FString> Labels, TArrayView<const FLinearColor> Colors, TArrayView<const float> Remaining, float CenterX, float Bottom, UFont* Font);
	/** The latest roll's result under its 3D die, ending just above Bottom; returns the new top edge (where the die goes). */
	float DrawRollPopup(const URRRandomDiceBuffComponent& Dice, float CenterX, float Bottom, UFont* Font);
	void DrawDamagePopups(const ARRRandomCharacter& Brawler, const ARRRandomCharacter* Viewer);
	void DrawDicePanel(const ARRRandomCharacter& Viewer);
	void DrawAmmoPanel(const ARRRandomCharacter& Brawler);
	/** Draws the gun's name chip ending at Right; nothing for the starting pistol. */
	void DrawWeaponChip(const struct FRRWeapon& Weapon, float Right, float CenterY, UFont* Font);
	void DrawScore();
	/** Online role (solo, hosting, joined), the host/join/leave keys and the latest online status. */
	void DrawOnlinePanel();
	void DrawRespawnCountdown(const ARRRandomCharacter& Viewer);

	/** Text with a dark drop shadow so it reads over any background. */
	void DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font);
	/** A downward triangle with a dark border, its top edge at Top, centered on CenterX. */
	void DrawFriendOrFoeMarker(float CenterX, float Top, const FLinearColor& Color);
};
