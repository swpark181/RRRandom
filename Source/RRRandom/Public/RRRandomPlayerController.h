#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RRRandomPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * Top-down controls: WASD to walk, left mouse to shoot toward the cursor (hold for full auto), R to reload,
 * Space to jump (climbing onto cover when facing it), E to roll the stored buff dice.
 * Online play: F1 hosts a game, F2 joins one, F3 leaves (also the console commands RRHost, RRJoin, RRLeave).
 * Input assets are built in code when none are assigned, so the project runs without .uasset files.
 */
UCLASS()
class RRRANDOM_API ARRRandomPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ARRRandomPlayerController();

	/** The capsule-center point a shot along this cursor ray should go for. */
	FVector GetAimTarget(const FVector& CursorOrigin, const FVector& CursorDirection) const;

	/** Hosts an online game (F1). */
	UFUNCTION(Exec)
	void RRHost();

	/** Joins an online game (F2). */
	UFUNCTION(Exec)
	void RRJoin();

	/** Leaves the online game and plays alone again (F3). */
	UFUNCTION(Exec)
	void RRLeave();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> DiceAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> HostAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> JoinAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LeaveAction;

private:
	void CreateDefaultInputAssets();

	void OnFire();
	void OnMove(const FInputActionValue& Value);
	void OnJump();
	void OnStopJumping();
	void OnRollDice();
	void OnReload();
};
