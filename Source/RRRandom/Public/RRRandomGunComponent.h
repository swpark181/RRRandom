#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "RRRandomGunComponent.generated.h"

/**
 * The rifle (/Game/Props/Rifle/SM_Rifle, origin in its pistol grip, muzzle +X) held in both of the brawler's hands,
 * on every machine. Every frame, after the animation has posed the body, it puts the grip in the right palm and
 * points the muzzle at the left hand (which holds the front of the gun), keeping the gun's top toward the sky.
 * So it follows whatever the animation does with the hands (slung across the body while idle, at the side walking).
 * Shooting, the body takes its aiming pose (URRRandomBodyAnimInstance: standing or strafing, the spine turned so the
 * hands point where the brawler faces), so the gun in the hands points down the line of fire; the gun remembers where
 * its muzzle sits in that pose (GetAimMuzzleOffset) and ARRRandomCharacter::ShootAt fires from there, so the bullets
 * come out of the barrel. Each gun has its own model, length and hold (GunLooks in the .cpp). Without the aiming
 * animations it falls back to the old way: the bladed fire stance standing, and on the move swinging the gun onto the
 * line shots fly with its tip on MuzzleOffset.
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
	float GunLength = 95.f;

	/**
	 * Push from the right palm in the brawler's own frame (forward, right, up), at normal size. The fox's stubby arms
	 * hold the gun inside its round body, where the big head hides it from the high camera; this brings it out front.
	 * Only for the old rifle (a gun whose own model is missing): the round-90 toy guns sit in the fist (GunLooks' grips).
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

	/**
	 * The foam finger (FRRWeapon::bFoamFinger) in place of the rifle: /Game/Props/FoamFinger/SM_FoamFinger, cuff at the
	 * origin and the pointing finger along +X, 100 long (fingertip at FoamFingerTip). Held on the right hand
	 * (FoamFingerHoldOffset, high enough to show over the big head) pointing ahead and a little up, FoamFingerLift
	 * degrees. Each poke is a short jab: over FoamFingerPokeTime it thrusts FoamFingerPokeDistance ahead, levelling down
	 * to FoamFingerPokeLift, and comes back.
	 */
	UPROPERTY(EditAnywhere, Category = "Gun")
	float FoamFingerLength = 110.f;

	/** From the right palm (forward, right, up), at normal size. */
	UPROPERTY(EditAnywhere, Category = "Gun")
	FVector FoamFingerHoldOffset = FVector(15.f, 25.f, 70.f);

	UPROPERTY(EditAnywhere, Category = "Gun")
	FVector FoamFingerTip = FVector(98.f, 8.f, -2.f);

	UPROPERTY(EditAnywhere, Category = "Gun")
	float FoamFingerLift = 35.f;

	UPROPERTY(EditAnywhere, Category = "Gun")
	float FoamFingerPokeTime = 0.22f;

	UPROPERTY(EditAnywhere, Category = "Gun")
	float FoamFingerPokeDistance = 45.f;

	UPROPERTY(EditAnywhere, Category = "Gun")
	float FoamFingerPokeLift = 10.f;

	/**
	 * Where this gun's muzzle tip sits in the aiming pose, from the capsule center (forward, right, up; turned with the
	 * brawler, grown with a giant): learned each frame the pose is settled. Before it has been seen, or for the foam
	 * finger, the brawler's MuzzleOffset.
	 */
	FVector GetAimMuzzleOffset() const;

	/** From this far into the aiming pose (0 .. 1) the gun counts as aimed, for GetAimMuzzleOffset. */
	UPROPERTY(EditAnywhere, Category = "Gun")
	float SettledAimPose = 0.95f;

	/** How fast the learned muzzle follows the pose's sway (exponential, per second). */
	UPROPERTY(EditAnywhere, Category = "Gun")
	float MuzzleFollowSpeed = 20.f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** 0 in the hands .. 1 aimed down the line shots fly (only without the aiming animations). */
	float AimBlend = 0.f;

	/** When rr.Gun.Debug last logged the hand. */
	double DebugLogTime = 0.0;

	/** GetAimMuzzleOffset at normal size, for the gun model MuzzleLook (GunLooks index; INDEX_NONE the rifle). */
	FVector AimMuzzle = FVector::ZeroVector;
	bool bMuzzleKnown = false;
	int32 MuzzleLook = INDEX_NONE - 1;

	UPROPERTY()
	TObjectPtr<UStaticMesh> RifleMesh;

	/**
	 * Each gun's own model (FRRWeapon::GetHitEffect: pistol, SMG, minigun, rifle, blaster, cannon), Tripo cartoon guns made
	 * like the rifle (char-pipeline/scripts/blender_guns.py): grip at the origin, muzzle +X, 100 long. A missing one
	 * falls back to the rifle.
	 */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> GunMeshes;

	UPROPERTY()
	TObjectPtr<UStaticMesh> FoamFingerMesh;
};
