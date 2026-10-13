#include "RRRandomSkidMarks.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName OpacityParameter(TEXT("Opacity"));
	// How far above and below the wheel spot a mark reaches onto the ground (the blocks' tops are bumpy)
	constexpr float ProjectionDepth = 30.f;
}

ARRRandomSkidMarks::ARRRandomSkidMarks()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mark(TEXT("/Game/Effects/Skid/M_SkidMark.M_SkidMark"));
	MarkMaterial = Mark.Succeeded() ? Mark.Object : nullptr;
}

ARRRandomSkidMarks* ARRRandomSkidMarks::Get(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	if (TActorIterator<ARRRandomSkidMarks> It(World); It)
	{
		return *It;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<ARRRandomSkidMarks>(Params);
}

void ARRRandomSkidMarks::AddMark(const FVector& From, const FVector& To, float Width, float Strength)
{
	if (!MarkMaterial || MaxMarks <= 0)
	{
		return;
	}
	// A new decal until the pool is full, then the oldest one again
	if (Decals.Num() < MaxMarks && !Decals.IsValidIndex(Next))
	{
		UDecalComponent* Decal = NewObject<UDecalComponent>(this);
		Decal->SetupAttachment(RootComponent);
		Decal->SetUsingAbsoluteLocation(true);
		Decal->SetUsingAbsoluteRotation(true);
		Decal->RegisterComponent();
		UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(MarkMaterial, this);
		Decal->SetDecalMaterial(Material);
		Decals.Add(Decal);
		Materials.Add(Material);
		Ages.Add(-1.f);
		Strengths.Add(0.f);
	}
	const int32 Index = Next;
	Next = (Next + 1) % MaxMarks;

	// Projected straight down (the decal's X), its length (Z) along the strip and its width (Y) across it
	const FVector Along = (To - From) * FVector(1.f, 1.f, 0.f);
	const float Length = Along.Size();
	if (Length < KINDA_SMALL_NUMBER)
	{
		return;
	}
	UDecalComponent* Decal = Decals[Index];
	Decal->SetWorldLocationAndRotation((From + To) * 0.5f, FRotationMatrix::MakeFromXZ(-FVector::UpVector, Along / Length).Rotator());
	Decal->DecalSize = FVector(ProjectionDepth, Width * 0.5f, Length * 0.5f);
	Decal->MarkRenderStateDirty();
	Decal->SetVisibility(true);
	Materials[Index]->SetScalarParameterValue(OpacityParameter, Strength);
	if (Ages[Index] < 0.f)
	{
		++Alive;
	}
	Ages[Index] = 0.f;
	Strengths[Index] = Strength;
}

void ARRRandomSkidMarks::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Alive == 0)
	{
		return;
	}
	for (int32 Index = 0; Index < Decals.Num(); ++Index)
	{
		if (Ages[Index] < 0.f)
		{
			continue;
		}
		Ages[Index] += DeltaSeconds;
		const float Fade = (Ages[Index] - HoldTime) / FMath::Max(FadeTime, KINDA_SMALL_NUMBER);
		if (Fade >= 1.f)
		{
			Ages[Index] = -1.f;
			--Alive;
			Decals[Index]->SetVisibility(false);
		}
		else if (Fade > 0.f)
		{
			Materials[Index]->SetScalarParameterValue(OpacityParameter, Strengths[Index] * (1.f - FMath::SmoothStep(0.f, 1.f, Fade)));
		}
	}
}
