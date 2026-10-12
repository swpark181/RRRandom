#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "RRRandomGunComponent.generated.h"

/**
 * The rifle (/Game/Props/Rifle/SM_Rifle, origin in its pistol grip, muzzle +X) held in both of the brawler's hands,
 * on every machine. Every frame, after the animation has posed the body, it puts the grip in the right palm and
 * points the muzzle at the left hand (which holds the front of the gun), keeping the gun's top toward the sky.
 * So it follows whatever the animation does with the hands (slung across the body while idle, at the side walking).
 * Standing and shooting, the body turns into the fire animation's bladed rifle stance (ARRRandomCharacter::FireStanceYaw)
 * so the gun in the hands points down the line of fire. Shooting on the move (ARRRandomCharacter::IsAiming without
 * the stance) it swings onto the line shots fly instead: pointing the way the
 * brawler faces, with the muzzle's tip where the shots appear, so the bullets come out of the barrel.
 * Hidden when the body lacks the hand bones (the mannequin) and while knocked out. Purely cosmetic.
 */
UCLASS(ClassGroup = (RRRandom))
class RRRANDOM_API URRRandomGunComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	URRRandomGunComponent();

	/** Length of the gun in the world at normal size (the mesh is 100 long). */
	UPROPERTY(EditAnywhere, Category = "Gun")
	float GunLength = 120.f;

	/**
	 * Push from the right palm in the brawler's own frame (forward, right, up), at normal size. The fox's stubby arms
	 * hold the gun inside its round body, where the big head hides it from the high camera; this brings it out front.
	 */
	UPROPERTY(EditAnywhere, Category = "Gun")
	FVector HoldOffset = FVector(30.f, 0.f, 10.f);

	UPROPERTY(EditAnywhere, Category = "Gun")
	FName RightHandBone = TEXT("righthand");

	/** The palm is halfway from the right hand bone to this one (the index finger's base). */
	UPROPERTY(EditAnywhere, Category = "Gun")
	FName RightPalmBone = TEXT("righthandindex1");

	UPROPERTY(EditAnywhere, Category = "Gun")
	FName LeftHandBone = TEXT("lefthand");

	/** From the grip (the mesh's origin) to the middle of the muzzle's tip, on the 100-long mesh (forward, right, up). */
	UPROPERTY(EditAnywhere, Category = "Gun")
	FVector MuzzleTip = FVector(69.f, 0.f, 14.f);

	/** How fast the gun swings between the hands and the aim (1 = a full swing takes a second). */
	UPROPERTY(EditAnywhere, Category = "Gun")
	float AimBlendSpeed = 10.f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** 0 in the hands .. 1 aimed down the line shots fly. */
	float AimBlend = 0.f;
};
