#include "RRRandomEventComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "RRRandomEvents"

namespace
{
	constexpr float SpinSpeed = 720.f;
}

URRRandomEventComponent::URRRandomEventComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void URRRandomEventComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Seed != 0)
	{
		Stream.Initialize(Seed);
	}
	else
	{
		Stream.GenerateNewSeed();
	}

	AActor* Owner = GetOwner();
	TArray<UMeshComponent*> OwnerMeshes;
	Owner->GetComponents<UMeshComponent>(OwnerMeshes);
	for (UMeshComponent* Mesh : OwnerMeshes)
	{
		Meshes.Add(Mesh);
		BaseMeshScales.Add(Mesh->GetRelativeScale3D());
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(Slot))
			{
				Materials.Add(Material);
			}
		}
	}
}

FText URRRandomEventComponent::RollRandomEvent()
{
	FText Description;
	switch (Stream.RandRange(0, 4))
	{
	case 0: Description = ApplyRandomColor(); break;
	case 1: Description = ApplyRandomSize(); break;
	case 2: Description = ApplyHop(); break;
	case 3: Description = ApplyRandomSpeed(); break;
	default: Description = ApplySpin(); break;
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow, Description.ToString());
	}
	OnRandomEvent.Broadcast(Description);
	return Description;
}

FText URRRandomEventComponent::ApplyRandomColor()
{
	const FLinearColor Color = FLinearColor::MakeFromHSV8(static_cast<uint8>(Stream.RandRange(0, 255)), 200, 255);
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		Material->SetVectorParameterValue(ColorParameterName, Color);
	}
	return LOCTEXT("Color", "New color!");
}

FText URRRandomEventComponent::ApplyRandomSize()
{
	const float Scale = Stream.FRandRange(0.6f, 1.6f);
	for (int32 Index = 0; Index < Meshes.Num(); ++Index)
	{
		Meshes[Index]->SetRelativeScale3D(BaseMeshScales[Index] * Scale);
	}
	FNumberFormattingOptions Format;
	Format.MaximumFractionalDigits = 1;
	return FText::Format(LOCTEXT("Size", "Size x{0}"), FText::AsNumber(Scale, &Format));
}

FText URRRandomEventComponent::ApplyHop()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->LaunchCharacter(FVector(0.f, 0.f, Stream.FRandRange(400.f, 900.f)), false, true);
	}
	return LOCTEXT("Hop", "Boing!");
}

FText URRRandomEventComponent::ApplyRandomSpeed()
{
	SpeedMultiplier = Stream.FRandRange(0.5f, 1.8f);
	GetWorld()->GetTimerManager().SetTimer(SpeedResetTimer, this, &URRRandomEventComponent::ResetSpeed, SpeedEffectDuration);
	return SpeedMultiplier >= 1.f ? LOCTEXT("Fast", "Zoom zoom!") : LOCTEXT("Slow", "Sleepy feet...");
}

FText URRRandomEventComponent::ApplySpin()
{
	SpinRemaining = 360.f * Stream.RandRange(1, 3);
	SetComponentTickEnabled(true);
	return LOCTEXT("Spin", "Wheee!");
}

void URRRandomEventComponent::ResetSpeed()
{
	SpeedMultiplier = 1.f;
}

void URRRandomEventComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float Step = FMath::Min(SpinSpeed * DeltaTime, SpinRemaining);
	GetOwner()->AddActorWorldRotation(FRotator(0.f, Step, 0.f));
	SpinRemaining -= Step;
	if (SpinRemaining <= 0.f)
	{
		SetComponentTickEnabled(false);
	}
}

#undef LOCTEXT_NAMESPACE
