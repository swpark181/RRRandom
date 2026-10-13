#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RRRandomFlightEffectsComponent.generated.h"

class UDecalComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;

/**
 * What makes a brawler's height read from the high camera, on every machine (/Game/Effects/Flight materials, made by
 * char-pipeline/scripts/ue_make_flight_fx.py). Purely cosmetic.
 * - A soft round shadow on the ground right under it (a decal), shrinking and fading the higher it goes, so a flyer
 *   is seen to leave the ground (the sun's own shadow falls off to the side).
 * - A ring of dust thrown out on the ground when it takes off (a jump or a flight lift) and when it lands from a height,
 *   bigger the further it fell.
 * - Wind streaks left in the air behind it while it flies fast: thin glowing lines along its path that fade at once.
 */
UCLASS(ClassGroup = (RRRandom))
class RRRANDOM_API URRRandomFlightEffectsComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URRRandomFlightEffectsComponent();

	/** The shadow's radius on the ground at normal size; at ShadowFadeHeight up it is ShadowSmallest of that. */
	UPROPERTY(EditAnywhere, Category = "Shadow")
	float ShadowRadius = 85.f;

	UPROPERTY(EditAnywhere, Category = "Shadow")
	float ShadowFadeHeight = 900.f;

	UPROPERTY(EditAnywhere, Category = "Shadow")
	float ShadowSmallest = 0.75f;

	/** Darkness on the ground and at ShadowFadeHeight (a little darker up there, where the body no longer covers it and it
	 * has to read on bright ground). */
	UPROPERTY(EditAnywhere, Category = "Shadow")
	FVector2D ShadowOpacity = FVector2D(0.75f, 0.85f);

	/** Widest a dust ring grows (at normal size, for the hardest landing) and how long it lasts. */
	UPROPERTY(EditAnywhere, Category = "Dust")
	float DustRadius = 170.f;

	UPROPERTY(EditAnywhere, Category = "Dust")
	float DustTime = 0.55f;

	/** A landing throws dust only after a fall at least this high, and the most at DustFullDrop. */
	UPROPERTY(EditAnywhere, Category = "Dust")
	float DustMinDrop = 140.f;

	UPROPERTY(EditAnywhere, Category = "Dust")
	float DustFullDrop = 700.f;

	/** Taking off: an upward speed at least this fast leaving the ground throws dust. */
	UPROPERTY(EditAnywhere, Category = "Dust")
	float TakeOffSpeed = 500.f;

	/** Streaks: how many a second at StreakFullSpeed, from StreakMinSpeed and StreakMinHeight off the ground up. */
	UPROPERTY(EditAnywhere, Category = "Streaks")
	float StreakRate = 30.f;

	UPROPERTY(EditAnywhere, Category = "Streaks")
	float StreakMinSpeed = 250.f;

	UPROPERTY(EditAnywhere, Category = "Streaks")
	float StreakFullSpeed = 700.f;

	UPROPERTY(EditAnywhere, Category = "Streaks")
	float StreakMinHeight = 120.f;

	/** Length range, width, and life in seconds of a streak; at most MaxStreaks at once. */
	UPROPERTY(EditAnywhere, Category = "Streaks")
	FVector2D StreakLength = FVector2D(110.f, 220.f);

	UPROPERTY(EditAnywhere, Category = "Streaks")
	float StreakWidth = 10.f;

	UPROPERTY(EditAnywhere, Category = "Streaks")
	float StreakLife = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Streaks")
	int32 MaxStreaks = 48;

	/** Feet off the ground this frame (0 standing; large when nothing is under the brawler). */
	float GetHeightOffGround() const { return HeightOffGround; }

protected:
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void UpdateShadow(const FVector& Feet, bool bShow, float Size);
	void UpdateDust(const FVector& Feet, bool bInAir, float DeltaTime, float Size);
	void UpdateStreaks(bool bFlyingFast, float Speed, float DeltaTime, float Size);
	void StartDust(const FVector& Ground, float Strength, float Size);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ShadowMaterialAsset;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DustMaterialAsset;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> StreakMaterialAsset;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> PlaneMesh;

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> Shadow;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShadowMaterial;

	/** A few dust rings, reused: each one's decal, its material, its age (negative: free) and how strong it is. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDecalComponent>> DustDecals;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DustMaterials;

	TArray<float> DustAge;
	TArray<float> DustStrength;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Streaks;

	struct FStreak
	{
		FTransform Transform;
		float Age = 0.f;
	};
	TArray<FStreak> LiveStreaks;
	float StreakDebt = 0.f;

	float HeightOffGround = 0.f;
	FVector GroundPoint = FVector::ZeroVector;
	bool bHasGround = false;
	bool bWasInAir = false;
	float AirPeakZ = 0.f;
};
