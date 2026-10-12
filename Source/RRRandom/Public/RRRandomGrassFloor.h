#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomGrassFloor.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * Covers the floor it stands over with grass tiles (/Game/Environment/Grass, 180 square, one per cell): each cell gets
 * a random tile of the set (weighted, never the same as the cell before it or the one beside it) turned a random
 * quarter turn, so the few Tripo tiles don't read as a repeating grid. A flat plane of the tiles' average green lies
 * just under them, so a hairline gap between two tiles shows grass rather than the floor.
 * Looks only: no collision, so the floor underneath stays what brawlers walk on and traces hit.
 * The game mode spawns it on the host; it replicates with its seed, and every machine finds the same floor under it
 * and lays the same tiles.
 */
UCLASS()
class RRRANDOM_API ARRRandomGrassFloor : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomGrassFloor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Same seed, same floor: the same tiles on every machine. Set before the floor begins play. */
	UPROPERTY(EditAnywhere, Replicated, Category = "Grass")
	int32 Seed = 1;

	/** The tile meshes, and how often each comes up relative to the others. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	TArray<TObjectPtr<UStaticMesh>> Tiles;

	UPROPERTY(EditAnywhere, Category = "Grass")
	TArray<float> TileWeights;

	/** Side of one tile. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	float TileSize = 180.f;

	/** Tile tops sit this far over the floor, the gap plane half that. */
	UPROPERTY(EditAnywhere, Category = "Grass")
	float Lift = 2.f;

protected:
	virtual void BeginPlay() override;

private:
	/** Finds the floor under the actor and lays the tiles over all of it. */
	void LayTiles();

	UPROPERTY(VisibleAnywhere, Category = "Grass")
	TObjectPtr<UStaticMeshComponent> Under;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> TileInstances;
};
