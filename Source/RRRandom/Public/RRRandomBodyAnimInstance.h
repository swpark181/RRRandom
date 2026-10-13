#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "RRRandomAimAnimations.h"
#include "RRRandomBodyAnimInstance.generated.h"

/** What the aiming layer needs for one frame, worked out on the game thread. */
struct FRRAimFrame
{
	FRRAimAnimations Animations;
	/** 0 .. 1: the aiming set over the brawler's own animation. */
	float Weight = 0.f;
	/** 0 standing .. 1 running. Standing mixes aim (0) and fire (1) by FireWeight; running mixes the four runs by MoveWeights. */
	float MoveWeight = 0.f;
	float FireWeight = 0.f;
	float MoveWeights[4] = { 1.f, 0.f, 0.f, 0.f };
	/** Times into the standing aim, the fire animation and (as a share of their length, all four in step) the runs. */
	float IdleTime = 0.f;
	float FireTime = 0.f;
	float RunPhase = 0.f;
	/** Degrees the chest kicks back from the last shot. */
	float Kick = 0.f;
	/** Where the gun should point, in the mesh's own space: where the brawler faces. */
	FVector Facing = FVector(0.f, 1.f, 0.f);
};

/** Plays the brawler's animations like the plain single-node instance, lays the aiming set over them, then bends the tail bones on top. */
USTRUCT()
struct FRRRandomBodyAnimInstanceProxy : public FAnimSingleNodeInstanceProxy
{
	GENERATED_BODY()

	FRRRandomBodyAnimInstanceProxy() = default;
	explicit FRRRandomBodyAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimSingleNodeInstanceProxy(InAnimInstance)
	{
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void EvaluateAim(FPoseContext& Output) const;
	/** Turns the spine so the two hands (the gun between them) point along Aim.Facing, level. */
	void PointGun(FPoseContext& Output) const;

	/** Copied from the instance each frame: per tail bone, root first, the side swing and the lift in degrees. */
	TArray<FName> TailBones;
	TArray<float> TailSwing;
	TArray<float> TailLift;
	FRRAimFrame Aim;
};

/**
 * The brawler's body animation: the single animation the character picks (idle, walk, fire), with a tail that lags
 * when the body turns or speeds up, springs back with a little overshoot, wags lazily when standing and livelier when
 * walking, and lifts a bit on the move. Purely cosmetic; it runs on every machine from what each one sees.
 *
 * While the brawler shoots (ARRRandomCharacter::WantsAimPose) the aiming set (FRRAimAnimations) takes over: standing,
 * the rifle aim and the fire animation, restarted with every shot; moving, the four aimed runs mixed by which way the
 * brawler goes relative to where it faces (it strafes while shooting), in step and at a rate matching its speed, with a
 * kick of the chest at each shot. Then the spine turns, and tilts, so the line from the right hand to the left (where
 * URRRandomGunComponent puts the gun) points level along where the brawler faces: the Mixamo rifle poses hold the gun
 * well left of the body's front, and standing the character also turns the whole body into that stance.
 *
 * Tail bones are TailBoneNames (added to the fox by char-pipeline/scripts/blender_fox_tail.py); a mesh without them
 * just plays its animations. Bends are about the mesh's up axis (swing) and side axis (lift), in mesh space, so they
 * don't depend on how the tail bones themselves are oriented.
 */
UCLASS(Transient, NotBlueprintable)
class RRRANDOM_API URRRandomBodyAnimInstance : public UAnimSingleNodeInstance
{
	GENERATED_BODY()

public:
	URRRandomBodyAnimInstance();

	/** Tail bones, root to tip. */
	UPROPERTY(EditAnywhere, Category = "Tail")
	TArray<FName> TailBoneNames;

	/** Of a turn, how much the tail stays behind (it keeps pointing where it did, then catches up). */
	UPROPERTY(EditAnywhere, Category = "Tail", meta = (ClampMin = "0", ClampMax = "1"))
	float TurnDrag = 0.7f;

