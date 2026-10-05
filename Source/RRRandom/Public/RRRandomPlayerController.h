#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RRRandomPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Top-down controls: hold left mouse to follow the cursor, click to walk to a spot,
 * WASD to walk, Space to roll a random event.
 * Input assets are built in code when none are assigned, so the project runs without .uasset files.
 */
UCLASS()
class RRRANDOM_API ARRRandomPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARRRandomPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ClickAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> RollAction;

	/** Presses shorter than this count as a click and walk to the clicked spot. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float ShortPressThreshold = 0.3f;

	/** How close to the clicked spot counts as arrived. */
	UPROPERTY(EditAnywhere, Category = "Input")
	float AcceptanceRadius = 30.f;

private:
	void CreateDefaultInputAssets();

	void OnClickStarted();
	void OnClickTriggered();
	void OnClickReleased();
	void OnMove(const FInputActionValue& Value);
	void OnRoll();

	FVector CachedDestination = FVector::ZeroVector;
	float FollowTime = 0.f;
	bool bWalkingToDestination = false;
};
