#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "RRRandomAimAnimations.h"
#include "RRRandomGunComponent.h"
#include "RRRandomBodyAnimInstance.generated.h"

/** The left hand on the held gun this frame (2026-11-20): where it goes, and how much of the way (eased in and out). */
struct FRRGunHandsFrame
{
	FRRGunSupportHold Hold;
	float Weight = 0.f;
};

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
	/** Final head/neck direction, after all weapon and locomotion layers. */
	FVector GazeFacing = FVector(0.f, 1.f, 0.f);
	float GazeWeight = 0.f;
	FVector ListeningDirection = FVector::ZeroVector;
	float ListeningWeight = 0.f;
	/** Persistent world headings keep a logical aim turn from snapping the rendered body. */
	bool bSeparateBody = false;
	bool bCombatFacing = false;
	float LowerYaw = 0.f, UpperYaw = 0.f, GazeYaw = 0.f;
	float LowerTurn = 0.f, UpperTurn = 0.f;
	float KeepLocomotion = 0.f, StrafeWeight = 0.f;
};

/**
 * Round 97, the light sword's layer for one frame: the slash (from where its blow starts, quickened to the swing) and the
 * held guard (deflecting), each with its weight, laid over the upper body (Spine and everything under it).
 */
struct FRRMeleeFrame
{
	const UAnimSequence* Slash = nullptr;
	float SlashTime = 0.f;
	float SlashWeight = 0.f;
	const UAnimSequence* PreviousSlash = nullptr;
	float PreviousSlashTime = 0.f;
	float PreviousSlashWeight = 0.f;
	float SlashBlend = 1.f;
	float SwingStamp = -100.f;
	const UAnimSequence* Guard = nullptr;
	float GuardTime = 0.f;
	float GuardWeight = 0.f;
	/** Round 101: the arms' pose while carrying a hammer and a shield, and how much of it is on. */
	const UAnimSequence* Carry = nullptr;
	float CarryWeight = 0.f;
	FVector Facing = FVector(0.f, 1.f, 0.f);
	bool bGuarding = false;
	float LowerBodyWeight = 0.f;
	bool bDedicated = false;
	/** Reuse the phrase with each rig's proportions and existing calibrated hand axes. */
	float BodyScale = 1.f;
	FQuat RightHandFix = FRotator(14.3f, -77.5f, 137.7f).Quaternion();
	float GripWeight = 0.f;
	FVector GripOffset = FVector(0.f, 10.f, -7.f);
	FQuat GripRotation = FQuat::Identity;
	FVector EntryGripOffset = FVector::ZeroVector;
	FQuat EntryGripRotation = FQuat::Identity;
	float GripSwingStamp = -100.f;
	/** Ground-up sword drive; absolute mesh-space yaw relative to the bind pose. */
	float DriveWeight = 0.f;
	float HipYaw = 0.f;
	float ChestYaw = 0.f;
	float ChestLean = 0.f;
	float GripCarryYaw = 0.f;
	float StanceLoad = 0.f;
	float WeightShift = 0.f;
	float EntryHipYaw = 0.f, EntryChestYaw = 0.f, EntryCarryYaw = 0.f;
	float EntryChestLean = 0.f, EntryWeightShift = 0.f, EntryStanceLoad = 0.f;
	float AttackAge = 0.f;
	float AttackDuration = 0.35f;
	float ParryWeight = 0.f, ParrySway = 0.f;
	float ParryStamp = -100.f;
	FVector ParryEntryGrip = FVector::ZeroVector;
	FQuat ParryEntryRotation = FQuat::Identity;
	float ParryEntryHip = 0.f, ParryEntryChest = 0.f, ParryEntryCarry = 0.f;
	float ParryEntryLean = 0.f, ParryEntryLoad = 0.f, ParryEntryShift = 0.f, ParryEntryWeight = 0.f;
	float FootPhase = 1.f;
	bool bPlantEligible = false;
	bool bPlantAttack = false;
	float DeltaSeconds = 0.f;
	FTransform MeshToWorld = FTransform::Identity;
	FTransform SupportToWorld = FTransform::Identity;
	uint32 SupportId = 0;
	FVector FloorNormal = FVector::UpVector;
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
	virtual void PreEvaluateAnimation(UAnimInstance* InAnimInstance) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	void EvaluateAim(FPoseContext& Output) const;
	void EvaluateGaze(FPoseContext& Output) const;
	void EvaluateMovingLegs(FPoseContext& Output) const;
	void EvaluateBodyFacing(FPoseContext& Output);
	FQuat LastUpperWorld = FQuat::Identity;
	bool bHasUpperHistory = false, bWasCombatFacing = false, bEaseUpperEntry = false;
	float UpperEntryAge = 0.f;
	/** Round 97: the light sword's slash and guard over the upper body. */
	void EvaluateMelee(FPoseContext& Output) const;
	void EvaluateSwordGrip(FPoseContext& Output) const;
	void UpdateSwordContacts(const FPoseContext& Output);
	void StoreSwordContacts(const FPoseContext& Output);
	FTransform ContactFeet[2];
	FTransform ContactToes[2];
	FVector LandingToe = FVector::ZeroVector;
	FQuat LandingFootRotation = FQuat::Identity;
	FQuat LandingToeRotation = FQuat::Identity;
	FQuat ContactFacing = FQuat::Identity;
	FTransform LastFeetWorld[2], LastToesWorld[2];
	FTransform RecoveryFeet[2], RecoveryToes[2], RecoveryGoals[2];
	FQuat RecoveryFootRotations[2];
	float RecoveryStartAge = 0.f;
	bool bRecovering = false;
	bool bGuardContacts = false;
	FTransform LastMeshWorld = FTransform::Identity;
	float ContactStamp = -100.f;
	float ContactWeight = 0.f;
	int32 ContactLeadFoot = 1;
	uint32 ContactSupportId = 0;
	bool bContactHistory = false;
	bool bContactsValid = false;
	FRRMeleeFrame Melee;
	/** Turns the spine so the two hands (the gun between them) point along Aim.Facing, level. */
	void PointGun(FPoseContext& Output) const;
	/** Bends the left arm so its fist holds the gun at GunHands.Hold.Support (after every other layer has moved the arms). */
	void EvaluateGunHands(FPoseContext& Output) const;
	FRRGunHandsFrame GunHands;

