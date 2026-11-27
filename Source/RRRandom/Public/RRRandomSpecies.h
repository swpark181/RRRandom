#pragma once

#include "CoreMinimal.h"
#include "RRRandomAimAnimations.h"
#include "RRRandomSpecies.generated.h"

class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class UTexture2D;

/**
 * The brawlers to pick from: the fox, the rabbit, the squirrel, the tiger, the dragon and the red fox. Each is a Tripo model rigged and
 * animated in Mixamo (char-pipeline/scripts/ue_import_animal.py; the red fox on the tiger's rig fitted in Blender,
 * blender_fit_rig.py + ue_import_rig_anims.py, with the tiger's Mixamo motions), in /Game/Characters/<Name>: SK_<Name>, M_<Name> and
 * Anims/A_<Name>_Idle, Walk, Fire, Death, FlyBack, Victory, Dance1..4, Defeat1..3, AimIdle, AimRunF/B/L/R, with its HUD card picture at
 * /Game/UI/T_<Name>Portrait. Players pick one on the title (URRRandomSessionSubsystem::GetSpecies); bots get one at random.
 */
namespace RRSpecies
{
	constexpr int32 Fox = 0;
	constexpr int32 Rabbit = 1;
	constexpr int32 Squirrel = 2;
	constexpr int32 Tiger = 3;
	constexpr int32 Dragon = 4;
	/** The red fennec fox girl (round 68): her own look, the fox's kit and ultimate (GetKitSpecies). */
	constexpr int32 RedFox = 5;
	/** The animal friends to pick (and the bots' random pick): the ones above. */
	constexpr int32 Count = 6;
	/**
	 * The defense mode's villains (round 90), never picked: the rat (every wave) and the cat (the bosses). Without their own
	 * assets yet they are drawn as a stand-in (the squirrel, the tiger) tinted in the villains' colors (FRRSpeciesLook::bStandIn).
	 */
	constexpr int32 Rat = 6;
	constexpr int32 Cat = 7;
	/** Every species with a look and a kit: the animal friends and the villains. */
	constexpr int32 AllCount = 8;
	inline bool IsVillain(int32 Species) { return Species == Rat || Species == Cat; }

	/** The species whose passive, own die and ultimate it has: the red fox has the fox's; the others their own. */
	int32 GetKitSpecies(int32 Species);
	/** "Fox", "Rabbit", "Squirrel", "Tiger", "Dragon", "RedFox": the asset names. */
	const TCHAR* GetAssetName(int32 Species);
	/** FOX, RABBIT, SQUIRREL, TIGER, DRAGON, RED FOX: for menus. */
	FString GetDisplayName(int32 Species);
	/** The species named (any case), or INDEX_NONE. */
	int32 FromName(const FString& Name);
	/** Its ultimate's name: FOX CLONES, MOON HOP, ACORN RAIN, TIGER SPIRIT, DRAGON FLIGHT. */
	FString GetUltimateName(int32 Species);
	/** The big words when its ultimate die lands on Face: "TIGER SPIRIT x4!", "ACORN RAIN 7s!", "DRAGON FLIGHT LV4!"... */
	FString GetUltimateBanner(int32 Species, int32 Face);
}

