#include "RRRandomSpecies.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
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
		/**
		 * FRRSpeciesLook::GunHandOffset and GunHandTurn: the fist every gun sits in, for this body's hand (round 90). The turn
		 * was measured on each rig (rr.Gun.Debug logs the right hand bone's axes, char-pipeline/scripts/gun_hand_turn.pl turns
		 * them into this): the average of the hold that points a gun from the right hand to the left with its top up in the
		 * rifle walk, and in the settled aiming pose. Each Mixamo rig rolls its hand bone its own way, so each has its own.
		 */
		FVector GunHandOffset = FVector::ZeroVector;
		FRotator GunHandTurn = FRotator::ZeroRotator;
		/** FRRSpeciesLook::TailRootBias and TailRootFrequency (2026-11-24, the user: the fox's, squirrel's, tiger's and red
		 * fox's tails should swing nearer the root; the dragon's is right as it was, bending toward the tip). */
		float TailRootBias = 1.f;
		float TailRootFrequency = 1.3f;
		bool bTailBigWag = true;
		float TailIdleCurl = 0.f;
		float TailIdleLift = 0.f;
		float TailIdleTipLift = 0.f;
		float TailAmount = 1.f;
		float TailWave = 1.f;
		float TailWagSize = 1.f;
		float TailWagSpeed = 1.f;
		float TailTipFrequency = -1.f;
		float TailMoveLift = 0.f;
		float TailMoveTipLift = 0.f;
		float TailDamping = -1.f;
		float TailSquiggle = 0.f;
		float TailSquiggleSide = 0.f;
		float TailSquiggleWaves = 1.f;
		float TailSquiggleSpeed = 0.4f;
		float TailSize = 1.f;
		float TailFollow = 0.f;
		float TailTurnDamping = -1.f;
		float TailTurnFollow = -1.f;
	};

	const FSpeciesTuning Tunings[RRSpecies::AllCount] = {
		// The fox, rabbit, squirrel and tiger remade in rounds 69-70 and rigged in Mixamo (hips heights of those rigs,
		// blender_rig_measure.py). The fox is still 100 units to its ear tips, so the same 1.8; the squirrel's longer legs
		// bring it down to between the fox's hips and head heights
		// Round 77 (film-style remakes of the fox, squirrel and tiger): still 100 units to the ear tips, same sizes; the hips
		// are the new rigs'
		// 2026-11-19: the fox remade once more as a blank-face T-pose Smart Mesh (texture expressions), its own Mixamo rig:
		// hips 20.7, and the gun hand turn measured on it (gun_hand_turn_log.pl: rifle run and the settled aim)
		// (the fox was generated again on 2026-11-24 with its tail held straight out behind it - the rule for every
		// character's tail from then on - and put onto this same skeleton: char-pipeline/scripts/blender_reskin.py.
		// Its tail is 8 bones. Standing, it curls to the right and lifts a little, as the old tail was drawn.)
		{ TEXT("Fox"), 1.8f, 20.7f, FVector::ZeroVector, FVector::ZeroVector, FRotator(29.0f, -82.0f, 89.7f), 1.f, 1.3f, true, 120.f, 22.f, 80.f },
		// T-pose Smart Mesh rabbit, rigged by the user in Mixamo: 100 units to the ear, hips 26.6716.
		// Its longer legs put the hips alongside the other animal friends at scale 1.9.
		// (2026-11-25: its round tail got a bone of its own, Tail1 - weighted over the hips and spine it was squashed out of
		// shape on the run - and only sways a little, quick and small: the user, "동그란 느낌을 유지하면서 살짝 흔들리기만")
		{ TEXT("Rabbit"), 1.9f, 26.6716f, FVector(0.f, 0.f, -6.f), FVector::ZeroVector, FRotator(27.f, -79.3f, 115.2f), 1.f, 2.5f, false, 0.f, 0.f, 0.f, 0.3f },
		{ TEXT("Squirrel"), 1.93f, 21.5f, FVector::ZeroVector, FVector::ZeroVector, FRotator(22.5f, -73.1f, 130.1f) },
		// The fluffy tiger (round 72) kept the round-70 tiger's size, and so does the round-77 one. Round 85: re-rigged in
		// Mixamo (the user placed the markers by hand), hips at 25.6
		// (generated again on 2026-11-25 with its tail held straight out behind it, like the fox the day before - the user: the
		// concept's S-curve was modelled in and the tail moved oddly - and put onto this same skeleton: 8 tail bones. Standing,
		// its tail stays low and curls up at the end, as the concept drew it: my pick of four tried in the game.)
		// (the same day, the user: an even thickness, and moving more like a cat's tail than the fox's: it bends toward the tip, a
		// wave runs down it, wider and slower, a loose end, carried up and hooked while walking)
		// (then, the user: "Stray 고양이 꼬리처럼". From the Stray gameplay trailer: the tail is heavy and low, dipping a little from
		// the root with the end curving gently up, swaying slowly and smoothly with hardly any bounce; standing, raised with the
		// end curled. So: walking the root dips (-8) and the end turns up (45) instead of carried up and hooked; a smaller,
		// slower sway (x1.0, x0.55); a damped spring (0.65) and a not-so-loose tip (0.75 Hz); standing, raised more (18) with
		// the end curled (90).)
		// (then, the user: "조금 더 꼬불거리는 모습도 보였으면 좋겠어. 지금은 너무 스틱 같아" - an S-curve running slowly down it,
		// up and down and to the sides, a little over one wave along the tail; 9/6/1 was tried first and barely showed)
		// (then, the user: "평상시엔 바닥쪽으로 늘어뜨리고 끝만 살짝 떠있는게" - standing and walking the tail hangs toward the floor
		// (root -40, walking -95: on the move the body leans forward and lifted it back to level at -60) and only its end
		// turns up (48, 50), curled to the side a little (25); the up-and-down
		// squiggle halved (7) so it doesn't slap the floor. And "30% 정도 두께와 길이를 줄여주고": TailSize 0.7; no part of it
		// goes through the floor any more, see the proxy.)
		{ TEXT("Tiger"), 1.75f, 22.47f, FVector::ZeroVector, FVector::ZeroVector, FRotator(16.5f, -86.7f, 109.2f), 0.25f, 1.6f, true, 25.f, -40.f, 48.f,
		// (then, the user: "꼬리가 너무 막대기 같은데 뛰거나 방향전환을 할 때 체인같은 느낌을" - TailFollow; and "조금 더 격하게": the end
		// keeps more than its own way (1.7, so it whips back past) and the springs bounce more (damping 0.65 -> 0.35); of
		// 0.85/0.65, 1.3/0.45 and 1.7/0.35 tried in the game, the last)
		// (then "걸을 땐 묵직하게, 방향 틀 때만 격하게 나눠줘": walking back to the heavy 0.65 springs with a light lag (0.5), the
		// 1.7 lag and 0.35 springs only while turning hard, easing back after)
			1.f, 2.f, 1.0f, 0.55f, 0.75f, -95.f, 50.f, 0.65f, 7.f, 9.f, 1.15f, 0.4f, 0.7f, 0.5f, 0.35f, 1.7f },
		// Its horns stand tall: between the fox's hips height (2.43) and head height (1.8), a little toward the hips
		// Round 77 film-style dragon: same size; its rig (Mixamo body, arm joints refitted, tail and wing chains) has hips at 16.1
		// Round 85: remodeled with its arms held away from the body (the old arms were fused to its sides, so Mixamo's weights
		// smeared) and rigged in Mixamo by the user's own markers (body without tail and wings), hips at 22.0
		{ TEXT("Dragon"), 2.15f, 22.0f, FVector::ZeroVector, FVector::ZeroVector, FRotator(27.7f, -84.8f, 109.9f), 0.25f, 2.f, false },
		// Her ears and head are big: between the fox's hips and head heights. The round-74 remake (Midjourney C-3, fluffy
		// crimson fur) is still 100 units to her ear tips; its own Mixamo rig's hips stand at 22.4
		{ TEXT("RedFox"), 1.95f, 22.4f, FVector::ZeroVector, FVector::ZeroVector, FRotator(35.2f, -96.9f, 82.9f) },
		// The defense mode's villains (round 90): the rat a little smaller than the animal friends, the cat a head taller
		// (each wave sizes them further, ARRRandomCharacter::SetupVillain)
		// Round 90 Midjourney A-3 rat and A-2 cat (Tripo, Mixamo rigs: hips 19.7 and 18.4); at these scales the rat stands
		// a little under the fox, the cat a little over it
		{ TEXT("Rat"), 2.4f, 19.7f, FVector::ZeroVector },
		{ TEXT("Cat"), 2.6f, 18.4f, FVector::ZeroVector },
	};
	// The ears (2026-11-25, the user: the fox, red fox, rabbit, tiger and dragon, in that order; not the squirrel): root
	// spring (oscillations a second), how hard the head's motion throws them, how far into the poses. Big ears are softer
	// and kept smaller in their poses (the red fox's are bigger than her face); the tiger's small round ones mostly turn
	// back; the dragon's are the fins at the sides of its head (not the horns).
	struct FEarTuning
	{
		const TCHAR* Name;
		float Frequency;
		float Gain;
		float Pose;
		// (the user, the same day: running, the fox's, red fox's and rabbit's ears should be pushed back more dramatically,
		// and so on a vehicle at speed - degrees more at full rush, see URRRandomBodyAnimInstance::EarRushSpeed)
		float Rush;
		// (the user, the same day: "여우 귀만 너무 젤리 같은데" - the fox's ears bent along their length and wobbled; a fox ear
		// is stiff, so it turns at its base and settles sooner)
		float Rigid = 0.f;
		float Damping = 0.3f;
	};
	const FEarTuning EarTunings[] = {
		{ TEXT("Fox"), 3.5f, 1.f, 1.f, 50.f, 0.85f, 0.55f },
		{ TEXT("RedFox"), 2.3f, 1.2f, 0.75f, 50.f },
		{ TEXT("Rabbit"), 2.f, 1.3f, 0.9f, 55.f },
		{ TEXT("Tiger"), 4.f, 0.6f, 0.8f, 15.f },
		{ TEXT("Dragon"), 3.5f, 0.7f, 0.8f, 20.f },
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
		// An asset not imported yet (the villains' before round 90's import) is just missing, without the finder's error
		FString Package;
		if (!Path.Split(TEXT("."), &Package, nullptr) || !FPackageName::DoesPackageExist(Package))
		{
			return nullptr;
		}
		ConstructorHelpers::FObjectFinderOptional<T> Finder(*Path);
		return Finder.Succeeded() ? Finder.Get() : nullptr;
	}
}