	/** Copied from the instance each frame: per tail bone, root first, the side swing and the lift in degrees. */
	TArray<FName> TailBones;
	TArray<float> TailSwing;
	TArray<float> TailLift;
	/** Copied from the instance each frame: how big the tail is drawn (on its first bone), and the floor under it (mesh space)
	 * with how far above it each joint stays. */
	float TailSize = 1.f;
	bool bTailFloor = false;
	FVector TailFloorPoint = FVector::ZeroVector;
	FVector TailFloorNormal = FVector::UpVector;
	float TailFloorMargin = 0.f;
	/** Copied from the instance each frame: the ears' bones and turns (reference pose, mesh space), the head and its reference turn. */
	TArray<FName> EarBones;
	TArray<FQuat> EarBends;
	FName EarHead;
	FQuat EarHeadRest = FQuat::Identity;
	/** Copied from the instance each frame: the flying pose's and the wings' bones and each one's turn in mesh space. */
	TArray<FName> FlightBones;
	TArray<FQuat> FlightBends;
	TArray<FName> JumpBones;
	TArray<FQuat> JumpBends;
	TArray<FName> SprintBones;
	TArray<FQuat> SprintBends;
	TArray<FName> RideBones;
	TArray<FQuat> RideBends;
	TArray<FName> WingBones;
	TArray<FQuat> WingBends;
	/** Turns each named bone in mesh space about its own root, in order (parents first). */
	static void TurnBones(FPoseContext& Output, const TArray<FName>& Names, const TArray<FQuat>& Bends);
	FRRAimFrame Aim;
	/** Motion matching test (2026-11-20): the driver's pose per bone of this mesh, which bones take it, and how much. */
	void EvaluateMotion(FPoseContext& Output) const;
	void EvaluateClimbGrip(FPoseContext& Output) const;
	static void Reach(FCompactPose& Pose, FCompactPoseBoneIndex End, const FVector& Target, const FVector& Pole);
	FVector GripGoals[2] = {};
	float GripWeights[2] = {};
	void PlantStrafeFeet(FPoseContext& Output);
	struct FStrafeFoot
	{
		FVector Anchor = FVector::ZeroVector;
		FVector Offset = FVector::ZeroVector;
		bool bPlanted = false;
	};
	FStrafeFoot StrafeFeet[2];
	bool bRightStrafe = false;
	float PivotLoad = 0.f;
	TArray<FTransform> MotionPose;
	TArray<uint8> MotionMask;
	float MotionWeight = 0.f;
	float MotionWholeWeight = 0.f;
	TArray<FTransform> MotionWholePose;
	/** Degrees the spine leans forward this frame; the neck and head lift this share of it back. */
	float MotionLean = 0.f;
	static constexpr float MotionHeadLift = 0.7f;
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
	float TurnDrag = 0.35f;

