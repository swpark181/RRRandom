#include "RRRandomCover.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName ColorParameter(TEXT("Color"));
	// The engine cube is 100 units on each side, centered on its pivot
	constexpr float CubeSize = 100.f;
}

ARRRandomCover::ARRRandomCover()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Block = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Block"));
	Block->SetupAttachment(RootComponent);
	// Stops brawlers and shots (WorldStatic); shots that hit it just disappear
	Block->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	if (CubeMesh.Succeeded())
	{
		Block->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Block->SetMaterial(0, ShapeMaterial.Object);
	}

	// The look: grass blocks over the same box. The cube keeps colliding, just unseen.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassBlockA(TEXT("/Game/Environment/Grass/SM_GrassBlock_A.SM_GrassBlock_A"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassBlockB(TEXT("/Game/Environment/Grass/SM_GrassBlock_B.SM_GrassBlock_B"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassBlockC(TEXT("/Game/Environment/Grass/SM_GrassBlock_C.SM_GrassBlock_C"));
	for (const ConstructorHelpers::FObjectFinder<UStaticMesh>* Finder : { &GrassBlockA, &GrassBlockB, &GrassBlockC })
	{
		if (!Finder->Succeeded())
		{
			continue;
		}
		BlockMeshes.Add(Finder->Object);
		UInstancedStaticMeshComponent* Look = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("GrassBlocks%d"), BlockLooks.Num()));
		Look->SetupAttachment(RootComponent);
		Look->SetStaticMesh(Finder->Object);
		Look->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Look->SetGenerateOverlapEvents(false);
		Look->SetCanEverAffectNavigation(false);
		BlockLooks.Add(Look);
	}
	Block->SetVisibility(BlockLooks.Num() == 0);
	SetSize(Size);

	// The game mode lays cover out on the server; clients need the same blocks to walk, climb and hide behind
	bReplicates = true;
}

void ARRRandomCover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ARRRandomCover, Size);
}

void ARRRandomCover::OnRep_Size()
{
	SetSize(Size);
	if (HasActorBegunPlay())
	{
		RebuildLook();
	}
}

void ARRRandomCover::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Picks up size changes made in the editor
	SetSize(Size);
	RebuildLook();
}

void ARRRandomCover::BeginPlay()
{
	Super::BeginPlay();

	SetSize(Size);
	RebuildLook();
	if (UMaterialInstanceDynamic* Material = Block->CreateAndSetMaterialInstanceDynamic(0))
	{
		Material->SetVectorParameterValue(ColorParameter, Color);
	}
}

void ARRRandomCover::SetSize(const FVector& NewSize)
{
	Size = NewSize.ComponentMax(FVector(1.f));
	Block->SetRelativeScale3D(Size / CubeSize);
	Block->SetRelativeLocation(FVector(0.f, 0.f, Size.Z * 0.5f));
}

void ARRRandomCover::RebuildLook()
{
	if (BlockLooks.Num() == 0 || BlockCellSize <= 0.f)
	{
		return;
	}
	for (UInstancedStaticMeshComponent* Look : BlockLooks)
	{
		Look->ClearInstances();
	}

	// Whole cells near BlockCellSize along each side, one block high (only the top block of a stack shows grass)
	const int32 CountX = FMath::Max(1, FMath::RoundToInt(Size.X / BlockCellSize));
	const int32 CountY = FMath::Max(1, FMath::RoundToInt(Size.Y / BlockCellSize));
	const FVector2D Cell(Size.X / CountX, Size.Y / CountY);
	// Squarish cells can take any quarter turn; long ones only half turns, so the block keeps its stretch
	const bool bSquare = FMath::Abs(Cell.X - Cell.Y) < 0.1f * FMath::Max(Cell.X, Cell.Y);

	// The same picks on every machine: seeded by where the cover stands
	const FVector Location = GetActorLocation();
	FRandomStream Stream(HashCombine(GetTypeHash(FMath::RoundToInt(Location.X)), GetTypeHash(FMath::RoundToInt(Location.Y))));
	for (int32 X = 0; X < CountX; ++X)
	{
		for (int32 Y = 0; Y < CountY; ++Y)
		{
			const int32 Turns = bSquare ? Stream.RandRange(0, 3) : Stream.RandRange(0, 1) * 2;
			const FVector2D Span = (Turns % 2 ? FVector2D(Cell.Y, Cell.X) : Cell) * BlockOverlap;
			const FVector Center((X + 0.5f) * Cell.X - Size.X * 0.5f, (Y + 0.5f) * Cell.Y - Size.Y * 0.5f, 0.f);
			const FTransform Instance(FRotator(0.f, 90.f * Turns, 0.f), Center, FVector(Span.X, Span.Y, Size.Z) / CubeSize);
			BlockLooks[Stream.RandRange(0, BlockLooks.Num() - 1)]->AddInstance(Instance);
		}
	}
}

float ARRRandomCover::GetExtentAlong(const FVector& Direction) const
{
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Direction.GetSafeNormal2D());
	const FVector Scale = GetActorScale3D();
	return FMath::Abs(Local.X) * Size.X * 0.5f * Scale.X + FMath::Abs(Local.Y) * Size.Y * 0.5f * Scale.Y;
}
