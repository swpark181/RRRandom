#include "RRRandomGunComponent.h"
#include "RRRandomCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

URRRandomGunComponent::URRRandomGunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the animation has posed the hands this frame
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rifle(TEXT("/Game/Props/Rifle/SM_Rifle.SM_Rifle"));
	if (Rifle.Succeeded())
	{
		SetStaticMesh(Rifle.Object);
	}
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	// Placed in the world every frame from the hands
	SetUsingAbsoluteLocation(true);
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
}

void URRRandomGunComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwner());
	const USkeletalMeshComponent* Body = Brawler ? Brawler->GetMesh() : nullptr;
	const bool bHands = Body && Body->GetBoneIndex(RightHandBone) != INDEX_NONE && Body->GetBoneIndex(RightPalmBone) != INDEX_NONE
		&& Body->GetBoneIndex(LeftHandBone) != INDEX_NONE;
	const bool bShow = bHands && Brawler->IsAlive();
	if (IsVisible() != bShow)
	{
		SetVisibility(bShow);
	}
	if (!bShow)
	{
		return;
	}

	const FVector RightHand = Body->GetBoneLocation(RightHandBone);
	const FVector Palm = (RightHand + Body->GetBoneLocation(RightPalmBone)) * 0.5f;
	const FVector Forward = (Body->GetBoneLocation(LeftHandBone) - RightHand).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		return;
	}
	// A giant's gun grows with it, and so does the push out of the body
	const float Size = Brawler->GetActorScale3D().Z;
	const FTransform Actor = Brawler->GetActorTransform();

	// In the hands: muzzle toward the left hand, top toward the sky as far as that allows
	// (a gun pointed straight up keeps the last roll). Pushed out of the round body, except in the rifle stance,
	// where the hands hold it out front on their own and the gun should sit right in them.
	const float Stance = Brawler->GetFireStanceAlpha();
	const FQuat HandRotation = FMath::Abs(Forward.Z) < 0.98f ? FRotationMatrix::MakeFromXZ(Forward, FVector::UpVector).ToQuat() : GetComponentQuat();
	const FVector HandLocation = Palm + Actor.TransformVectorNoScale(HoldOffset) * Size * (1.f - Stance);

	// Shooting on the move (no rifle stance, the walk keeps the hands low): aimed along the way shots fly
	// (the brawler faces its target to shoot) with the muzzle's tip on the spot shots appear
	// (ARRRandomCharacter::ShootAt: capsule center + MuzzleOffset turned with the brawler, not scaled)
	const FQuat AimRotation = Actor.GetRotation();
	const FVector AimLocation = Actor.GetLocation() + AimRotation.RotateVector(Brawler->GetMuzzleOffset())
		- AimRotation.RotateVector(MuzzleTip * GunLength / 100.f * Size);

	AimBlend = FMath::FInterpConstantTo(AimBlend, Brawler->IsAiming() && Stance < 0.5f ? 1.f : 0.f, DeltaTime, AimBlendSpeed);
	const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, AimBlend, 2.f);
	SetWorldLocationAndRotation(FMath::Lerp(HandLocation, AimLocation, Alpha), FQuat::Slerp(HandRotation, AimRotation, Alpha));
	SetWorldScale3D(FVector(GunLength / 100.f * Size));
}
