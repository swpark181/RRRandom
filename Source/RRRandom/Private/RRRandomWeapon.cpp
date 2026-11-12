#include "RRRandomWeapon.h"
#include "Math/RandomStream.h"

namespace
{
	/**
	 * A gun type: multipliers before the random spread. Magazine is a share of the brawler's base magazine. Round 97 added
	 * Pellets and Spread (degrees either way: the shotgun's fan), Range (a share of the attack range the shots fly; 0 for the
	 * whole map) and the sword.
	 */
	struct FRRWeaponType
	{
		const TCHAR* Name;
		float Damage;
		float MagazineShare;
		float FireInterval;
		float ReloadTime;
		float ProjectileSpeed;
		float ProjectileScale;
		int32 Pellets;
		float Spread;
		float Range;
		bool bSword;
		ERRHitEffect Effect;
	};

	// Many soft, fast shots at one end; few heavy, slow ones at the other. Round 97: the SNIPER (one hard, very fast shot,
	// slow to fire again; bots keep 1.6 times as far back), the SHOTGUN (seven soft pellets in a fan that fizzle out at 70%
	// of the attack range: deadly up close) and the LIGHT SWORD (a swing at everyone in front within SwordReach, one about every
	// 0.4 s so each blow of a combo plays out; bots close in)
	const FRRWeaponType WeaponTypes[] =
	{
		{ TEXT("MINIGUN"),     0.5f,  4.f,   0.4f,  1.6f, 0.9f,  0.8f,  1, 0.f, 0.f,  false, ERRHitEffect::Minigun },
		{ TEXT("SMG"),         0.65f, 2.f,   0.55f, 1.f,  1.f,   0.8f,  1, 0.f, 0.f,  false, ERRHitEffect::Smg },
		{ TEXT("RIFLE"),       1.15f, 1.33f, 1.1f,  1.1f, 1.25f, 1.f,   1, 0.f, 0.f,  false, ERRHitEffect::Rifle },
		{ TEXT("BLASTER"),     1.6f,  0.67f, 1.8f,  1.f,  0.7f,  1.5f,  1, 0.f, 0.f,  false, ERRHitEffect::Blaster },
		{ TEXT("CANNON"),      2.4f,  0.34f, 3.5f,  1.4f, 0.45f, 2.2f,  1, 0.f, 0.f,  false, ERRHitEffect::Cannon },
		{ TEXT("SNIPER"),      2.7f,  0.27f, 3.2f,  1.5f, 2.4f,  0.9f,  1, 0.f, 1.6f, false, ERRHitEffect::Sniper },
		{ TEXT("SHOTGUN"),     0.42f, 0.5f,  2.f,   1.3f, 1.05f, 0.65f, 7, 9.f, 0.7f, false, ERRHitEffect::Shotgun },
		{ TEXT("LIGHT SWORD"), 3.f,   0.4f,  2.6f,  0.7f, 1.f,   1.f,   1, 0.f, 0.27f, true,  ERRHitEffect::Sword },
	};
	constexpr int32 GunTypeCount = UE_ARRAY_COUNT(WeaponTypes);

	// Damage per shot at which shots get flashier
	constexpr float RareDamage = 1.1f;
	constexpr float EpicDamage = 1.5f;
	constexpr float LegendaryDamage = 2.1f;

	// Round 97, the bag's tiers: damage of a gun at each, as a share of the common one's
	constexpr float TierDamage[] = { 1.f, 1.2f, 1.45f, 1.75f };

	// The foam finger pokes this often (a share of the pistol's interval) and this far (a share of the attack range)
	constexpr float FoamFingerPokeInterval = 1.2f;
	constexpr float FoamFingerRange = 0.25f;

	/** Copies a type's fixed traits (not its damage, magazine, interval or speed, which are spread or tiered). */
	void ApplyTraits(FRRWeapon& Weapon, const FRRWeaponType& Type, int32 TypeIndex)
	{
		Weapon.Name = Type.Name;
		Weapon.bIsDefault = false;
		Weapon.Type = static_cast<uint8>(TypeIndex);
		Weapon.ReloadTimeMultiplier = Type.ReloadTime;
		Weapon.ProjectileScale = Type.ProjectileScale;
		Weapon.Pellets = static_cast<uint8>(Type.Pellets);
		Weapon.SpreadDegrees = Type.Spread;
		Weapon.RangeMultiplier = Type.Range > 0.f ? Type.Range : 1.f;
		Weapon.bSword = Type.bSword;
	}
}

