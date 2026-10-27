#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomDiceTown.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
struct FRandomStream;
struct FRRWreck;
struct FRRBrokenBit;
struct FRRRegrow;

/**
 * "Dice Town", the third person mode's big map (about ten times Dice Land's ground): a floating island town built of
 * square blocks stacked like MapleStory 2, from the first concept image of round 36. Built in code on a grid of 100-unit
 * blocks (char-pipeline/scripts/blender_block_town.py: a bevelled block whose top can take a second color, and a die
 * block), point-symmetric so both teams get the same ground (blue at -X, red at +X):
 * - The middle: a plaza round a giant die on a stepped pedestal, crossed by the two main roads, a ring road further out.
 * - Downtown round the plaza: two-story houses (hip roofs) and shops (flat roofs with a parapet) with alleys between.
 * - Each team's base courtyard on the main road, a watchtower either side; behind it a stepped hill with a keep.
 * - Outside the ring: a block forest with a hill of giant dice on each team's left, a canal quarter with arched
 *   footbridges and a waterfall off the rim on each team's right.
 * - The dreamier pass (round 47, after a reference picture): the dice hill is a mountain of giant dice with stairs to
 *   its top, a house that is a giant die downtown and outside the ring, rainbows of dice across the main roads, dice on
 *   the roofs, candy trees, spotted mushrooms and lollipops, star lamps along the roads, floating islands joined by a
 *   rainbow of dice, moons, and dice and stars drifting in the air (-RRTownClassic builds the town without it).
 * Every building with an upper floor has a way up: an outside staircase to a landing at a door on the upper floor, and on
 * flat roofs a second flight to the roof. Every door and window is 3 blocks wide and 4 high (300 x 400), so any brawler
 * fits through, a giant too (168 x 384); the windows' sills are one block high, so a jump climbs onto them and the far side
 * drops to the ground.
 * Stairs are 45 degree steps of half a block: they walk as a ramp (brawlers on this map walk slopes up to 46 degrees).
 * The grid of blocks also gives the bots walking routes (FindPath): each spot with room to stand, linked to its neighbors
 * on the same level, up and down stairs, up one block (a jump climbs it) and down drops off ledges.
 * What stops things is hidden boxes (world static, block everything), merged from the grid; a tall hidden wall round the
 * rim stops brawlers only. Every machine builds the same town from the same seeds; the game mode spawns it on the host.
 * Round 89: everything breaks (the giant dice whole), what is no longer held up from the ground comes down as one piece,
 * the ground digs (three layers, craters as stepped bowls) and a crater dug through all three fills with lava from the
 * mantle (ARRRandomLava); all of it worked out from the wrecks on every machine alike (ApplyWrecks).
 */
