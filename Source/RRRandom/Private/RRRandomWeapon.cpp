#include "RRRandomWeapon.h"
#include "Math/RandomStream.h"

namespace
{
	/** A gun type: multipliers before the random spread. Magazine is a share of the brawler's base magazine. */
	struct FRRWeaponType
	{
		const TCHAR* Name;
		float Damage;
		float MagazineShare;
		float FireInterval;
		float ReloadTime;
		float ProjectileSpeed;
		float ProjectileScale;
	};

	// Many soft, fast shots at one end; few heavy, slow ones at the other
	const FRRWeaponType WeaponTypes[] =
	{
		{ TEXT("MINIGUN"), 0.5f,  4.f,   0.4f, 1.6f, 0.9f,  0.8f },
		{ TEXT("SMG"),     0.65f, 2.f,   0.55f, 1.f, 1.f,   0.8f },
		{ TEXT("RIFLE"),   1.15f, 1.33f, 1.1f, 1.1f, 1.25f, 1.f },
		{ TEXT("BLASTER"), 1.6f,  0.67f, 1.8f, 1.f,  0.7f,  1.5f },
		{ TEXT("CANNON"),  2.4f,  0.34f, 3.5f, 1.4f, 0.45f, 2.2f },
	};

	// Damage per shot at which shots get flashier
	constexpr float RareDamage = 1.1f;
	constexpr float EpicDamage = 1.5f;
	constexpr float LegendaryDamage = 2.1f;
}

FRRWeapon FRRWeapon::Roll(FRandomStream& Stream, int32 Quality, int32 BaseMagazineSize)
{
	const FRRWeaponType& Type = WeaponTypes[Stream.RandRange(0, UE_ARRAY_COUNT(WeaponTypes) - 1)];

	FRRWeapon Weapon;
	Weapon.Name = Type.Name;
	Weapon.bIsDefault = false;
	// A 1 on the die gives about x0.91 damage, a 6 about x1.21, each with some spread
	Weapon.DamageMultiplier = Type.Damage * (0.85f + 0.06f * Quality) * Stream.FRandRange(0.9f, 1.1f);
	Weapon.MagazineSize = FMath::Max(1, FMath::RoundToInt(BaseMagazineSize * Type.MagazineShare * Stream.FRandRange(0.75f, 1.3f)));
	Weapon.FireIntervalMultiplier = Type.FireInterval * Stream.FRandRange(0.9f, 1.1f);
	Weapon.ReloadTimeMultiplier = Type.ReloadTime;
	Weapon.ProjectileSpeedMultiplier = Type.ProjectileSpeed * Stream.FRandRange(0.85f, 1.15f);
	Weapon.ProjectileScale = Type.ProjectileScale;
	Weapon.Tier = GetTierForDamage(Weapon.DamageMultiplier);
	return Weapon;
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
