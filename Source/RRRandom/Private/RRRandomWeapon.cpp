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

	// The foam finger pokes this often (a share of the pistol's interval) and this far (a share of the attack range)
	constexpr float FoamFingerPokeInterval = 1.2f;
	constexpr float FoamFingerRange = 0.25f;
}

FRRWeapon FRRWeapon::MakeFoamFinger(int32 BaseMagazineSize)
{
	FRRWeapon Weapon;
	Weapon.Name = TEXT("FOAM FINGER");
	Weapon.bIsDefault = false;
	Weapon.bFoamFinger = true;
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

FRRWeapon FRRWeapon::Make(int32 Type, int32 Quality, int32 BaseMagazineSize)
{
	if (Type == UE_ARRAY_COUNT(WeaponTypes) + 1)
	{
		return MakeFoamFinger(BaseMagazineSize);
	}
	if (Type <= 0 || Type > UE_ARRAY_COUNT(WeaponTypes))
	{
		return FRRWeapon();
	}
	// Roll with every spread at its middle
	const FRRWeaponType& Picked = WeaponTypes[Type - 1];
	FRRWeapon Weapon;
	Weapon.Name = Picked.Name;
	Weapon.bIsDefault = false;
	Weapon.DamageMultiplier = Picked.Damage * (0.85f + 0.06f * Quality);
	Weapon.MagazineSize = FMath::Max(1, FMath::RoundToInt(BaseMagazineSize * Picked.MagazineShare));
	Weapon.FireIntervalMultiplier = Picked.FireInterval;
	Weapon.ReloadTimeMultiplier = Picked.ReloadTime;
	Weapon.ProjectileSpeedMultiplier = Picked.ProjectileSpeed;
	Weapon.ProjectileScale = Picked.ProjectileScale;
	Weapon.Tier = GetTierForDamage(Weapon.DamageMultiplier);
	return Weapon;
}

int32 FRRWeapon::GetTypeCount()
{
	// The pistol, the gun types and the foam finger
	return UE_ARRAY_COUNT(WeaponTypes) + 2;
}

FString FRRWeapon::GetTypeName(int32 Type)
{
	if (Type == UE_ARRAY_COUNT(WeaponTypes) + 1)
	{
		return TEXT("FINGER");
	}
	return Type > 0 && Type <= UE_ARRAY_COUNT(WeaponTypes) ? FString(WeaponTypes[Type - 1].Name) : FRRWeapon().Name;
}

ERRHitEffect FRRWeapon::GetHitEffect() const
{
	if (bFoamFinger)
	{
		return ERRHitEffect::Poke;
	}
	if (Name == TEXT("SMG"))
	{
		return ERRHitEffect::Smg;
	}
	if (Name == TEXT("MINIGUN"))
	{
		return ERRHitEffect::Minigun;
	}
	if (Name == TEXT("RIFLE"))
	{
		return ERRHitEffect::Rifle;
	}
	if (Name == TEXT("BLASTER"))
	{
		return ERRHitEffect::Blaster;
	}
	if (Name == TEXT("CANNON"))
	{
		return ERRHitEffect::Cannon;
	}
	return ERRHitEffect::Pistol;
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
