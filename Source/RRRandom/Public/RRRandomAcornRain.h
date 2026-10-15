#pragma once

#include "CoreMinimal.h"
#include "RRRandomUltimate.h"
#include "RRRandomAcornRain.generated.h"

class UInstancedStaticMeshComponent;

/**
 * The squirrel's ultimate, "Acorn Rain" (도토리 비): its player picks one wide area (ARRRandomUltimate: anywhere; a bot's
 * squirrel picks where most enemies are), then acorns pour down on it for 3 + the die's face seconds while the squirrel
 * goes back to the fight. Every acorn that lands hurts the enemies right under it a little and sends them skidding, so the
 * area is a place to get out of.
 *
 * The rain is the same on every machine: MulticastRain hands out the spot and a seed, and each acorn's time and place come
 * from that seed (the server's landings deal the damage). Acorns are SM_AcornNut + SM_AcornCap (char-pipeline/scripts/
 * blender_acorn.py, ue_import_acorn.py) as two instanced meshes in Dice Land's candy material, colored per instance.
 */
UCLASS()
class RRRANDOM_API ARRRandomAcornRain : public ARRRandomUltimate
{
	GENERATED_BODY()

public:
	ARRRandomAcornRain();

	virtual bool WantsSpots() const override { return true; }
	virtual bool IsBusy() const override { return bRaining; }
	virtual bool HoldsBrawler() const override { return IsPicking(); }
	virtual FString GetTitle() const override { return TEXT("ACORN RAIN"); }
	virtual FString GetStatusLine() const override;

	/** Acorns a second, how high they fall from and how long it takes. */
	UPROPERTY(EditAnywhere, Category = "Acorn Rain")
	float AcornsPerSecond = 32.f;
	UPROPERTY(EditAnywhere, Category = "Acorn Rain")
	float DropHeight = 1400.f;
	UPROPERTY(EditAnywhere, Category = "Acorn Rain")
	float FallTime = 0.6f;

	/** Each acorn: damage to an enemy within HitRadius of where it lands, and how hard it sends them skidding. */
	UPROPERTY(EditAnywhere, Category = "Acorn Rain")
	float AcornDamage = 8.f;
	UPROPERTY(EditAnywhere, Category = "Acorn Rain")
	float HitRadius = 130.f;
	UPROPERTY(EditAnywhere, Category = "Acorn Rain")
	float SkidSpeed = 520.f;

	/** How big the acorns are drawn (the meshes are 55 units tall): cartoon big, to read from high up. */
	UPROPERTY(EditAnywhere, Category = "Acorn Rain")
	float AcornScale = 2.5f;

	/** Seconds of rain on top of the die's face. */
	UPROPERTY(EditAnywhere, Category = "Acorn Rain")
	float BaseSeconds = 3.f;

protected:
	virtual void OnStarted() override;
	virtual void TickUltimate(float DeltaSeconds) override;
	virtual void StartAction(const FVector& Target) override;

private:
	UFUNCTION(NetMulticast, Reliable)
	void MulticastRain(FVector_NetQuantize Spot, int32 Seed);

	/** Acorn Index of the rain: when it lands (seconds after the start) and where. */
	void GetAcorn(int32 Index, float& OutLandTime, FVector& OutWhere, float& OutSpin) const;
	/** An acorn down: a pop here, damage on the server. */
	void LandAcorn(const FVector& Where);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Nuts;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInstancedStaticMeshComponent> Caps;

	bool bRaining = false;
	FVector RainSpot = FVector::ZeroVector;
	int32 RainSeed = 0;
	float RainTime = 0.f;
	float RainSeconds = 0.f;
	int32 AcornCount = 0;
	/** The next acorn that hasn't landed. */
	int32 NextToLand = 0;
	/** Server: enemies hit so far. */
	int32 AcornHits = 0;
};
