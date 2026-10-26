#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomLava.generated.h"

class ARRRandomCharacter;
class UBoxComponent;
class UInstancedStaticMeshComponent;
class UPointLightComponent;

/** A column of Dice Town's ground flooded with lava: its cell, its middle in the world (flat) and the top of the floor under it. */
struct FRRLavaColumn
{
	FIntPoint Cell;
	FVector2D Middle;
	float FloorTop;
};

/**
 * Dice Town's lava (round 89): where a crater breaks through the island's floor, the mantle under it wells up and fills the
 * crater. Dice Town works out which columns are lava from the wrecks (the same on every machine, so nothing more is
 * replicated) and hands them over (SetColumns); this draws them and burns who stands in them, each machine its own (the
 * server's burns count).
 * - Breaking through: the cracks glow for GlowTime, the lava bursts up in a fountain (BLOOP!) flinging blobs that leave
 *   embers on the floor round it for EmberSeconds, then it rises over RiseTime to LavaBelowFloor under the floor. Columns
 *   that stop being lava (the crater mends) sink away.
 * - Looks: a box per column from the crater's floor to the lava's top in glowing candy jam (/Game/Effects/Lava/M_Lava,
 *   char-pipeline/scripts/ue_make_lava.py), bubbles popping and sparks rising off it, and at most MaxLights point lights
 *   spread over it, flickering.
 * - Burns (server): an animal friend whose feet are in it takes BurnPerSecond, hops up out of it (HOT!) and keeps burning
 *   AfterBurnPerSecond for AfterBurnSeconds once out; the fountain's embers burn like the dragon's (EmberDamage a tick).
 * - Bots: the server lays boxes over it that cut it out of the navmesh, so they walk round it.
 */
UCLASS(NotPlaceable)
class RRRANDOM_API ARRRandomLava : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomLava();

	/** The columns flooded now (by cell), the lava's top in the world, and the breakthroughs new just now (for the fountain). */
	void SetColumns(const TArray<FRRLavaColumn>& Columns, float InTopZ, const TArray<FVector>& NewBreaches);
	/** Whether a spot's column is lava that has risen (for burns and melting ore), and its top. */
	bool IsRisenAt(const FIntPoint& Cell) const;

	virtual void Tick(float DeltaSeconds) override;

	static constexpr float GlowTime = 0.5f;
	static constexpr float RiseTime = 1.5f;
	static constexpr float SinkTime = 0.6f;
	static constexpr float BurnPerSecond = 25.f;
	static constexpr float AfterBurnPerSecond = 8.f;
	static constexpr float AfterBurnSeconds = 2.f;
	static constexpr float EmberSeconds = 3.f;
	static constexpr float EmberDamage = 4.f;
	static constexpr float EmberRadius = 90.f;
	static constexpr float BurnTick = 0.25f;
	static constexpr float HopSpeed = 560.f;
	static constexpr int32 MaxLights = 8;

protected:
	virtual void BeginPlay() override;

private:
	struct FColumn
	{
		FIntPoint Cell;
		FVector2D Middle;
		float FloorTop;
		/** When it was flooded (the breakthrough's glow first), and when it stopped (sinking), or a negative. */
		float Born;
		float Gone = -1.f;
	};
	struct FFountain
	{
		FVector Where;
		float Start;
		bool bBurst = false;
	};
	struct FEmber
	{
		FVector Where;
		float From;
		float Until;
	};

	/** How far up a column has risen (0 to 1, down again as it sinks). */
	float RiseOf(const FColumn& Column, float Now) const;
	void UpdateSurface(float Now);
	void UpdateLights(float Now);
	void UpdateEffects(float DeltaSeconds, float Now);
	void BurnPeople(float Now);
	/** Server: the boxes that cut the lava out of the navmesh, rows of columns at a time. */
	void UpdateNavBlockers();

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Surface;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UPointLightComponent>> Lights;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> NavBlockers;

	TMap<FIntPoint, FColumn> Columns;
	/** The columns drawn, in the order of Surface's instances (rebuilt when they change). */
	TArray<FIntPoint> Drawn;
	TArray<FFountain> Fountains;
	TArray<FEmber> Embers;
	float TopZ = 0.f;
	float BurnClock = 0.f;
	float BubbleClock = 0.f;
	/** Server: who was in the lava last and when, when each last hopped out; every machine: when each last said HOT!. */
	TMap<TWeakObjectPtr<ARRRandomCharacter>, float> LastInLava;
	TMap<TWeakObjectPtr<ARRRandomCharacter>, float> LastHop;
	TMap<TWeakObjectPtr<ARRRandomCharacter>, float> LastHot;
	bool bNavDirty = false;
};
