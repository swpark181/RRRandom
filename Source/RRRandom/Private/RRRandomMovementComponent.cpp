#include "RRRandomMovementComponent.h"

#include "RRRandomCharacter.h"

bool URRRandomMovementComponent::IsSprinting() const
{
	if (!bWantsToSprint || !(IsMovingOnGround() || IsFalling()))
	{
		return false;
	}
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(CharacterOwner);
	return Brawler && Brawler->CanSprint();
}

float URRRandomMovementComponent::GetMaxSpeed() const
{
	const float Speed = Super::GetMaxSpeed();
	return IsSprinting() ? Speed * SprintSpeedScale : Speed;
}

void URRRandomMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* URRRandomMovementComponent::GetPredictionData_Client() const
{
	if (!ClientPredictionData)
	{
		URRRandomMovementComponent* MutableThis = const_cast<URRRandomMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FRRNetworkPredictionData_Client(*this);
	}
	return ClientPredictionData;
}

void FRRSavedMove::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
}

uint8 FRRSavedMove::GetCompressedFlags() const
{
	uint8 Flags = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Flags |= FLAG_Custom_0;
	}
	return Flags;
}

bool FRRSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	if (bSavedWantsToSprint != static_cast<const FRRSavedMove*>(NewMove.Get())->bSavedWantsToSprint)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FRRSavedMove::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const URRRandomMovementComponent* Movement = Cast<URRRandomMovementComponent>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Movement->bWantsToSprint;
	}
}

void FRRSavedMove::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (URRRandomMovementComponent* Movement = Cast<URRRandomMovementComponent>(C->GetCharacterMovement()))
	{
		Movement->bWantsToSprint = bSavedWantsToSprint;
	}
}

float URRRandomMovementComponent::GetGravityZ() const
{
	const float Gravity = Super::GetGravityZ();
	if (!bGliding)
	{
		return Gravity;
	}
	return Gravity * (Velocity.Z > 0.f ? GlideBrakeGravityScale : GlideGravityScale);
}

bool URRRandomMovementComponent::DoJump(bool bReplayingMoves, float DeltaTime)
{
	const bool bJumped = Super::DoJump(bReplayingMoves, DeltaTime);
	// A held jump re-applies its speed every frame; at the ceiling it must not, or the move (which averages
	// the old and new speed) keeps carrying the glider up
	if (bGliding && UpdatedComponent && UpdatedComponent->GetComponentLocation().Z >= FlightCeilingZ)
	{
		Velocity.Z = FMath::Min<FVector::FReal>(Velocity.Z, 0.f);
	}
	return bJumped;
}

FVector URRRandomMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	FVector Result = Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);
	if (bGliding)
	{
		Result.Z = FMath::Max<FVector::FReal>(Result.Z, -GlideMaxFallSpeed);
		// At the ceiling, no more rising
		if (UpdatedComponent && UpdatedComponent->GetComponentLocation().Z >= FlightCeilingZ)
		{
			Result.Z = FMath::Min<FVector::FReal>(Result.Z, 0.f);
		}
	}
	return Result;
}
