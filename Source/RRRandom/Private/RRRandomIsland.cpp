#include "RRRandomIsland.h"
#include "RRRandomGrassFloor.h"
#include "EngineUtils.h"
#include "Algo/IndexOf.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The engine's cube and plane are 100 units across, centered on their pivots
	constexpr float BasicSize = 100.f;
	// How far above and below the actor to look for the floor
	constexpr float FloorSearchUp = 500.f;
	// Flying: the feet up to this far over the highest cliff
	constexpr float FlightHeadroom = 400.f;
	constexpr float FloorSearchDown = 3000.f;
	// Grass blocks overlap their neighbors this much so their grass tops meet (as on cover)
	constexpr float GrassOverlap = 1.08f;
	// Water planes overlap their neighbors this much (they are drawn alike, so the overlap doesn't show)
	constexpr float WaterOverlap = 1.04f;
	// A road is an unseen slab this thick under its dirt tiles, solid so feet stand on it; its top (and the dirt tiles' tops)
	// a little over the grass floor's tiles' (ARRRandomGrassFloor::Lift), so the dirt's dips stay over the grass's green underlay
	constexpr float SandThickness = 10.f;
	constexpr float SandTop = 7.f;
	// Dock planks per cell, the gaps between them, and their thickness
	constexpr int32 PlanksPerCell = 3;
	constexpr float PlankGap = 0.12f;
	constexpr float PlankThickness = 8.f;
	constexpr int32 LookSeed = 4242;
	// How far the sea runs on past the level's floor
	constexpr float SeaReach = 80000.f;
	// Roads (Tripo, char-pipeline/scripts/blender_grass_tiles.py and blender_props.py): the dirt tiles' side, how many
	// grass clumps (and how big) on each side where a road meets grass, how often a pebble cluster sits there too, and
	// the seed
	// The shore mask for the water's shallows: texels per cell, and the blur that spreads the land over the water
	constexpr int32 ShoreTexelsPerCell = 8;
	constexpr int32 ShoreBlurRadius = 5;
	constexpr int32 ShoreBlurPasses = 2;
	constexpr float RoadTileSize = 180.f;
	// The dirt tiles' lumps squashed this much, so their dips stay over the grass floor's green underlay
	constexpr float RoadTileFlatten = 0.6f;
	const FIntPoint TuftsPerSide(2, 3);
	const FVector2D TuftScale(0.55f, 0.95f);
	constexpr float PebbleChance = 0.3f;
	const FVector2D PebbleScale(0.6f, 1.f);
	constexpr int32 RoadSeed = 2468;
	// A bridge cell's walkway: an invisible slab this high over the water, under the bridge model's planks
	constexpr float BridgeDeck = 30.f;
	constexpr float BridgeDeckThickness = 20.f;

	/** How a prop is turned: any way, a quarter turn, or a set way (Yaw, give or take YawJitter). */
	enum class EPropTurn : uint8 { Any, Quarter, Set };
	/** What a prop stops: nothing (looks only), brawlers only (a fence), or everything like cover (shots and sight too). */
	enum class EPropBlock : uint8 { None, Brawlers, All };
	/**
	 * One kind of prop (char-pipeline/scripts/blender_props.py, ue_import_props.py; Tripo models, pivot in the middle of
	 * the bottom): its PropRows letter and mesh, how small to how big it comes, how many stand on a cell (a cluster is a
	 * patch of cells with the same letter, so a wood is several trees to a cell) and how far off the cell's center (a
	 * share of a cell), how it is turned, and what it stops: a hidden box of its bounds, BlockShare of its width (a
	 * mushroom's stem, not its cap).
	 */
	struct FPropKind
	{
		TCHAR Letter;
		const TCHAR* Path;
		FVector2D Scale;
		FIntPoint Count;
		float Jitter;
		EPropTurn Turn;
		float Yaw;
		float YawJitter;
		EPropBlock Block;
		float BlockShare;
	};
	// The Tripo models face their +Y; yaw 90 turns that to -X, toward the camera (doors and signs read on screen).
	// The fence and the bridge are long along their X and Y.
	const FPropKind PropKinds[] =
	{
		{ 'P', TEXT("/Game/Environment/Props/SM_PineTree.SM_PineTree"), FVector2D(0.7f, 1.15f), FIntPoint(2, 2), 0.4f, EPropTurn::Any, 0.f, 0.f, EPropBlock::None, 0.f },
		{ 'T', TEXT("/Game/Environment/Props/SM_RoundTree.SM_RoundTree"), FVector2D(0.65f, 1.f), FIntPoint(1, 2), 0.4f, EPropTurn::Any, 0.f, 0.f, EPropBlock::None, 0.f },
		{ 'B', TEXT("/Game/Environment/Props/SM_FlowerBush.SM_FlowerBush"), FVector2D(0.55f, 1.05f), FIntPoint(2, 3), 0.4f, EPropTurn::Any, 0.f, 0.f, EPropBlock::None, 0.f },
		{ 'R', TEXT("/Game/Environment/Props/SM_Boulders.SM_Boulders"), FVector2D(0.45f, 1.05f), FIntPoint(1, 2), 0.4f, EPropTurn::Any, 0.f, 0.f, EPropBlock::None, 0.f },
		{ 'L', TEXT("/Game/Environment/Props/SM_PondPlants.SM_PondPlants"), FVector2D(0.55f, 0.95f), FIntPoint(1, 2), 0.4f, EPropTurn::Any, 0.f, 0.f, EPropBlock::None, 0.f },
		{ 'M', TEXT("/Game/Environment/Village/SM_GiantMushroom.SM_GiantMushroom"), FVector2D(0.7f, 1.1f), FIntPoint(1, 1), 0.25f, EPropTurn::Any, 0.f, 0.f, EPropBlock::All, 0.3f },
		{ 'H', TEXT("/Game/Environment/Village/SM_MushroomHouse.SM_MushroomHouse"), FVector2D(0.95f, 1.05f), FIntPoint(1, 1), 0.f, EPropTurn::Set, 90.f, 12.f, EPropBlock::All, 0.75f },
		{ 'S', TEXT("/Game/Environment/Village/SM_Signpost.SM_Signpost"), FVector2D(0.9f, 1.f), FIntPoint(1, 1), 0.15f, EPropTurn::Set, 90.f, 20.f, EPropBlock::None, 0.f },
		{ 'N', TEXT("/Game/Environment/Village/SM_Fence.SM_Fence"), FVector2D(1.f, 1.f), FIntPoint(1, 1), 0.f, EPropTurn::Set, 0.f, 2.f, EPropBlock::Brawlers, 1.f },
		{ 'Z', TEXT("/Game/Environment/Village/SM_Fence.SM_Fence"), FVector2D(1.f, 1.f), FIntPoint(1, 1), 0.f, EPropTurn::Set, 90.f, 2.f, EPropBlock::Brawlers, 1.f },
		{ 'O', TEXT("/Game/Environment/Village/SM_StreetLamp.SM_StreetLamp"), FVector2D(0.72f, 0.8f), FIntPoint(1, 1), 0.15f, EPropTurn::Any, 0.f, 0.f, EPropBlock::Brawlers, 0.25f },
		{ 'C', TEXT("/Game/Environment/Village/SM_Crates.SM_Crates"), FVector2D(1.1f, 1.25f), FIntPoint(1, 1), 0.1f, EPropTurn::Quarter, 0.f, 8.f, EPropBlock::All, 0.9f },
		{ 'Y', TEXT("/Game/Environment/Village/SM_Flowers.SM_Flowers"), FVector2D(0.6f, 1.f), FIntPoint(1, 2), 0.35f, EPropTurn::Any, 0.f, 0.f, EPropBlock::None, 0.f },
		{ 'J', TEXT("/Game/Environment/Village/SM_Bridge.SM_Bridge"), FVector2D(1.f, 1.f), FIntPoint(1, 1), 0.f, EPropTurn::Set, 0.f, 0.f, EPropBlock::None, 0.f },
		{ 'V', TEXT("/Game/Environment/Village/SM_Windmill.SM_Windmill"), FVector2D(1.f, 1.f), FIntPoint(1, 1), 0.f, EPropTurn::Set, 90.f, 10.f, EPropBlock::All, 0.55f },
		{ 'K', TEXT("/Game/Environment/Village/SM_ToyBlocks.SM_ToyBlocks"), FVector2D(1.2f, 1.4f), FIntPoint(1, 1), 0.15f, EPropTurn::Quarter, 0.f, 0.f, EPropBlock::All, 0.9f },
	};
	// Props on a cell stand at least PropSpacing (a share of a cell, times their size) apart where it can manage;
	// boulders in water sit BoulderSink deep
	constexpr float PropSpacing = 0.35f;
	constexpr int32 PropTries = 8;
	constexpr float BoulderSink = 40.f;
	constexpr int32 PropSeed = 777;
}

