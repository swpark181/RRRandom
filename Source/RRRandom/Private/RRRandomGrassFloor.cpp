#include "RRRandomGrassFloor.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The engine plane is 100 units square, facing up, centered on its pivot
	constexpr float PlaneSize = 100.f;
	// How far above and below the actor to look for the floor
	constexpr float FloorSearchUp = 500.f;
	constexpr float FloorSearchDown = 3000.f;

	void MakeLooksOnly(UStaticMeshComponent* Mesh)
	{
		Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		// Flat ground: nothing worth a shadow, and hundreds of tiles cost less without one
		Mesh->CastShadow = false;
	}
}

ARRRandomGrassFloor::ARRRandomGrassFloor()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// Plain grass most often; the flowery ones now and then
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plain(TEXT("/Game/Environment/Grass/SM_GrassTile_A.SM_GrassTile_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Flowers(TEXT("/Game/Environment/Grass/SM_GrassTile_B.SM_GrassTile_B"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FewFlowers(TEXT("/Game/Environment/Grass/SM_GrassTile_C.SM_GrassTile_C"));
	auto AddTile = [this](const ConstructorHelpers::FObjectFinder<UStaticMesh>& Finder, float Weight)
	{
		if (Finder.Succeeded())
		{
			Tiles.Add(Finder.Object);
			TileWeights.Add(Weight);
		}
	};
	AddTile(Plain, 5.f);
	AddTile(FewFlowers, 3.f);
	AddTile(Flowers, 2.f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnderMaterial(TEXT("/Game/Environment/Grass/M_GrassUnder.M_GrassUnder"));
	Under = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Under"));
	Under->SetupAttachment(RootComponent);
	MakeLooksOnly(Under);
	if (PlaneMesh.Succeeded())
	{
		Under->SetStaticMesh(PlaneMesh.Object);
	}
	if (UnderMaterial.Succeeded())
	{
		Under->SetMaterial(0, UnderMaterial.Object);
	}
	// Laid in the world in BeginPlay, over the floor found there
	Under->SetUsingAbsoluteLocation(true);
	Under->SetUsingAbsoluteRotation(true);
	Under->SetUsingAbsoluteScale(true);

	// The host spawns it; clients need it (and its seed) to lay the same tiles
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ARRRandomGrassFloor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ARRRandomGrassFloor, Seed, COND_InitialOnly);
}

void ARRRandomGrassFloor::BeginPlay()
{
	Super::BeginPlay();
	LayTiles();
}

void ARRRandomGrassFloor::LayTiles()
{
	UWorld* World = GetWorld();
	if (Tiles.Num() == 0 || TileSize <= 0.f)
	{
		return;
	}

	// The floor: whatever is right under the actor (the same trace the game mode stands brawlers on), and all of it
	FHitResult Hit;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, FloorSearchUp);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomGrassFloor), false, this);
	if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, FloorSearchUp + FloorSearchDown), ECC_Visibility, Params)
		|| !Hit.GetComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no floor under the grass at %s."), *GetActorLocation().ToString());
		return;
	}
	const FBox Floor = Hit.GetComponent()->Bounds.GetBox();
	const float Z = Floor.Max.Z;

	// As many whole tiles as cover the floor, centered on it (a little may hang past the edges)
	const int32 CountX = FMath::Max(1, FMath::RoundToInt((Floor.Max.X - Floor.Min.X) / TileSize));
	const int32 CountY = FMath::Max(1, FMath::RoundToInt((Floor.Max.Y - Floor.Min.Y) / TileSize));
	const FVector Center(Floor.GetCenter().X, Floor.GetCenter().Y, Z + Lift);
	const FVector Corner = Center - FVector(CountX - 1, CountY - 1, 0.f) * (TileSize * 0.5f);

	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		UHierarchicalInstancedStaticMeshComponent* Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		Instances->SetStaticMesh(Tiles[Index]);
		MakeLooksOnly(Instances);
		Instances->SetupAttachment(RootComponent);
		Instances->SetUsingAbsoluteLocation(true);
		Instances->SetUsingAbsoluteRotation(true);
		Instances->SetUsingAbsoluteScale(true);
		Instances->RegisterComponent();
		Instances->SetWorldTransform(FTransform::Identity);
		TileInstances.Add(Instances);
	}

	float TotalWeight = 0.f;
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		TotalWeight += TileWeights.IsValidIndex(Index) ? FMath::Max(0.f, TileWeights[Index]) : 1.f;
	}
	FRandomStream Stream(Seed);
	auto PickTile = [&]()
	{
		float Roll = Stream.FRand() * TotalWeight;
		for (int32 Index = 0; Index < Tiles.Num(); ++Index)
		{
			Roll -= TileWeights.IsValidIndex(Index) ? FMath::Max(0.f, TileWeights[Index]) : 1.f;
			if (Roll < 0.f)
			{
				return Index;
			}
		}
		return Tiles.Num() - 1;
	};

	// Row by row; a tile differs from the one before it and the one beside it in the last row, when the set allows
	TArray<TArray<FTransform>> Transforms;
	Transforms.SetNum(Tiles.Num());
	TArray<int32> PreviousRow;
	PreviousRow.Init(INDEX_NONE, CountY);
	for (int32 X = 0; X < CountX; ++X)
	{
		int32 Left = INDEX_NONE;
		for (int32 Y = 0; Y < CountY; ++Y)
		{
			int32 Tile = PickTile();
			for (int32 Retry = 0; Retry < 4 && Tiles.Num() > 2 && (Tile == Left || Tile == PreviousRow[Y]); ++Retry)
			{
				Tile = PickTile();
			}
			const FRotator Turn(0.f, 90.f * Stream.RandRange(0, 3), 0.f);
			Transforms[Tile].Emplace(Turn, Corner + FVector(X * TileSize, Y * TileSize, 0.f));
			Left = PreviousRow[Y] = Tile;
		}
	}
	for (int32 Index = 0; Index < Tiles.Num(); ++Index)
	{
		TileInstances[Index]->AddInstances(Transforms[Index], false, true);
	}

	// The gap plane, just over the floor and under the tiles' tops
	Under->SetWorldLocationAndRotation(FVector(Center.X, Center.Y, Z + Lift * 0.5f), FRotator::ZeroRotator);
	Under->SetWorldScale3D(FVector(CountX * TileSize / PlaneSize, CountY * TileSize / PlaneSize, 1.f));

	UE_LOG(LogTemp, Log, TEXT("RRRandom: grass floor %d x %d tiles over %s (seed %d)."), CountX, CountY, *Hit.GetComponent()->GetName(), Seed);
}
