#include "RRRandomCover.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
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
}

void ARRRandomCover::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Picks up size changes made in the editor
	SetSize(Size);
}

void ARRRandomCover::BeginPlay()
{
	Super::BeginPlay();

	SetSize(Size);
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

float ARRRandomCover::GetExtentAlong(const FVector& Direction) const
{
	const FVector Local = GetActorTransform().InverseTransformVectorNoScale(Direction.GetSafeNormal2D());
	const FVector Scale = GetActorScale3D();
	return FMath::Abs(Local.X) * Size.X * 0.5f * Scale.X + FMath::Abs(Local.Y) * Size.Y * 0.5f * Scale.Y;
}
