#include "RRRandomGunComponent.h"
#include "RRRandomCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/**
	 * A gun's model: the asset, its length in the hands at normal size, its muzzle tip on the 100-long mesh, and where on
	 * the gun the hands hold it, from the grip (the mesh's origin), also on the 100-long mesh. The aiming pose holds a gun
	 * at the cheek and close to the body; the chunky toy guns are held by their body, toward the back, so they sit lower
	 * and further out: clear of the fox's big head, and with the muzzle showing past it from the high camera.
	 */
	struct FRRGunLook
	{
		const TCHAR* Path;
		float Length;
		FVector Tip;
		FVector Hold;
	};
	// By ERRHitEffect, pistol to cannon. The rifle keeps the component's own GunLength and MuzzleTip.
	const FRRGunLook GunLooks[] =
	{
		{ TEXT("/Game/Props/Guns/SM_Pistol.SM_Pistol"), 65.f, FVector(72.f, 0.f, 33.f), FVector(-30.f, 0.f, 30.f) },
		{ TEXT("/Game/Props/Guns/SM_Smg.SM_Smg"), 75.f, FVector(63.f, 0.f, 7.f), FVector(-25.f, 0.f, 5.f) },
		{ TEXT("/Game/Props/Guns/SM_Minigun.SM_Minigun"), 90.f, FVector(52.f, 0.f, 27.f), FVector(-22.f, 0.f, 27.f) },
		{ nullptr, 0.f, FVector::ZeroVector, FVector::ZeroVector },
		{ TEXT("/Game/Props/Guns/SM_Blaster.SM_Blaster"), 75.f, FVector(33.f, 0.f, 30.f), FVector(-50.f, 0.f, 28.f) },
		{ TEXT("/Game/Props/Guns/SM_Cannon.SM_Cannon"), 90.f, FVector(46.f, 0.f, 29.f), FVector(-22.f, 0.f, 29.f) },
	};
}

