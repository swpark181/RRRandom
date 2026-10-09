#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "RRRandomOverheadDieComponent.generated.h"

/**
 * The 3D die (/Game/Props/Dice/SM_Dice) that shows each roll over its brawler's head, on every machine.
 * When the brawler's dice component throws a die it pops up, tumbles along a little arc for the die's RollSpinTime,
 * lands with the rolled face toward this machine's camera (when the roll takes effect), stays up for HoldTime
 * and shrinks away. The HUD tells it where its indicator stack ends (SetStackTop) so the die floats just above it.
 * Purely cosmetic: not replicated, no collision, no shadow.
 */
UCLASS(ClassGroup = (RRRandom))
class RRRANDOM_API URRRandomOverheadDieComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	URRRandomOverheadDieComponent();

	/** Top of the HUD's indicators over the head this frame, in the world; the die sits on top of it. */
	void SetStackTop(const FVector& WorldLocation);

	/** Seconds the die stays up after landing, before shrinking away. The HUD's result label is timed to match. */
	static constexpr float HoldTime = 1.3f;
	static constexpr float ExitTime = 0.3f;

	/** Size of the die; the mesh's longest edge is 100. */
	UPROPERTY(EditAnywhere, Category = "Dice")
	float DieScale = 0.55f;

	/** Gap between the HUD's indicators and the die, in world units along the screen's up. */
	UPROPERTY(EditAnywhere, Category = "Dice")
	float StackGap = 10.f;

	/** How high the tumbling die's arc goes above where it lands. */
	UPROPERTY(EditAnywhere, Category = "Dice")
	float HopHeight = 45.f;

	/** Full turns the die makes while tumbling, slowing down to land. */
	UPROPERTY(EditAnywhere, Category = "Dice")
	float SpinTurns = 2.5f;

	/** Above the capsule top when no HUD places the die. */
	UPROPERTY(EditAnywhere, Category = "Dice")
	float DefaultLift = 120.f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** Rotation that turns the given face toward the camera, with its pips upright on screen. */
	static FQuat FaceToward(int32 Face, const FQuat& Camera);

	/** RollCount of the roll being shown; a different count is a new throw. */
	int32 ShownRoll = 0;
	FVector SpinAxis = FVector::UpVector;
	/** Stack top relative to the owner, eased so the die doesn't jump when chips come and go. */
	FVector StackOffset = FVector::ZeroVector;
	FVector TargetStackOffset = FVector::ZeroVector;
	bool bHasStackTop = false;
	bool bSnapStack = true;
};