	/** Swing in degrees per cm/s² of sideways speeding up (the tail trails the other way). */
	UPROPERTY(EditAnywhere, Category = "Tail")
	float SideAccelSwing = 0.006f;

	/** How fast the tail springs back to straight (oscillations per second) and how much it overshoots (0 none, 1 a lot). */
	UPROPERTY(EditAnywhere, Category = "Tail", meta = (ClampMin = "0.1"))
	float SpringFrequency = 1.4f;
	UPROPERTY(EditAnywhere, Category = "Tail", meta = (ClampMin = "0", ClampMax = "1"))
	float SpringBounce = 0.65f;

	/** Most the tail swings to a side, in degrees, all bones together. */
	UPROPERTY(EditAnywhere, Category = "Tail")
	float MaxSwing = 55.f;

	/** Wag: side to side, in degrees at the tip and wags per second, standing and at WagRunSpeed. */
	UPROPERTY(EditAnywhere, Category = "Tail")
	FVector2D WagAmplitude = FVector2D(9.f, 14.f);
	UPROPERTY(EditAnywhere, Category = "Tail")
	FVector2D WagFrequency = FVector2D(0.6f, 1.6f);
	/** The wag runs down the tail, the tip this far behind the root (radians). */
	UPROPERTY(EditAnywhere, Category = "Tail")
	float WagDelay = 2.f;

	/** Degrees the tail lifts at WagRunSpeed, plus a small bob with each step. */
	UPROPERTY(EditAnywhere, Category = "Tail")
	float RunLift = 14.f;
	UPROPERTY(EditAnywhere, Category = "Tail")
	float RunBob = 4.f;
	UPROPERTY(EditAnywhere, Category = "Tail")
	float WagRunSpeed = 600.f;

	/** Seconds to go into the aiming pose and to come back out of it. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float AimInTime = 0.1f;
	UPROPERTY(EditAnywhere, Category = "Aim")
	float AimOutTime = 0.25f;

	/** Ground speed at which the aimed run has fully taken over from the standing aim. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float AimRunSpeed = 150.f;

	/** How fast the mix of run directions follows a change of direction (1 = a full swap takes a second). */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float DirectionBlendSpeed = 6.f;

	/** The run's play rate follows the ground speed within these bounds. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	FVector2D RunRateRange = FVector2D(0.6f, 1.8f);

	/** Standing, the fire animation plays this long after a shot, then the aim settles. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float FireHold = 0.3f;

	/** Each shot on the run kicks the chest back this many degrees, over KickTime. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float KickAngle = 6.f;
	UPROPERTY(EditAnywhere, Category = "Aim")
	float KickTime = 0.16f;

	const TArray<FName>& GetTailBones() const { return TailBoneNames; }
	const TArray<float>& GetTailSwing() const { return BoneSwing; }
	const TArray<float>& GetTailLift() const { return BoneLift; }
	const FRRAimFrame& GetAimFrame() const { return Aim; }
	float GetAimWeight() const { return Aim.Weight; }
	float GetAimMoveWeight() const { return Aim.MoveWeight; }

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	/** Degrees the tail is swung to the brawler's right (negative: left), and how fast it moves. */
	float Swing = 0.f;
	float SwingSpeed = 0.f;
	float Lift = 0.f;
	float WagPhase = 0.f;
	/** Fades the wag in and out (out once knocked out). */
	float Life = 1.f;
	bool bHasLast = false;
	float LastYaw = 0.f;
	FVector LastVelocity = FVector::ZeroVector;
	TArray<float> BoneSwing;
	TArray<float> BoneLift;
	FRRAimFrame Aim;
	/** The run directions' mix, eased toward where the brawler is going. */
	float TargetMoveWeights[4] = { 1.f, 0.f, 0.f, 0.f };

	void UpdateAim(float DeltaSeconds);
};
