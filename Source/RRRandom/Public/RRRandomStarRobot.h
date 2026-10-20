#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomStarRobot.generated.h"

class ARRRandomCharacter;
class ARRRandomVehicle;
class UCapsuleComponent;
class UMaterialInstanceDynamic;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** What an animal friend is doing with the star robot: combining it, or which seat it has inside. */
UENUM()
enum class ERRRobotSeat : uint8
{
	None,
	/** Pressed F by the gathered cars: stands still until the robot is done; knocked out, the combining is off. */
	Combiner,
	/** Walks it (only with all three seats taken). */
	Move,
	/** Turns its upper body and fires its rocket fists. */
	Attack,
	/** Holds up its shield. */
	Defend,
};

/**
 * The star team's combiner robot. When its three cars (ARRRandomVehicle StarChest, StarArms and StarLegs) are parked or
 * driven within GatherRadius of their middle, a combine sign shows there; the first animal friend within CombineReach
 * to press F (ARRRandomCharacter::ToggleRide) starts it: the riders are put off, the cars hidden, and this robot plays
 * the combining like a cartoon over CombineSeconds, the cars hopping, flying up in a spiral, turning into the robot's
 * parts and slotting together (legs, chest, arms, head), with a gauge over it. The one who started it stands still the
 * whole time; if it is knocked out the combining is off and the cars drop back where they were.
 * Done, the robot belongs to that one's team. Its three seats are taken with F next to it, in order: MOVE (walks it,
 * only once all three are in), ATTACK (its upper body turns to where that one aims and its fists fly off as rockets),
 * DEFEND (holding fire holds up a shield that takes most of the damage, for as long as its energy lasts). The crew are
 * hidden inside, out of harm's way: what would hit them hits the robot. At no health it breaks apart into the three cars
 * and the crew are thrown out. F again gets out.
 * The walking robot rides along with its driver's capsule (like a vehicle, so the driver's movement, prediction and all,
 * moves it); the other two ride along attached to it.
 * Server: everything; replicated: the cars and where they started, the combiner, the start time, the crew, health,
 * shield and shots. Every machine plays the combining and the walking from those.
 */