	/** Swing in degrees per cm/s짼 of sideways speeding up (the tail trails the other way). */
	UPROPERTY(EditAnywhere, Category = "Tail")
	float SideAccelSwing = 0.003f;

	/** How fast the tail springs back to straight (oscillations per second) and how much it overshoots (0 none, 1 a lot). */
	UPROPERTY(EditAnywhere, Category = "Tail", meta = (ClampMin = "0.1"))
	float SpringFrequency = 1.1f;
	UPROPERTY(EditAnywhere, Category = "Tail", meta = (ClampMin = "0", ClampMax = "1"))
	float SpringBounce = 0.25f;

	/** Most the tail swings to a side, in degrees, all bones together. */
	UPROPERTY(EditAnywhere, Category = "Tail")
	float MaxSwing = 30.f;

	/** Wag: side to side, in degrees at the tip and wags per second, standing and at WagRunSpeed. */
	UPROPERTY(EditAnywhere, Category = "Tail")
	FVector2D WagAmplitude = FVector2D(8.f, 12.f);
	UPROPERTY(EditAnywhere, Category = "Tail")
	FVector2D WagFrequency = FVector2D(0.38f, 0.85f);
	/** The wag runs down the tail, the tip this far behind the root (radians). */
	UPROPERTY(EditAnywhere, Category = "Tail")
	float WagDelay = 1.45f;

	/** Degrees the tail lifts at WagRunSpeed, plus a small bob with each step. */
	UPROPERTY(EditAnywhere, Category = "Tail")
	float RunLift = 14.f;
	UPROPERTY(EditAnywhere, Category = "Tail")
	float RunBob = 2.f;
	UPROPERTY(EditAnywhere, Category = "Tail")
	float WagRunSpeed = 600.f;

	/** Degrees the tail lifts while riding (UpdateRidePose), up off the seat and out of the vehicle's body. */
	UPROPERTY(EditAnywhere, Category = "Tail")
	float RideTailLift = 60.f;

