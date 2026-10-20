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
	/** Copied from the instance each frame: the flying pose's and the wings' bones and each one's turn in mesh space. */
	TArray<FName> FlightBones;
	TArray<FQuat> FlightBends;
	TArray<FName> WingBones;
	TArray<FQuat> WingBends;
	/** Turns each named bone in mesh space about its own root, in order (parents first). */
	static void TurnBones(FPoseContext& Output, const TArray<FName>& Names, const TArray<FQuat>& Bends);
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

	/**
	 * Wing bones (the dragon's own wings): left root, left tip, right root, right tip. A mesh without them skips this.
	 * In the air they flap; gliding (and dashing) they spread out flat and hold still like an eagle's; on the ground they
	 * stir a little, more on the move. Built for the dragon rig: the mesh's +X is the left, +Y the front.
	 */
	UPROPERTY(EditAnywhere, Category = "Wings")
	TArray<FName> WingBoneNames;

	/** Flaps per second and degrees up and down at the root (the tip follows, later and by half): in the air, rising on
	 * the flight power, and on the ground standing and at WagRunSpeed. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D AirFlap = FVector2D(3.f, 36.f);
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D RisingFlap = FVector2D(4.2f, 42.f);
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D GroundFlap = FVector2D(0.7f, 4.f);
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D WalkFlap = FVector2D(1.4f, 9.f);

	/** The glide's spread from the wings' rest pose: the root turned down and forward, the tip further out in line with it. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D GlideRootDownForward = FVector2D(16.f, 20.f);
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D GlideTipDownForward = FVector2D(6.f, 15.f);

	/** Seconds to spread into the glide and to fold out of it. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	float GlideBlendTime = 0.2f;

	/** Gliding, now and then one big beat of the spread wings: every so many seconds (min, max), lasting so long, so many
	 * degrees down and back up at the root (the tip lags), the legs tucking in a little with it. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D GlideStrokeEvery = FVector2D(1.6f, 3.f);
	UPROPERTY(EditAnywhere, Category = "Wings")
	float GlideStrokeTime = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Wings")
	float GlideStrokeSize = 42.f;

	/**
	 * The flying pose over the idle while ARRRandomCharacter::IsInFlight (any brawler: the Mixamo leg and neck bones):
	 * thighs swung back, knees bent, toes pointed, the head lifted against the body's lean, and a slow kick like
	 * treading air. Degrees, about the mesh's side axis (negative swings back), hovering or rising (X) and gliding down (Y).
	 */
	UPROPERTY(EditAnywhere, Category = "Flight")
	FVector2D FlightThigh = FVector2D(-18.f, -38.f);
	UPROPERTY(EditAnywhere, Category = "Flight")
	FVector2D FlightKnee = FVector2D(-42.f, -16.f);
	UPROPERTY(EditAnywhere, Category = "Flight")
	FVector2D FlightToes = FVector2D(-28.f, -45.f);
	/** Share of the body's lean the neck and head lift back up, so it looks where it flies. */
	UPROPERTY(EditAnywhere, Category = "Flight")
	float FlightHeadLift = 0.6f;
	/** The kick: per second, and degrees each way at the thigh (less gliding). */
	UPROPERTY(EditAnywhere, Category = "Flight")
	FVector2D FlightKick = FVector2D(1.6f, 8.f);
	/** Seconds to go into the flying pose and back out of it. */
	UPROPERTY(EditAnywhere, Category = "Flight")
	float FlightBlendTime = 0.25f;

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
	const TArray<FName>& GetFlightBones() const { return FlightBoneNames; }
	const TArray<FQuat>& GetFlightBends() const { return FlightBends; }
	const TArray<FName>& GetWingBones() const { return WingBoneNames; }
	const TArray<FQuat>& GetWingBends() const { return WingBends; }
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
	/** The wing bones' turns for this frame (WingBends), from how the brawler is moving. */
	void UpdateWings(float DeltaSeconds);
	/** The flying pose's turns for this frame (FlightBends). */
	void UpdateFlightPose(float DeltaSeconds);

	/** The legs (thigh, knee, foot; left then right) and the neck and head, in that order. */
	TArray<FName> FlightBoneNames;
	TArray<FQuat> FlightBends;
	float FlightWeight = 0.f;
	float FlightGlide = 0.f;
	float KickPhase = 0.f;

	TArray<FQuat> WingBends;
	float FlapPhase = 0.f;
	/** Eased: the flap's rate and size, and 0 .. 1 into the glide. */
	float FlapRate = 0.f;
	float FlapSize = 0.f;
	float GlideWeight = 0.f;
	float AirTime = 0.f;
	/** The glide's beat: seconds to the next one, and how far through the one going on (0 .. 1; above 1 none). */
	float StrokeWait = 1.f;
	float StrokeAlpha = 2.f;
};
