#include "RRRandomSpecies.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FSpeciesTuning
	{
		const TCHAR* Name;
		/** Mesh scale on the brawler. The fox is 100 units to its ear tips at 1.8, filling the capsule (192) the way the
		 * mannequin did; the others sit halfway between having the fox's hips height and the fox's head height, so none
		 * towers over it (char-pipeline/scripts/blender_rig_measure.py). */
		float MeshScale;
		/** The rest pose's hips height at scale 1 (the strides scale with it). Across and front to back the rest pose doesn't
		 * matter: every Mixamo animation plays with the hips over the origin, whatever the bind pose. */
		float HipsHeight;
		/** FRRSpeciesLook::RideFit: forward, right, up, on the brawler. */
		FVector RideFit;
	};

	const FSpeciesTuning Tunings[RRSpecies::Count] = {
		// The fox, rabbit, squirrel and tiger remade in rounds 69-70 and rigged in Mixamo (hips heights of those rigs,
		// blender_rig_measure.py). The fox is still 100 units to its ear tips, so the same 1.8; the squirrel's longer legs
		// bring it down to between the fox's hips and head heights
		// Round 77 (film-style remakes of the fox, squirrel and tiger): still 100 units to the ear tips, same sizes; the hips
		// are the new rigs'
		{ TEXT("Fox"), 1.8f, 19.8f, FVector::ZeroVector },
		// (Riding, the rabbit's long body sits 6 lower, so it shows no more above the sides than the others.) The round-76
		// remake (A-3 moon rabbit) is still 100 units to its ear tips; its own Mixamo rig's hips stand at 18.5
		{ TEXT("Rabbit"), 2.45f, 18.5f, FVector(0.f, 0.f, -6.f) },
		{ TEXT("Squirrel"), 1.93f, 21.5f, FVector::ZeroVector },
		// The fluffy tiger (round 72) kept the round-70 tiger's size, and so does the round-77 one. Round 85: re-rigged in
		// Mixamo (the user placed the markers by hand), hips at 25.6
		{ TEXT("Tiger"), 1.75f, 25.6f, FVector::ZeroVector },
		// Its horns stand tall: between the fox's hips height (2.43) and head height (1.8), a little toward the hips
		// Round 77 film-style dragon: same size; its rig (Mixamo body, arm joints refitted, tail and wing chains) has hips at 16.1
		// Round 85: remodeled with its arms held away from the body (the old arms were fused to its sides, so Mixamo's weights
		// smeared) and rigged in Mixamo by the user's own markers (body without tail and wings), hips at 22.0
		{ TEXT("Dragon"), 2.15f, 22.0f, FVector::ZeroVector },
		// Her ears and head are big: between the fox's hips and head heights. The round-74 remake (Midjourney C-3, fluffy
		// crimson fur) is still 100 units to her ear tips; its own Mixamo rig's hips stand at 22.4
		{ TEXT("RedFox"), 1.95f, 22.4f, FVector::ZeroVector },
	};
	// The speeds below were measured on the first fox's rig (round 9), whose hips stood this high at scale 1; every species'
	// copy of a motion strides in proportion to its own hips height against it
	constexpr float SpeedHipsHeight = 27.5f;

	// The first fox's walk covers 79 units a second at scale 1 (its hips' drift before it was made to walk in place), and its
	// aimed runs (forward, backward, left, right) 148, 134, 125, 148 (how fast the planted foot slides back,
	// blender_anim_speed.py). The others' Mixamo copies stride in proportion to their hips height (the rabbit's walk: 52).
	constexpr float FoxWalkSpeed = 79.f;
	constexpr float FoxAimRunSpeeds[4] = { 148.f, 134.f, 125.f, 148.f };
	// Mixamo "Rifle Run" (rifle down; round 67, ue_import_shared_anim.py from the dragon body rig): its feet show 105 a
	// second on that rig's hips height (20.4, blender_anim_speed.py), so 141.5 on the fox's
	constexpr float FoxRunSpeed = 141.5f;

	template <typename T>
	T* Find(const FString& Path)
	{
		ConstructorHelpers::FObjectFinderOptional<T> Finder(*Path);
		return Finder.Succeeded() ? Finder.Get() : nullptr;
	}
}

const TCHAR* RRSpecies::GetAssetName(int32 Species)
{
	return Tunings[FMath::Clamp(Species, 0, Count - 1)].Name;
}

int32 RRSpecies::GetKitSpecies(int32 Species)
{
	return Species == RedFox ? Fox : Species;
}

FString RRSpecies::GetDisplayName(int32 Species)
{
	return Species == RedFox ? TEXT("RED FOX") : FString(GetAssetName(Species)).ToUpper();
}

int32 RRSpecies::FromName(const FString& Name)
{
	for (int32 Species = 0; Species < Count; ++Species)
	{
		if (Name.Equals(Tunings[Species].Name, ESearchCase::IgnoreCase))
		{
			return Species;
		}
	}
	return INDEX_NONE;
}

