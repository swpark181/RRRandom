#pragma once

#include "CoreMinimal.h"
#include "RRRandomWeapon.generated.h"

struct FRandomStream;

/** How flashy a weapon's shots look, decided by its damage per shot. */
UENUM()
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
 * Replicated as part of the brawler so every machine shows the same gun.
 */
USTRUCT()
struct RRRANDOM_API FRRWeapon
{
	GENERATED_BODY()

	/** Short upper-case name for the HUD, e.g. "CANNON". */
	UPROPERTY()
	FString Name = TEXT("PISTOL");

	UPROPERTY()
	ERRWeaponTier Tier = ERRWeaponTier::Common;

	UPROPERTY()
	float DamageMultiplier = 1.f;

	/** Rounds per magazine; 0 means the brawler's own MaxAmmo. */
	UPROPERTY()
	int32 MagazineSize = 0;

	/** Above 1 fires slower. */
	UPROPERTY()
	float FireIntervalMultiplier = 1.f;

	UPROPERTY()
	float ReloadTimeMultiplier = 1.f;

	UPROPERTY()
	float ProjectileSpeedMultiplier = 1.f;

	UPROPERTY()
	float ProjectileScale = 1.f;

	UPROPERTY()
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