ARRRandomIsland::ARRRandomIsland()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// The map (char-pipeline/scripts/make_island_map.py draws it and checks it; island_map.txt shows it): a toy-town valley.
	// High grass cliffs along the top with woods, a terrace under them with the windmill on a low hill; a waterfall at the
	// top right into a river down the right side, crossed by a bridge on each village road to the east bank; a wall over a
	// terrace down the left with a mushroom forest under it; a beach with two docks onto the bay at the bottom. Red's
	// village is on the top road (row 9), blue's on the bottom one (row 26): mushroom houses, fences, lamps, a signpost.
	// The two village roads, a west and an east lane make a ring road, with the main road down the middle, for vehicles.
	// The field between has flowers, crates and toy blocks (solid, like cover), and two low hills to climb; the game
	// mode's cover walls stand on flat cells there. Each step up out of the valley is at most two levels, but the walls
	// behind the terraces are three or more, so nobody climbs off the island.
	TypeRows = {
		TEXT("gggggggggggggggggggggggggggggFFFgggg"),
		TEXT("gggggggggggggggggggggggggggggFFFgggg"),
		TEXT("gggggggggggggggggggggggggggggFFFgggg"),
		TEXT("gggggggg.gg.gggggggggggg.ggggwwwgggg"),
		TEXT("gg..ggg..........ss..........www...g"),
		TEXT("gg..ggg..........ss..........www...g"),
		TEXT("gg...............ss..........www...g"),
		TEXT("gg...............ss..........www...g"),
		TEXT("gg.............ssssss........www...g"),
		TEXT("ggsssssssssssssssssssssssssssbbbsssg"),
		TEXT("g......s.......ssssss......s.www...g"),
		TEXT("gg.....s.........ss........s.www...g"),
		TEXT("gg.....s.........ss........s.www...g"),
		TEXT("gg.....s..ggg....ss........s.www...g"),
		TEXT("gg.....s..ggg....ss........s.www...g"),
		TEXT("gg.....s.........ss........s.www...g"),
		TEXT("gg.....s.........ss........s.www...g"),
		TEXT("g......s.........ss........s.www...g"),
		TEXT("gg.....s.........ss........s.www...g"),
		TEXT("gg.....s.........ss........s.www...g"),
		TEXT("gg.....s.........ss........s.www...g"),
		TEXT("gg.....s.........ss...ggg..s.www...g"),
		TEXT("gg.....s.........ss...ggg..s.www...g"),
		TEXT("gg.....s.........ss........s.www...g"),
		TEXT("g......s.........ss........s.www...g"),
		TEXT("gg.....s.......ssssss......s.www...g"),
		TEXT("ggsssssssssssssssssssssssssssbbbsssg"),
		TEXT("gg.............ssssss........www...g"),
		TEXT("gg...............ss..........www...g"),
		TEXT("gg...............ss..........www...g"),
		TEXT("wwssssssssssssssssssssssssssswwwsssw"),
		TEXT("wwwwwwwddwwwwwwwwwwwwwddwwwwwwwwwwww"),
		TEXT("wwwwwwwddwwwwwwwwwwwwwddwwwwwwwwwwww"),
		TEXT("wwwwwwwddwwwwwwwwwwwwwddwwwwwwwwwwww"),
		TEXT("wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww"),
		TEXT("wwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwwww"),
	};
	HeightRows = {
		TEXT("988988889999899999898898989988888899"),
		TEXT("788778887787777788788878778776667788"),
		TEXT("776767676767776776677766667774447667"),
		TEXT("622122120120112122122112012110002226"),
		TEXT("710012100000000000000000000000000006"),
		TEXT("620011100000000000000000000000000006"),
		TEXT("610000000000000000000000000000000006"),
		TEXT("610000000000000000000000000000000006"),
		TEXT("710000000000000000000000000000000006"),
		TEXT("710000000000000000000000000000000006"),
		TEXT("600000000000000000000000000000000006"),
		TEXT("720000000000000000000000000000000006"),
		TEXT("620000000000000000000000000000000006"),
		TEXT("620000000012100000000000000000000006"),
		TEXT("610000000011100000000000000000000006"),
		TEXT("620000000000000000000000000000000006"),
		TEXT("720000000000000000000000000000000006"),
		TEXT("700000000000000000000000000000000006"),
		TEXT("720000000000000000000000000000000006"),
		TEXT("610000000000000000000000000000000006"),
		TEXT("710000000000000000000000000000000006"),
		TEXT("720000000000000000000012100000000006"),
		TEXT("610000000000000000000011100000000006"),
		TEXT("720000000000000000000000000000000006"),
		TEXT("700000000000000000000000000000000006"),
		TEXT("610000000000000000000000000000000006"),
		TEXT("710000000000000000000000000000000006"),
		TEXT("720000000000000000000000000000000006"),
		TEXT("710000000000000000000000000000000006"),
		TEXT("720000000000000000000000000000000006"),
		TEXT("000000000000000000000000000000000000"),
		TEXT("000000000000000000000000000000000000"),
		TEXT("000000000000000000000000000000000000"),
		TEXT("000000000000000000000000000000000000"),
		TEXT("000000000000000000000000000000000000"),
		TEXT("000000000000000000000000000000000000"),
	};
	PropRows = {
		TEXT("P.PPPPPP.PPPPP.PPP....TTPPP.P...PPPP"),
		TEXT("PPT.TPPPPT.TPPPPTT.PP.PTP.PPT...PPPP"),
		TEXT(".T...TT...T....T.BBT...T.B.........."),
		TEXT("............Y...B.B.B.....B.M...R..."),
		TEXT(".....V...B.......................B.."),
		TEXT(".MY.................Y.....Y.......T."),
		TEXT(".........H...H..........H....L......"),
		TEXT("..Y.................Y.....Y........."),
		TEXT(".T.ZZZZ..ZZZZ......C..ZZZZZ..L.LO..."),
		TEXT("..............................J....."),
		TEXT("......O..C....O.S....O...C..OL......"),
		TEXT(".M...MB......Y........Y..........TB."),
		TEXT(".M..M.M........K.........Y.........."),
		TEXT(".TBM..M.........Y............L...Y.."),
		TEXT(".M.MM.B....Y...................L..B."),
		TEXT(".B.B.B.................K..........Y."),
		TEXT("...Y..Y..Y...............Y.......Y.."),
		TEXT("....K.Y.......Y..............L......"),
		TEXT("....MB...............Y..K....L....Y."),
		TEXT("...M.MM..Y...............Y.....L.YB."),
		TEXT(".TM..Y.....K........................"),
		TEXT(".T.....................Y..........Y."),
		TEXT(".B.Y...............Y................"),
		TEXT(".B..M.B..Y..........K..........L.Y.."),
		TEXT("...Y.M......Y..........Y............"),
		TEXT("......O...C...O.S....O..C...O......."),
		TEXT(".M............................J....."),
		TEXT("...ZZZZ..ZZZZ.C.......ZZZZZ.....O..."),
		TEXT("..Y.................Y.....Y..L.L...."),
		TEXT(".M.......H...H..........H..........."),
		TEXT("......O..............O.............."),
		TEXT("....R.......L.....RL........L....L.."),
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT("...................................."),
		TEXT("...................................."),
	};

	static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassA(TEXT("/Game/Environment/Grass/SM_GrassBlock_A.SM_GrassBlock_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassB(TEXT("/Game/Environment/Grass/SM_GrassBlock_B.SM_GrassBlock_B"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassC(TEXT("/Game/Environment/Grass/SM_GrassBlock_C.SM_GrassBlock_C"));
	for (const ConstructorHelpers::FObjectFinder<UStaticMesh>* Finder : { &GrassA, &GrassB, &GrassC })
	{
		if (Finder->Succeeded())
		{
			GrassBlocks.Add(Finder->Object);
		}
	}
	// The grass blocks' soil alone (char-pipeline/scripts/blender_soil_blocks.py), stacked under them
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SoilA(TEXT("/Game/Environment/Grass/SM_SoilBlock_A.SM_SoilBlock_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SoilB(TEXT("/Game/Environment/Grass/SM_SoilBlock_B.SM_SoilBlock_B"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SoilC(TEXT("/Game/Environment/Grass/SM_SoilBlock_C.SM_SoilBlock_C"));
	for (const ConstructorHelpers::FObjectFinder<UStaticMesh>* Finder : { &SoilA, &SoilB, &SoilC })
	{
		if (Finder->Succeeded())
		{
			SoilBlocks.Add(Finder->Object);
		}
	}
	for (const FPropKind& Kind : PropKinds)
	{
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Finder(Kind.Path);
		PropMeshes.Add(Finder.Get());
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Sand(TEXT("/Game/Environment/Island/M_IslandSand.M_IslandSand"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(TEXT("/Game/Environment/Island/M_IslandWood.M_IslandWood"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Water(TEXT("/Game/Environment/Island/M_IslandWater.M_IslandWater"));
	Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	Plane = PlaneMesh.Succeeded() ? PlaneMesh.Object : nullptr;
	SandMaterial = Sand.Succeeded() ? Sand.Object : nullptr;
	WoodMaterial = Wood.Succeeded() ? Wood.Object : nullptr;
	WaterMaterial = Water.Succeeded() ? Water.Object : nullptr;
	for (const TCHAR* Path : { TEXT("/Game/Environment/Path/SM_DirtTile_A.SM_DirtTile_A"), TEXT("/Game/Environment/Path/SM_DirtTile_B.SM_DirtTile_B"),
		TEXT("/Game/Environment/Path/SM_DirtTile_C.SM_DirtTile_C") })
	{
		ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Finder(Path);
		if (Finder.Get())
		{
			DirtTiles.Add(Finder.Get());
		}
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Tuft(TEXT("/Game/Environment/Path/SM_RoadTuft.SM_RoadTuft"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RoadStones(TEXT("/Game/Environment/Path/SM_RoadPebbles.SM_RoadPebbles"));
	RoadTuft = Tuft.Succeeded() ? Tuft.Object : nullptr;
	RoadPebbles = RoadStones.Succeeded() ? RoadStones.Object : nullptr;

	// The host spawns it; clients build the same island from the same map (they need its walls to move)
	bReplicates = true;
	bAlwaysRelevant = true;
}

FBox2D ARRRandomIsland::GetMapBounds() const
{
	const int32 Rows = TypeRows.Num();
	const int32 Cols = Rows > 0 ? TypeRows[0].Len() : 0;
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	const FVector2D Half(Rows * CellSize * 0.5f, Cols * CellSize * 0.5f);
	return FBox2D(Center - Half, Center + Half);
}

FBox ARRRandomIsland::GetFlightArea() const
{
	// Over the island, the feet up to over its highest cliffs (levels of LevelHeight, a digit each) with room to spare; the
	// view from above keeps flyers in sight
	const FBox2D Map = GetMapBounds();
	return FBox(FVector(Map.Min, FloorZ - 1000.f), FVector(Map.Max, FloorZ + 9.f * LevelHeight + FlightHeadroom));
}

bool ARRRandomIsland::IsRoadAt(const FVector2D& WorldLocation) const
{
	int32 Row;
	int32 Col;
	return GetCellAt(WorldLocation, Row, Col) && TypeRows[Row][Col] == TEXT('s');
}

bool ARRRandomIsland::GetCellAt(const FVector2D& WorldLocation, int32& OutRow, int32& OutCol) const
{
	const FBox2D Bounds = GetMapBounds();
	const int32 Rows = TypeRows.Num();
	const int32 Cols = Rows > 0 ? TypeRows[0].Len() : 0;
	// Row 0 is the far end (+X), column 0 the left (-Y)
	OutRow = FMath::FloorToInt((Bounds.Max.X - WorldLocation.X) / CellSize);
	OutCol = FMath::FloorToInt((WorldLocation.Y - Bounds.Min.Y) / CellSize);
	return OutRow >= 0 && OutRow < Rows && OutCol >= 0 && OutCol < Cols && HeightRows.IsValidIndex(OutRow) && TypeRows[OutRow].Len() == Cols;
}

void ARRRandomIsland::BeginPlay()
{
	Super::BeginPlay();
	Build();
	bBuilt = true;
	// The grass floor makes way for the dirt tiles on the roads (if it was laid out first)
	for (TActorIterator<ARRRandomGrassFloor> Grass(GetWorld()); Grass; ++Grass)
	{
		Grass->ClearRoadTiles(this);
	}
}

UInstancedStaticMeshComponent* ARRRandomIsland::MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bSolid, int32 CustomFloats)
{
	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
	Instances->SetStaticMesh(Mesh);
	if (Material)
	{
		Instances->SetMaterial(0, Material);
	}
	Instances->NumCustomDataFloats = CustomFloats;
	Instances->SetCollisionProfileName(bSolid ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Instances->SetGenerateOverlapEvents(false);
	Instances->SetCanEverAffectNavigation(false);
	Instances->SetupAttachment(RootComponent);
	// Instances are placed in world space
	Instances->SetUsingAbsoluteLocation(true);
	Instances->SetUsingAbsoluteRotation(true);
	Instances->SetUsingAbsoluteScale(true);
	Instances->RegisterComponent();
	Instances->SetWorldTransform(FTransform::Identity);
	return Instances;
}

void ARRRandomIsland::Build()
{
	UWorld* World = GetWorld();
	const int32 Rows = TypeRows.Num();
	const int32 Cols = Rows > 0 ? TypeRows[0].Len() : 0;
	if (Rows == 0 || Cols == 0 || HeightRows.Num() != Rows || !Cube || !Plane)
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: island map or meshes missing."));
		return;
	}
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		if (TypeRows[Row].Len() != Cols || HeightRows[Row].Len() != Cols)
		{
			UE_LOG(LogTemp, Warning, TEXT("RRRandom: island map row %d is not %d cells wide."), Row, Cols);
			return;
		}
	}

	// The floor under the actor: the island stands on it
	FHitResult Hit;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, FloorSearchUp);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomIsland), false, this);
	if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, FloorSearchUp + FloorSearchDown), ECC_Visibility, Params) || !Hit.GetComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no floor under the island at %s."), *GetActorLocation().ToString());
		return;
	}
	const float Z = Hit.ImpactPoint.Z;
	FloorZ = Z;
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	auto CellCenter = [&](int32 Row, int32 Col)
	{
		return FVector(Center.X + (Rows * 0.5f - Row - 0.5f) * CellSize, Center.Y + (Col - Cols * 0.5f + 0.5f) * CellSize, Z);
	};

	// What can be seen, and what stops things
	TArray<UInstancedStaticMeshComponent*> Grass;
	for (UStaticMesh* Mesh : GrassBlocks)
	{
		Grass.Add(MakeInstances(Mesh, nullptr, false));
	}
	TArray<UInstancedStaticMeshComponent*> Soil;
	for (UStaticMesh* Mesh : SoilBlocks)
	{
		Soil.Add(MakeInstances(Mesh, nullptr, false));
	}
	// Sand paths and dock planks are walked on (a step up from the floor), the rest only seen
	UInstancedStaticMeshComponent* Sand = MakeInstances(Cube, SandMaterial, true);
	UInstancedStaticMeshComponent* Planks = MakeInstances(Cube, WoodMaterial, true);
	UInstancedStaticMeshComponent* WaterSurface = MakeInstances(Plane, WaterMaterial, false);
	UInstancedStaticMeshComponent* Falls = MakeInstances(Cube, WaterMaterial, false);
	for (UInstancedStaticMeshComponent* Flat : { Sand, Planks, WaterSurface })
	{
		Flat->CastShadow = false;
	}
	// Hills, rock and falls: a hidden box per cell, solid like cover (the grass block meshes have no collision)
	UInstancedStaticMeshComponent* Solid = MakeInstances(Cube, nullptr, true);
	Solid->SetVisibility(false);
	Solid->SetHiddenInGame(true);
	// Water: a hidden wall that stops brawlers only; shots and the bots' sight (world static sweeps) pass
	UInstancedStaticMeshComponent* WaterWalls = MakeInstances(Cube, nullptr, false);
	WaterWalls->SetVisibility(false);
	WaterWalls->SetHiddenInGame(true);
	WaterWalls->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	WaterWalls->SetCollisionObjectType(ECC_WorldDynamic);
	WaterWalls->SetCollisionResponseToAllChannels(ECR_Ignore);
	WaterWalls->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	// Bridges: an invisible walkway over the water, solid like the docks; the bridge model is a prop on top
	UInstancedStaticMeshComponent* Decks = MakeInstances(Cube, nullptr, true);
	Decks->SetVisibility(false);
	Decks->SetHiddenInGame(true);

	const float CellScale = CellSize / BasicSize;
	FRandomStream Stream(LookSeed);
	int32 Counts[4] = {};
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Col = 0; Col < Cols; ++Col)
		{
			const TCHAR Type = TypeRows[Row][Col];
			const TCHAR Digit = HeightRows[Row][Col];
			const int32 Levels = FChar::IsDigit(Digit) ? Digit - TEXT('0') : 0;
			const float Height = Levels * LevelHeight;
			const FVector Base = CellCenter(Row, Col);

			const bool bSolid = (Type == TEXT('g') || Type == TEXT('r') || Type == TEXT('F')) && Levels > 0;
			if (bSolid)
			{
				Solid->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, Height * 0.5f), FVector(CellScale, CellScale, Height / BasicSize)));
				++Counts[0];
			}

			if ((Type == TEXT('g') || Type == TEXT('r')) && Levels > 0 && Grass.Num() > 0)
			{
				// Soil blocks, one per level, under a grass block on top (its grass skirt hangs over the seam); each a random
				// one of its set at any quarter turn. Without soil blocks, one grass block stretched to the full height.
				const int32 Pieces = Soil.Num() > 0 ? Levels : 1;
				const float PieceHeight = Height / Pieces;
				for (int32 Piece = 0; Piece < Pieces; ++Piece)
				{
					const TArray<UInstancedStaticMeshComponent*>& Set = Piece == Pieces - 1 ? Grass : Soil;
					const FRotator Turn(0.f, 90.f * Stream.RandRange(0, 3), 0.f);
					Set[Stream.RandRange(0, Set.Num() - 1)]->AddInstance(FTransform(Turn, Base + FVector(0.f, 0.f, Piece * PieceHeight),
						FVector(CellScale * GrassOverlap, CellScale * GrassOverlap, PieceHeight / BasicSize)));
				}
			}
			else if (Type == TEXT('F') && Levels > 0)
			{
				Falls->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, Height * 0.5f), FVector(CellScale, CellScale, Height / BasicSize)));
			}
			else if (Type == TEXT('s'))
			{
				Sand->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, SandTop - SandThickness * 0.5f), FVector(CellScale, CellScale, SandThickness / BasicSize)));
				++Counts[1];
			}
			else if (Type == TEXT('w') || Type == TEXT('d') || Type == TEXT('b'))
			{
				// A little bigger than the cell, so neighbors overlap and no hairline gap shows between them
				WaterSurface->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, WaterLift), FVector(CellScale * WaterOverlap, CellScale * WaterOverlap, 1.f)));
				if (Type == TEXT('b'))
				{
					Decks->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, WaterLift + BridgeDeck - BridgeDeckThickness * 0.5f),
						FVector(CellScale, CellScale, BridgeDeckThickness / BasicSize)));
					++Counts[3];
				}
				else if (Type == TEXT('w'))
				{
					WaterWalls->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, WaterWallHeight * 0.5f), FVector(CellScale, CellScale, WaterWallHeight / BasicSize)));
					++Counts[2];
				}
				else
				{
					// Planks across the cell, walkable (no wall), with gaps between them
					const float PlankLength = CellSize / PlanksPerCell;
					for (int32 Plank = 0; Plank < PlanksPerCell; ++Plank)
					{
						const FVector Offset((Plank + 0.5f) * PlankLength - CellSize * 0.5f, 0.f, WaterLift + DockLift - PlankThickness * 0.5f + Stream.FRandRange(-1.5f, 1.5f));
						Planks->AddInstance(FTransform(FRotator::ZeroRotator, Base + Offset, FVector(PlankLength * (1.f - PlankGap) / BasicSize, CellScale * 0.96f, PlankThickness / BasicSize)));
					}
					++Counts[3];
				}
			}
		}
	}

	// The water's shallows (M_IslandWater, made by char-pipeline/scripts/ue_make_water.py): a mask of the land, white, blurred
	// out over the water, so the water is light and clear near the shore, with foam at its edge, and deep further out
	if (WaterMaterial)
	{
		const int32 Width = Cols * ShoreTexelsPerCell;
		const int32 Height = Rows * ShoreTexelsPerCell;
		TArray<float> Mask;
		Mask.SetNumZeroed(Width * Height);
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Col = 0; Col < Cols; ++Col)
			{
				const TCHAR Type = TypeRows[Row][Col];
				if (Type == TEXT('w') || Type == TEXT('d') || Type == TEXT('b'))
				{
					continue;
				}
				for (int32 Y = Row * ShoreTexelsPerCell; Y < (Row + 1) * ShoreTexelsPerCell; ++Y)
				{
					for (int32 X = Col * ShoreTexelsPerCell; X < (Col + 1) * ShoreTexelsPerCell; ++X)
					{
						Mask[Y * Width + X] = 1.f;
					}
				}
			}
		}
		// Box blurs along each axis, a few times over: the land's whiteness spreads a cell or so over the water
		TArray<float> Pass;
		Pass.SetNumZeroed(Width * Height);
		for (int32 Round = 0; Round < ShoreBlurPasses; ++Round)
		{
			for (int32 Axis = 0; Axis < 2; ++Axis)
			{
				for (int32 Y = 0; Y < Height; ++Y)
				{
					for (int32 X = 0; X < Width; ++X)
					{
						float Sum = 0.f;
						for (int32 Step = -ShoreBlurRadius; Step <= ShoreBlurRadius; ++Step)
						{
							const int32 SX = Axis == 0 ? FMath::Clamp(X + Step, 0, Width - 1) : X;
							const int32 SY = Axis == 1 ? FMath::Clamp(Y + Step, 0, Height - 1) : Y;
							Sum += Mask[SY * Width + SX];
						}
						Pass[Y * Width + X] = Sum / (ShoreBlurRadius * 2 + 1);
					}
				}
				Swap(Mask, Pass);
			}
		}
		ShoreMask = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
		// Sampled as an sRGB color like the material's default (the engine's black); written encoded so it reads back linear
		ShoreMask->SRGB = true;
		ShoreMask->Filter = TF_Bilinear;
		ShoreMask->AddressX = TA_Clamp;
		ShoreMask->AddressY = TA_Clamp;
		ShoreMask->UpdateResource();
		FColor* Pixels = new FColor[Width * Height];
		for (int32 Index = 0; Index < Width * Height; ++Index)
		{
			const float Value = FMath::Clamp(Mask[Index], 0.f, 1.f);
			Pixels[Index] = FLinearColor(Value, Value, Value, 1.f).ToFColor(true);
		}
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);
		ShoreMask->UpdateTextureRegions(0, 1, Region, Width * sizeof(FColor), sizeof(FColor), reinterpret_cast<uint8*>(Pixels),
			[](uint8* Data, const FUpdateTextureRegion2D* Regions)
			{
				delete[] reinterpret_cast<FColor*>(Data);
				delete Regions;
			});
		UMaterialInstanceDynamic* Water = UMaterialInstanceDynamic::Create(WaterMaterial, this);
		Water->SetTextureParameterValue(TEXT("ShoreMask"), ShoreMask);
		// Its far X, its left Y, its size along X and Y (the mask's rows run down the screen like the map's)
		Water->SetVectorParameterValue(TEXT("MapArea"), FLinearColor(Center.X + Rows * CellSize * 0.5f, Center.Y - Cols * CellSize * 0.5f, Rows * CellSize, Cols * CellSize));
		WaterSurface->SetMaterial(0, Water);
		Falls->SetMaterial(0, Water);
	}

	// Dirt roads: one Tripo dirt tile per road cell (a random one of the set, any quarter turn), level with the grass
	// floor's tiles (which the grass floor takes off these cells), and along every side where a road meets grass, a few
	// Tripo grass clumps and now and then a little cluster of pebbles straddling the seam, so the road's edge is grass
	// spilling over it rather than a straight line
	TArray<UInstancedStaticMeshComponent*> Dirt;
	for (UStaticMesh* Mesh : DirtTiles)
	{
		Dirt.Add(MakeInstances(Mesh, nullptr, false));
	}
	UInstancedStaticMeshComponent* Tufts = RoadTuft ? MakeInstances(RoadTuft, nullptr, false) : nullptr;
	UInstancedStaticMeshComponent* Pebbles = RoadPebbles ? MakeInstances(RoadPebbles, nullptr, false) : nullptr;
	for (UInstancedStaticMeshComponent* Instances : Dirt)
	{
		Instances->CastShadow = false;
	}
	if (Dirt.Num() > 0)
	{
		Sand->SetVisibility(false);
		FRandomStream RoadStream(RoadSeed);
		auto IsRoadCell = [&](int32 Row, int32 Col)
		{
			return Row >= 0 && Row < Rows && Col >= 0 && Col < Cols && (TypeRows[Row][Col] == TEXT('s') || TypeRows[Row][Col] == TEXT('b'));
		};
		auto IsGrassCell = [&](int32 Row, int32 Col)
		{
			return Row >= 0 && Row < Rows && Col >= 0 && Col < Cols && (TypeRows[Row][Col] == TEXT('.') || TypeRows[Row][Col] == TEXT('g'));
		};
		const FIntPoint Sides[] = { FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1) };
		for (int32 Row = 0; Row < Rows; ++Row)
		{
			for (int32 Col = 0; Col < Cols; ++Col)
			{
				if (TypeRows[Row][Col] != TEXT('s'))
				{
					continue;
				}
				const FVector Base = CellCenter(Row, Col);
				const FRotator Turn(0.f, 90.f * RoadStream.RandRange(0, 3), 0.f);
				Dirt[RoadStream.RandRange(0, Dirt.Num() - 1)]->AddInstance(FTransform(Turn, Base + FVector(0.f, 0.f, SandTop), FVector(CellSize / RoadTileSize, CellSize / RoadTileSize, RoadTileFlatten)));

				for (const FIntPoint& Side : Sides)
				{
					if (IsRoadCell(Row + Side.X, Col + Side.Y) || !IsGrassCell(Row + Side.X, Col + Side.Y))
					{
						continue;
					}
					// The seam: halfway to the neighbor, running across; rows go down the screen (-X), columns right (+Y)
					const FVector Out(-Side.X, Side.Y, 0.f);
					const FVector Along(Side.Y != 0 ? 1.f : 0.f, Side.X != 0 ? 1.f : 0.f, 0.f);
					const int32 Count = RoadStream.RandRange(TuftsPerSide.X, TuftsPerSide.Y);
					for (int32 Item = 0; Tufts && Item < Count; ++Item)
					{
						const FVector Spot = Base + Out * CellSize * (0.5f + RoadStream.FRandRange(-0.08f, 0.05f))
							+ Along * CellSize * RoadStream.FRandRange(-0.5f, 0.5f);
						Tufts->AddInstance(FTransform(FRotator(0.f, RoadStream.FRandRange(0.f, 360.f), 0.f), Spot + FVector(0.f, 0.f, 1.f),
							FVector(RoadStream.FRandRange(TuftScale.X, TuftScale.Y))));
					}
					if (Pebbles && RoadStream.FRand() < PebbleChance)
					{
						const FVector Spot = Base + Out * CellSize * RoadStream.FRandRange(0.3f, 0.45f) + Along * CellSize * RoadStream.FRandRange(-0.4f, 0.4f);
						Pebbles->AddInstance(FTransform(FRotator(0.f, RoadStream.FRandRange(0.f, 360.f), 0.f), Spot + FVector(0.f, 0.f, SandTop - 2.f),
							FVector(RoadStream.FRandRange(PebbleScale.X, PebbleScale.Y))));
					}
				}
			}
		}
	}

	// Scenery on top of it all
	TArray<UInstancedStaticMeshComponent*> Props;
	for (UStaticMesh* Mesh : PropMeshes)
	{
		Props.Add(Mesh ? MakeInstances(Mesh, nullptr, false) : nullptr);
	}
	FRandomStream PropStream(PropSeed);
	int32 PropCount = 0;
	int32 BlockerCount = 0;
	for (int32 Row = 0; Row < FMath::Min(Rows, PropRows.Num()); ++Row)
	{
		for (int32 Col = 0; Col < FMath::Min(Cols, PropRows[Row].Len()); ++Col)
		{
			const int32 KindIndex = Algo::IndexOfByPredicate(PropKinds, [Letter = PropRows[Row][Col]](const FPropKind& Kind) { return Kind.Letter == Letter; });
			if (KindIndex == INDEX_NONE || !Props.IsValidIndex(KindIndex) || !Props[KindIndex])
			{
				continue;
			}
			const FPropKind& Kind = PropKinds[KindIndex];
			// The cell's top: a hill or cliff, sand, a dock, the water's surface or the floor
			const TCHAR Type = TypeRows[Row][Col];
			const TCHAR Digit = HeightRows[Row][Col];
			const int32 Levels = FChar::IsDigit(Digit) ? Digit - TEXT('0') : 0;
			float Top = 0.f;
			switch (Type)
			{
			case TEXT('g'):
			case TEXT('r'):
			case TEXT('F'):
				Top = Levels * LevelHeight;
				break;
			case TEXT('s'):
				Top = SandTop;
				break;
			case TEXT('d'):
				Top = WaterLift + DockLift;
				break;
			case TEXT('b'):
				Top = WaterLift;
				break;
			case TEXT('w'):
				Top = WaterLift - (Kind.Letter == TEXT('R') ? BoulderSink : 0.f);
				break;
			default:
				break;
			}
			// A few to the cell, spread out (bigger ones further apart), each turned and sized its own way
			TArray<FVector2D, TInlineAllocator<4>> Taken;
			const int32 Count = PropStream.RandRange(Kind.Count.X, Kind.Count.Y);
			for (int32 Item = 0; Item < Count; ++Item)
			{
				const float Size = PropStream.FRandRange(Kind.Scale.X, Kind.Scale.Y);
				FVector2D Spot = FVector2D::ZeroVector;
				for (int32 Try = 0; Try < PropTries; ++Try)
				{
					Spot = FVector2D(PropStream.FRandRange(-Kind.Jitter, Kind.Jitter), PropStream.FRandRange(-Kind.Jitter, Kind.Jitter));
					if (!Taken.ContainsByPredicate([&Spot, Size](const FVector2D& Other) { return FVector2D::Distance(Spot, Other) < PropSpacing * Size; }))
					{
						break;
					}
				}
				Taken.Add(Spot);
				float Yaw = Kind.Yaw + PropStream.FRandRange(-Kind.YawJitter, Kind.YawJitter);
				if (Kind.Turn == EPropTurn::Any)
				{
					Yaw = PropStream.FRandRange(0.f, 360.f);
				}
				else if (Kind.Turn == EPropTurn::Quarter)
				{
					Yaw += 90.f * PropStream.RandRange(0, 3);
				}
				const FTransform Placed(FRotator(0.f, Yaw, 0.f), CellCenter(Row, Col) + FVector(Spot.X * CellSize, Spot.Y * CellSize, Top), FVector(Size));
				Props[KindIndex]->AddInstance(Placed);
				++PropCount;

				// What it stops: a hidden box over its bounds, narrowed to its solid part, turned with it
				if (Kind.Block != EPropBlock::None)
				{
					const FBox Bounds = PropMeshes[KindIndex]->GetBoundingBox();
					const FVector Extent = Bounds.GetExtent() * Size;
					const FVector BoxCenter = Placed.TransformPosition(FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, Bounds.Max.Z * 0.5f));
					const FVector BoxScale(Extent.X * 2.f * Kind.BlockShare / BasicSize, Extent.Y * 2.f * Kind.BlockShare / BasicSize, Bounds.Max.Z * Size / BasicSize);
					(Kind.Block == EPropBlock::All ? Solid : WaterWalls)->AddInstance(FTransform(Placed.GetRotation(), BoxCenter, BoxScale));
					++BlockerCount;
				}
			}
		}
	}

	// Water over the rest of the floor around the island, so its edge is a shore and not the end of the world
	// Out past the level's floor to the horizon, so the third person camera sees open sea and not where it stops
	const FBox Floor = Hit.GetComponent()->Bounds.GetBox().ExpandBy(FVector(SeaReach, SeaReach, 0.f));
	const FVector2D IslandMin(Center.X - Rows * CellSize * 0.5f, Center.Y - Cols * CellSize * 0.5f);
	const FVector2D IslandMax(Center.X + Rows * CellSize * 0.5f, Center.Y + Cols * CellSize * 0.5f);
	auto Sea = [&](float MinX, float MaxX, float MinY, float MaxY)
	{
		if (MaxX > MinX && MaxY > MinY)
		{
			WaterSurface->AddInstance(FTransform(FRotator::ZeroRotator, FVector((MinX + MaxX) * 0.5f, (MinY + MaxY) * 0.5f, Z + WaterLift),
				FVector((MaxX - MinX) / BasicSize, (MaxY - MinY) / BasicSize, 1.f)));
		}
	};
	Sea(IslandMax.X, Floor.Max.X, Floor.Min.Y, Floor.Max.Y);
	Sea(Floor.Min.X, IslandMin.X, Floor.Min.Y, Floor.Max.Y);
	Sea(IslandMin.X, IslandMax.X, Floor.Min.Y, IslandMin.Y);
	Sea(IslandMin.X, IslandMax.X, IslandMax.Y, Floor.Max.Y);

	UE_LOG(LogTemp, Log, TEXT("RRRandom: island %d x %d cells: %d solid, %d sand, %d water, %d dock or bridge, %d props (%d solid)."), Cols, Rows, Counts[0], Counts[1], Counts[2], Counts[3], PropCount, BlockerCount);
}
