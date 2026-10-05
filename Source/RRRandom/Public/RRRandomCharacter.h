#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "RRRandomCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class URRRandomEventComponent;

/** Player pawn seen from a fixed top-down camera. Built from engine basic shapes so it needs no project assets. */
UCLASS()
class RRRANDOM_API ARRRandomCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARRRandomCharacter();

	URRRandomEventComponent* GetRandomEvents() const { return RandomEvents; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> TopDownCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Body")
	TObjectPtr<UStaticMeshComponent> Nose;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Random")
	TObjectPtr<URRRandomEventComponent> RandomEvents;
};