/** One brawler's body and animations, and how big it is drawn. */
USTRUCT()
struct FRRSpeciesLook
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<USkeletalMesh> Mesh;

	/** Round 95: its shell fur (SK_<Name>Fur on the same skeleton), if it has one. */
	UPROPERTY()
	TObjectPtr<USkeletalMesh> FurMesh;

	/** Tinted toward "DiffuseColor", with the pastel switch and hit flash of the fox's material. */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Idle;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Walk;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Fire;

	/** Running with Shift (round 67): Mixamo "Rifle Run" (running with the rifle down), in place; null: the walk, sped up. */
	UPROPERTY()
	TObjectPtr<UAnimSequence> Run;

	/** Ground speed at which the run plays at its normal rate, at MeshScale. */
	UPROPERTY()
	float RunSpeed = 300.f;

	UPROPERTY()
	TObjectPtr<UAnimSequence> Death;

	/** Knocked out by a vehicle: thrown backward through the air (Mixamo "Flying Back Death"). */
	UPROPERTY()
	TObjectPtr<UAnimSequence> FlyBack;

	/** The victory dance first, then the others. */
	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> Dances;

	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> Slumps;

	/** With each run's ground speed at MeshScale. */
	UPROPERTY()
	FRRAimAnimations Aim;

	/** Round 97, the light sword (Mixamo, one rig's motion on every skeleton): a slash, and the held guard (deflecting). */
	UPROPERTY()
	TObjectPtr<UAnimSequence> SwordSlash;
	UPROPERTY()
	TObjectPtr<UAnimSequence> SwordGuard;
	/** The 2nd and 3rd blows of a combo, and the parry flick from the guard (Tripo text-to-motion). */
	UPROPERTY()
	TObjectPtr<UAnimSequence> SwordSlash2;
	UPROPERTY()
	TObjectPtr<UAnimSequence> SwordSlash3;
	UPROPERTY()
	TObjectPtr<UAnimSequence> SwordParry;

	/** Round 98: the emote wheel's motions (A_<Name>_Emote<Wave|Clap|Cheer|Laugh|Dance|Taunt>; missing ones are null). */
	UPROPERTY()
	TArray<TObjectPtr<UAnimSequence>> Emotes;
	/** Round 98: the tiger's hammer blow and the red fox's dagger stab (Tripo text-to-motion), when imported. */
	UPROPERTY()
	TObjectPtr<UAnimSequence> HammerSwings[3];
	UPROPERTY()
	TObjectPtr<UAnimSequence> DaggerStabs[3];
	UPROPERTY()
	TObjectPtr<UAnimSequence> ShieldBlock;
	/** Round 101: the arms while carrying the hammer and the shield (one held pose). */
	UPROPERTY()
	TObjectPtr<UAnimSequence> ShieldCarry;

	/** The mesh's scale on the brawler (all about the fox's size). */
	UPROPERTY()
	float MeshScale = 1.f;

	/** Ground speed at which the walk plays at its normal rate, at MeshScale. */
	UPROPERTY()
	float WalkSpeed = 200.f;

	/** The rest pose's hips height on the brawler (at MeshScale). */
	UPROPERTY()
	float HipsHeight = 43.f;

	/**
	 * Riding: this body's own nudge from where the seat puts it (forward, right, up; at MeshScale), for the shape of its
	 * bottom and legs (ARRRandomCharacter::GetRideOffset).
	 */
	UPROPERTY()
	FVector RideFit = FVector::ZeroVector;

	/**
	 * Holding a gun (round 90): this body's own fix on the fist every gun sits in (URRRandomGunComponent: GunLooks' grips),
	 * for its hand's size and shape: moved along the fist's hold (forward = the muzzle's way, right, up; cm at normal size)
	 * and turned in it. Tuned live with the console's RRGunHand.
	 */
	UPROPERTY()
	FVector GunHandOffset = FVector::ZeroVector;

	UPROPERTY()
	FRotator GunHandTurn = FRotator::ZeroRotator;

	/**
	 * The tail chain (URRRandomBodyAnimInstance, 2026-11-24): where along the tail it bends, 0 = toward the tip .. 1 =
	 * toward the root (the whole tail swings from its base), and how stiff the root's spring is (oscillations a second;
	 * the tip's is the animation instance's ChainTipFrequency).
	 */
	float TailRootBias = 1.f;
	float TailRootFrequency = 1.3f;
	/** Whether the wag itself grows with the chain's gain (the dragon's does not: its tail was right as it was). */
	bool bTailBigWag = true;
	/**
	 * The tail's idle pose (2026-11-24, the user: standing still, the tail a little lifted and curled to one side, as
	 * it used to be). Tails are modelled straight out behind the body; this is what they do standing: degrees in all
	 * the tail curls to the animal friend's right (negative: left), more toward the tip, degrees in all it lifts at
	 * its root, and degrees in all its far end turns up (the user, the same day: the tip a little higher, the way the
	 * old tail curled). Moving straightens it again. 0, 0: it stays straight (and the tails still modelled swept to a side
	 * stay as they are).
	 */
	float TailIdleCurl = 0.f;
	float TailIdleLift = 0.f;
	float TailIdleTipLift = 0.f;

	/** How much of the tail chain's motion this animal friend's tail makes, 1 = all (2026-11-25: the rabbit's round tail only sways a
	 * little, the user: "동그란 느낌을 유지하면서 살짝 흔들리기만"). */
	float TailAmount = 1.f;
	/**
	 * A cat's tail (2026-11-25, the user: the tiger's tail should move "여우 꼬리 보다는 조금 더 고양이 꼬리처럼"): the sway runs down
	 * the tail as a wave (TailWave times the usual delay from root to tip), wider (TailWagSize) and slower (TailWagSpeed),
	 * a looser tip (TailTipFrequency, oscillations a second; -1 the usual), and carried up with its end hooked while moving
	 * (TailMoveLift at the root, TailMoveTipLift toward the end, degrees in all, the way the idle pose is given).
	 */
	float TailWave = 1.f;
	float TailWagSize = 1.f;
	float TailWagSpeed = 1.f;
	float TailTipFrequency = -1.f;
	float TailMoveLift = 0.f;
	float TailMoveTipLift = 0.f;
	/** The tail springs' damping ratio for this animal friend (-1: the animation instance's ChainDamping); higher, the sway
	 * settles smoothly without bouncing (2026-11-25, the Stray cat's heavy, smooth tail). */
	float TailDamping = -1.f;
	/**
	 * A squiggle (2026-11-25, the user on the Stray-like tiger tail: "조금 더 꼬불거리는 모습도 보였으면 좋겠어. 지금은 너무 스틱
	 * 같아"): an S-curve that runs slowly down the tail, bone against bone, so it never looks like one straight stick.
	 * TailSquiggle: degrees each bone bends up and down at the most (for an 8-bone tail; more toward the tip),
	 * TailSquiggleSide: the same to the sides, TailSquiggleWaves: how many waves fit along the tail at once (about 1: an S),
	 * TailSquiggleSpeed: waves a second running down it. 0, 0: none (every other animal friend).
	 */
	float TailSquiggle = 0.f;
	float TailSquiggleSide = 0.f;
	float TailSquiggleWaves = 1.f;
	float TailSquiggleSpeed = 0.4f;
	/** How big the tail is drawn, length and thickness together, 1 = as modelled (2026-11-25, the user: the tiger's tail
	 * 30% thinner and shorter - the first tail bone is scaled, the fur on it too). */
	float TailSize = 1.f;
	/** How much of its way in the world the tail's end keeps as the hips turn, tip and bob, 0 .. 1 (2026-11-25, the user on
	 * the tiger: "꼬리가 너무 막대기 같은데 뛰거나 방향전환을 할 때 체인같은 느낌을"): the root goes with the body at once, the end
	 * a beat later. 0: none; over 1 the end keeps more than its own way and whips back past (the tiger's 1.7, "조금 더 격하게"). */
	float TailFollow = 0.f;
	/** Turning (2026-11-25, the user: "걸을 땐 묵직하게, 방향 틀 때만 격하게 나눠줘"): the tail's springs' damping ratio and its chain
	 * lag while the body turns hard (URRRandomBodyAnimInstance::TailTurnFrom .. TailTurnFull degrees a second), easing back
	 * to TailDamping and TailFollow over about a second after. -1: no change when turning. */
	float TailTurnDamping = -1.f;
	float TailTurnFollow = -1.f;

	/**
	 * The ears (URRRandomBodyAnimInstance, 2026-11-25; bones EarL1.., EarR1.. from char-pipeline/scripts/blender_ear_bones.py):
	 * how stiff the root's spring is (oscillations a second; softer toward the tip), how hard the head's motion throws
	 * them, and how far they go into the poses (running, aiming, hit, knocked out, the idle twitch), 1 = the fox's.
	 */
	float EarFrequency = 3.f;
	float EarGain = 1.f;
	float EarPose = 1.f;
	/** Degrees more the ears are swept back by the wind of speed beyond walking (running, a vehicle), at full rush. */
	float EarRush = 15.f;
	/** 0 .. 1: how much of the bend the root bone takes alone (1: the ear stays straight and turns at its base, like stiff
	 * cartilage; 0: shared along it, the tip most), and the springs' damping ratio (higher settles sooner, less wobble). */
	float EarRigid = 0.f;
	float EarDamping = 0.3f;

	UPROPERTY()
	TObjectPtr<UTexture2D> Portrait;

	/** A villain's stand-in: another species' look until its own assets are imported. */
	UPROPERTY()
	bool bStandIn = false;

	bool IsUsable() const { return Mesh && Material && Idle && Walk; }

	/** Every species' assets, found the first time from a constructor (they stay loaded). */
	static const TArray<FRRSpeciesLook>& GetAll();
};