const TCHAR* RRSpecies::GetAssetName(int32 Species)
{
	return Tunings[FMath::Clamp(Species, 0, AllCount - 1)].Name;
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
	for (int32 Species = 0; Species < RRSpecies::AllCount; ++Species)
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
		Look.FurMesh = Find<USkeletalMesh>(FString::Printf(TEXT("%sSK_%sFur.SK_%sFur"), *Folder, *Name, *Name));
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
		Look.SwordSlash = Anim(TEXT("SwordSlash"));
		Look.SwordGuard = Anim(TEXT("SwordGuard"));
		Look.SwordSlash2 = Anim(TEXT("SwordSlash2"));
		Look.SwordSlash3 = Anim(TEXT("SwordSlash3"));
		Look.SwordParry = Anim(TEXT("SwordParry"));
		// Round 98: the emote wheel (always six places, a missing one null) and the roles' melee blows
		for (const TCHAR* Emote : { TEXT("EmoteWave"), TEXT("EmoteClap"), TEXT("EmoteCheer"), TEXT("EmoteLaugh"), TEXT("EmoteDance"), TEXT("EmoteTaunt") })
		{
			Look.Emotes.Add(Anim(Emote));
		}
		for (int32 Step = 0; Step < 3; ++Step)
		{
			Look.HammerSwings[Step] = Anim(*FString::Printf(TEXT("HammerSwing%d"), Step + 1));
			Look.DaggerStabs[Step] = Anim(*FString::Printf(TEXT("DaggerStab%d"), Step + 1));
		}
		Look.ShieldBlock = Anim(TEXT("ShieldBlock"));
		Look.ShieldCarry = Anim(TEXT("ShieldCarry"));
		// Each playable rig gets its own retargeted copy of the connected phrase.
		if (Species < RRSpecies::Count)
		{
			UAnimSequence* First = Anim(TEXT("SwordDedicated1"));
			UAnimSequence* Second = Anim(TEXT("SwordDedicated2"));
			UAnimSequence* Third = Anim(TEXT("SwordDedicated3"));
			if (First && Second && Third)
			{
				Look.SwordSlash = First;
				Look.SwordSlash2 = Second;
				Look.SwordSlash3 = Third;
			}
		}
		Look.MeshScale = Tuning.MeshScale;
		Look.HipsHeight = Tuning.HipsHeight * Tuning.MeshScale;
		Look.RideFit = Tuning.RideFit;
		Look.GunHandOffset = Tuning.GunHandOffset;
		Look.GunHandTurn = Tuning.GunHandTurn;
		Look.TailRootBias = Tuning.TailRootBias;
		Look.TailRootFrequency = Tuning.TailRootFrequency;
		Look.bTailBigWag = Tuning.bTailBigWag;
		Look.TailIdleCurl = Tuning.TailIdleCurl;
		Look.TailIdleLift = Tuning.TailIdleLift;
		Look.TailIdleTipLift = Tuning.TailIdleTipLift;
		Look.TailAmount = Tuning.TailAmount;
		Look.TailWave = Tuning.TailWave;
		Look.TailWagSize = Tuning.TailWagSize;
		Look.TailWagSpeed = Tuning.TailWagSpeed;
		Look.TailTipFrequency = Tuning.TailTipFrequency;
		Look.TailMoveLift = Tuning.TailMoveLift;
		Look.TailMoveTipLift = Tuning.TailMoveTipLift;
		Look.TailDamping = Tuning.TailDamping;
		Look.TailSquiggle = Tuning.TailSquiggle;
		Look.TailSquiggleSide = Tuning.TailSquiggleSide;
		Look.TailSquiggleWaves = Tuning.TailSquiggleWaves;
		Look.TailSquiggleSpeed = Tuning.TailSquiggleSpeed;
		Look.TailSize = Tuning.TailSize;
		Look.TailFollow = Tuning.TailFollow;
		Look.TailTurnDamping = Tuning.TailTurnDamping;
		Look.TailTurnFollow = Tuning.TailTurnFollow;
		for (const FEarTuning& Ear : EarTunings)
		{
			if (FCString::Strcmp(Ear.Name, Tuning.Name) == 0)
			{
				Look.EarFrequency = Ear.Frequency;
				Look.EarGain = Ear.Gain;
				Look.EarPose = Ear.Pose;
				Look.EarRush = Ear.Rush;
				Look.EarRigid = Ear.Rigid;
				Look.EarDamping = Ear.Damping;
			}
		}
		const float Stride = Tuning.HipsHeight / SpeedHipsHeight * Tuning.MeshScale;
		Look.WalkSpeed = FoxWalkSpeed * Stride;
		Look.RunSpeed = FoxRunSpeed * Stride;
		for (int32 Direction = 0; Direction < 4; ++Direction)
		{
			Look.Aim.MoveSpeed[Direction] = FoxAimRunSpeeds[Direction] * Stride;
		}
		Look.Portrait = Find<UTexture2D>(FString::Printf(TEXT("/Game/UI/T_%sPortrait.T_%sPortrait"), *Name, *Name));
		// A villain without its own assets yet: a stand-in's look (the squirrel for the rat, the tiger for the cat), tinted
		if (!Look.IsUsable() && RRSpecies::IsVillain(Species))
		{
			const int32 StandIn = Species == RRSpecies::Rat ? RRSpecies::Squirrel : RRSpecies::Tiger;
			if (Looks[StandIn].IsUsable())
			{
				Look = Looks[StandIn];
				Look.bStandIn = true;
				Look.Portrait = nullptr;
				UE_LOG(LogTemp, Log, TEXT("RRRandom: no %s assets yet: drawn as the %s, tinted."), *Name, Tunings[StandIn].Name);
				continue;
			}
		}
		UE_CLOG(!Look.IsUsable(), LogTemp, Warning, TEXT("RRRandom: the %s's assets are missing: it is drawn as the fox."), *Name);
	}
	return Looks;
}
