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
	// Rock blocks in a cliff vary this much in width and sit this far off center, so the stack reads as rocks
	constexpr float RockShrink = 0.06f;
	constexpr float RockJitter = 5.f;
	constexpr int32 LookSeed = 4242;
}

ARRRandomIsland::ARRRandomIsland()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// The map: rock cliffs and a waterfall (F) at the top, a river down the right, a bay with docks at the bottom,
	// low grass hills (g) and sand paths (s) around a flat middle. Blue starts on the bottom sand path (row 18),
	// red on the top one (row 7); the game mode's cover stands on flat cells in rows 10 to 15.
	TypeRows = {
		TEXT("rrrrrrrrrrrrrrrrrrrrFwwwww"),
		TEXT("rrrrrrrrrrrrrrrrrrrrFwwwww"),
		TEXT("rrrrgggrrrrrrgggrrrrFwwwww"),
		TEXT("rrrg...ggrrgg...grrrwwwwww"),
		TEXT("rrg..............grr.wwwww"),
		TEXT("rr................g..swwww"),
		TEXT("rr...ss..........g...swwww"),
		TEXT("rg....sssssssssss.....dwww"),
		TEXT("rg.........s.........s.www"),
		TEXT("r..........s.........s.www"),
		TEXT("r..g.......s.........s..ww"),
		TEXT("r..g.......s.........s..ww"),
		TEXT("rr.........s......g..s..ww"),
		TEXT("rr.........s......g..s..ww"),
		TEXT("rg.........s.........s..ww"),
		TEXT("rg.........s.........s.www"),
		TEXT("r..........s.........s.www"),
		TEXT("r..........s.........s.www"),
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
		TEXT("98999899989999899999800000"),
		TEXT("87888778887788878888600000"),
		TEXT("77663337666773336677400000"),
		TEXT("65520002244220002455000000"),
		TEXT("54200000000000000133000000"),
		TEXT("43000000000000000010000000"),
		TEXT("43000000000000000100000000"),
		TEXT("42000000000000000000000000"),
		TEXT("32000000000000000000000000"),
		TEXT("30000000000000000000000000"),
		TEXT("30010000000000000000000000"),
		TEXT("30010000000000000000000000"),
		TEXT("32000000000000000010000000"),
		TEXT("32000000000000000010000000"),
		TEXT("32000000000000000000000000"),
		TEXT("32000000000000000000000000"),
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Rock(TEXT("/Game/Environment/Island/M_IslandRock.M_IslandRock"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Sand(TEXT("/Game/Environment/Island/M_IslandSand.M_IslandSand"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Wood(TEXT("/Game/Environment/Island/M_IslandWood.M_IslandWood"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Water(TEXT("/Game/Environment/Island/M_IslandWater.M_IslandWater"));
	Cube = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	Plane = PlaneMesh.Succeeded() ? PlaneMesh.Object : nullptr;
	RockMaterial = Rock.Succeeded() ? Rock.Object : nullptr;
	SandMaterial = Sand.Succeeded() ? Sand.Object : nullptr;
	WoodMaterial = Wood.Succeeded() ? Wood.Object : nullptr;
	WaterMaterial = Water.Succeeded() ? Water.Object : nullptr;

	// The host spawns it; clients build the same island from the same map (they need its walls to move)
	bReplicates = true;
	bAlwaysRelevant = true;
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
	UInstancedStaticMeshComponent* Rocks = MakeInstances(Cube, RockMaterial, false, 1);
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

			if (Type == TEXT('g') && Levels > 0 && Grass.Num() > 0)
			{
				// One grass block stretched to the hill's height, a random one of the set, any quarter turn
				const FRotator Turn(0.f, 90.f * Stream.RandRange(0, 3), 0.f);
				Grass[Stream.RandRange(0, Grass.Num() - 1)]->AddInstance(FTransform(Turn, Base, FVector(CellScale * GrassOverlap, CellScale * GrassOverlap, Height / BasicSize)));
			}
			else if (Type == TEXT('r') && Levels > 0)
			{
				// A stack of rocks, one per level, each a little narrower or off center and its own shade
				for (int32 Level = 0; Level < Levels; ++Level)
				{
					const float Width = CellScale * (1.f - Stream.FRandRange(0.f, RockShrink));
					const FVector Offset(Stream.FRandRange(-RockJitter, RockJitter), Stream.FRandRange(-RockJitter, RockJitter), (Level + 0.5f) * LevelHeight);
					const int32 Index = Rocks->AddInstance(FTransform(FRotator::ZeroRotator, Base + Offset, FVector(Width, Width, LevelHeight / BasicSize)));
					Rocks->SetCustomDataValue(Index, 0, Stream.FRand());
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

	UE_LOG(LogTemp, Log, TEXT("RRRandom: island %d x %d cells: %d solid, %d sand, %d water, %d dock."), Cols, Rows, Counts[0], Counts[1], Counts[2], Counts[3]);
}