URRRandomGunComponent::URRRandomGunComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the animation has posed the hands this frame
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
	SetIsReplicatedByDefault(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Rifle(TEXT("/Game/Props/Rifle/SM_Rifle.SM_Rifle"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> FoamFinger(TEXT("/Game/Props/FoamFinger/SM_FoamFinger.SM_FoamFinger"));
	RifleMesh = Rifle.Succeeded() ? Rifle.Object : nullptr;
	FoamFingerMesh = FoamFinger.Succeeded() ? FoamFinger.Object : nullptr;
	for (const FRRGunLook& Look : GunLooks)
	{
		UStaticMesh* Mesh = nullptr;
		if (Look.Path)
		{
			ConstructorHelpers::FObjectFinderOptional<UStaticMesh> Finder(Look.Path);
			Mesh = Finder.Get();
		}
		GunMeshes.Add(Mesh);
	}
	SetStaticMesh(RifleMesh);
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
	// Put away while knocked out and while dancing after a win
	const bool bShow = bHands && Brawler->IsAlive() && !Brawler->IsCelebrating();
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
	// The gun's own model (the rifle when its model is missing), or the foam finger
	const bool bFinger = Brawler->GetWeapon().bFoamFinger && FoamFingerMesh;
	const int32 Kind = static_cast<int32>(Brawler->GetWeapon().GetHitEffect());
	const bool bOwnLook = !bFinger && GunMeshes.IsValidIndex(Kind) && GunMeshes[Kind];
	UStaticMesh* WantedMesh = bFinger ? FoamFingerMesh.Get() : (bOwnLook ? GunMeshes[Kind].Get() : RifleMesh.Get());
	if (GetStaticMesh() != WantedMesh)
	{
		SetStaticMesh(WantedMesh);
		// The random events gave this component an instance of the rifle's material; the new mesh brings its own
		EmptyOverrideMaterials();
	}
	const float Length = bFinger ? FoamFingerLength : (bOwnLook ? GunLooks[Kind].Length : GunLength);
	const FVector Tip = bFinger ? FoamFingerTip : (bOwnLook ? GunLooks[Kind].Tip : MuzzleTip);

	// A giant's gun grows with it, and so does the push out of the body
	const float Size = Brawler->GetActorScale3D().Z;
	const FTransform Actor = Brawler->GetActorTransform();

	// The foam finger is worn on the right hand, pointing ahead and a little up, with a quick jab on every poke
	if (bFinger)
	{
		const float SincePoke = Brawler->GetTimeSinceShot();
		const float Jab = SincePoke >= 0.f && SincePoke < FoamFingerPokeTime ? FMath::Sin(PI * SincePoke / FMath::Max(FoamFingerPokeTime, KINDA_SMALL_NUMBER)) : 0.f;
		const FVector Pointing = Actor.GetRotation().RotateVector(FRotator(FMath::Lerp(FoamFingerLift, FoamFingerPokeLift, Jab), 0.f, 0.f).Vector());
		const FVector Location = Palm + (Actor.TransformVectorNoScale(FoamFingerHoldOffset) + Pointing * FoamFingerPokeDistance * Jab) * Size;
		SetWorldLocationAndRotation(Location, FRotationMatrix::MakeFromXZ(Pointing, FVector::UpVector).ToQuat());
		SetWorldScale3D(FVector(Length / 100.f * Size));
		AimBlend = 0.f;
		return;
	}

	// In the hands: muzzle toward the left hand, top toward the sky as far as that allows
	// (a gun pointed straight up keeps the last roll). Pushed out of the round body, except in the rifle stance,
	// where the hands hold it out front on their own and the gun should sit right in them.
	// The aiming pose (URRRandomBodyAnimInstance) points the hands where the brawler faces, so there the gun sits right in them
	const bool bAimSet = Brawler->GetAimAnimations().IsComplete();
	const float AimPose = Brawler->GetAimPoseWeight();
	const float Stance = FMath::Max(Brawler->GetFireStanceAlpha(), AimPose);
	const FVector Holding = Forward;
	const FQuat HandRotation = FMath::Abs(Holding.Z) < 0.98f ? FRotationMatrix::MakeFromXZ(Holding, FVector::UpVector).ToQuat() : GetComponentQuat();
	const FVector Hold = bOwnLook ? GunLooks[Kind].Hold : FVector::ZeroVector;
	const FVector HandLocation = Palm + Actor.TransformVectorNoScale(HoldOffset) * Size * (1.f - Stance)
		- HandRotation.RotateVector(Hold) * Length / 100.f * Size;

	// Without the aiming set, shooting on the move (no rifle stance, the walk keeps the hands low): aimed along the way
	// shots fly (the brawler faces its target to shoot) with the muzzle's tip on the spot shots appear
	// (ARRRandomCharacter::ShootAt: capsule center + MuzzleOffset turned with the brawler, not scaled)
	const FQuat AimRotation = Actor.GetRotation();
	const FVector AimLocation = Actor.GetLocation() + AimRotation.RotateVector(Brawler->GetMuzzleOffset())
		- AimRotation.RotateVector(Tip * Length / 100.f * Size);

	AimBlend = FMath::FInterpConstantTo(AimBlend, !bAimSet && Brawler->IsAiming() && Stance < 0.5f ? 1.f : 0.f, DeltaTime, AimBlendSpeed);
	const float Alpha = FMath::InterpEaseInOut(0.f, 1.f, AimBlend, 2.f);
	SetWorldLocationAndRotation(FMath::Lerp(HandLocation, AimLocation, Alpha), FQuat::Slerp(HandRotation, AimRotation, Alpha));
	SetWorldScale3D(FVector(Length / 100.f * Size));

	// Settled in the aiming pose: remember where this gun's muzzle is, from the brawler, for the shots
	// (ARRRandomCharacter::ShootAt), so they leave the barrel even when the first one comes before the pose does
	const int32 Look = bOwnLook ? Kind : INDEX_NONE;
	if (Look != MuzzleLook)
	{
		MuzzleLook = Look;
		bMuzzleKnown = false;
	}
	if (bAimSet && AimPose >= SettledAimPose)
	{
		const FVector Muzzle = GetComponentTransform().TransformPosition(Tip);
		const FVector Offset = Actor.GetRotation().UnrotateVector(Muzzle - Actor.GetLocation()) / Size;
		AimMuzzle = bMuzzleKnown ? FMath::Lerp(AimMuzzle, Offset, FMath::Min(1.f, DeltaTime * MuzzleFollowSpeed)) : Offset;
		bMuzzleKnown = true;
	}
}

FVector URRRandomGunComponent::GetAimMuzzleOffset() const
{
	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwner());
	if (!Brawler || !IsVisible() || !bMuzzleKnown || Brawler->GetWeapon().bFoamFinger)
	{
		return Brawler ? Brawler->GetMuzzleOffset() : FVector::ZeroVector;
	}
	return AimMuzzle * Brawler->GetActorScale3D().Z;
}
