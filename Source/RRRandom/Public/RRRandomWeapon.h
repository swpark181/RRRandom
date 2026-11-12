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
	// Round 97's weapons (after the sticker sheet's cells: their stickers aren't drawn, only their hit bursts)
	Sniper,
	Shotgun,
	Sword,
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

	/** Which gun it is, as Make's types: 0 the starting pistol, 1 .. GetTypeCount() - 2 the guns, the last the foam finger. */
	UPROPERTY()
	uint8 Type = 0;

	/**
	 * Round 97: shots per pull of the trigger (the SHOTGUN's pellets), fanned out up to SpreadDegrees either way. Each pellet
	 * does DamageMultiplier's damage.
	 */
	UPROPERTY()
	uint8 Pellets = 1;

	UPROPERTY()
	float SpreadDegrees = 0.f;

	/**
	 * Round 97, the LIGHT SWORD: no shot; each swing hurts every opponent within SwordReach in front, at once. Its magazine
	 * is swings before a short recharge (the reload).
	 */
	UPROPERTY()
	bool bSword = false;

	/**
	 * Round 97, the SNIPER's scope: aiming (the right button, over the shoulder) looks through it, this many times closer than
	 * the plain sights; better a tier up (GetScopeZoom). 0 for a gun without one.
	 */
	UPROPERTY()
	float ScopeZoom = 0.f;
	static float GetScopeZoom(ERRWeaponTier Tier);

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
	/**
	 * Round 97 (the battle royale's bag, URRRandomLoadoutComponent): one gun type at a tier, no random spread. Each tier up
	 * hits harder (GetTierDamageScale); the foam finger is always legendary.
	 */
	static FRRWeapon MakeTiered(int32 Type, ERRWeaponTier Tier, int32 BaseMagazineSize);
	/** A weapon die's gun type (1 .. GetFingerType(); the foam finger FingerChance of the time). */
	static int32 RollType(FRandomStream& Stream, float FingerChance);
	/** The tier a weapon die's face gives in the bag: a 6 rare, the rest common. */
	static ERRWeaponTier GetTierForFace(int32 Face);
	static float GetTierDamageScale(ERRWeaponTier Tier);
	/** The foam finger's type (the last). */
	static int32 GetFingerType();
	/** How far a LIGHT SWORD's swing reaches from the middle of its holder (at normal size), and its half angle in front. */
	static constexpr float SwordReach = 380.f;
	static constexpr float SwordHalfAngle = 100.f;
	/** Round 97: walking speed with the sword in hand (a little quicker than with a gun). */
	static constexpr float SwordMoveShare = 1.1f;
	/** How many types Make knows, the pistol included. */
	static int32 GetTypeCount();
	/** Upper-case name of a Make type, e.g. "CANNON". */
	static FString GetTypeName(int32 Type);

	/** The sticker its hits show (by gun type). */
	ERRHitEffect GetHitEffect() const;
	/** Its model in the hands (URRRandomGunComponent): 0 pistol, 1 SMG, 2 minigun, 3 rifle, 4 blaster, 5 cannon, 6 sniper, 7 shotgun, 8 sword. */
	int32 GetLookIndex() const;
	/** How many dots a tier shows on the bag's cards (common 1 .. legendary 4). */
	static int32 GetTierStars(ERRWeaponTier Tier) { return static_cast<int32>(Tier) + 1; }

	static ERRWeaponTier GetTierForDamage(float DamageMultiplier);
	static FLinearColor GetTierColor(ERRWeaponTier Tier);
	static FString GetTierName(ERRWeaponTier Tier);
};