FRRWeapon FRRWeapon::MakeFoamFinger(int32 BaseMagazineSize)
{
	FRRWeapon Weapon;
	Weapon.Name = TEXT("FOAM FINGER");
	Weapon.bIsDefault = false;
	Weapon.bFoamFinger = true;
	Weapon.Type = static_cast<uint8>(GetFingerType());
	// Damage itself is a flat 1 (see the projectile); this only sets the tier's look
	Weapon.DamageMultiplier = 0.01f;
	Weapon.Tier = ERRWeaponTier::Legendary;
	// Poke after poke: no real magazine to speak of
	Weapon.MagazineSize = FMath::Max(1, BaseMagazineSize * 4);
	Weapon.FireIntervalMultiplier = FoamFingerPokeInterval;
	Weapon.ReloadTimeMultiplier = 0.5f;
	Weapon.ProjectileSpeedMultiplier = 1.5f;
	Weapon.RangeMultiplier = FoamFingerRange;
	return Weapon;
}

FRRWeapon FRRWeapon::Roll(FRandomStream& Stream, int32 Quality, int32 BaseMagazineSize)
{
	if (Stream.FRand() < FoamFingerChance)
	{
		return MakeFoamFinger(BaseMagazineSize);
	}
	const int32 Index = Stream.RandRange(0, GunTypeCount - 1);
	const FRRWeaponType& Type = WeaponTypes[Index];

	FRRWeapon Weapon;
	ApplyTraits(Weapon, Type, Index + 1);
	// A 1 on the die gives about x0.91 damage, a 6 about x1.21, each with some spread
	Weapon.DamageMultiplier = Type.Damage * (0.85f + 0.06f * Quality) * Stream.FRandRange(0.9f, 1.1f);
	Weapon.MagazineSize = FMath::Max(1, FMath::RoundToInt(BaseMagazineSize * Type.MagazineShare * Stream.FRandRange(0.75f, 1.3f)));
	Weapon.FireIntervalMultiplier = Type.FireInterval * Stream.FRandRange(0.9f, 1.1f);
	Weapon.ProjectileSpeedMultiplier = Type.ProjectileSpeed * Stream.FRandRange(0.85f, 1.15f);
	// (a shotgun's tier by its whole blast, not one pellet)
	Weapon.Tier = GetTierForDamage(Weapon.DamageMultiplier * FMath::Sqrt(float(Type.Pellets)));
	Weapon.ScopeZoom = Type.Effect == ERRHitEffect::Sniper ? GetScopeZoom(Weapon.Tier) : 0.f;
	return Weapon;
}

FRRWeapon FRRWeapon::Make(int32 Type, int32 Quality, int32 BaseMagazineSize)
{
	if (Type == GetFingerType())
	{
		return MakeFoamFinger(BaseMagazineSize);
	}
	if (Type <= 0 || Type > GunTypeCount)
	{
		return FRRWeapon();
	}
	// Roll with every spread at its middle
	const FRRWeaponType& Picked = WeaponTypes[Type - 1];
	FRRWeapon Weapon;
	ApplyTraits(Weapon, Picked, Type);
	Weapon.DamageMultiplier = Picked.Damage * (0.85f + 0.06f * Quality);
	Weapon.MagazineSize = FMath::Max(1, FMath::RoundToInt(BaseMagazineSize * Picked.MagazineShare));
	Weapon.FireIntervalMultiplier = Picked.FireInterval;
	Weapon.ProjectileSpeedMultiplier = Picked.ProjectileSpeed;
	Weapon.Tier = GetTierForDamage(Weapon.DamageMultiplier * FMath::Sqrt(float(Picked.Pellets)));
	Weapon.ScopeZoom = Picked.Effect == ERRHitEffect::Sniper ? GetScopeZoom(Weapon.Tier) : 0.f;
	return Weapon;
}