	/**
	 * The chain (2026-11-24, rr.Tail.Chain 0 = as before): every tail bone the mesh has is its own damped spring, softer
	 * toward the tip, and a bone is left behind by what the bones above it do (ChainInertia), so a move runs down the
	 * tail as a wave instead of all the joints turning together. The wag, the lift and the turn swing above are what the
	 * springs are pulled toward. Bones named Tail5, Tail6 ... after TailBoneNames are found on the mesh and join in
	 * (the cat has nine, the rat eleven; only the first four used to bend).
	 * Oscillations per second at the root and at the tip, and the damping ratio (1 = no overshoot).
	 */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain", meta = (ClampMin = "0.1"))
	float ChainRootFrequency = 1.3f;
	UPROPERTY(EditAnywhere, Category = "Tail|Chain", meta = (ClampMin = "0.1"))
	float ChainTipFrequency = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Tail|Chain", meta = (ClampMin = "0.05", ClampMax = "1.5"))
	float ChainDamping = 0.3f;
	/** 0 .. 1: how much of the turning of the bones above a bone does not follow at once. */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain", meta = (ClampMin = "0", ClampMax = "1"))
	float ChainInertia = 0.8f;
	/** How many times over the body's motion (turning, speeding up, braking, jumping, each step) moves the tail. */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain", meta = (ClampMin = "0"))
	float ChainGain = 3.f;
	/** Where along the tail it bends: 0 toward the tip (the root hardly moves) .. 1 toward the root (the whole tail swings
	 * from its base). This and ChainRootFrequency are for bodies that are no animal friend's: an animal friend's come
	 * from its FRRSpeciesLook (TailRootBias, TailRootFrequency). */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain", meta = (ClampMin = "0", ClampMax = "1"))
	float ChainRootBias = 1.f;
	/** Most the turn and sideways swing reaches in the chain, in degrees, all bones together. */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain")
	float MaxChainSwing = 70.f;
	/** Degrees the tail floats up per cm/s of falling (and is dragged down by, rising): a jump shows in the tail. */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain")
	float FallLift = 0.08f;
	/** Degrees the tail is thrown up per cm/s2 of braking (and trails down by, speeding up). */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain")
	float BrakeLift = 0.008f;
	/** Most the two above lift or lower the tail together, in degrees. */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain")
	float MaxChainLift = 60.f;
	/** Each shot flicks the tail up: degrees a second, a bone (half at the root, half again more at the tip). */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain")
	float ShotTailKick = 90.f;
	/** Degrees a second of the body's turn at which the tail starts to go from its walking springs and chain lag to its
	 * turning ones (FRRSpeciesLook::TailTurnDamping, TailTurnFollow), and at which it is all the way there. */
	UPROPERTY(EditAnywhere, Category = "Tail|Chain", meta = (ClampMin = "0"))
	float TailTurnFrom = 60.f;
	UPROPERTY(EditAnywhere, Category = "Tail|Chain", meta = (ClampMin = "1"))
	float TailTurnFull = 240.f;

	/**
	 * Ears (2026-11-25, the user: the fox, red fox, rabbit, tiger and dragon). Bones EarL1.., EarR1.. under the head, each
	 * ear ending in an unweighted EarLEnd / EarREnd (char-pipeline/scripts/blender_ear_bones.py); a mesh without them skips
	 * this. Each ear bone is a damped spring in two ways, both taken from the ear's own direction in the reference pose:
	 * "back" (the tip toward the back of the head; negative forward) and "out" (the tip away from the head's middle and
	 * down: the drooping 'airplane ear'; for a fin pointing sideways, down). The springs are thrown by how the ear's tip
	 * would move if it were rigid on the head (the head bobbing, turning, landing) and pulled toward the pose of the moment:
	 * swept back on the run, forward and alert while aiming, flattened when hit, drooping when knocked out, a twitch now and
	 * then standing. All in degrees for the whole ear (shared over its bones, more toward the tip), times the animal
	 * friend's FRRSpeciesLook::EarPose; rr.Ear.* try other amounts without a rebuild, rr.Ear 0 turns them off.
	 */
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarRunBack = 30.f;
	/**
	 * The rush (2026-11-25, the user: "뛰어갈 때 여우, 레드폭스, 토끼는 좀 더 드라마틱 하게 귀가 뒤로 밀렸으면 좋겠어. 나중에 탈것을
	 * 타서 속도가 발생할때도 비슷하게"): past EarRushFrom cm/s (a walk is 300) the ears are swept back by the animal friend's
	 * FRRSpeciesLook::EarRush more, 1 - e^(-over / EarRushSpeed) of it (a sprint about 60%, a vehicle at 1000 nearly all), and
	 * flutter in the wind by EarFlutter degrees at full rush. Riding, the vehicle's speed counts. rr.Ear.Rush scales it.
	 */
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarRushFrom = 300.f;
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarRushSpeed = 230.f;
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarFlutter = 5.f;
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarAimForward = 12.f;
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarHitBack = 55.f;
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarHitOut = 20.f;
	/** Seconds a hit keeps the ears down (they ease back up over it). */
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarHitTime = 0.9f;
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarDownBack = 20.f;
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarDownOut = 45.f;
	/** Each shot: the ears flinch back, degrees a second at the root. */
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarShotKick = 160.f;
	/** Standing, one ear twitches every so often (seconds between, at random within), this hard (degrees a second). */
	UPROPERTY(EditAnywhere, Category = "Ears")
	FVector2D EarTwitchEvery = FVector2D(2.5f, 6.f);
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarTwitch = 420.f;
	/** How hard the head's motion throws the ears: times the tip's acceleration across the ear over the ear's length (as
	 * an angular push), at the fox's EarGain 1. At 0.45 the fox showed 2-3 degrees walking and up to 14 stopping a
	 * sprint; the user found that too faint (2026-11-25: "출렁임이 약하다 Throw 2로 올려서"). */
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarThrow = 2.f;
	/** The tip's spring as a share of the root's frequency; the damping ratio. */
	UPROPERTY(EditAnywhere, Category = "Ears", meta = (ClampMin = "0.1", ClampMax = "1"))
	float EarTipFrequencyShare = 0.65f;
	UPROPERTY(EditAnywhere, Category = "Ears", meta = (ClampMin = "0.05", ClampMax = "1.5"))
	float EarDamping = 0.3f;
	/** Most an ear goes from its modelled place either way, in degrees for the whole ear. */
	UPROPERTY(EditAnywhere, Category = "Ears")
	float EarMost = 75.f;

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