UCLASS(NotPlaceable)
class RRRANDOM_API ARRRandomStarRobot : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomStarRobot();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** The robot in this world (there is one star team), combining or done, or null. */
	static ARRRandomStarRobot* Get(const UObject* WorldContext);
	/**
	 * The star team's three cars, gathered within GatherRadius of their middle, none in a robot and no robot about;
	 * fills them in (chest, arms, legs) and their middle on the floor.
	 */
	static bool FindGathered(const UObject* WorldContext, ARRRandomVehicle* OutCars[3], FVector& OutCenter);
	/** Whether this animal friend may start combining now (alive, on foot or in a car, close enough to the gathered cars). */
	static bool CanStartCombine(const ARRRandomCharacter* Who);
	/** Server: starts combining the gathered cars, Who being the one who started it. */
	static ARRRandomStarRobot* StartCombine(ARRRandomCharacter* Who);

	/** Gathered cars are this close to their middle at most. */
	static constexpr float GatherRadius = 800.f;
	/** Animal friends this close (flat) to the gathered cars' middle can start combining. */
	static constexpr float CombineReach = 1000.f;
	/** Animal friends this close (flat) to the robot can get in. */
	static constexpr float BoardReach = 460.f;
	/** How long the combining takes, seconds. */
	static constexpr float CombineSeconds = 10.f;

	bool IsAssembled() const { return bAssembled; }
	/** 0 to 1 over the combining (1 once done). */
	float GetCombineProgress() const;
	int32 GetTeam() const { return Team; }
	ARRRandomCharacter* GetCombiner() const { return Combiner; }
	/** Who sits in a seat (Move, Attack or Defend), or null. */
	ARRRandomCharacter* GetCrew(ERRRobotSeat Seat) const;
	int32 CountCrew() const;
	bool IsCrewFull() const { return CountCrew() == 3; }
	/** The seat the next one in would get: Move, then Attack, then Defend; None if full. */
	ERRRobotSeat GetFreeSeat() const;
	/** Whether this animal friend could get in now (done, same team, a seat free, close enough unless bAnyDistance). */
	bool CanBoard(const ARRRandomCharacter* Who, bool bAnyDistance = false) const;
	/** Server: in it goes, to the free seat. */
	void Board(ARRRandomCharacter* Who, bool bAnyDistance = false);
	/** Server: out it gets (the combiner too: that calls the combining off). */
	void Leave(ARRRandomCharacter* Who);
	/** Server: the crew's fire button. ATTACK: the upper body turns to Target and a rocket fist flies at it (now and then).
	 * DEFEND: the shield stays up a moment longer (held fire keeps it up). */
	void CrewFire(ARRRandomCharacter* Who, const FVector& Target);
	/** Server, a cheat: comes apart now (combining: called off, the cars back where they were; done: broken). */
	void CheatBreak() { if (HasAuthority()) { BreakApart(!bAssembled); } }
	/** Server: the one hit for the whole crew (the first one in) passes damage on to the robot; the others take none. */
	bool IsDamageProxy(const ARRRandomCharacter* Who) const;

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsShieldUp() const { return bShieldUp; }
	/** 0 to 1. */
	float GetShieldEnergy() const { return ShieldEnergy; }
	/** Seconds since this machine saw it finish combining (for the HUD's banner), or a big number. */
	float GetSecondsSinceAssembled() const;
	/**
	 * Just combined, it turns round from facing the one who combined it to that one's way (its driver turns it, once,
	 * unless it is driven off first): whether it should, and to what yaw.
	 */
	bool GetIntroTurn(float& OutYaw) const { OutYaw = IntroTurnYaw; return bIntroTurn; }
	/** Seconds after it is done before it turns round (its pose first). */
	static constexpr float IntroTurnDelay = 0.4f;
	/** Where its head is, for the HUD (gauge, seats). */
	FVector GetTopLocation() const;

	/**
	 * A rocket fist (ARRRandomProjectile::MakeRocketFist): the left or right forearm mesh, and where to put it relative
	 * to the shot so it flies fist first with its middle on the shot; its length (elbow to fist).
	 */
	static UStaticMesh* LoadForearm(bool bLeft);
	static FTransform GetForearmFlightPose(bool bLeft);
	static float GetForearmLength();

	/** The driver's walking with the robot: a share of its walking speed, how fast it gets going and turns. */
	static constexpr float WalkSpeedShare = 1.25f;
	static constexpr float WalkAcceleration = 900.f;
	static constexpr float WalkTurnRate = 150.f;
	static constexpr float WalkBraking = 1600.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Server: done combining. */
	void FinishCombine();
	/** Server: the cars come back out, around Where (scattered) or where they started; the crew out; the robot goes. */
	void BreakApart(bool bBackToStart);
	/** Server: one of the crew leaving or knocked out. */
	void ClearSeat(ARRRandomCharacter* Who);

	/** Every machine: the combining's choreography at this time (0 to CombineSeconds). */
	void PoseCombining(float Time, float DeltaSeconds);
	/** Every machine: standing or walking, the upper body turning, fists punching. */
	void PoseAssembled(float DeltaSeconds);
	/** Every machine: the jointed parts (walking, firing forearms) instead of the whole legs and arms, or back. */
	void ShowJointed(bool bJointedParts);
	/** Every machine, at a moment in the combining: its pop of light, stars and a shake. */
	void PlayBeat(int32 Beat);

	/** Every machine: it breaks apart (bFizzle: the combining called off, a puff; otherwise a burst). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBreak(bool bFizzle);

	UFUNCTION()
	void OnRep_Assembled();
	UFUNCTION()
	void OnRep_Shot();

	UPROPERTY(VisibleAnywhere, Category = "Robot")
	TObjectPtr<USceneComponent> Root;
	/** What shots hit (not walls, sight or animal friends). */
	UPROPERTY(VisibleAnywhere, Category = "Robot")
	TObjectPtr<UCapsuleComponent> Hull;
	/** Bobs and sways as it walks; everything else hangs off it. */
	UPROPERTY()
	TObjectPtr<USceneComponent> Hips;
	/** Turns to where the ATTACK seat aims: the chest, head and arms. */
	UPROPERTY()
	TObjectPtr<USceneComponent> Upper;
	UPROPERTY()
	TObjectPtr<USceneComponent> Neck;
	UPROPERTY()
	TObjectPtr<USceneComponent> ShoulderL;
	UPROPERTY()
	TObjectPtr<USceneComponent> ShoulderR;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Legs;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Torso;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ArmL;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ArmR;
	/**
	 * Once combined, the legs and arms are drawn jointed (char-pipeline/scripts/blender_robot_joints.py) so it can walk
	 * and fire its forearms: the pelvis on the hips, each thigh on a hip joint and shin on a knee joint, each upper arm on
	 * its shoulder and forearm (the rocket gauntlet and fist) on an elbow joint. The whole Legs, ArmL and ArmR fly in the
	 * combining. Without the jointed meshes the whole parts stay.
	 */
	UPROPERTY()
	TObjectPtr<USceneComponent> HipL;
	UPROPERTY()
	TObjectPtr<USceneComponent> HipR;
	UPROPERTY()
	TObjectPtr<USceneComponent> KneeL;
	UPROPERTY()
	TObjectPtr<USceneComponent> KneeR;
	UPROPERTY()
	TObjectPtr<USceneComponent> ElbowL;
	UPROPERTY()
	TObjectPtr<USceneComponent> ElbowR;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Pelvis;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ThighL;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ThighR;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ShinL;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ShinR;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> UpperArmL;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> UpperArmR;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ForearmL;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ForearmR;
	bool bHasJoints = false;

	/** The three cars as they fly about while combining (the real ones are hidden meanwhile). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CarMeshes;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Shield;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ShieldMaterial;

	/** The real cars, chest, arms and legs. */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<ARRRandomVehicle>> Cars;
	/** Where each car was when the combining started, and which way it faced. */
	UPROPERTY(Replicated)
	TArray<FVector> CarStarts;
	UPROPERTY(Replicated)
	TArray<float> CarYaws;
	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomCharacter> Combiner;
	/** Server time the combining started (GetServerWorldTimeSeconds). */
	UPROPERTY(Replicated)
	float CombineStartTime = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Assembled)
	bool bAssembled = false;
	UPROPERTY(Replicated)
	int32 Team = 0;
	/** MOVE, ATTACK, DEFEND. */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<ARRRandomCharacter>> Crew;
	UPROPERTY(Replicated)
	float Health = 0.f;
	UPROPERTY(Replicated)
	bool bShieldUp = false;
	UPROPERTY(Replicated)
	float ShieldEnergy = 1.f;
	/** Where the upper body turns to, relative to the legs, degrees (the ATTACK seat's aim). */
	UPROPERTY(Replicated)
	float AimYaw = 0.f;
	/** Counts the fists fired: each change punches (odd: the left arm, even: the right). */
	UPROPERTY(ReplicatedUsing = OnRep_Shot)
	int32 ShotCount = 0;
	/** Done combining: turn round to this yaw (GetIntroTurn). */
	UPROPERTY(Replicated)
	bool bIntroTurn = false;
	UPROPERTY(Replicated)
	float IntroTurnYaw = 0.f;

	UPROPERTY(EditAnywhere, Category = "Robot")
	float MaxHealth = 1500.f;

	/** The meshes: the robot's five parts, each with the whole robot's pivot. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> PartMeshes;

	/** Server: when the shield's hold runs out, and when the next fist may fly. */
	float ShieldHoldUntil = 0.f;
	bool bShieldSpent = false;
	float NextFistTime = 0.f;
	/** Every machine: the combining beats already played, the moment it was seen done, the walk's phase, the shown aim
	 * and each arm's punch (1 just fired, easing back to 0). */
	int32 BeatsPlayed = 0;
	float AssembledSeenTime = -1.f;
	float WalkPhase = 0.f;
	float WalkAmount = 0.f;
	float ShownAimYaw = 0.f;
	float PunchL = 0.f;
	float PunchR = 0.f;
	/** When this machine last saw a fist fly, and how far the arms are raised into the aim (0 to 1). */
	float AimSeenTime = -100.f;
	float ArmsUp = 0.f;
	/** Which way it faced last frame (turning on the spot steps). */
	float LastPoseYaw = 0.f;
	/** A jolt through the body as a foot comes down (1, easing to 0). */
	float StepJolt = 0.f;
	/** When each forearm (left, right) that flew off as a rocket grows back (this machine's time; below 0: in place). */
	float ForearmBackL = -1.f;
	float ForearmBackR = -1.f;
	int32 LastShotSeen = 0;
	float ShieldShown = 0.f;
};
