#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomFirework.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * One firework for the results screen: a shell climbs from the ground trailing sparks, bursts into a ball of glowing
 * sparks that fall, slow down, crackle and fade, and lights the scene around it with a colored point light that flares
 * at the burst and dies away. Destroys itself when done. Cosmetic and local to one machine (not replicated).
 * Sparks are instances of a small sphere with /Game/Effects/Fireworks/M_FireworkSpark (per-instance color).
 */
UCLASS(NotPlaceable)
class RRRANDOM_API ARRRandomFirework : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomFirework();

	virtual void Tick(float DeltaSeconds) override;

	/** Starts the shell from Ground up to Ground + Height, bursting in Color. Seed varies the burst. */
	void Launch(const FVector& Ground, float Height, const FLinearColor& Color, int32 Seed);

	/** Called with the burst's color the moment the shell bursts. */
	TFunction<void(const FLinearColor&)> OnBurst;

	UPROPERTY(EditAnywhere, Category = "Firework")
	float RiseTime = 0.75f;

	UPROPERTY(EditAnywhere, Category = "Firework", meta = (ClampMin = "1"))
	int32 BurstSparks = 70;

	/** Spark speed out of the burst (cm/s), and how fast air slows them (per second) and gravity pulls them. */
	UPROPERTY(EditAnywhere, Category = "Firework")
	FVector2D BurstSpeed = FVector2D(320.f, 520.f);
	UPROPERTY(EditAnywhere, Category = "Firework")
	float Drag = 1.6f;
	UPROPERTY(EditAnywhere, Category = "Firework")
	float Gravity = 260.f;

	/** Seconds a burst spark glows. */
	UPROPERTY(EditAnywhere, Category = "Firework")
	FVector2D SparkLife = FVector2D(1.3f, 2.f);

	/** Spark size (cm) at the burst. */
	UPROPERTY(EditAnywhere, Category = "Firework")
	float SparkSize = 16.f;

	/** The burst's light: brightest right at the burst, gone after LightTime. */
	UPROPERTY(EditAnywhere, Category = "Firework")
	float LightIntensity = 60000.f;
	UPROPERTY(EditAnywhere, Category = "Firework")
	float LightRadius = 1800.f;
	UPROPERTY(EditAnywhere, Category = "Firework")
	float LightTime = 1.3f;

private:
	struct FSpark
	{
		FVector Location;
		FVector Velocity;
		float Age = 0.f;
		float Life = 1.f;
		float Size = 1.f;
		/** Falls and slows like a burst spark (trail sparks just hang and fade). */
		bool bBurst = false;
		/** White-hot instead of the burst's color. */
		bool bWhite = false;
	};

	void AddSpark(const FVector& Location, const FVector& Velocity, float Life, float Size, bool bInBurst);
	void Burst();
	void UpdateSparks(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Firework")
	TObjectPtr<UInstancedStaticMeshComponent> Sparks;

	UPROPERTY(VisibleAnywhere, Category = "Firework")
	TObjectPtr<UPointLightComponent> Light;

	FRandomStream Stream;
	TArray<FSpark> SparkList;
	FVector Start = FVector::ZeroVector;
	FVector Peak = FVector::ZeroVector;
	FLinearColor BurstColor = FLinearColor::White;
	float Elapsed = 0.f;
	float TrailTimer = 0.f;
	bool bLaunched = false;
	bool bBurst = false;
};