	/** The glide's spread from the wings' rest pose: the root turned down and forward, the tip further out in line with it.
	 * GlideWingPitch then turns both spread wings about the side axis (positive tips their upper edges back), so with the
	 * dragon lying out flat in the glide (round 85) the membranes lie flat too instead of standing up behind it. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D GlideRootDownForward = FVector2D(16.f, 20.f);
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D GlideTipDownForward = FVector2D(6.f, 15.f);
	UPROPERTY(EditAnywhere, Category = "Wings")
	float GlideWingPitch = 30.f;

	/** Seconds to spread into the glide and to fold out of it. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	float GlideBlendTime = 0.2f;

	/** Gliding, the spread wings are held still (round 85: the old beats every couple of seconds are gone) and only their
	 * tips flutter in the wind: so many shivers a second and so many degrees, the full amount at GlideFlutterSpeed. */
	UPROPERTY(EditAnywhere, Category = "Wings")
	FVector2D GlideTipFlutter = FVector2D(9.f, 7.f);
	UPROPERTY(EditAnywhere, Category = "Wings")
	float GlideFlutterSpeed = 600.f;

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

	const TArray<FName>& GetTailBones() const { return ActiveTailBones.Num() > 0 ? ActiveTailBones : TailBoneNames; }
	const TArray<FName>& GetFlightBones() const { return FlightBoneNames; }
	const TArray<FQuat>& GetFlightBends() const { return FlightBends; }
	const TArray<FName>& GetJumpBones() const { return JumpBoneNames; }
	const TArray<FQuat>& GetJumpBends() const { return JumpBends; }
	const TArray<FName>& GetRideBones() const { return RideBoneNames; }
	const TArray<FQuat>& GetRideBends() const { return RideBends; }
	/** How far the body sinks to sit in the ride pose this frame (the character lowers the mesh by it). */
	float GetRideDrop() const { return RideDrop; }
	/** 0 .. 1: how far into the ride pose. */
	float GetRideWeight() const { return RideWeight; }
	const TArray<FName>& GetSprintBones() const { return SprintBoneNames; }
	const TArray<FQuat>& GetSprintBends() const { return SprintBends; }
	const TArray<FName>& GetWingBones() const { return WingBoneNames; }
	const TArray<FQuat>& GetWingBends() const { return WingBends; }
	/** The ears' bones (every ear, root to tip, one ear after the other), each one's turn in the reference pose's mesh space, and the head they hang from. */
	const TArray<FName>& GetEarBones() const { return EarBoneNames; }
	const TArray<FQuat>& GetEarBends() const { return EarBends; }
	FName GetEarHead() const { return EarHead; }
	const FQuat& GetEarHeadRest() const { return EarHeadRest; }
	const TArray<float>& GetTailSwing() const { return BoneSwing; }
	const TArray<float>& GetTailLift() const { return BoneLift; }
	float GetTailSize() const { return TailSize; }
	bool GetTailFloor(FVector& Point, FVector& Normal, float& Margin) const { Point = TailFloorPoint; Normal = TailFloorNormal; Margin = TailFloorMargin; return bTailFloor; }
	const FRRAimFrame& GetAimFrame() const { return Aim; }
	const FRRMeleeFrame& GetMeleeFrame() const { return Melee; }
	/**
	 * Round 97, the slash (Tripo text-to-motion "Sword Slash Attack" on the fox's Mixamo rig: its sweep is frames 12..22 of 47) played
	 * from SlashFrom to SlashTo of its length (the wind-up and the recovery trimmed) over
	 * SlashTime seconds after each swing, easing in and out; the guard eases in and out over GuardBlendTime.
	 */
	// (per combo blow, from their hand traces: 1st Tripo "Sword Slash Attack" right to left 12..22 of 47, 2nd "Backhand Slash
	// Combo" its backhand left to right 27..36, 3rd "Diagonal Slash Strike" a low thrust rising up high right 9..27)
	UPROPERTY(EditAnywhere, Category = "Sword")
	float SlashFrom[3] = { 0.22f, 0.55f, 0.19f };
	UPROPERTY(EditAnywhere, Category = "Sword")
	float SlashTo[3] = { 0.52f, 0.79f, 0.58f };
	/** The third blow (the finisher) takes this much longer. */
	UPROPERTY(EditAnywhere, Category = "Sword")
	float FinisherTimeShare = 1.25f;
	UPROPERTY(EditAnywhere, Category = "Sword")
	float SlashTime = 0.42f;
	UPROPERTY(EditAnywhere, Category = "Sword")
	float GuardBlendTime = 0.12f;
	/** On guard, a shot knocked away plays the slash's blow this quickly over the guard (a parry). */
	UPROPERTY(EditAnywhere, Category = "Sword")
	float ParryTime = 0.28f;
	/** The parry motion's flick, as a share of it ("Laser Sword Parry": up and in, frames 6..12 of 47, then back toward the guard). */
	UPROPERTY(EditAnywhere, Category = "Sword")
	float ParryFrom = 0.1f;
	UPROPERTY(EditAnywhere, Category = "Sword")
	float ParryTo = 0.4f;
	float GetAimWeight() const { return Aim.Weight; }
	float GetAimMoveWeight() const { return Aim.MoveWeight; }
	/**
	 * Motion matching test (2026-11-20, RRRandomMotionMatching.h): while the animal friend walks about and has a driver
	 * body (ARRRandomCharacter::GetMotionDriver), the driver's local bone transforms become this body's base pose for
	 * the legs, hips, spine, neck and head. The arms keep the body's own animation (they hold the gun) and the tail its
	 * own bends. MotionWeight eases in and out over MotionBlendTime.
	 */
	const TArray<FTransform>& GetMotionPose() const { return MotionPose; }
	const TArray<uint8>& GetMotionMask() const { return MotionMask; }
	float GetMotionWeight() const { return MotionWeight; }
	float GetMotionLean() const { return MotionLean; }
	float GetPivotLoad() const { return PivotLoad; }
	const FRRGunHandsFrame& GetGunHandsFrame() const { return GunHands; }
	/** 0 .. 1: the driver's whole pose is taken, arms too (a climbing clip, ARRRandomCharacter::WantsClimbMotion). */
	float GetMotionWholeWeight() const { return MotionWholeWeight; }
	const TArray<FTransform>& GetMotionWholePose() const { return MotionWholePose; }
	UPROPERTY(EditAnywhere, Category = "Motion Matching")
	float MotionBlendTime = 0.2f;
	/**
	 * Degrees the upper body leans forward at the walking speed while matched with its upper body (rr.MotionMatch.Upper 1):
	 * the matched clips run upright and empty-handed, which looked stiff with a gun in the hands (the user, 2026-11-20).
	 * rr.MotionMatch.Lean overrides it for trying amounts. 22 after the user compared 0, 14 and 22 ("기존 처럼 22 까지 숙이면
	 * 좋겠네": about as far as the old walk animation bends).
	 */
	UPROPERTY(EditAnywhere, Category = "Motion Matching")
	float MotionRunLean = 22.f;

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	/** Degrees the tail is swung to the brawler's right (negative: left), and how fast it moves. */
	float Swing = 0.f;
	float SwingSpeed = 0.f;
	float Lift = 0.f;
	float WagPhase = 0.f;
	float TailTime = 0.f, TailTrail = 0.f, TailPace = 0.f;
	/** Fades the wag in and out (out once knocked out). */
	float Life = 1.f;
	bool bHasLast = false;
	float LastYaw = 0.f;
	FVector LastVelocity = FVector::ZeroVector;
	TArray<float> BoneSwing;
	TArray<float> BoneLift;
	/** The chain: how fast each bone's swing and lift move, the tail bones found on this mesh, the lift from falling and braking, the last shot. */
	TArray<float> BoneSwingSpeed;
	TArray<float> BoneLiftSpeed;
	TArray<FName> ActiveTailBones;
	TWeakObjectPtr<const USkeletalMesh> TailBonesMesh;
	float LiftTrail = 0.f;
	float LastSinceShot = 0.f;
	/** This body's own (its species'): see ChainRootBias. */
	float TailRootBias = 1.f;
	float TailRootHz = 1.3f;
	bool bTailBigWag = true;
	float TailIdleCurl = 0.f, TailIdleLift = 0.f, TailIdleTipLift = 0.f;
	/** How much the whole tail motion is, 1 = full (FRRSpeciesLook::TailAmount). */
	float TailAmount = 1.f;
	/** FRRSpeciesLook's cat-tail numbers (TailWave ...). */
	float TailWave = 1.f, TailWagSize = 1.f, TailWagSpeed = 1.f, TailTipHz = -1.f, TailMoveLift = 0.f, TailMoveTipLift = 0.f, TailDampingRatio = -1.f;
	/** FRRSpeciesLook::TailSquiggle ..., and where the squiggle wave is (radians). */
	float TailSquiggle = 0.f, TailSquiggleSide = 0.f, TailSquiggleWaves = 1.f, TailSquiggleSpeed = 0.4f, SquigglePhase = 0.f;
	/** FRRSpeciesLook::TailFollow; the hips' frame-to-frame turn, tip (degrees) and the tail root's rise (cm) it lags behind;
	 * the tail's base direction in its parent bone and that bone, its length (mesh units), the last frame's readings. */
	float TailFollow = 0.f;
	/** FRRSpeciesLook::TailTurnDamping, TailTurnFollow (-1: as walking), and how hard the body is turning now, 0 .. 1. */
	float TailTurnDamping = -1.f, TailTurnFollow = -1.f, TailTurning = 0.f;
	float TailFollowYaw = 0.f, TailFollowPitch = 0.f, TailFollowRise = 0.f;
	TWeakObjectPtr<const USkeletalMesh> TailDirMesh;
	FVector TailDirLocal = FVector::ZeroVector;
	FName TailParent;
	float TailLengthMesh = 0.f;
	float LastTailYaw = 0.f, LastTailPitch = 0.f, LastTailRootZ = 0.f;
	bool bTailFollowLast = false;
	/** FRRSpeciesLook::TailSize; and the floor under the tail this frame (mesh space, see the proxy). */
	float TailSize = 1.f;
	bool bTailFloor = false;
	FVector TailFloorPoint = FVector::ZeroVector;
	FVector TailFloorNormal = FVector::UpVector;
	float TailFloorMargin = 0.f;
	/** 0 moving .. 1 standing still, eased. */
	float TailIdle = 0.f;
	float TailBrakeHold = 0.f;
	float PivotTime = 0.f;
	float PivotLoad = 0.f;

