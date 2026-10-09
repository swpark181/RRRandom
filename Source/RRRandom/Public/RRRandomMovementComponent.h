#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "RRRandomMovementComponent.generated.h"

/**
 * Character movement with the flight power's glide, Pharah style. Rising is the character's held jump
 * (ARRRandomCharacter raises the jump speed and hold time while flying); this adds what happens after letting go:
 * leftover upward speed brakes quickly, then the fall is gentle and capped.
 * Runs the same on the server and the owning client, so the glide is predicted like any other move.
 */
UCLASS()
class RRRANDOM_API URRRandomMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual float GetGravityZ() const override;
	virtual FVector NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const override;
	// The one-argument DoJump forwards to this one
	using Super::DoJump;
	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

	/** Set every frame by the brawler: the flight power is on, or the brawler is still in the air from it. */
	bool bGliding = false;

	/** Highest capsule center a glider can rise to; set by the brawler. Holding jump there just hovers. */
	float FlightCeilingZ = UE_BIG_NUMBER;

	/** Gravity while gliding down, as a share of normal. */
	UPROPERTY(EditAnywhere, Category = "Flight", meta = (ClampMin = "0"))
	float GlideGravityScale = 0.25f;

	/** Gravity while still going up after letting go of jump, so the rise stops soon after the key does. */
	UPROPERTY(EditAnywhere, Category = "Flight", meta = (ClampMin = "0"))
	float GlideBrakeGravityScale = 4.f;

	/** Fastest fall while gliding. */
	UPROPERTY(EditAnywhere, Category = "Flight", meta = (ClampMin = "0"))
	float GlideMaxFallSpeed = 250.f;
};