FString RRSpecies::GetUltimateName(int32 Species)
{
	switch (GetKitSpecies(Species))
	{
	case Fox: return TEXT("FOX CLONES");
	case Rabbit: return TEXT("MOON HOP");
	case Squirrel: return TEXT("ACORN RAIN");
	case Dragon: return TEXT("DRAGON FLIGHT");
	default: return TEXT("TIGER SPIRIT");
	}
}

FString RRSpecies::GetUltimateBanner(int32 Species, int32 Face)
{
	// The squirrel's face is seconds of rain on top of three (ARRRandomAcornRain::BaseSeconds); the dragon's, how big
	// it and its breath get (ARRRandomDragonFlight)
	return Species == Squirrel ? FString::Printf(TEXT("%s  %ds!"), *GetUltimateName(Species), Face + 3)
		: Species == Dragon ? FString::Printf(TEXT("%s  LV%d!"), *GetUltimateName(Species), Face)
		: FString::Printf(TEXT("%s  x%d!"), *GetUltimateName(Species), Face);
}

const TArray<FRRSpeciesLook>& FRRSpeciesLook::GetAll()
{
	static TArray<FRRSpeciesLook> Looks;
	if (Looks.Num() > 0)
	{
		return Looks;
	}
	for (int32 Species = 0; Species < RRSpecies::Count; ++Species)
	{
		const FSpeciesTuning& Tuning = Tunings[Species];
		const FString Name = Tuning.Name;
		const FString Folder = FString::Printf(TEXT("/Game/Characters/%s/"), *Name);
		auto Anim = [&](const TCHAR* What)
		{
			const FString Asset = FString::Printf(TEXT("A_%s_%s"), *Name, What);
			return Find<UAnimSequence>(Folder + TEXT("Anims/") + Asset + TEXT(".") + Asset);
		};

		FRRSpeciesLook& Look = Looks.AddDefaulted_GetRef();
		Look.Mesh = Find<USkeletalMesh>(FString::Printf(TEXT("%sSK_%s.SK_%s"), *Folder, *Name, *Name));
		Look.Material = Find<UMaterialInterface>(FString::Printf(TEXT("%sM_%s.M_%s"), *Folder, *Name, *Name));
		Look.Idle = Anim(TEXT("Idle"));
		Look.Walk = Anim(TEXT("Walk"));
		Look.Fire = Anim(TEXT("Fire"));
		Look.Death = Anim(TEXT("Death"));
		// One Mixamo rig's motion on every skeleton (char-pipeline/scripts/ue_import_flyback.py, ue_import_shared_anim.py)
		Look.FlyBack = Anim(TEXT("FlyBack"));
		Look.Run = Anim(TEXT("Run"));
		// Mixamo "House Dancing", then twist, shimmy, running man and chicken dance; frustrated, crying and sad
		for (const TCHAR* Dance : { TEXT("Victory"), TEXT("Dance1"), TEXT("Dance2"), TEXT("Dance3"), TEXT("Dance4") })
		{
			if (UAnimSequence* Found = Anim(Dance))
			{
				Look.Dances.Add(Found);
			}
		}
		for (const TCHAR* Slump : { TEXT("Defeat1"), TEXT("Defeat2"), TEXT("Defeat3") })
		{
			if (UAnimSequence* Found = Anim(Slump))
			{
				Look.Slumps.Add(Found);
			}
		}
		// Mixamo rifle aiming set, in place: "Rifle Aiming Idle" and "Rifle Run Forward/Backward/Left/Right"
		Look.Aim.Idle = Anim(TEXT("AimIdle"));
		Look.Aim.Fire = Look.Fire;
		Look.Aim.Move[0] = Anim(TEXT("AimRunF"));
		Look.Aim.Move[1] = Anim(TEXT("AimRunB"));
		Look.Aim.Move[2] = Anim(TEXT("AimRunL"));
		Look.Aim.Move[3] = Anim(TEXT("AimRunR"));
		Look.MeshScale = Tuning.MeshScale;
		Look.HipsHeight = Tuning.HipsHeight * Tuning.MeshScale;
		Look.RideFit = Tuning.RideFit;
		const float Stride = Tuning.HipsHeight / SpeedHipsHeight * Tuning.MeshScale;
		Look.WalkSpeed = FoxWalkSpeed * Stride;
		Look.RunSpeed = FoxRunSpeed * Stride;
		for (int32 Direction = 0; Direction < 4; ++Direction)
		{
			Look.Aim.MoveSpeed[Direction] = FoxAimRunSpeeds[Direction] * Stride;
		}
		Look.Portrait = Find<UTexture2D>(FString::Printf(TEXT("/Game/UI/T_%sPortrait.T_%sPortrait"), *Name, *Name));
		UE_CLOG(!Look.IsUsable(), LogTemp, Warning, TEXT("RRRandom: the %s's assets are missing: it is drawn as the fox."), *Name);
	}
	return Looks;
}
