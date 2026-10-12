#include "RRRandomIsland.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The engine's cube and plane are 100 units across, centered on their pivots
	constexpr float BasicSize = 100.f;
	// How far above and below the actor to look for the floor
	constexpr float FloorSearchUp = 500.f;
	constexpr float FloorSearchDown = 3000.f;
	// Grass blocks overlap their neighbors this much so their grass tops meet (as on cover)
	constexpr float GrassOverlap = 1.08f;
	// A sand path is a slab this thick, its top over the grass floor's blades (which poked through at +5), solid so feet stand on it
	constexpr float SandThickness = 10.f;
	constexpr float SandTop = 16.f;
	// Dock planks per cell, the gaps between them, and their thickness
	constexpr int32 PlanksPerCell = 3;
	constexpr float PlankGap = 0.12f;
	constexpr float PlankThickness = 8.f;
	constexpr int32 LookSeed = 4242;
	// Props (char-pipeline/scripts/blender_props.py, ue_import_props.py; Tripo models): the letters PropRows uses, in the
	// order of PropMeshes, how small to how big each kind comes, and how many of it stand on one cell (a cluster is a
	// patch of cells with the same letter, so a wood is several trees to a cell)
	const TCHAR* const PropLetters = TEXT("PTBRL");
	const FVector2D PropScales[] = { FVector2D(0.7f, 1.15f), FVector2D(0.65f, 1.f), FVector2D(0.55f, 1.05f), FVector2D(0.45f, 1.05f), FVector2D(0.55f, 0.95f) };
	const FIntPoint PropCounts[] = { FIntPoint(2, 2), FIntPoint(1, 2), FIntPoint(2, 3), FIntPoint(1, 2), FIntPoint(1, 2) };
	// Props on a cell stand up to this share of a cell off its center, at least PropSpacing (a share of a cell, times
	// their size) apart where it can manage; boulders in water sit BoulderSink deep
	constexpr float PropJitter = 0.4f;
	constexpr float PropSpacing = 0.35f;
	constexpr int32 PropTries = 8;
	constexpr float BoulderSink = 40.f;
	constexpr int32 PropSeed = 777;
}