	/** One ear: its bones, its two turning axes and where its tip moves on each (reference pose, mesh space), its length, its springs. */
	struct FEar
	{
		TArray<FName> Bones;
		FVector BackAxis = FVector::ZeroVector, OutAxis = FVector::ZeroVector;
		FVector BackMove = FVector::ZeroVector, OutMove = FVector::ZeroVector;
		/** The tip's place on the head (head bone space, unscaled) and the ear's length (cm, unscaled). */
		FVector TipOnHead = FVector::ZeroVector;
		float Length = 1.f;
		TArray<float> Back, Out, BackSpeed, OutSpeed;
		FVector LastTip = FVector::ZeroVector, LastTipVelocity = FVector::ZeroVector, TipAcceleration = FVector::ZeroVector;
		bool bHasLast = false;
	};
	TArray<FEar> Ears;
	TWeakObjectPtr<const USkeletalMesh> EarMesh;
	FName EarHead;
	FQuat EarHeadRest = FQuat::Identity;
	TArray<FName> EarBoneNames;
	TArray<FQuat> EarBends;
	float EarFrequency = 3.f, EarGain = 1.f, EarPose = 1.f, EarRushDegrees = 15.f, EarTime = 0.f, EarRigidity = 0.f, EarDampingRatio = 0.3f;
	float EarHit = 0.f, LastHitFlash = 0.f, EarSinceShot = 0.f, EarTwitchIn = 3.f;
	void UpdateEars(float DeltaSeconds);
	FRRAimFrame Aim;
	bool bHasBodyHeading = false;
	FVector LastBodyLocation = FVector::ZeroVector;
	/** The run directions' mix, eased toward where the brawler is going. */
	float TargetMoveWeights[4] = { 1.f, 0.f, 0.f, 0.f };