FRRWeapon FRRWeapon::MakeTiered(int32 Type, ERRWeaponTier Tier, int32 BaseMagazineSize)
{
	if (Type == GetFingerType())
	{
		return MakeFoamFinger(BaseMagazineSize);
	}
	if (Type <= 0 || Type > GunTypeCount)
	{
		return FRRWeapon();
	}
	const FRRWeaponType& Picked = WeaponTypes[Type - 1];
	FRRWeapon Weapon;
	ApplyTraits(Weapon, Picked, Type);
	Weapon.Tier = Tier;
	Weapon.DamageMultiplier = Picked.Damage * GetTierDamageScale(Tier);
	Weapon.MagazineSize = FMath::Max(1, FMath::RoundToInt(BaseMagazineSize * Picked.MagazineShare));
	Weapon.FireIntervalMultiplier = Picked.FireInterval;
	Weapon.ProjectileSpeedMultiplier = Picked.ProjectileSpeed;
	Weapon.ScopeZoom = Picked.Effect == ERRHitEffect::Sniper ? GetScopeZoom(Tier) : 0.f;
	return Weapon;
}

int32 FRRWeapon::RollType(FRandomStream& Stream, float FingerChance)
{
	return Stream.FRand() < FingerChance ? GetFingerType() : Stream.RandRange(1, GunTypeCount);
}

ERRWeaponTier FRRWeapon::GetTierForFace(int32 Face)
{
	return Face >= 6 ? ERRWeaponTier::Rare : ERRWeaponTier::Common;
}

float FRRWeapon::GetScopeZoom(ERRWeaponTier Tier)
{
	// The user's: 1.5 times at first, better with each upgrade
	constexpr float Zooms[] = { 1.5f, 2.f, 2.5f, 3.f };
	return Zooms[FMath::Clamp(static_cast<int32>(Tier), 0, 3)];
}

float FRRWeapon::GetTierDamageScale(ERRWeaponTier Tier)
{
	return TierDamage[FMath::Clamp(static_cast<int32>(Tier), 0, static_cast<int32>(UE_ARRAY_COUNT(TierDamage)) - 1)];
}

int32 FRRWeapon::GetFingerType()
{
	return GunTypeCount + 1;
}

int32 FRRWeapon::GetTypeCount()
{
	// The pistol, the gun types and the foam finger
	return GunTypeCount + 2;
}

FString FRRWeapon::GetTypeName(int32 Type)
{
	if (Type == GetFingerType())
	{
		return TEXT("FINGER");
	}
	return Type > 0 && Type <= GunTypeCount ? FString(WeaponTypes[Type - 1].Name) : FRRWeapon().Name;
}

ERRHitEffect FRRWeapon::GetHitEffect() const
{
	if (bFoamFinger)
	{
		return ERRHitEffect::Poke;
	}
	// By name: a weapon from before round 97 (Type 0) still finds its gun
	for (const FRRWeaponType& Each : WeaponTypes)
	{
		if (Name == Each.Name)
		{
			return Each.Effect;
		}
	}
	return ERRHitEffect::Pistol;
}

int32 FRRWeapon::GetLookIndex() const
{
	switch (GetHitEffect())
	{
	case ERRHitEffect::Smg: return 1;
	case ERRHitEffect::Minigun: return 2;
	case ERRHitEffect::Rifle: return 3;
	case ERRHitEffect::Blaster: return 4;
	case ERRHitEffect::Cannon: return 5;
	case ERRHitEffect::Sniper: return 6;
	case ERRHitEffect::Shotgun: return 7;
	case ERRHitEffect::Sword: return 8;
	default: return 0;
	}
}

ERRWeaponTier FRRWeapon::GetTierForDamage(float DamageMultiplier)
{
	if (DamageMultiplier >= LegendaryDamage)
	{
		return ERRWeaponTier::Legendary;
	}
	if (DamageMultiplier >= EpicDamage)
	{
		return ERRWeaponTier::Epic;
	}
	return DamageMultiplier >= RareDamage ? ERRWeaponTier::Rare : ERRWeaponTier::Common;
}

FLinearColor FRRWeapon::GetTierColor(ERRWeaponTier Tier)
{
	switch (Tier)
	{
	case ERRWeaponTier::Rare: return FLinearColor(0.3f, 0.65f, 1.f);
	case ERRWeaponTier::Epic: return FLinearColor(0.8f, 0.35f, 1.f);
	case ERRWeaponTier::Legendary: return FLinearColor(1.f, 0.7f, 0.1f);
	default: return FLinearColor(0.85f, 0.85f, 0.85f);
	}
}

FString FRRWeapon::GetTierName(ERRWeaponTier Tier)
{
	switch (Tier)
	{
	case ERRWeaponTier::Rare: return TEXT("RARE");
	case ERRWeaponTier::Epic: return TEXT("EPIC");
	case ERRWeaponTier::Legendary: return TEXT("LEGEND");
	default: return TEXT("COMMON");
	}
}
