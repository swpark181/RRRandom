#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RRRandomDummy.generated.h"

class UAnimSequence;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UTextRenderComponent;

/**
 * Training dummy: an Unreal mannequin that stands still and takes hits.
 * Flashes and shows damage numbers when hit, falls as a ragdoll at 0 health and stands back up after a while.
 */
UCLASS()
class RRRANDOM_API ARRRandomDummy : public ACharacter
{
	GENERATED_BODY()

public:
	ARRRandomDummy();

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Tick(float DeltaSeconds) override;

	bool IsDown() const { return bIsDown; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy")
	float MaxHealth = 100.f;

	/** Seconds spent lying down before standing back up at full health. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy")
	float RespawnDelay = 3.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy")
	FLinearColor BodyColor = FLinearColor(0.9f, 0.45f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy")
	FLinearColor HitColor = FLinearColor(1.f, 0.05f, 0.05f);

	/** Speed the ragdoll is thrown with along the final shot. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dummy")
	float KnockdownSpeed = 500.f;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dummy")
	TObjectPtr<UTextRenderComponent> HealthText;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> IdleAnimation;

	/** Skeletal-mesh-ready material with a "DiffuseColor" parameter; the mannequin's own material has a fixed color and can't be tinted. */
	UPROPERTY(EditAnywhere, Category = "Dummy")
	TObjectPtr<UMaterialInterface> BodyBaseMaterial;

private:
	void FallDown(const FVector& ShotDirection);
	void StandUp();
	void StartHitFlash();
	void EndHitFlash();
	void SpawnDamageNumber(float Damage);
	void UpdateHealthText();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> DamageNumbers;

	TArray<float> DamageNumberAges;
	float Health = 0.f;
	bool bIsDown = false;
	FVector MeshRelativeLocation = FVector::ZeroVector;
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;
	FName MeshCollisionProfile;
	FTimerHandle RespawnTimer;
	FTimerHandle HitFlashTimer;
};