ARRRandomIsland::ARRRandomIsland()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// The map: grass cliffs stepping up along the top (a low terrace, then a wall at least three levels higher, then
	// uneven peaks) and down the left, a waterfall (F) at the top right into a river down the right, a bay with docks at
	// the bottom, a few low hills (g) and sand paths (s) around a flat middle. Each step up out of the middle is at most
	// two levels, but the wall behind a terrace is three or more, so nobody climbs off the island. Blue starts on the
	// bottom sand path (row 18), red on the top one (row 7); the game mode's cover stands on flat cells in rows 10 to 15.
	TypeRows = {
		TEXT("ggggggggggggggggggggFwwwww"),
		TEXT("ggggggggggggggggggggFwwwww"),
		TEXT("ggggggggggggggggggggFwwwww"),
		TEXT("gggg.gggg.gggg.gggggwwwwww"),
		TEXT("ggg..............ggg.wwwww"),
		TEXT("gg................g..swwww"),
		TEXT("gg...ss..........g...swwww"),
		TEXT("gg....sssssssssss.....dwww"),
		TEXT("gg.........s.........s.www"),
		TEXT("g..........s.........s.www"),
		TEXT("g..g.......s.........s..ww"),
		TEXT("gg.g.......s.........s..ww"),
		TEXT("gg.........s......g..s..ww"),
		TEXT("gg.........s......g..s..ww"),
		TEXT("g..........s.........s..ww"),
		TEXT("gg.........s.........s.www"),
		TEXT("g..........s.........s.www"),
		TEXT("g..........s.........s.www"),
		TEXT("w..ssssssssssssssssss..www"),
		TEXT("ww..................wwwwww"),
		TEXT("wwd..g...........g..wwwwww"),
		TEXT("wwd.....ss....ss...wwwwwww"),
		TEXT("wwwddddsswwwwwssddwwwwwwww"),
		TEXT("wwwwwwwdwwwwwwwwdwwwwwwwww"),
		TEXT("wwwwwwwwwwwwwwwwwwwwwwwwww"),
		TEXT("wwwwwwwwwwwwwwwwwwwwwwwwww"),
	};
	HeightRows = {
		TEXT("89987899879878999999800000"),
		TEXT("78765787656765677889600000"),
		TEXT("66544565444554445678400000"),
		TEXT("33210232101221012345000000"),
		TEXT("63100000000000000123000000"),
		TEXT("52000000000000000010000000"),
		TEXT("41000000000000000100000000"),
		TEXT("51000000000000000000000000"),
		TEXT("52000000000000000000000000"),
		TEXT("30000000000000000000000000"),
		TEXT("40010000000000000000000000"),
		TEXT("41010000000000000000000000"),
		TEXT("52000000000000000010000000"),
		TEXT("51000000000000000010000000"),
		TEXT("30000000000000000000000000"),
		TEXT("41000000000000000000000000"),
		TEXT("30000000000000000000000000"),
		TEXT("30000000000000000000000000"),
		TEXT("00000000000000000000000000"),
		TEXT("00000000000000000000000000"),
		TEXT("00000100000000000100000000"),
		TEXT("00000000000000000000000000"),
		TEXT("00000000000000000000000000"),
		TEXT("00000000000000000000000000"),
		TEXT("00000000000000000000000000"),
		TEXT("00000000000000000000000000"),
	};
	// Scenery (see PropRows), in clusters: woods of pines and round trees on the top cliffs and down the left wall,
	// bushes along the terraces and on the hills, boulders where the waterfall lands and around the docks, reed beds along
	// the shores of the river and the bay (low things only near the camera)
	PropRows = {
		TEXT("PTPPTPP...PPP.TPPTP......."),
		TEXT("TPPT.PT.....PPT..PP......."),
		TEXT("PT.B.......B..BB...R.R...."),
		TEXT(".BBB...BB.......BBB.RL...."),
		TEXT("TB...............BB..L...."),
		TEXT("PB........................"),
		TEXT("T........................."),
		TEXT("PB........................"),
		TEXT("TT.....................L.."),
		TEXT(".......................L.."),
		TEXT("T........................."),
		TEXT("PB........................"),
		TEXT("TT........................"),
		TEXT("PB........................"),
		TEXT("B.......................L."),
		TEXT("RB.....................L.."),
		TEXT(".........................."),
		TEXT("T........................."),
		TEXT("L........................."),
		TEXT("LL..................LR...."),
		TEXT("R....B...........B..LR...."),
		TEXT("L..................L......"),
		TEXT(".L.......LLLL.....R......."),
		TEXT(".........................."),
		TEXT(".........................."),
		TEXT(".........................."),
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Pine(TEXT("/Game/Environment/Props/SM_PineTree.SM_PineTree"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> RoundTree(TEXT("/Game/Environment/Props/SM_RoundTree.SM_RoundTree"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Bush(TEXT("/Game/Environment/Props/SM_FlowerBush.SM_FlowerBush"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Boulders(TEXT("/Game/Environment/Props/SM_Boulders.SM_Boulders"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Pond(TEXT("/Game/Environment/Props/SM_PondPlants.SM_PondPlants"));
	for (const ConstructorHelpers::FObjectFinder<UStaticMesh>* Finder : { &Pine, &RoundTree, &Bush, &Boulders, &Pond })
	{
		PropMeshes.Add(Finder->Succeeded() ? Finder->Object : nullptr);
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
			else if (Type == TEXT('w') || Type == TEXT('d'))
			{
				WaterSurface->AddInstance(FTransform(FRotator::ZeroRotator, Base + FVector(0.f, 0.f, WaterLift), FVector(CellScale, CellScale, 1.f)));
				if (Type == TEXT('w'))
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

	// Scenery on top of it all
	TArray<UInstancedStaticMeshComponent*> Props;
	for (UStaticMesh* Mesh : PropMeshes)
	{
		Props.Add(Mesh ? MakeInstances(Mesh, nullptr, false) : nullptr);
	}
	FRandomStream PropStream(PropSeed);
	int32 PropCount = 0;
	const FString Letters(PropLetters);
	for (int32 Row = 0; Row < FMath::Min(Rows, PropRows.Num()); ++Row)
	{
		for (int32 Col = 0; Col < FMath::Min(Cols, PropRows[Row].Len()); ++Col)
		{
			int32 Kind = INDEX_NONE;
			if (!Letters.FindChar(PropRows[Row][Col], Kind) || !Props.IsValidIndex(Kind) || !Props[Kind])
			{
				continue;
			}
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
			case TEXT('w'):
				Top = WaterLift - (PropLetters[Kind] == TEXT('R') ? BoulderSink : 0.f);
				break;
			default:
				break;
			}
			// A few to the cell, spread out (bigger ones further apart), each turned and sized its own way
			TArray<FVector2D, TInlineAllocator<4>> Taken;
			const int32 Count = PropStream.RandRange(PropCounts[Kind].X, PropCounts[Kind].Y);
			for (int32 Item = 0; Item < Count; ++Item)
			{
				const float Size = PropStream.FRandRange(PropScales[Kind].X, PropScales[Kind].Y);
				FVector2D Spot = FVector2D::ZeroVector;
				for (int32 Try = 0; Try < PropTries; ++Try)
				{
					Spot = FVector2D(PropStream.FRandRange(-PropJitter, PropJitter), PropStream.FRandRange(-PropJitter, PropJitter));
					if (!Taken.ContainsByPredicate([&Spot, Size](const FVector2D& Other) { return FVector2D::Distance(Spot, Other) < PropSpacing * Size; }))
					{
						break;
					}
				}
				Taken.Add(Spot);
				const FRotator Turn(0.f, PropStream.FRandRange(0.f, 360.f), 0.f);
				Props[Kind]->AddInstance(FTransform(Turn, CellCenter(Row, Col) + FVector(Spot.X * CellSize, Spot.Y * CellSize, Top), FVector(Size)));
				++PropCount;
			}
		}
	}

	// Water over the rest of the floor around the island, so its edge is a shore and not the end of the world
	const FBox Floor = Hit.GetComponent()->Bounds.GetBox();
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

	UE_LOG(LogTemp, Log, TEXT("RRRandom: island %d x %d cells: %d solid, %d sand, %d water, %d dock, %d props."), Cols, Rows, Counts[0], Counts[1], Counts[2], Counts[3], PropCount);
}
