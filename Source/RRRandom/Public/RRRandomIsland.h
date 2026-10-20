#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomIsland.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UTexture2D;
class UStaticMesh;

/**
 * A voxel island built from a character map around the arena, a toy-town valley (MapleStory / Roblox-like): grass cliffs
 * stepping up along the top and the left edge, a waterfall at the top right into a river down the right side with a
 * bridge on each village road, a bay with docks at the bottom, two villages on the teams' starting roads, a ring road for
 * vehicles, low grass hills and a flat field where the fight happens (the game mode's cover and starting spots sit on
 * flat cells). 36 x 36 cells, drawn and checked by char-pipeline/scripts/make_island_map.py.
 *
 * The map is two grids of the same size, one character per cell (CellSize square), top row = far up the screen (+X),
 * first column = far left (-Y), centered on the actor:
 * - Types: '.' grass floor, 'g' grass hill or cliff ('r' too, as before), 's' sand path, 'w' water, 'd' dock over water,
 *   'F' waterfall, 'b' bridge over water (an invisible walkway under the bridge prop).
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
	 * (reeds and lily pads, on water), M giant mushroom, H mushroom house, S signpost, N / Z fence along X / Y, O street
	 * lamp, C crates, Y flowers, J bridge, V windmill, K toy blocks; '.' nothing. Each stands on its cell's top, a little
	 * off center, sized at random and turned its kind's way (doors and signs toward the camera; the same on every
	 * machine). Trees, bushes, flowers and signs are looks only; houses, the windmill, crates, toy blocks and mushroom
	 * stems stop everything like cover, fences and lamps stop brawlers only (PropKinds in the .cpp). Tall things stand
	 * along the top and the sides, where they can't hide the arena from the camera.
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
	/** Where flying is allowed (round 65): over the map (X, Y) with the feet up to Max.Z (ARRRandomCharacter keeps flyers inside). */
	FBox GetFlightArea() const;
	/** The map cell under a world spot: its row and column, or false off the map. */
	bool GetCellAt(const FVector2D& WorldLocation, int32& OutRow, int32& OutCol) const;
	/** True on a road cell ('s'), where the island lays its own dirt tiles instead of the grass floor's. */
	bool IsRoadAt(const FVector2D& WorldLocation) const;
	/** True once the island has laid itself out (BeginPlay). */
	bool IsBuilt() const { return bBuilt; }

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

	/** The props in PropKinds' order (the .cpp); any that failed to load are null and left out. */
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

	/** The roads: Tripo dirt floor tiles (/Game/Environment/Path/SM_DirtTile_*, 180 square, origin in the middle of the top),
	 * and the grass clumps and pebbles along their edges. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMesh>> DirtTiles;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RoadTuft;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> RoadPebbles;

	/** Where the land is, blurred out over the water, for the water's light shallows and foam (M_IslandWater's "ShoreMask"). */
	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ShoreMask;

	/** The floor the island stands on (Build). */
	float FloorZ = 0.f;
	bool bBuilt = false;
};
