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

	/**
	 * Shift held (and not shooting or in the sights): set by the owning player every frame and sent with each move
	 * (FLAG_Custom_0), so the server and the client run at the same speed and the sprint is predicted.
	 */
	bool bWantsToSprint = false;

	/** Sprinting on foot: this many times the walking speed (the character says when it may: not on a vehicle etc.). */
	UPROPERTY(EditAnywhere, Category = "Sprint", meta = (ClampMin = "1"))
	float SprintSpeedScale = 1.5f;

	/** Sprinting right now: wanted, allowed, and on the ground or in a jump. */
	bool IsSprinting() const;

	virtual float GetMaxSpeed() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};

/** A saved move that remembers the sprint, so replayed moves and the server see it. */
class FRRSavedMove : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	bool bSavedWantsToSprint = false;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
};

class FRRNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FRRNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) {}

	virtual FSavedMovePtr AllocateNewMove() override { return FSavedMovePtr(new FRRSavedMove()); }
};
