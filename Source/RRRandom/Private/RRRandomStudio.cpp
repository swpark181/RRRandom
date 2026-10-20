#include "RRRandomStudio.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// The floor: this wide each way, its grid this far out from the middle, a line every GridStep, every MajorEvery-th darker
	constexpr float FloorHalf = 20000.f;
	constexpr float GridHalf = 6000.f;
	constexpr float GridStep = 100.f;
	constexpr int32 MajorEvery = 5;
	constexpr float MinorWidth = 3.f;
	constexpr float MajorWidth = 8.f;
	constexpr float AxisWidth = 14.f;
	// The lines lie just over the floor
	constexpr float LineLift = 0.6f;
	constexpr float FloorSearchUp = 500.f;
	constexpr float FloorSearchDown = 3000.f;

	const FLinearColor FloorColor(0.24f, 0.25f, 0.29f);
	const FLinearColor MinorColor(0.13f, 0.14f, 0.17f);
	const FLinearColor MajorColor(0.05f, 0.055f, 0.07f);
	const FLinearColor AxisXColor(0.95f, 0.38f, 0.55f);
	const FLinearColor AxisYColor(0.3f, 0.82f, 0.62f);
	const FName ColorParameter(TEXT("Color"));
}

ARRRandomStudio::ARRRandomStudio()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	bReplicates = true;
	bAlwaysRelevant = true;
}

FBox ARRRandomStudio::GetFlightArea() const
{
	// Over the grid and a little past it, the feet up to well over anything tested here (the robot stands 640)
	const FVector Middle = GetActorLocation();
	return FBox(FVector(Middle.X - GridHalf - 2000.f, Middle.Y - GridHalf - 2000.f, FloorZ - 1000.f),
		FVector(Middle.X + GridHalf + 2000.f, Middle.Y + GridHalf + 2000.f, FloorZ + 3000.f));
}

void ARRRandomStudio::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ARRRandomStudio::Build()
{
	UWorld* World = GetWorld();
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Shape = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!World || !Cube)
	{
		return;
	}
	// The level's floor under it: the studio's floor goes on top and hides it
	FloorZ = GetActorLocation().Z;
	FHitResult Hit;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, FloorSearchUp);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomStudio), false, this);
	if (World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, FloorSearchUp + FloorSearchDown), ECC_Visibility, Params) && Hit.GetComponent())
	{
		FloorZ = Hit.ImpactPoint.Z;
		Hit.GetComponent()->SetVisibility(false);
	}
	auto Tinted = [&](UMeshComponent* Mesh, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* Material = Shape ? UMaterialInstanceDynamic::Create(Shape, this) : nullptr)
		{
			Material->SetVectorParameterValue(ColorParameter, Color);
			Mesh->SetMaterial(0, Material);
		}
	};

	// A wide slab, its top on the floor: stops everything like the ground does
	Floor = NewObject<UStaticMeshComponent>(this);
	Floor->SetStaticMesh(Cube);
	Floor->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Floor->SetupAttachment(RootComponent);
	Floor->RegisterComponent();
	Floor->SetWorldLocationAndRotation(FVector(GetActorLocation().X, GetActorLocation().Y, FloorZ - 50.f), FRotator::ZeroRotator);
	Floor->SetWorldScale3D(FVector(FloorHalf * 2.f / 100.f, FloorHalf * 2.f / 100.f, 1.f));
	Tinted(Floor, FloorColor);

	auto MakeLines = [&](const FLinearColor& Color)
	{
		UInstancedStaticMeshComponent* Lines = NewObject<UInstancedStaticMeshComponent>(this);
		Lines->SetStaticMesh(Cube);
		Lines->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Lines->SetGenerateOverlapEvents(false);
		Lines->SetCanEverAffectNavigation(false);
		Lines->CastShadow = false;
		Lines->SetupAttachment(RootComponent);
		Lines->SetUsingAbsoluteLocation(true);
		Lines->SetUsingAbsoluteRotation(true);
		Lines->SetUsingAbsoluteScale(true);
		Lines->RegisterComponent();
		Lines->SetWorldTransform(FTransform::Identity);
		Tinted(Lines, Color);
		return Lines;
	};
	MinorLines = MakeLines(MinorColor);
	MajorLines = MakeLines(MajorColor);
	AxisX = MakeLines(AxisXColor);
	AxisY = MakeLines(AxisYColor);
	const FVector Middle(GetActorLocation().X, GetActorLocation().Y, FloorZ + LineLift * 0.5f);
	const int32 Count = FMath::RoundToInt(GridHalf / GridStep);
	for (int32 Index = -Count; Index <= Count; ++Index)
	{
		const float Offset = Index * GridStep;
		UInstancedStaticMeshComponent* Along = Index == 0 ? nullptr : (Index % MajorEvery == 0 ? MajorLines.Get() : MinorLines.Get());
		const float Width = Index == 0 ? AxisWidth : (Index % MajorEvery == 0 ? MajorWidth : MinorWidth);
		// Running along X (across Y at this offset), and along Y
		(Along ? Along : AxisX.Get())->AddInstance(FTransform(FRotator::ZeroRotator, Middle + FVector(0.f, Offset, 0.f),
			FVector(GridHalf * 2.f / 100.f, Width / 100.f, LineLift / 100.f)), true);
		(Along ? Along : AxisY.Get())->AddInstance(FTransform(FRotator::ZeroRotator, Middle + FVector(Offset, 0.f, 0.f),
			FVector(Width / 100.f, GridHalf * 2.f / 100.f, LineLift / 100.f)), true);
	}
	bBuilt = true;
	UE_LOG(LogTemp, Log, TEXT("RRRandom: studio floor at %.0f, a %.0f grid line every %.0f."), FloorZ, GridHalf * 2.f, GridStep);
}
