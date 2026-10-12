#include "RRRandomDummy.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "RRRandomDummy"

namespace
{
	const FName ColorParameter(TEXT("DiffuseColor"));
	const FName RagdollProfile(TEXT("Ragdoll"));

	// Tilted to face the player's fixed top-down camera (boom pitch -60, looking along +X)
	const FRotator FacingCamera(60.f, 180.f, 0.f);

	constexpr float HitFlashDuration = 0.12f;
	constexpr float DamageNumberLifetime = 0.8f;
	constexpr float DamageNumberRiseSpeed = 120.f;
}

ARRRandomDummy::ARRRandomDummy()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Nobody possesses the dummy, but it should still settle onto the floor
	AutoPossessAI = EAutoPossessAI::Disabled;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannequinMesh(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/TutorialTPP.TutorialTPP"));
	static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAnim(TEXT("/Engine/Tutorial/SubEditors/TutorialAssets/Character/Tutorial_Idle.Tutorial_Idle"));
	// Engine material that works on skeletal meshes and has a "DiffuseColor" parameter
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TintableMaterial(TEXT("/Engine/TemplateResources/M_Template_Master.M_Template_Master"));

	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));
	if (MannequinMesh.Succeeded())
	{
		BodyMesh->SetSkeletalMeshAsset(MannequinMesh.Object);
	}
	if (TintableMaterial.Succeeded())
	{
		BodyBaseMaterial = TintableMaterial.Object;
	}
	if (IdleAnim.Succeeded())
	{
		IdleAnimation = IdleAnim.Object;
		BodyMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		BodyMesh->AnimationData.AnimToPlay = IdleAnimation;
		BodyMesh->AnimationData.bSavedLooping = true;
		BodyMesh->AnimationData.bSavedPlaying = true;
	}

	HealthText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("HealthText"));
	HealthText->SetupAttachment(RootComponent);
	HealthText->SetUsingAbsoluteRotation(true);
	HealthText->SetRelativeLocationAndRotation(FVector(0.f, 0.f, 130.f), FacingCamera);
	HealthText->SetHorizontalAlignment(EHTA_Center);
	HealthText->SetVerticalAlignment(EVRTA_TextCenter);
	HealthText->SetWorldSize(36.f);
}

void ARRRandomDummy::BeginPlay()
{
	Super::BeginPlay();

	USkeletalMeshComponent* BodyMesh = GetMesh();
	MeshRelativeLocation = BodyMesh->GetRelativeLocation();
	MeshRelativeRotation = BodyMesh->GetRelativeRotation();
	MeshCollisionProfile = BodyMesh->GetCollisionProfileName();

	// One tintable material shared by every slot so the whole body changes color and flashes together
	BodyMaterial = UMaterialInstanceDynamic::Create(BodyBaseMaterial, this);
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(ColorParameter, BodyColor);
		for (int32 Slot = 0; Slot < FMath::Max(1, BodyMesh->GetNumMaterials()); ++Slot)
		{
			BodyMesh->SetMaterial(Slot, BodyMaterial);
		}
	}

	Health = MaxHealth;
	UpdateHealthText();
}

float ARRRandomDummy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (Damage <= 0.f || bIsDown)
	{
		return 0.f;
	}

	Health = FMath::Max(0.f, Health - Damage);
	SpawnDamageNumber(Damage);
	UpdateHealthText();

	if (Health > 0.f)
	{
		StartHitFlash();
		return Damage;
	}

	FVector ShotDirection = DamageCauser ? DamageCauser->GetActorForwardVector() : -GetActorForwardVector();
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		ShotDirection = static_cast<const FPointDamageEvent&>(DamageEvent).ShotDirection;
	}
	FallDown(ShotDirection);
	return Damage;
}

void ARRRandomDummy::FallDown(const FVector& ShotDirection)
{
	bIsDown = true;
	EndHitFlash();

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();

	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetCollisionProfileName(RagdollProfile);
	BodyMesh->SetSimulatePhysics(true);
	BodyMesh->SetAllPhysicsLinearVelocity(ShotDirection.GetSafeNormal2D() * KnockdownSpeed + FVector(0.f, 0.f, 200.f));

	HealthText->SetText(LOCTEXT("KnockedOut", "KO!"));
	GetWorldTimerManager().SetTimer(RespawnTimer, this, &ARRRandomDummy::StandUp, RespawnDelay);
}

void ARRRandomDummy::StandUp()
{
	USkeletalMeshComponent* BodyMesh = GetMesh();
	BodyMesh->SetSimulatePhysics(false);
	BodyMesh->SetCollisionProfileName(MeshCollisionProfile);
	BodyMesh->AttachToComponent(GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	BodyMesh->SetRelativeLocationAndRotation(MeshRelativeLocation, MeshRelativeRotation);
	if (IdleAnimation)
	{
		BodyMesh->PlayAnimation(IdleAnimation, true);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	Health = MaxHealth;
	bIsDown = false;
	UpdateHealthText();
}

void ARRRandomDummy::StartHitFlash()
{
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(ColorParameter, HitColor);
	}
	GetWorldTimerManager().SetTimer(HitFlashTimer, this, &ARRRandomDummy::EndHitFlash, HitFlashDuration);
}

void ARRRandomDummy::EndHitFlash()
{
	GetWorldTimerManager().ClearTimer(HitFlashTimer);
	if (BodyMaterial)
	{
		BodyMaterial->SetVectorParameterValue(ColorParameter, BodyColor);
	}
}

void ARRRandomDummy::SpawnDamageNumber(float Damage)
{
	UTextRenderComponent* Number = NewObject<UTextRenderComponent>(this);
	Number->SetupAttachment(RootComponent);
	Number->SetUsingAbsoluteRotation(true);
	Number->SetRelativeLocationAndRotation(FVector(FMath::FRandRange(-20.f, 20.f), FMath::FRandRange(-40.f, 40.f), 160.f), FacingCamera);
	Number->SetHorizontalAlignment(EHTA_Center);
	Number->SetVerticalAlignment(EVRTA_TextCenter);
	Number->SetWorldSize(48.f);
	Number->SetTextRenderColor(FColor::Yellow);
	Number->SetText(FText::AsNumber(FMath::RoundToInt(Damage)));
	Number->RegisterComponent();

	DamageNumbers.Add(Number);
	DamageNumberAges.Add(0.f);
}

void ARRRandomDummy::UpdateHealthText()
{
	HealthText->SetText(FText::Format(LOCTEXT("Health", "HP {0}"), FText::AsNumber(FMath::CeilToInt(Health))));
}

void ARRRandomDummy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	for (int32 Index = DamageNumbers.Num() - 1; Index >= 0; --Index)
	{
		DamageNumberAges[Index] += DeltaSeconds;
		if (DamageNumberAges[Index] >= DamageNumberLifetime)
		{
			DamageNumbers[Index]->DestroyComponent();
			DamageNumbers.RemoveAt(Index);
			DamageNumberAges.RemoveAt(Index);
			continue;
		}
		DamageNumbers[Index]->AddWorldOffset(FVector(0.f, 0.f, DamageNumberRiseSpeed * DeltaSeconds));
	}
}

#undef LOCTEXT_NAMESPACE
