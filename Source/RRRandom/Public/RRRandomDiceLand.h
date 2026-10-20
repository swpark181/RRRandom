#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomDiceLand.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;
struct FRandomStream;

/**
 * "Dice Land", the third person mode's map: a pastel island floating over a sea of clouds, built in code around the
 * arena (the top view keeps ARRRandomIsland). Everything is made of a few smooth shapes (char-pipeline/scripts/
 * blender_dice_land.py: a puffy die, a cushion tile, a ball, a trunk, a ring), tinted per instance, the way the concept
 * sketch was drawn. Laid out point-symmetrically, so both teams get the same ground:
 * - A plaza in the middle: two rounded slabs under a spinning rainbow die with two halos, ringed by small dice.
 * - A ring road for vehicles: the main road down the middle (X), each team's starting road across it, and a lane down
 *   each side; floor tiles are puffy cushions, cream on the roads, mint on the grass, with flowers about.
 * - The field inside the ring, full of cover: walls of dice, ball hedges, big half-buried dice and dice barricades.
 * - Outside the side lanes, a wood of trees with dice for leaves on one end and a field of giant dice on the other
 *   (swapped on the far side).
 * - Under the rim, an upside-down cone of stacked dice; around it a sea of ball clouds, cumulus in the sky, an arch of
 *   rainbow dice beyond each end, rainbow dice bobbing in the air and three small sky islands out of reach.
 * Brawlers walk on the level's own floor (hidden here; the tiles are looks only). Cover is hidden boxes, solid like the
 * cover blocks (world static, block everything), so shots and the bots' sight treat it as walls; tree trunks too. A tall
 * hidden wall around the rim stops brawlers only, flying ones included.
 * Every machine builds the same land from the same seeds; the game mode spawns it on the host and it replicates.
 */
UCLASS()
class RRRANDOM_API ARRRandomDiceLand : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomDiceLand();

	virtual void Tick(float DeltaSeconds) override;

	/** The map's area in the world (X up the minimap, Y to the right), a square around the island. */
	FBox2D GetMapBounds() const;
	/** What the minimap shows at a world spot (sky off the island), and how high it stands there (for its shading). */
	FColor GetMapColor(const FVector2D& WorldLocation, int32& OutLevels) const;
	/** True once the land has laid itself out (BeginPlay). */
	bool IsBuilt() const { return bBuilt; }

	/** For wrecks (round 60, ARRRandomWreckage): the hidden boxes that stop things, and the looks that may stand over them. */
	UInstancedStaticMeshComponent* GetWreckSolid() const { return Solid; }
	void GetWreckLooks(TArray<UInstancedStaticMeshComponent*>& OutLooks) const { OutLooks = { Dice.Get(), BlankDice.Get(), Balls.Get(), Trunks.Get() }; }

protected:
	virtual void BeginPlay() override;

private:
	/** Something solid on the minimap: a turned box (center, half size, yaw) in a color, so many levels high. */
	struct FFootprint
	{
		FVector2D Center;
		FVector2D Half;
		float Yaw;
		FColor Color;
		int32 Levels;
	};

	void Build();
	UInstancedStaticMeshComponent* MakeInstances(UStaticMesh* Mesh, bool bSolid, int32 CustomFloats);
	/** The rim's distance from the middle in a direction (radians from +X toward +Y). */
	float RimRadius(float Theta) const;
	/** The floor tile's color at a spot (relative to the middle): plaza, road, wood or grass. */
	FColor GroundColor(const FVector2D& Local, bool bChecker) const;
	bool IsOnRoad(const FVector2D& Local, float Margin) const;
	/** Free for a thing this wide: on the island, off the roads, the plaza and the starting rows, clear of what's placed. */
	bool IsClear(const FVector2D& Local, float Radius) const;
	/** A spot relative to the middle, on the floor, lifted this high. */
	FVector At(const FVector2D& Local, float Lift) const;

	/** One instance tinted a color (and its pips another, on the dice). */
	void AddColored(UInstancedStaticMeshComponent* Instances, const FTransform& Transform, const FColor& Color, const FColor& PipColor = FColor::White) const;
	/** A die (with pips) in a color; solid adds its box to the hidden cover. */
	void AddDie(const FVector2D& Local, float Lift, float Size, const FRotator& Turn, const FColor& Color, const FColor& PipColor, bool bSolid);
	/** Hidden cover over a turned box standing on the floor (Half is its half size along its own X and Y), on the minimap. */
	void AddCover(const FVector2D& Local, float Yaw, const FVector2D& Half, float Height, const FColor& MapColor);
	/** Both halves: the thing here and the same thing turned half way round the middle. */
	void Mirrored(TFunctionRef<void(const FVector2D&, float)> Place, const FVector2D& Local, float Yaw);
	/** A trunk with dice for leaves: one, two stacked, or a cluster of three. */
	void AddTree(const FVector2D& Local, float Yaw, float Size, int32 Kind);
	/** A row of soft balls with a few flowers, about chest high. Yaw is the way its length runs. */
	void AddHedge(const FVector2D& Local, float Yaw);
	/** Two rows of dice, about chest high. Yaw is the way its length runs. */
	void AddWall(const FVector2D& Local, float Yaw);
	void AddBarricade(const FVector2D& Local, float Yaw);
	void AddBlock(const FVector2D& Local, float Size, float Yaw);
	/** Rings of dice under an island's rim, wide at the top and narrowing down to a point. */
	void AddUnderside(const FVector& Middle, TFunctionRef<float(float)> Radius, int32 LayerCount, float DieSize);
	void AddSkyIsland(const FVector& Middle, float Radius, int32 Kind);
	/** Seven bands of dice in a half circle standing across Across (a unit direction), its middle at Base. */
	void AddRainbowArch(const FVector& Base, const FVector& Across, float Radius, float DieSize);
	/** A heap of balls: a cloud. */
	void AddCloud(const FVector& Middle, float Size, float Flatten, FRandomStream& Stream);

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Die;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> BlankDie;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> RainbowDie;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Tile;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Ball;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Trunk;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Ring;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cube;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> TileMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CloudMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> CandyMaterial;

	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Dice;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> BlankDice;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Tiles;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Underlay;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Balls;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Trunks;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Clouds;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Solid;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> RimWalls;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Floaters;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Centerpiece;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> Halos;
	/** Every material on the land, made dynamic so rr.DiceLand.Saturation / Brightness reach them live. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Tunable;

	/** The floor's height under the middle; everything stands on it. */
	float FloorZ = 0.f;
	/** The bobbing rainbow dice: where each one rests, how far and fast it bobs and spins, and its phase. */
	TArray<FTransform> FloaterRest;
	TArray<FVector4f> FloaterMotion;
	/** What's been placed (spot relative to the middle, radius), so nothing lands on top of something else. */
	TArray<FVector3f> Taken;
	TArray<FFootprint> Footprints;
	float Clock = 0.f;
	float PastelTimer = 0.f;
	/** The color values last given to Tunable (none yet: below any real value). */
	float AppliedSaturation = -1.f;
	float AppliedBrightness = -1.f;
	bool bBuilt = false;
};
