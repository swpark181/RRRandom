#include "RRRandomEventComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "RRRandomEvents"

namespace
{
	const FName ColorParameter(TEXT("Color"));
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
	TArray<UStaticMeshComponent*> OwnerMeshes;
	Owner->GetComponents<UStaticMeshComponent>(OwnerMeshes);
	for (UStaticMeshComponent* Mesh : OwnerMeshes)
	{
		Meshes.Add(Mesh);
		BaseMeshScales.Add(Mesh->GetRelativeScale3D());
		if (UMaterialInstanceDynamic* Material = Mesh->CreateAndSetMaterialInstanceDynamic(0))
		{
			Materials.Add(Material);
		}
	}

	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		BaseWalkSpeed = Character->GetCharacterMovement()->MaxWalkSpeed;
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
		Material->SetVectorParameterValue(ColorParameter, Color);
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
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return FText::GetEmpty();
	}

	const float Multiplier = Stream.FRandRange(0.5f, 1.8f);
	Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * Multiplier;
	GetWorld()->GetTimerManager().SetTimer(SpeedResetTimer, this, &URRRandomEventComponent::ResetSpeed, SpeedEffectDuration);
	return Multiplier >= 1.f ? LOCTEXT("Fast", "Zoom zoom!") : LOCTEXT("Slow", "Sleepy feet...");
}

FText URRRandomEventComponent::ApplySpin()
{
	SpinRemaining = 360.f * Stream.RandRange(1, 3);
	SetComponentTickEnabled(true);
	return LOCTEXT("Spin", "Wheee!");
}

void URRRandomEventComponent::ResetSpeed()
{
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Character->GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	}
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