	void UpdateAim(float DeltaSeconds);
	/** Motion matching test: copies the driver's pose (MotionPose) and eases MotionWeight. */
	void UpdateMotion(float DeltaSeconds);
	TArray<FTransform> MotionPose;
	TArray<uint8> MotionMask;
	float MotionWeight = 0.f;
	/** The driver mesh the mask and bone map were made for, and per bone of this mesh its bone in the driver's. */
	TWeakObjectPtr<const USkeletalMesh> MotionMappedMesh;
	TWeakObjectPtr<const USkeletalMesh> MotionMappedBody;
	TArray<int32> MotionBoneMap;
	TArray<FTransform> SourceComponentPose, TargetComponentPose;
	TArray<FQuat> MotionBindRotations;
	FVector SourceBindHips = FVector::ZeroVector, TargetBindHips = FVector::ZeroVector;
	int32 MotionHips = INDEX_NONE;
	float MotionWholeWeight = 0.f;
	TArray<FTransform> MotionWholePose;
	/** rr.MotionMatch.Upper when the mask was made. */
	int32 MotionUpperMode = -1;
	float MotionLean = 0.f;
	/** The left hand on the held gun (GunHands), eased in and out as the gun comes and goes. */
	void UpdateGunHands(float DeltaSeconds);
	FRRGunHandsFrame GunHands;
	/** Round 97: the light sword's layer (Melee) from the brawler's last swing and its guard. */
	void UpdateMelee(float DeltaSeconds);
	FRRMeleeFrame Melee;
	/** The wing bones' turns for this frame (WingBends), from how the brawler is moving. */
	void UpdateWings(float DeltaSeconds);
	/** The flying pose's turns for this frame (FlightBends). */
	void UpdateFlightPose(float DeltaSeconds);
	/** The jumping pose's turns for this frame (JumpBends). */
	void UpdateJumpPose(float DeltaSeconds);
	/** The legs (thigh, knee, foot; left then right), the spine, then the arms (upper arm, forearm; left then right). */
	TArray<FName> JumpBoneNames;
	TArray<FQuat> JumpBends;
	float JumpWeight = 0.f;
	float JumpRise = 0.f;

	/**
	 * Riding a vehicle: at the wheel, a pose for the kind of vehicle (standing on the kickboard with both hands on the
	 * bar, astride the motorbike, sitting in a car with the hands on the wheel); in a car's rear seat, sitting with the
	 * arms left free to shoot. The legs (thigh, knee, foot; left then right), the spine, then the arms (upper arm, forearm;
	 * left then right), like the jump.
	 */
	void UpdateRidePose(float DeltaSeconds);
	TArray<FName> RideBoneNames;
	TArray<FQuat> RideBends;
	float RideWeight = 0.f;
	float RideDrop = 0.f;
	/** The last pose ridden, kept while easing out of it. */
	int32 RidePose = 0;

	/** Running (Shift): the gun held low, muzzle down (round 67). The upper arms (left, right), then the hands. */
	void UpdateSprintPose(float DeltaSeconds);
	TArray<FName> SprintBoneNames;
	TArray<FQuat> SprintBends;
	float SprintWeight = 0.f;

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
};
