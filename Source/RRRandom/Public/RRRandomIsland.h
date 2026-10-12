#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomIsland.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;

/**
 * A voxel island built from a character map around the arena: grass cliffs stepping up along the top and the left edge,
 * a waterfall at the top right, a river down the right side, a bay with docks at the bottom, low grass hills and sand paths in between, and a flat middle where
 * the fight happens (the game mode's cover and starting spots sit on flat cells).
 *
 * The map is two grids of the same size, one character per cell (CellSize square), top row = far up the screen (+X),
 * first column = far left (-Y), centered on the actor:
 * - Types: '.' grass floor, 'g' grass hill or cliff ('r' too, as before), 's' sand path, 'w' water, 'd' dock over water,
 *   'F' waterfall.
 * - Heights: a digit per cell, in LevelHeight steps; used by hills and the waterfall (a step of 1 or 2 can be climbed
 *   onto, anything from 3 up is a wall).
 * Hills and the waterfall are solid like cover (world static, block everything), so brawlers, shots and the bots'
 * sight treat them as walls. Water stops brawlers only (an invisible wall that shots and the bots' sight pass), so
 * nobody walks out to sea. Sand paths and docks are low solid slabs, stepped onto; water surfaces are looks only.
 * All of them sit over the grass floor's blades. Beyond the map the floor
 * is covered with water too.
 * Looks: a hill or cliff is a stack of soil blocks, one per level, under one of the grass blocks (the same Tripo grass as
 * the cover); sand, docks, water and the waterfall are engine cubes and planes with /Game/Environment/Island materials.
 * Every machine builds the same island from the same map; the game mode spawns it on the host and it replicates.
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

	/**
	 * Scenery, one letter per cell laid out like TypeRows: P pine, T round tree, B flowering bush, R boulders, L pond plants
	 * (reeds and lily pads, on water), '.' nothing. Each stands on its cell's top, a little off center, turned and sized at
	 * random (the same on every machine). Looks only, no collision: never in the way of brawlers, shots or sight. Tall trees
	 * stand only along the top and the sides, where they can't hide the arena from the camera.
	 */
	UPROPERTY(EditAnywhere, Category = "Island")
	TArray<FString> PropRows;

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

	/** The map's area in the world (X up the screen, Y to the right). */
	FBox2D GetMapBounds() const;
	/** The map cell under a world spot: its row and column, or false off the map. */
	bool GetCellAt(const FVector2D& WorldLocation, int32& OutRow, int32& OutCol) const;

protected:
	virtual void BeginPlay() override;

private:
	/** Reads the map and lays every block. */
	void Build();
	UInstancedStaticMeshComponent* MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bSolid, int32 CustomFloats = 0);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> GrassBlocks;

	/** The grass blocks' soil alone, stacked under a grass block for each level of a hill or cliff. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> SoilBlocks;

	/** The props in PropRows' letter order (PTBRL); any that failed to load are null and left out. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> PropMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Cube;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> Plane;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SandMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WoodMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WaterMaterial;
};
