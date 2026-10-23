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
	/** Turns its upper body to the aim: fire swings its light sword (up to three in a combo), the right button fires a
	 * spread of little missiles that curve in on the aim. */
	Attack,
	/** Holds up the glowing shield in its left hand (the robot stops attacking meanwhile). */
	Defend,
};

/** A loud moment over the robot, a comic word on everyone's screen (ARRRandomHUD). */
UENUM()
enum class ERRRobotShout : uint8
{
	None,
	/** Its shield went up just before a blow landed: the blow bounces off. */
	Parry,
	/** Its shield was smashed down by a slam. */
	GuardBreak,
	/** Locked blade to blade with the rival. */
	Clash,
	/** Won the clash. */
	ClashWin,
	/** Knocked down, kneeling. */
	Down,
	/** Up again, not finished off in time. */
	Rise,
	/** Finished the rival off with a slam. */
	Finish,
};

/**
 * A team's combiner robot (each team has its own three star cars and so its own robot). When its three cars
 * (ARRRandomVehicle StarChest, StarArms and StarLegs) are parked or driven within GatherRadius of their middle, a combine
 * sign shows there; the first animal friend of the team within CombineReach to press F (ARRRandomCharacter::ToggleRide)
 * starts it: the riders are put off, the cars hidden, and this robot plays the combining like a cartoon over
 * CombineSeconds, the cars hopping, flying up in a spiral, turning into the robot's
 * parts and slotting together (legs, chest, arms, head), with a gauge over it. The one who started it stands still the
 * whole time; if it is knocked out the combining is off and the cars drop back where they were.
 * Done, the robot belongs to that one's team. Its three seats are taken with F next to it, in order: MOVE (walks it,
 * only once all three are in), ATTACK (its upper body turns to where that one aims; fire swings the light sword in its
 * right hand, pressed again in time up to a three-hit combo, which also smashes structures; the right button fires a
 * spread of little missiles that curve in on the aim; both with a cooldown), DEFEND (holding fire stops the attacks and
 * holds up a glowing shield in its left hand that stops what comes at it from the front, until its own health runs out;
 * lowered, the shield slowly mends). The crew are hidden inside, out of harm's way: what would hit them hits the robot.
 * At no health it breaks apart into the three cars and the crew are thrown out. F again gets out.
 * Against the other team's robot: a slam breaks a raised shield, a shield raised just before a blow parries it (the
 * swinger reels), a missile in the middle of a swing stops it, sword blows hit a robot harder and make it reel; two
 * robots swinging at each other at once lock blades in a clash, three exchanges long, while each crew races to press
 * their own ten random W A S D keys (whoever's crew is done first wins it; the other reels, wide open); brought down
 * near the rival it kneels a moment, where the rival's slam finishes it off, or it gets up again.
 * The walking robot rides along with its driver's capsule (like a vehicle, so the driver's movement, prediction and all,
 * moves it); the other two ride along attached to it.
 * Server: everything; replicated: the cars and where they started, the combiner, the start time, the crew, health,
 * shield, swings and volleys. Every machine plays the combining, the walking and the attacks from those.
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

	/** A team's robot in this world, combining or done, or null. */
	static ARRRandomStarRobot* Get(const UObject* WorldContext, int32 InTeam);
	/** The other team's robot once it's done combining, or null. */
	ARRRandomStarRobot* GetRival() const;
	/**
	 * A team's three star cars, gathered within GatherRadius of their middle, none in a robot and no robot of the team
	 * about; fills them in (chest, arms, legs) and their middle on the floor.
	 */
	static bool FindGathered(const UObject* WorldContext, int32 InTeam, ARRRandomVehicle* OutCars[3], FVector& OutCenter);
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
	/** Server: the crew's fire button. ATTACK: the upper body turns to Target and the sword swings (pressed during a swing,
	 * the next of the combo follows it). DEFEND: the shield stays up a moment longer (held fire keeps it up). */
	void CrewFire(ARRRandomCharacter* Who, const FVector& Target);
	/** Server: the crew's other button. ATTACK: a spread of little missiles toward Target (when ready). */
	void CrewAltFire(ARRRandomCharacter* Who, const FVector& Target);
	/** Server, a cheat: comes apart now (combining: called off, the cars back where they were; done: broken). */
	void CheatBreak() { if (HasAuthority()) { BreakApart(!bAssembled); } }
	/** Server, a cheat: the shield up for this many seconds, as if its DEFEND seat held fire (unless it's broken). */
	void CheatShield(float Seconds);
	/** Server, a cheat: Who's team's three cars brought to Where and combined at once (no show), facing Yaw, and the
	 * team's others put in it wherever they are. */
	static ARRRandomStarRobot* CheatAssemble(ARRRandomCharacter* Who, const FVector& Where, float Yaw);
	/** Server, a cheat: the team's animal friends (players and bots) get in, wherever they are, until it's full. */
	void CheatFillCrew();
	/** Server, a cheat: turned to Other, a first swing of the sword right now (cooldown and all set aside). */
	void CheatSwingAt(const ARRRandomStarRobot* Other);
	/** Server, a cheat: down to this much health. */
	void CheatHurt(float NewHealth) { if (HasAuthority() && bAssembled) { Health = FMath::Clamp(NewHealth, 1.f, MaxHealth); } }
	/** Server: the one hit for the whole crew (the first one in) passes damage on to the robot; the others take none. */
	bool IsDamageProxy(const ARRRandomCharacter* Who) const;

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsShieldUp() const { return bShieldUp; }
	/** The shield's own health, and 0 to 1 of it; broken (emptied), it can't go up until it has mended a share. */
	float GetShieldHealth() const { return ShieldHealth; }
	float GetShieldEnergy() const { return ShieldHealth / ShieldMaxHealth; }
	bool IsShieldBroken() const { return bShieldBroken; }
	static constexpr float ShieldMaxHealth = 300.f;
	/** The attacks' cooldowns (seconds, the sword's counted from the end of a combo) and how long until each is ready. */
	static constexpr float SwordCooldown = 1.2f;
	static constexpr float MissileCooldown = 4.f;
	float GetSwordReadyIn() const;
	float GetMissileReadyIn() const;
	/** The sword: the combo's hit now swinging or last swung (1 to 3; 0 none since the last cooldown). */
	int32 GetComboStage() const { return SwingStage; }
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

	/** Reeling (a parried blow, a broken guard, a lost clash, a missile mid-swing, a sword blow): no attacks, no shield. */
	bool IsStaggered() const;
	/** Knocked down, kneeling a moment: only the rival's slam finishes it off, else it gets up again. */
	bool IsDowned() const { return bDowned; }
	/** In a clash, reeling or down: its driver can't walk it. */
	bool IsHeldStill() const { return IsClashing() || bDowned || IsStaggered(); }
	/** The clash it's locked in with the rival (and for a moment after, showing how it ended), or null. */
	ARRRandomStarRobot* GetClashRival() const { return ClashRival; }
	bool IsClashing() const { return ClashRival && ClashOutcome == 0; }
	/** Keys each seat has to press in a clash (W A S D), and how long the clash lasts (three exchanges of blows). */
	static constexpr int32 ClashKeyCount = 10;
	static constexpr float ClashSeconds = 4.5f;
	/**
	 * The clash: seconds into it; the key a seat has to press at a place in its row (0 W, 1 A, 2 S, 3 D); how many each
	 * seat has got right and wrong; this side's share done (0 to 1, all its crew's rows); how it ended (0 still on,
	 * 1 won, 2 lost, 3 even).
	 */
	float GetClashTime() const;
	uint8 GetClashKey(int32 Seat, int32 Index) const;
	int32 GetClashDone(int32 Seat) const;
	int32 GetClashMisses(int32 Seat) const;
	float GetClashShare() const;
	int32 GetClashOutcome() const { return ClashOutcome; }
	/** Server: one of the crew pressed a key in the clash (0 W, 1 A, 2 S, 3 D). */
	void ClashKey(ARRRandomCharacter* Who, uint8 Key);
	/** The sword swing this machine shows (1 to 3, 0 none) and how far into it (0 to 1): the rival's HUD warns of it. */
	int32 GetShownSwing(float& OutInto) const;
	/** The last loud moment over it, and how long ago this machine saw it. */
	ERRRobotShout GetShout(float& OutAge) const;
	/** The glow of its team (the blade, the shield, the missiles' trails). */
	FLinearColor GetTeamGlow() const;
	/** In a clash: between the two robots at the height their blades meet (the crew's cameras close in on it). */
	FVector GetClashFocus() const;
	/** This screen only: its head (and mane) hidden, for a camera looking out from where its eyes are. */
	void HideHeadOnThisScreen(bool bHide);

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
	/** Server: the robot for Who's team out of these cars (chest, arms, legs) at Center facing Yaw, combining from now. */
	static ARRRandomStarRobot* SpawnCombining(ARRRandomCharacter* Who, ARRRandomVehicle* const Found[3], const FVector& Center, float Yaw);
	/** Server: done combining. */
	void FinishCombine();
	/** Server: the cars come back out, around Where (scattered) or where they started; the crew out; the robot goes (bFinished:
	 * finished off by the rival's slam, a bigger burst). */
	void BreakApart(bool bBackToStart, bool bFinished = false);
	/** Server: one of the crew leaving or knocked out. */
	void ClearSeat(ARRRandomCharacter* Who);

	/** Every machine: the combining's choreography at this time (0 to CombineSeconds). */
	void PoseCombining(float Time, float DeltaSeconds);
	/** Server: the next swing of the combo starts now. */
	void StartSwing(int32 Stage);
	/** Server: a swing's blow lands: who is in the blade's sweep is hit, structures there are smashed. */
	void SwingHits();
	/** Server: the blow (which hit of the combo, Index 0 to 2) at the rival robot if it's in the sweep: a clash, a parry,
	 * a broken guard, a finishing slam, or a hit. */
	void SwingAtRival(ARRRandomStarRobot* Rival, int32 Index, ARRRandomCharacter* Who);
	/** Server: reels this long (cuts off a swing, drops the shield). */
	void Stagger(float Seconds);
	/** Server: a loud moment everyone sees. */
	void Shout(ERRRobotShout Word);
	/** Server: locks blades with the rival (both sides). */
	void StartClash(ARRRandomStarRobot* Rival);
	/** Server: the clash ends, this side winning (Outcome 1), losing (2) or even (3); both sides. */
	void EndClash(uint8 Outcome);
	/** Server: the clash under way: bots press their keys, it ends when a side is done or time is up. */
	void UpdateClash(float Now);
	/** Server: brought down near the rival: kneels; and finished off by the rival's slam. */
	void KnockDown();
	void FinishOff(ARRRandomStarRobot* By);
	/** Server: a bot in the DEFEND seat raises the shield in time for the rival's blow, now and then (a parry). */
	void UpdateBotDefender(float Now);
	/** Server: a bot in the ATTACK seat goes at the rival robot (whose crew it can't see, hidden inside): combos of one to
	 * three swings in reach, missiles from further off. */
	void UpdateBotAttacker(float Now);
	/** Server: the shield goes up (when it went up from down counts for a parry). */
	void RaiseShield(float Now, float Hold);

	/** Every machine: standing or walking, the upper body turning, the sword swinging, the shield up. */
	void PoseAssembled(float DeltaSeconds);
	/** Every machine: the jointed parts (walking, firing forearms) instead of the whole legs and arms, or back. */
	void ShowJointed(bool bJointedParts);
	/** Every machine, at a moment in the combining: its pop of light, stars and a shake. */
	void PlayBeat(int32 Beat);

	/** Every machine: it breaks apart (bFizzle: the combining called off, a puff; otherwise a burst). */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBreak(bool bFizzle, bool bFinished);

	UFUNCTION()
	void OnRep_Assembled();
	UFUNCTION()
	void OnRep_Missiles();
	UFUNCTION()
	void OnRep_Swing();
	UFUNCTION()
	void OnRep_Shout();

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
	/** The light sword in its right fist (a hilt, a glowing blade round a white core) and the grip it turns about. */
	UPROPERTY()
	TObjectPtr<USceneComponent> SwordGrip;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SwordHilt;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SwordBlade;
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> SwordCore;

	/** The three cars as they fly about while combining (the real ones are hidden meanwhile). */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CarMeshes;
	/** A light where the blades bite in a clash. */
	UPROPERTY()
	TObjectPtr<class UPointLightComponent> ClashLight;
	/** The glowing shield, a disc held up in front of its left fist. */
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
	float ShieldHealth = ShieldMaxHealth;
	UPROPERTY(Replicated)
	bool bShieldBroken = false;
	/** Where the upper body turns to, relative to the legs, degrees (the ATTACK seat's aim). */
	UPROPERTY(Replicated)
	float AimYaw = 0.f;
	/** Counts the volleys of missiles fired: each change puffs at the launchers on its shoulders. */
	UPROPERTY(ReplicatedUsing = OnRep_Missiles)
	int32 MissileVolleys = 0;
	/** Counts the sword's swings (each change starts one) and which hit of the combo the last one is (1 to 3). */
	UPROPERTY(ReplicatedUsing = OnRep_Swing)
	int32 SwingCount = 0;
	UPROPERTY(Replicated)
	uint8 SwingStage = 0;
	/** Server time each attack is ready again. */
	UPROPERTY(Replicated)
	float SwordReadyTime = 0.f;
	UPROPERTY(Replicated)
	float MissileReadyTime = 0.f;
	/** Robot against robot: reeling until this server time; knocked down; the clash (the rival, when it started, every
	 * seat's row of keys, how many each got right and wrong, how it ended). */
	UPROPERTY(Replicated)
	float StaggerUntil = 0.f;
	UPROPERTY(Replicated)
	bool bDowned = false;
	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomStarRobot> ClashRival;
	UPROPERTY(Replicated)
	float ClashStartTime = 0.f;
	UPROPERTY(Replicated)
	TArray<uint8> ClashKeys;
	UPROPERTY(Replicated)
	TArray<uint8> ClashDone;
	UPROPERTY(Replicated)
	TArray<uint8> ClashMisses;
	UPROPERTY(Replicated)
	uint8 ClashOutcome = 0;
	/** The last loud moment: which, and a count that changes with each (to see the same word twice). */
	UPROPERTY(Replicated)
	ERRRobotShout ShoutWord = ERRRobotShout::None;
	UPROPERTY(ReplicatedUsing = OnRep_Shout)
	int32 ShoutCount = 0;
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

	/** Server: when the shield's hold runs out and when it starts mending; the last attack (the aim turns back to the
	 * front a while after) and the last hit taken (a bot in the DEFEND seat only raises the shield under fire); the swing
	 * under way (when it started, whether its blow has landed, whether the next of the combo is wanted). */
	float ShieldHoldUntil = 0.f;
	float ShieldMendFrom = 0.f;
	float LastAttackTime = -100.f;
	float LastHitTime = -100.f;
	float SwingStartTime = -100.f;
	bool bSwingLanded = true;
	bool bComboQueued = false;
	/** Server: when the shield last went up (a parry within ParryWindow), when the swing's blow landed (a clash with a
	 * rival blow a moment later still counts), when it gets up again if down, when the clash ended (showing how), each
	 * seat's next key allowed after a wrong one, each bot seat's next key, the rival swing a bot defender has judged. */
	float ShieldRaisedTime = -100.f;
	float SwingLandedTime = -100.f;
	float DownedUntil = 0.f;
	float ClashEndTime = 0.f;
	float ClashLockUntil[3] = { 0.f, 0.f, 0.f };
	float ClashBotNext[3] = { 0.f, 0.f, 0.f };
	int32 RivalSwingJudged = 0;
	/** Server: a bot attacker's combo, how many swings it goes for, and when it may start the next. */
	int32 BotComboGoal = 1;
	/** Server: a sword blow can't make it reel again until this time (no reeling it over and over). */
	float ReelGuardUntil = 0.f;
	float BotAttackNext = 0.f;
	/** Server, a cheat (CheatAssemble): the team's bots get in as soon as it's done. */
	bool bCheatFillCrew = false;
	/** Every machine: the combining beats already played, the moment it was seen done, the walk's phase, the shown aim. */
	int32 BeatsPlayed = 0;
	float AssembledSeenTime = -1.f;
	float WalkPhase = 0.f;
	float WalkAmount = 0.f;
	float ShownAimYaw = 0.f;
	/** Which way it faced last frame (turning on the spot steps). */
	float LastPoseYaw = 0.f;
	/** A jolt through the body as a foot comes down (1, easing to 0). */
	float StepJolt = 0.f;
	/** Jumping: its driver in the air last frame, how far into the air pose (0 to 1), a landing's crouch (1 easing to 0). */
	bool bWasAirborne = false;
	float AirAmount = 0.f;
	float LandJolt = 0.f;
	/** The swing this machine is showing: when it saw it start, which hit of the combo, the blade's tip last frame (its
	 * sweep's trail). */
	int32 LastSwingSeen = 0;
	float SwingSeenTime = -100.f;
	int32 ShownStage = 0;
	FVector LastTip = FVector::ZeroVector;
	int32 LastVolleySeen = 0;
	/** How far the shield is up (0 to 1), its health as last seen (a drop flashes it) and that flash (1 easing to 0). */
	float ShieldShown = 0.f;
	float ShieldHealthSeen = ShieldMaxHealth;
	float ShieldFlash = 0.f;
	/** The last loud moment as this machine saw it; how far into reeling and kneeling the pose is (0 to 1). */
	int32 LastShoutSeen = 0;
	float ShoutSeenTime = -100.f;
	float ReelShown = 0.f;
	float DownShown = 0.f;
};
