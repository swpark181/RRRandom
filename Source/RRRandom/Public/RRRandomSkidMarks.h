#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomSkidMarks.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/**
 * Tyre marks on the ground, for every machine's own screen (not replicated): drifting vehicles (ARRRandomVehicle) lay
 * down short dark strips behind their rear wheels, darker and wider the faster they slide, which stay a moment and then
 * fade away. Each strip is a deferred decal (/Game/Effects/Skid/M_SkidMark, char-pipeline/scripts/ue_make_skid_mark.py),
 * so it lies on the bumpy blocks and goes into the base color the toon post process works from. A fixed pool of decals,
 * reused oldest first. One per world, made on first use (Get).
 */
UCLASS(NotPlaceable)
class RRRANDOM_API ARRRandomSkidMarks : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomSkidMarks();

	/** The world's marks, made if there are none yet. */
	static ARRRandomSkidMarks* Get(UWorld* World);

	/** A strip from one wheel spot to the next on the ground, this wide, Strength 0..1 dark. */
	void AddMark(const FVector& From, const FVector& To, float Width, float Strength);

	virtual void Tick(float DeltaSeconds) override;

	/** Seconds a mark stays at full strength, and then takes to fade away. */
	UPROPERTY(EditAnywhere, Category = "Skid")
	float HoldTime = 2.f;

	UPROPERTY(EditAnywhere, Category = "Skid")
	float FadeTime = 3.f;

	/** Marks on the ground at once; past this the oldest goes first. */
	UPROPERTY(EditAnywhere, Category = "Skid")
	int32 MaxMarks = 400;

private:
	UPROPERTY()
	TObjectPtr<UMaterialInterface> MarkMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UDecalComponent>> Decals;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	/** Per decal: seconds since laid down (negative when unused), and its strength. */
	TArray<float> Ages;
	TArray<float> Strengths;
	int32 Next = 0;
	int32 Alive = 0;
};
