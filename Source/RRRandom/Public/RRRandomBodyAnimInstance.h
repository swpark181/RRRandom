#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "RRRandomBodyAnimInstance.generated.h"

/** Plays the brawler's animations like the plain single-node instance, then bends the tail bones on top. */
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
	/** Copied from the instance each frame: per tail bone, root first, the side swing and the lift in degrees. */
	TArray<FName> TailBones;
	TArray<float> TailSwing;
	TArray<float> TailLift;
};

/**
 * The brawler's body animation: the single animation the character picks (idle, walk, fire), with a tail that lags
 * when the body turns or speeds up, springs back with a little overshoot, wags lazily when standing and livelier when
 * walking, and lifts a bit on the move. Purely cosmetic; it runs on every machine from what each one sees.
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

	const TArray<FName>& GetTailBones() const { return TailBoneNames; }
	const TArray<float>& GetTailSwing() const { return BoneSwing; }
	const TArray<float>& GetTailLift() const { return BoneLift; }

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
};
