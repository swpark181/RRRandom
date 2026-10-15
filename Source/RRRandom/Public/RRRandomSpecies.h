#pragma once

#include "CoreMinimal.h"
#include "RRRandomAimAnimations.h"
#include "RRRandomSpecies.generated.h"

class UAnimSequence;
class UMaterialInterface;
class USkeletalMesh;
class UTexture2D;

/**
 * The brawlers to pick from: the fox, the rabbit, the squirrel, the tiger and the dragon. Each is a Tripo model rigged and animated in
 * Mixamo (char-pipeline/scripts/ue_import_animal.py), in /Game/Characters/<Name>: SK_<Name>, M_<Name> and
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
	constexpr int32 Count = 5;

	/** "Fox", "Rabbit", "Squirrel", "Tiger", "Dragon": the asset names. */
	const TCHAR* GetAssetName(int32 Species);
	/** FOX, RABBIT, SQUIRREL, TIGER, DRAGON: for menus. */
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

	UPROPERTY()
	TObjectPtr<UTexture2D> Portrait;

	bool IsUsable() const { return Mesh && Material && Idle && Walk; }

	/** Every species' assets, found the first time from a constructor (they stay loaded). */
	static const TArray<FRRSpeciesLook>& GetAll();
};
