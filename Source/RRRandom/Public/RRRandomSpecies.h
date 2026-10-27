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

	UPROPERTY()
	TObjectPtr<UTexture2D> Portrait;

	/** A villain's stand-in: another species' look until its own assets are imported. */
	UPROPERTY()
	bool bStandIn = false;

	bool IsUsable() const { return Mesh && Material && Idle && Walk; }

	/** Every species' assets, found the first time from a constructor (they stay loaded). */
	static const TArray<FRRSpeciesLook>& GetAll();
};
