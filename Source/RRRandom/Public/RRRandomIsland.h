#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomIsland.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * A voxel island built from a character map around the arena: rock cliffs and a waterfall along the top, a river down
 * the right side, a bay with docks at the bottom, low grass hills and sand paths in between, and a flat middle where
 * the fight happens (the game mode's cover and starting spots sit on flat cells).
 *
 * The map is two grids of the same size, one character per cell (CellSize square), top row = far up the screen (+X),
 * first column = far left (-Y), centered on the actor:
 * - Types: '.' grass floor, 'g' grass hill, 'r' rock, 's' sand path, 'w' water, 'd' dock over water, 'F' waterfall.
 * - Heights: a digit per cell, in LevelHeight steps; used by hills, rock and waterfall (a hill of 1 or 2 can be climbed
 *   onto, anything from 3 up is a wall).
 * Hills, rock and the waterfall are solid like cover (world static, block everything), so brawlers, shots and the bots'
 * sight treat them as walls. Water stops brawlers only (an invisible wall that shots and the bots' sight pass), so
 * nobody walks out to sea. Sand paths and docks are low solid slabs, stepped onto; water surfaces are looks only.
 * All of them sit over the grass floor's blades. Beyond the map the floor
 * is covered with water too.
 * Stand-in looks until Tripo blocks arrive: grass hills use the grass blocks, the rest engine cubes and planes with
 * /Game/Environment/Island materials (rock blocks each a slightly different brown). Every machine builds the same
 * island from the same map; the game mode spawns it on the host and it replicates.
 */
UCLASS()
class RRRANDOM_API ARRRandomIsland : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomIsland();

	UPROPERTY(EditAnywhere, Category = "Island")
	TArray<FString> TypeRows;

	UPROPERTY(EditAnywhere, Category = "Island")
	TArray<FString> HeightRows;

	UPROPERTY(EditAnywhere, Category = "Island")
	float CellSize = 180.f;

	UPROPERTY(EditAnywhere, Category = "Island")
	float LevelHeight = 80.f;

	/** Water surfaces sit this far over the floor (over the grass floor's blades), docks this far over the water. */
	UPROPERTY(EditAnywhere, Category = "Island")
	float WaterLift = 16.f;

	UPROPERTY(EditAnywhere, Category = "Island")
	float DockLift = 8.f;

	/** Height of the invisible wall over water. */
	UPROPERTY(EditAnywhere, Category = "Island")
	float WaterWallHeight = 400.f;

protected:
	virtual void BeginPlay() override;

private:
	/** Reads the map and lays every block. */
	void Build();
	UInstancedStaticMeshComponent* MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bSolid, int32 CustomFloats = 0);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> GrassBlocks;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Cube;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Plane;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RockMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SandMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WoodMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WaterMaterial;
};
