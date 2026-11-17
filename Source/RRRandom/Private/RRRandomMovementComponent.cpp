#include "RRRandomMovementComponent.h"

#include "RRRandomCharacter.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"

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

// ---------------------------------------------------------------------------------------------------------------------
// Round 100: smoother bodies over a laggy connection

static TAutoConsoleVariable<int32> CVarNetSmooth(TEXT("rr.Net.Smooth"), 1,
	TEXT("0: other machines' animal friends are smoothed as before round 100 (fixed times, the body's own offset wiping the glide out)."));
static TAutoConsoleVariable<int32> CVarNetLogSmooth(TEXT("rr.Net.LogSmooth"), 0,
	TEXT("1: every second, log how far other machines' animal friends jumped on this screen (their capsules and their bodies)."));

bool URRRandomMovementComponent::IsNetSmoothOn()
{
	return CVarNetSmooth.GetValueOnGameThread() != 0;
}

bool URRRandomMovementComponent::IsNetSmoothLogged()
{
	return CVarNetLogSmooth.GetValueOnGameThread() != 0;
}

FVector URRRandomMovementComponent::GetNetSmoothOffset() const
{
	// Only where the engine smooths: another machine's body on a client, a joiner's body on the host
	if (!CharacterOwner || !UpdatedComponent || bNetworkSmoothingComplete || !HasPredictionData_Client() || NetworkSmoothingMode != ENetworkSmoothingMode::Exponential
		|| (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy && CharacterOwner->GetRemoteRole() != ROLE_AutonomousProxy))
	{
		return FVector::ZeroVector;
	}
	const FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	return ClientData ? UpdatedComponent->GetComponentToWorld().InverseTransformVectorNoScale(ClientData->MeshTranslationOffset) : FVector::ZeroVector;
}

void URRRandomMovementComponent::SmoothClientPosition(float DeltaSeconds)
{
	Super::SmoothClientPosition(DeltaSeconds);
	// The engine has just put the body at its plain offset plus the glide: the seat and the crouch go back on top
	if (IsNetSmoothOn())
	{
		if (ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(CharacterOwner))
		{
			Brawler->PlaceBodyMesh();
		}
	}
}

void URRRandomMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	UpdateNetSmoothing(DeltaTime);
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void URRRandomMovementComponent::UpdateNetSmoothing(float DeltaTime)
{
	NetSmoothingTimer -= DeltaTime;
	if (NetSmoothingTimer > 0.f || !CharacterOwner)
	{
		return;
	}
	NetSmoothingTimer = 0.5f;
	const UWorld* World = GetWorld();
	const bool bSmoothed = CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy || CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy;
	if (!World || !bSmoothed)
	{
		return;
	}
	// The connection the updates come over: a joiner's own on the host, the host's on a joiner
	const UNetConnection* Connection = CharacterOwner->GetNetConnection();
	if (!Connection)
	{
		const UNetDriver* Driver = World->GetNetDriver();
		Connection = Driver ? Driver->ServerConnection.Get() : nullptr;
	}
	float Trouble = 0.f;
	if (Connection && IsNetSmoothOn())
	{
		// Lag over a good connection's 60 ms, and jitter (which is what makes updates disagree) counted double
		Trouble = FMath::Clamp(FMath::Max(0.f, Connection->AvgLag - 0.06f) + Connection->GetAverageJitterInMS() * 0.002f, 0.f, 0.5f);
	}
	// (eased, so the times don't jump about with every measurement)
	NetTrouble = FMath::Lerp(NetTrouble, Trouble, 0.35f);
	// The engine's own values at no trouble; up to about three times as long and as far at half a second of it
	NetworkSimulatedSmoothLocationTime = FMath::Clamp(0.1f + 0.5f * NetTrouble, 0.1f, 0.28f);
	NetworkSimulatedSmoothRotationTime = FMath::Clamp(0.05f + 0.25f * NetTrouble, 0.05f, 0.14f);
	ListenServerNetworkSimulatedSmoothLocationTime = FMath::Clamp(0.04f + 0.5f * NetTrouble, 0.04f, 0.22f);
	ListenServerNetworkSimulatedSmoothRotationTime = FMath::Clamp(0.033f + 0.25f * NetTrouble, 0.033f, 0.12f);
	NetworkMaxSmoothUpdateDistance = FMath::Clamp(256.f + 1500.f * NetTrouble, 256.f, 700.f);
	NetworkNoSmoothUpdateDistance = NetworkMaxSmoothUpdateDistance * 1.5f;
}
