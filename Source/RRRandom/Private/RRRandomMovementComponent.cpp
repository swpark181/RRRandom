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

FVector URRRandomMovementComponent::NewFallVelocity(const FVector& InitialVelocity, const FVector& Gravity, float DeltaTime) const
{
	FVector Result = Super::NewFallVelocity(InitialVelocity, Gravity, DeltaTime);
	if (bGliding)
	{
		Result.Z = FMath::Max<FVector::FReal>(Result.Z, -GlideMaxFallSpeed);
	}
	return Result;
}
