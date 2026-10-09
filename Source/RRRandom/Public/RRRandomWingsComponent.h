#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RRRandomWingsComponent.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Glowing feathered wings on a brawler's back while it has the flight power (or is still gliding down after it),
 * on every machine. Two copies of /Game/Effects/Wings/SM_Wing (the left one mirrored), animated here:
 * folded up behind the back on the ground, beating hard while rising, held open with a slow ripple while gliding,
 * swept back while dashing. They unfold when the power starts and fold away when it ends.
 * The wings shed little glowing stars (SM_WingSparkle, drawn as instances turned toward the camera) that pop, twinkle
 * and drift down behind the brawler: a trickle on the ground, a shower while beating up. Purely cosmetic.
 * Sits where the owner attaches it (between the shoulder blades); the wing roots are spread WingRootGap apart.
 */
UCLASS(ClassGroup = (RRRandom))
class RRRANDOM_API URRRandomWingsComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URRRandomWingsComponent();

	UPROPERTY(EditAnywhere, Category = "Wings")
	TObjectPtr<UStaticMesh> WingMesh;

	/** Size of each wing; the mesh is 142 long from root to tip. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	float WingScale = 0.85f;

	/** Distance between the two wing roots. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	float WingRootGap = 16.f;

	/** Seconds to unfold when the flight power starts, or fold away when it ends. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	float ShowTime = 0.25f;

	/** The star the wings shed; radius 5. */
	UPROPERTY(EditAnywhere, Category = "Wings|Sparkles")
	TObjectPtr<UStaticMesh> SparkleMesh;

	/** Each star's size is picked from this range (times the mesh's radius of 5). */
	UPROPERTY(EditAnywhere, Category = "Wings|Sparkles")
	FVector2D SparkleScale = FVector2D(1.3f, 2.6f);

	/** Each star lasts a random number of seconds in this range. */
	UPROPERTY(EditAnywhere, Category = "Wings|Sparkles")
	FVector2D SparkleLife = FVector2D(0.4f, 0.9f);

	/** Most stars alive at once per brawler. */
	UPROPERTY(EditAnywhere, Category = "Wings|Sparkles")
	int32 MaxSparkles = 80;

protected:
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** How the wings are held: angles in degrees, beat rate in beats per second, size as a share of WingScale, stars per second. */
	struct FWingPose
	{
		float Lift = 0.f;
		float Sweep = 0.f;
		float BeatAngle = 0.f;
		float BeatRate = 0.f;
		float Size = 1.f;
		float SparkleRate = 0.f;
	};

	struct FSparkle
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		float Age = 0.f;
		float Life = 1.f;
		float Size = 1.f;
		float Spin = 0.f;
	};

	void PlaceWing(UStaticMeshComponent* Wing, float Side, float Lift, float Sweep, float Size) const;
	/** Sheds new stars from the wings (SpawnRate per second) and moves, ages and draws the live ones. */
	void UpdateSparkles(float DeltaTime, float SpawnRate);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> LeftWing;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> RightWing;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Sparkles;

	FWingPose Pose;
	/** 0 hidden .. 1 fully out. */
	float Shown = 0.f;
	/** Where in the wing beat we are, in radians. */
	float BeatPhase = 0.f;
	TArray<FSparkle> LiveSparkles;
	/** Fraction of a star carried over to the next frame. */
	float SparkleDebt = 0.f;
	TArray<FTransform> SparkleTransforms;
};