UCLASS()
class RRRANDOM_API ARRRandomDiceTown : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomDiceTown();

	virtual void Tick(float DeltaSeconds) override;

	/** The teams start this far apart, on the main road at each base (the game mode's TeamSpawnDistance on this map). */
	static constexpr float TeamSpawnDistance = 8000.f;

	/** The map's area in the world (X up the minimap, Y to the right), a square round the island. */
	FBox2D GetMapBounds() const;
	/**
	 * Where flying is allowed (round 65): over the map (X, Y) with the feet up to Max.Z, high enough to land on its tallest
	 * structure and look over it (ARRRandomCharacter keeps flyers inside).
	 */
	FBox GetFlightArea() const;
	/** What the minimap shows at a world spot (sky off the island), and how many blocks high it stands there. */
	FColor GetMapColor(const FVector2D& WorldLocation, int32& OutLevels) const;
	/** True once the town has laid itself out (BeginPlay). */
	bool IsBuilt() const { return bBuilt; }

	/**
	 * A walking route from a spot to another (feet or capsule middles, either works): the route's corners at foot height,
	 * from just after From to the spot nearest To that can be reached. False when either end isn't near walkable ground.
	 */
	bool FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const;
	/** A spot on the ground anyone can walk to, at foot height, at most Radius from the middle (in 2D). */
	FVector GetRandomWalkSpot(FRandomStream& Stream, float Radius) const;

	/**
	 * Smashes and mends this machine's town to match the wrecks in force (round 60, ARRRandomWreckage): the blocks over the
	 * floor inside them go (giant dice stay), and with them what stands there drawn as a model (roofs, Tripo trees, hedges,
	 * lamps, awnings, whole flights of stairs); the hidden boxes that stop things are cut round the holes, and blocks inside
	 * walls that a hole lays bare are drawn. OutBroken gets the blocks that went just now (for the rubble), OutRegrow what is
	 * shown again (to grow back into place). The bots' routes stay as built.
	 */
	void ApplyWrecks(TConstArrayView<FRRWreck> InWrecks, TArray<FRRBrokenBit>& OutBroken, TArray<FRRRegrow>& OutRegrow);

	// Round 89: everything breaks, what loses its support falls, the ground digs down to the lava
	/**
	 * Server: the boxes of big things the last ApplyWrecks made (a part of a building coming down, a lava pool), once; the
	 * wreckage keeps the wrecks that made them longer (ARRRandomWreckage::MarkBigWrecks). False when there were none.
	 */
	bool ConsumeBigAreas(TArray<FBox>& OutAreas);
	/** Whether there is lava at a spot (in the world, flat), and how high its top is. */
	bool GetLavaAt(const FVector& WorldLocation, float& OutTop) const;
	/** The lava's top: this far under the floor. */
	static constexpr float LavaBelowFloor = 60.f;
	/** The grid cell (X, Y) a spot in the world is over. */
	FIntPoint GetCellAt(const FVector& WorldLocation) const;
	/** The floor's height (level 0's top). */
	float GetFloorZ() const { return FloorZ; }
	/** Changes each time something broken, mended or flooded with lava changes the minimap. */
	int32 GetMapVersion() const { return MapVersion; }

	/**
	 * A defense stage (round 90, 1..3; 0 for the town itself): set before it finishes spawning, replicated so every machine
	 * builds the same. A stage is the town's look laid out round the giant dice tower in its middle (ARRRandomDiceTower): a
	 * paved plaza inside the wall ring, a ring road round it, eight roads out to the villains' gates near the rim, and between
	 * the roads houses, towers, trees and cover. Stage 1 is half the town's width, stage 2 two thirds, stage 3 the whole
	 * town; the bigger, the more there is between the roads.
	 */
	void SetDefenseStage(int32 InStage) { DefenseStage = InStage; }
	int32 GetDefenseStage() const { return DefenseStage; }
	/** The villains' gates (feet on the road just inside each), in the order they open (RRDefense::MaxGates). */
	void GetDefenseGates(TArray<FVector>& OutGates) const;
	/** How far the wall ring and the island's rim (on average) are from the middle. */
	float GetDefenseRingRadius() const;
	float GetDefenseRimRadius() const;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Placed in a level (/Game/Maps/DiceTown), the editor builds it too, never saved (RF_Transient), to show it and its navmesh. */
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	/** A block's two colors: its body, and its top and grass lip (or a die's pips). */
	struct FLook
	{
		FColor Body;
		FColor Accent;
		bool bDie;
		/** Solid but drawn by something else (a roof or tree mesh); its color still goes on the minimap. */
		bool bUnderMesh;
		/** Round 91: polished (GlossBlocks, the sky and the town show in it): the plaza's tiles. */
		bool bGloss = false;
	};

	/** Where a structure stands: a cell and a quarter turn, mapping its own (U, V) cells onto the grid. */
	struct FFrame
	{
		FIntPoint Origin;
		int32 Turn = 0;
		/** True on the red half (Mirror flips it), for team colors. */
		bool bRed = false;
		FIntPoint Cell(int32 U, int32 V) const;
		FIntPoint Dir(int32 U, int32 V) const;
		/** The same structure on the other team's half: turned half way round the middle. */
		FFrame Mirror() const;
		/** Turned Turn quarters round the middle of the island (not round a cell). */
		static FFrame Around(int32 InTurn);
	};

	/** A flight of stairs: each step half a block deep and high, from a level up Rise blocks. */
	struct FStair
	{
		FIntPoint Start;
		FIntPoint Dir;
		FIntPoint Across;
		int32 Width;
		int32 Base;
		int32 Rise;
		/** Its looks (Steps) and what stops (Solid), as instance ranges: a wreck takes the whole flight. */
		int32 StepFirst = 0;
		int32 StepCount = 0;
		int32 SolidFirst = 0;
		int32 SolidCount = 0;
	};

	/** A spot the bots can stand: a cell and the height of its floor (above the island's floor), maybe on stairs. */
	struct FWalkNode
	{
		FIntPoint Cell;
		float Z;
		/** The run direction of the stairs it is on, or zero. */
		FIntPoint StairDir;
	};

	void Build();
	/** Every district on the grid: ground, roads, plaza, bases, hills, downtown, canals, woods, cover. */
	void LayOut();
	/** A defense stage's layout (round 90) instead of LayOut. */
	void LayOutDefense();
	bool IsDefenseRoad(int32 X, int32 Y) const;
	/** A defense stage's castle (its wall, the plaza inside, the stairs): nothing breaks or digs there. */
	bool IsCastle(int32 X, int32 Y) const;
	/** A defense stage's road out to gate Lane: which way it runs. */
	static FVector2D GetLaneWay(int32 Lane);
	UPROPERTY(Replicated)
	int32 DefenseStage = 0;
	/** The island's size: 1 for the town, less for the smaller defense stages. */
	float RimScale = 1.f;
	/** The defense gates, relative to the middle. */
	TArray<FVector2D> DefenseGates;
	UInstancedStaticMeshComponent* MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bSolid, int32 CustomFloats);

	// The grid
	bool IsInGrid(int32 X, int32 Y, int32 Level) const;
	int32 GridIndex(int32 X, int32 Y, int32 Level) const;
	uint8 Get(int32 X, int32 Y, int32 Level) const;
	void Set(int32 X, int32 Y, int32 Level, uint8 Value);
	/** Fills a box of cells (corners included, in either order) and levels with a look, or clears it with 0. */
	void Fill(const FFrame& Frame, int32 U0, int32 V0, int32 U1, int32 V1, int32 L0, int32 L1, uint8 Value);
	uint8 MakeLook(const FColor& Body, const FColor& Accent, bool bDie = false, bool bUnderMesh = false, bool bGloss = false);
	uint8 HiddenLook(const FColor& Color) { return MakeLook(Color, Color, false, true); }
	uint8 Plain(const FColor& Color) { return MakeLook(Color, Color); }
	bool IsOnIsland(int32 X, int32 Y) const;
	/** The rim's distance from the middle in a direction (radians from +X toward +Y). */
	float RimRadius(float Theta) const;
	/** A cell's middle relative to the island's middle, and a spot there this high above the floor. */
	static FVector2D CellCenter(const FIntPoint& Cell);
	FVector At(const FVector2D& Local, float Height) const;

	/** Room on the grid: inside the island, off the roads, clear of what was placed, on both halves. */
	bool IsFree(const FIntRect& Rect) const;
	void Reserve(const FIntRect& Rect);
	/** The cells a structure's local box covers, with a margin. */
	static FIntRect Footprint(const FFrame& Frame, int32 U0, int32 V0, int32 U1, int32 V1, int32 Margin);
	bool IsRoad(int32 X, int32 Y) const;
	/** Calls Place with the frame and with its mirror on the other half. */
	void Both(const FFrame& Frame, TFunctionRef<void(const FFrame&)> Place);

	// Structures (in their own frame: U across, V deep; the front is V = -1)
	void AddStairs(const FFrame& Frame, int32 U, int32 V, int32 DirU, int32 DirV, int32 AcrossU, int32 AcrossV, int32 Width, int32 Base, int32 Rise, const FColor& Color);
	/**
	 * A building W by D, Stories high (6 blocks each), on the ground: a door front and left, windows all round, and with two
	 * stories an outside staircase up the right side to a landing at the upper door. Flat roofs get a parapet and a second
	 * flight along the back to the roof.
	 */
	void AddBuilding(const FFrame& Frame, int32 W, int32 D, int32 Stories, bool bFlatRoof, int32 Style, int32 Seed);
	/** A door or window: clears it and frames it. Along U on the front or back wall (Side 0/2), along V on the sides. */
	void CutOpening(const FFrame& Frame, int32 W, int32 D, int32 Side, int32 Start, int32 Width, int32 L0, int32 L1, uint8 FrameLook);
	void AddTower(const FFrame& Frame, int32 Style);
	void AddTree(const FIntPoint& Cell, int32 Kind, int32 Seed);
	void AddDie(const FIntPoint& Cell, int32 Level, int32 Size, int32 Kind);
	/** A stepped hill: Levels terraces, each 3 cells in from the one below, with a ramp up each. */
	void AddHill(const FFrame& Frame, int32 W, int32 D, int32 Levels);
	void AddCanal(const FFrame& Frame, int32 Length, int32 Width);
	/** Seven bands of blocks in a half circle across Across, standing at Base. */
	void AddRainbow(const FVector& Base, const FVector& Across, float Radius);
	void AddCloud(const FVector& Middle, float Size, FRandomStream& Stream);

	// After the layout
	void MakeBlocks();
	void MakeCollision();
	void MakeWalkGraph();
	void MakeUnderside();
	void MakeSky();
	void AddColored(UInstancedStaticMeshComponent* Instances, const FTransform& Transform, const FColor& Body, const FColor& Accent);
	/** Draws the block of a cell (one with a face showing), remembered in CellBlocks. */
	void AddCellBlock(int32 X, int32 Y, int32 Level);
	FTransform GetCellTransform(int32 X, int32 Y, int32 Level) const;
	/** The minimap's color and height for a column, as it stands now (broken blocks gone, lava in a crater). */
	void RefreshMapColumn(int32 X, int32 Y);

	// Wrecks (round 60)
	/** Solid and not broken. */
	bool IsFilled(int32 X, int32 Y, int32 Level) const;
	/** A wreck reaches into a box in the world (a column only between the floor and the grid's top). */
	bool Touches(const FRRWreck& Wreck, const FBox& Box) const;
	/** A cell's block drawn or not, as it should be now: broken or buried, hidden; laid bare, drawn (added if never drawn). */
	void RefreshCellBlock(int32 X, int32 Y, int32 Level, bool bGrow, TArray<FRRRegrow>& OutRegrow);

	// The dreamier pass (round 47; -RRTownClassic leaves it out)
	/** Something that drifts in the air (a die, a star): bobbing, turning, maybe circling a point. */
	struct FFloater
	{
		UInstancedStaticMeshComponent* Instances;
		int32 Index;
		FVector Base;
		FRotator Turn;
		float Scale;
		float Bob;
		float Phase;
		/** Degrees a second round its own up axis, and tumbling (dice). */
		float Spin;
		float Tumble;
		/** Circling Base this far out, this many radians a second (0: in place). */
		float Orbit;
		float OrbitRate;
		/** Gone with a wreck (drawn at no size). */
		bool bWrecked = false;
	};
	/** A spot in a frame (fractional cells), this high above the floor, and the yaw its +U faces. */
	FVector FrameSpot(const FFrame& Frame, float U, float V, float Height) const;
	static float FrameYaw(const FFrame& Frame);
	void AddDieInFrame(const FFrame& Frame, int32 U, int32 V, int32 Level, int32 Size, int32 Kind);
	/** Giant dice stacked in three tiers against one corner, a flight of stairs up each tier (replaces the dice hill). */
	void AddDiceMountain(const FFrame& Frame);
	/** A rainbow of dice in seven bands standing across a main road (U across it, two cells thick), high enough to pass. */
	void AddDiceArch(const FFrame& Frame);
	/** A house that is a giant die (12 cells a side): pips painted on the walls and raised on the roof, doors on three sides, an outside staircase to the roof. */
	void AddDieHouse(const FFrame& Frame, int32 Seed);
	/** Candy trees (Kind 2), spotted mushrooms (3), lollipops (4). */
	void AddDreamTree(const FIntPoint& Cell, int32 Kind, int32 Seed);
	/**
	 * A tree as a Tripo model (char-pipeline/input/dicetown_props, /Game/Environment/DiceTown/Props): green round or tiered
	 * (Kind 0 small, 1 big), candy (2), mushroom (3), lollipop (4). Only its trunk (a mushroom's stem and cap) is solid on the
	 * grid, colored for the minimap. False when the models are missing (the trees are blocks then).
	 */
	bool AddPropTree(const FIntPoint& Cell, int32 Kind, int32 Seed);
	/** A striped awning over a door on a building's front, Start and Width along it. */
	void AddAwning(const FFrame& Frame, int32 Start, int32 Width, const FColor& Color);
	/** Candy striped lamp posts with a turning star on top along both main roads. */
	void AddStarLamps();
	/**
	 * Round 91: rain puddles here and there on the roads (round flat discs, two or three overlapping, mirror smooth: the sky,
	 * the buildings and passing friends show in them). Gone while the road under them is dug out (RefreshMapColumn).
	 */
	void AddPuddles();
	/** Round 91: the sun's light shafts and the sky light's reflections, on the level's own lights (BeginPlay). */
	void SetUpLightEffects();
	/** The draw set a cell's block is in. */
	UInstancedStaticMeshComponent* GetBlockSet(bool bDie, bool bGloss) const;
	/** Floating islands near the rim joined by a rainbow of dice, moons, floating dice and stars over the town. */
	void MakeDreamSky();
	void AddFloater(UInstancedStaticMeshComponent* Instances, const FVector& Base, const FRotator& Turn, float Scale, const FColor& Body, const FColor& Accent,
		float Bob, float Spin, float Tumble, float Orbit, float OrbitRate, float Phase);
	void AnimateFloaters(float Time);

	// Walking routes
	int32 FindNode(const FVector& Location, int32 SearchCells) const;
	void ForEachLink(int32 Node, TFunctionRef<void(int32, float)> Visit) const;
	bool IsClearAbove(int32 X, int32 Y, int32 Level, int32 Count) const;

	UPROPERTY(Transient) TObjectPtr<UStaticMesh> BlockMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> DieMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> Cube;

	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Blocks;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> DieBlocks;
	/** Round 91: polished blocks (FLook::bGloss), and the puddles' discs (the engine cylinder) with their cells. */
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> GlossBlocks;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Puddles;
	/** Round 91: neon tubes along the shop awnings (glowing M_BTBlock). */
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Neon;
	TMap<FIntPoint, TArray<TPair<int32, FTransform>>> PuddleDiscs;
	/** Round 91: the star lamps' light (warm points, no shadows). */
	UPROPERTY(Transient) TArray<TObjectPtr<class UPointLightComponent>> LampLights;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Steps;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Water;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Clouds;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Underside;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Solid;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> RimWalls;
	// The dreamier pass: a marshmallow block, a star and a crescent (char-pipeline/scripts/blender_dream_shapes.py)
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> PuffMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> StarMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> MoonMesh;
	/** A cartoon hip roof stretched over each house (char-pipeline/scripts/blender_dream_roof.py). */
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> RoofMesh;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Roofs;
	/** Marshmallow blocks that stop nothing (round hedges over their hidden cells). */
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Marshmallows;
	/** Round posts (the engine cylinder), stopping nothing: the star lamps' candy stripes. */
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Poles;
	/** The Tripo trees: round, tiered, candy, mushroom, lollipop (empty without the dreamier pass). */
	UPROPERTY(Transient) TArray<TObjectPtr<UInstancedStaticMeshComponent>> PropTrees;
	/** Blocks that stop nothing (awnings). */
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Decor;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Stars;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Moons;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> FloatDice;
	TArray<FFloater> Floaters;
	bool bDream = false;
	/** Every material on the town, made dynamic so rr.DiceTown.Saturation / Brightness reach them live. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> Tunable;

	/** The blocks: a look per cell and level (0 empty, 255 solid but drawn by something else, like a big die). */
	TArray<uint8> Grid;
	TArray<FLook> Looks;
	TArray<FIntRect> Reserved;
	TArray<FStair> Stairs;
	/** Cells under stairs (they walk as the stairs, not as the ground under them). */
	TSet<FIntPoint> StairCells;

	/** Levels added to every level written (and read) while building on top of something, like a hill. */
	int32 Lift = 0;
	/** The minimap: each cell's color and height in blocks (-1 off the island). */
	TArray<FColor> MapColors;
	TArray<int8> MapLevels;

	TArray<FWalkNode> Nodes;
	/** The nodes of each cell, NodeStart[i] .. NodeStart[i + 1] (cells row by row). */
	TArray<int32> NodeStart;
	/** Nodes on the ground anyone can reach from the plaza (for the bots' roaming). */
	TArray<int32> Roamable;

	/** Each drawn block by cell (grid index): a die block or a plain one, its instance, and whether it shows now. */
	struct FCellBlock
	{
		bool bDie;
		int32 Index;
		bool bShown;
		bool bGloss = false;
	};
	TMap<int32, FCellBlock> CellBlocks;

	/** The hidden boxes merged from the grid (MakeCollision), Solid's instances from FirstGridBox on, in cells and levels. */
	struct FGridBox
	{
		FIntVector Min;
		FIntVector Max;
	};
	TArray<FGridBox> GridBoxes;
	int32 FirstGridBox = 0;
	/** The box covering each cell (by grid index; INDEX_NONE for none). */
	TArray<int32> BoxOfCell;

	/** Wrecks: the broken cells, and the boxes cut round them (by box, the boxes over what's left of it, in WreckSolid). */
	TBitArray<> Broken;
	TArray<int32> BrokenCells;
	TMap<int32, TArray<FTransform>> CutBoxes;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> WreckSolid;
	/** Model instances gone with a wreck (by their instances, then index), and where they stood. */
	TMap<UInstancedStaticMeshComponent*, TMap<int32, FTransform>> Gone;

	// Round 89
	/** A giant die (AddDie): its lowest corner cell (level too), size in cells, its instance in DieBlocks and colors. */
	struct FGiantDie
	{
		FIntVector Low;
		int32 Size;
		int32 Instance;
		FColor Body;
		FColor Pips;
	};
	TArray<FGiantDie> GiantDice;
	/** Cells that were never held up from the ground as built (none may fall), found the first time anything breaks. */
	TBitArray<> Anchored;
	bool bFoundAnchored = false;
	/** Each dug column's depth in layers (by column index), and the columns flooded with lava. */
	TMap<int32, int32> DugDepth;
	TSet<int32> LavaColumns;
	/** Cells that fell (no longer held up) as things stand now. */
	TBitArray<> Fallen;

	/** A part of a building coming down as one piece (on every machine): its looks as they stood, falling and tipping. */
	struct FFallingPiece
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Instances;
		int32 Index;
		FTransform Was;
		/** The cell it is the block of (it stays gone if it still is when the fall ends); INDEX_NONE for a model. */
		int32 Cell;
	};
	struct FFalling
	{
		TArray<FFallingPiece> Pieces;
		/** The cells it covered (flat), to land on whoever is under it. */
		TArray<FIntPoint> Footprint;
		FVector Pivot;
		FVector TipAxis;
		float Drop;
		float Duration;
		float Age = 0.f;
		/** The floor it lands on (the top of what still stands under it). */
		float LandZ;
	};
	TArray<FFalling> Falling;
	/** Server: big things the last ApplyWrecks made, for the wreckage (ConsumeBigAreas). */
	TArray<FBox> NewBigAreas;
	int32 MapVersion = 0;
	/** The lava (its looks, its glow and its burns), each machine its own. */
	UPROPERTY(Transient)
	TObjectPtr<class ARRRandomLava> Lava;

	/** Cells held up from the ground (or ones never held up as built) given these broken ones. */
	void FindHeldUp(const TBitArray<>& BrokenNow, TBitArray<>& OutHeld) const;
	/** Starts the cells that fall just now coming down, a piece for each part that hangs together (looks taken as they stood). */
	void StartFalling(const TArray<int32>& Cells, const TArray<FFallingPiece>& BlockPieces, const TMap<UInstancedStaticMeshComponent*, TMap<int32, FTransform>>& NewlyGone);
	void TickFalling(float DeltaSeconds);
	/** A piece reaches the floor: gone for good (as long as it still is), rubble, dust, CRASH!, and a hit on whoever is under it. */
	void LandFalling(FFalling& Piece);

	/** The floor's height under the middle; level 0's top. */
	float FloorZ = 0.f;
	/** The highest level anything solid reaches (MakeCollision), for the flight ceiling. */
	int32 TopLevel = 0;
	float AppliedSaturation = -1.f;
	float AppliedBrightness = -1.f;
	float RulesTimer = 0.f;
	bool bBuilt = false;
};
