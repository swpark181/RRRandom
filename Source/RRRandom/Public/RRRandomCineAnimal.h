#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RRRandomCineAnimal.generated.h"

class URRRandomFurComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * Cinematic (2026-11-19, the fox vs tiger Sequencer short): an animal friend as an actor for a Level Sequence, with no game
 * logic. Its look is the game's (FRRSpeciesLook: body, shell fur, size, turned on its feet like ARRRandomCharacter's mesh)
 * and it can hold the game's props the way the game holds them: the toy pistol or the squeaky hammer in the right fist
 * (URRRandomGunComponent's own-look formula with the species' GunHandTurn), the shield on the left forearm (the carried
 * pose of ARRRandomCharacter::UpdateShield). The sequence plays animations on Body and keys the bHold* flags; the props
 * follow the bones every frame, in the editor too, so scrubbing shows them.
 */
UCLASS()
class RRRANDOM_API ARRRandomCineAnimal : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomCineAnimal();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override { return true; }

	/** RRSpecies index (0 the fox, 3 the tiger). */
	UPROPERTY(EditAnywhere, Category = "Cine")
	int32 Species = 0;

	UPROPERTY(EditAnywhere, Interp, Category = "Cine")
	bool bHoldPistol = false;

	UPROPERTY(EditAnywhere, Interp, Category = "Cine")
	bool bHoldHammer = false;

	UPROPERTY(EditAnywhere, Interp, Category = "Cine")
	bool bHoldShield = false;

	/**
	 * The face (texture expression swap, the plan's choice): -1 the game's own body material, otherwise the body material
	 * of Expressions[Expression] (a copy of the species' material with an expression painted into its base color,
	 * Tools/cinematic). The fur follows it (URRRandomFurComponent shows only over its own base material).
	 */
	UPROPERTY(EditAnywhere, Interp, Category = "Cine")
	int32 Expression = -1;

	UPROPERTY(EditAnywhere, Category = "Cine")
	TArray<TObjectPtr<UMaterialInterface>> Expressions;

	/**
	 * 2026-11-20: the shell fur's material for each expression (same order; a copy of M_<Name>Fur with the expression's
	 * base color, ue_cine_expressions.py). The fur draws the body's texture again on its strands, so without this the
	 * fur over the mouth kept the neutral mouth and hid a grin painted under it. Empty or null: the fur's own material.
	 */
	UPROPERTY(EditAnywhere, Category = "Cine")
	TArray<TObjectPtr<UMaterialInterface>> ExpressionFurs;

	/**
	 * The cinematic's own body (null: the game's), on the species' skeleton: the game mesh with a smooth face mask
	 * joined to it as the material slot "Face" (char-pipeline/scripts/blender_face_flatten.py; what stood out of the face
	 * is pushed behind it). With a "Face" slot the body keeps the game's material and only the mask takes
	 * Expressions[Expression] (a picture of the whole face, UVs = the front view).
	 */
	UPROPERTY(EditAnywhere, Category = "Cine")
	TObjectPtr<USkeletalMesh> BodyMesh;

	/** Its fur (null: the game's), thinned away over the mask. */
	UPROPERTY(EditAnywhere, Category = "Cine")
	TObjectPtr<USkeletalMesh> FurMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cine")
	TObjectPtr<USceneComponent> Feet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cine")
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cine")
	TObjectPtr<URRRandomFurComponent> Fur;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cine")
	TObjectPtr<UStaticMeshComponent> Pistol;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cine")
	TObjectPtr<UStaticMeshComponent> Hammer;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cine")
	TObjectPtr<UStaticMeshComponent> Shield;

private:
	void ApplyLook();
	void PlaceProps();
	/** A fist-held prop (the gun component's own-look formula): its length (100 = its mesh) and its grip point. */
	void PlaceInFist(UStaticMeshComponent* Prop, float Length, const FVector& Grip) const;
	void PlaceShield() const;

	void ApplyExpression();

	int32 AppliedSpecies = -1;
	int32 AppliedExpression = -2;
	/**
	 * 2026-11-20: in a game world the first tick compiles every expression material's shaders in full. An uncooked
	 * run compiles a material's shaders only when it is first drawn, so the frame an expression first showed (and the
	 * frame the clones appeared) drew grey, the default material: there since the first expression render, a whole
	 * frame in the 1-sample review takes, an eighth of one in the 8-sample finals.
	 */
	bool bShadersReady = false;
};

/** Editor helpers for the cinematic set-up scripts (Tools/cinematic). */
UCLASS()
class RRRANDOM_API URRRandomCineLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Editor only: in Material, every texture sample of From is pointed at To (the copy then recompiles). Returns how many. */
	UFUNCTION(BlueprintCallable, Category = "Cine")
	static int32 ReplaceMaterialTexture(UMaterial* Material, UTexture* From, UTexture* To);

	/**
	 * A sequencer section's ease in / out, in seconds (2026-11-20: two animation sections that overlap by the ease time
	 * cross-fade, the tiger's run into its hammer swing). The section's easing settings are not editable from Python.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cine")
	static void SetSectionEasing(class UMovieSceneSection* Section, float EaseInSeconds, float EaseOutSeconds);

	/**
	 * The rotation that turns a die's face (1-6) toward a camera with its pips upright, as the game's die over a head
	 * lands (URRRandomOverheadDieComponent::FaceToward); for the cinematic's keyed ultimate die.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cine")
	static FRotator DieFaceRotation(int32 Face, FRotator Camera);
};
