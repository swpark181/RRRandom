#pragma once

#include "CoreMinimal.h"
#include "RRRandomWeapon.generated.h"

struct FRandomStream;

/**
 * The 2D cartoon sticker that pops on a brawler when it is hit (/Game/UI/T_HitEffects, one cell each, in this order;
 * char-pipeline/scripts/make_hit_effects.py), picked by the gun that hit it. Skull: a knockout by the foam finger.
 */
UENUM()
enum class ERRHitEffect : uint8
{
	Pistol,
	Smg,
	Minigun,
	Rifle,
	Blaster,
	Cannon,
	Poke,
	Skull,
	Count UMETA(Hidden),
};

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
	 * The foam finger (a rare weapon die prize): a short, unseen poke instead of a shot, always exactly 1 damage, but a
	 * poke in the back knocks the target out at once. Held in place of the rifle (URRRandomGunComponent).
	 */
	UPROPERTY()
	bool bFoamFinger = false;

	/** Reach, as a share of the brawler's attack range. */
	UPROPERTY()
	float RangeMultiplier = 1.f;

	/**
	 * Makes a random gun. Quality is the face of the weapon die (1 to 6, higher may go a little beyond):
	 * it raises damage. The gun type sets the trade-offs: big magazines hit softly, heavy shots fly slowly.
	 */
	static FRRWeapon Roll(FRandomStream& Stream, int32 Quality, int32 BaseMagazineSize);
	/** The foam finger; Roll gives it FoamFingerChance of the time. */
	static FRRWeapon MakeFoamFinger(int32 BaseMagazineSize);
	static constexpr float FoamFingerChance = 0.1f;

	/**
	 * One gun type with no random spread (cheats): type 0 is the starting pistol, 1 and up the types Roll picks from,
	 * in GetTypeName order, and the last the foam finger. Quality as in Roll.
	 */
	static FRRWeapon Make(int32 Type, int32 Quality, int32 BaseMagazineSize);
	/** How many types Make knows, the pistol included. */
	static int32 GetTypeCount();
	/** Upper-case name of a Make type, e.g. "CANNON". */
	static FString GetTypeName(int32 Type);

	/** The sticker its hits show (by gun type). */
	ERRHitEffect GetHitEffect() const;

	static ERRWeaponTier GetTierForDamage(float DamageMultiplier);
	static FLinearColor GetTierColor(ERRWeaponTier Tier);
	static FString GetTierName(ERRWeaponTier Tier);
};
