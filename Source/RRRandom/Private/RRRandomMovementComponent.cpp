#include "RRRandomMovementComponent.h"

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
