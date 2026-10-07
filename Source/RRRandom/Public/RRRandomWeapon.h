#pragma once

#include "CoreMinimal.h"

struct FRandomStream;

/** How flashy a weapon's shots look, decided by its damage per shot. */
enum class ERRWeaponTier : uint8
{
	Common,
	Rare,
	Epic,
	Legendary,
};

/**
 * A brawler's gun, as multipliers on the brawler's own base stats (damage, fire interval, reload time)
 * and the projectile's (speed, size). The default is the starting pistol, all 1s.
 */
struct RRRANDOM_API FRRWeapon
{
	/** Short upper-case name for the HUD, e.g. "CANNON". */
	FString Name = TEXT("PISTOL");
	ERRWeaponTier Tier = ERRWeaponTier::Common;
	float DamageMultiplier = 1.f;
	/** Rounds per magazine; 0 means the brawler's own MaxAmmo. */
	int32 MagazineSize = 0;
	/** Above 1 fires slower. */
	float FireIntervalMultiplier = 1.f;
	float ReloadTimeMultiplier = 1.f;
	float ProjectileSpeedMultiplier = 1.f;
	float ProjectileScale = 1.f;
	bool bIsDefault = true;

	/**
	 * Makes a random gun. Quality is the face of the weapon die (1 to 6, higher may go a little beyond):
	 * it raises damage. The gun type sets the trade-offs: big magazines hit softly, heavy shots fly slowly.
	 */
	static FRRWeapon Roll(FRandomStream& Stream, int32 Quality, int32 BaseMagazineSize);

	static ERRWeaponTier GetTierForDamage(float DamageMultiplier);
	static FLinearColor GetTierColor(ERRWeaponTier Tier);
	static FString GetTierName(ERRWeaponTier Tier);
};
