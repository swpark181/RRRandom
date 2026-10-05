#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "RRRandomEventComponent.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRRRandomEventSignature, const FText&, Description);

/** Rolls a harmless random effect on its owner: a new color, size, hop, speed or spin. */
UCLASS(ClassGroup = (RRRandom), meta = (BlueprintSpawnableComponent))
class RRRANDOM_API URRRandomEventComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URRRandomEventComponent();

	/** Picks and applies one random event, returning what happened. */
	UFUNCTION(BlueprintCallable, Category = "RRRandom")
	FText RollRandomEvent();

	/** Fires after every roll so UI can show the result. */
	UPROPERTY(BlueprintAssignable, Category = "RRRandom")
	FRRRandomEventSignature OnRandomEvent;

	/** 0 picks a new seed each play session. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RRRandom")
	int32 Seed = 0;

	/** How long a speed change lasts before walking speed returns to normal. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RRRandom")
	float SpeedEffectDuration = 5.f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	FText ApplyRandomColor();
	FText ApplyRandomSize();
	FText ApplyHop();
	FText ApplyRandomSpeed();
	FText ApplySpin();
	void ResetSpeed();

	FRandomStream Stream;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Meshes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	TArray<FVector> BaseMeshScales;
	float BaseWalkSpeed = 0.f;
	float SpinRemaining = 0.f;
	FTimerHandle SpeedResetTimer;
};
