#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Math/RandomStream.h"
#include "RRRandomWeapon.h"
#include "RRRandomAimAnimations.h"
#include "RRRandomSpecies.h"
#include "RRRandomStarRobot.h"
#include "RRRandomCharacter.generated.h"

class ARRRandomProjectile;
class ARRRandomVehicle;
class ARRRandomUltimate;
class UAnimSequence;
class UCameraComponent;
class UMaterialInterface;
class USpringArmComponent;
class URRRandomEventComponent;
class URRRandomDiceBuffComponent;
class URRRandomOverheadDieComponent;
class URRRandomWingsComponent;
class URRRandomFlightEffectsComponent;
class URRRandomGunComponent;
class URRRandomKitComponent;
class URRRandomOreCarryComponent;

/** A damage number floating over a brawler's head, drawn by the HUD. */
struct FRRDamagePopup
{
	float Amount = 0.f;
	float Age = 0.f;
	/** Sideways screen offset so numbers from quick hits don't overlap. */
	float OffsetX = 0.f;
	/** The cartoon sticker that pops on the body with it (the gun that hit), and its random tilt and nudge. */
	ERRHitEffect Effect = ERRHitEffect::Pistol;
	float Spin = 0.f;
	FVector2D Nudge = FVector2D::ZeroVector;
	/** Seconds it stays up (a skull lingers). */
	float Lifetime = 0.8f;
	/** A fox's hit from behind or out of invisibility (FRRKit::bBackstab): a bigger gold number. */
	bool bBackstab = false;
};

/**
 * Brawler seen from a fixed top-down camera, used by both the player and the bots.
 * Shoots full auto from a magazine that reloads all at once (spare ammo is unlimited), heals after staying out of combat,
 * swaps to a random gun when a weapon die comes up (back to the pistol on knockout),
 * turns giant for a while when a giant die comes up (bigger, more damage dealt, less taken),
 * flies for a while when a flight die comes up (hold jump to rise, let go to glide down), jumps and climbs onto cover,
 * and is knocked out at 0 health, getting back up at its starting spot after a delay.
 * Each species plays a little differently through its kit (URRRandomKitComponent, FRRKit): an always-on passive and a die
 * of its own; the giant die is the tiger's and the flight die the dragon's.
 * Looks like the Mixamo-rigged fox in /Game/Characters/Fox (idle, walk, fire, death); without those assets it falls back
 * to the engine's mannequin with idle and walk only.
 *
 * Networking: the server (host) owns health, ammo, the gun, dice and knockouts and replicates them.
 * The owning client predicts its fire cooldown and climbs locally, asking the server through RPCs.
 */
UCLASS()
class RRRANDOM_API ARRRandomCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARRRandomCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	URRRandomEventComponent* GetRandomEvents() const { return RandomEvents; }
	URRRandomDiceBuffComponent* GetDiceBuffs() const { return DiceBuffs; }
	/** The dice ore carried in Meteor Rush. */
	URRRandomOreCarryComponent* GetOreCarry() const { return OreCarry; }
	/** Its species' passive and own die (URRRandomKitComponent). */
	URRRandomKitComponent* GetKit() const { return Kit; }
	/** The gun in its hands. */
	URRRandomGunComponent* GetGun() const { return Gun; }
	/** Where this brawler starts and gets back up. */
	const FVector& GetHomeLocation() const { return HomeLocation; }

	/** The ultimate running (ARRRandomUltimate: each species has its own), or null. */
	ARRRandomUltimate* GetUltimate() const { return Ultimate; }
	/** The ultimate holds this brawler: it can't move, shoot, roll or be hurt meanwhile (the tiger's giant, the rabbit's hops). */
	bool IsUsingUltimate() const;
	/** Server: the ultimate is over; the brawler is back. */
	void EndUltimate(ARRRandomUltimate* Which);

	/**
	 * A fox's clone (ARRRandomFoxClones): looks and fights like its fox, run by a bot, but its shots do nothing, it vanishes
	 * at the first hit, scores nothing, holds no team spot and leaves no trace in the results.
	 */
	bool IsDecoy() const { return bDecoy; }
	/** Server, before begin play: a clone of Source (look, team, health, gun). */
	void MakeDecoyOf(const ARRRandomCharacter* Source);
	/** Server: the clone goes up in smoke. */
	void VanishDecoy();
	URRRandomOverheadDieComponent* GetOverheadDie() const { return OverheadDie; }

	/**
	 * Turns toward a target and shoots a projectile at it if the magazine has a round and the fire cooldown allows.
	 * TargetLocation is where the target's capsule center is (or would be), so shots stay level between brawlers on
	 * the same floor and angle up or down at ones on another level. Returns whether it fired.
	 * On a client this only predicts the cooldown and asks the server to shoot.
	 */
	bool FireAt(const FVector& TargetLocation);
	/** In the star robot's ATTACK seat: its other attack (the missiles) at TargetLocation; on a client, asks the server. */
	void RobotAltFireAt(const FVector& TargetLocation);
	/** In the star robot locked in a sword clash: a key pressed (0 W, 1 A, 2 S, 3 D); on a client, tells the server. */
	void RobotClashKey(uint8 Key);
	/** This machine's camera in a robot's sword clash (ARRRandomPlayerController::RRClashCam): see ClashCamAhead. */
	void SetClashCameraTuning(float Ahead, float Side, float Up, float LookRival, float FieldOfView);
	/**
	 * Where a shot at TargetLocation would set off (just ahead of the muzzle, facing the target) and which way it would fly.
	 * False when that spot is buried in a wall the brawler hugs: then it can't fire (the HUD shows it on the aim).
	 */
	bool GetShotStart(const FVector& TargetLocation, FTransform& OutSpawn) const;

	/** Throws one stored die (once the previous one has landed). On a client, asks the server to roll. */
	void RollDice();

	/** Climbs onto cover in front if its top is within reach, otherwise jumps. */
	virtual void Jump() override;
	bool IsClimbing() const { return bClimbing; }

	/** Starts refilling the magazine unless it is already full or reloading. Spare ammo is unlimited. On a client, asks the server. */
	void StartReload();

	/** Set on the server before the brawler finishes spawning; replicates and changes its body color. */
	void SetTeam(int32 NewTeam) { Team = NewTeam; }
	int32 GetTeam() const { return Team; }

	/**
	 * Which brawler this is (RRSpecies: fox, rabbit, squirrel, tiger). Server only; set before begin play when it can be
	 * (bots, the host's own brawler), otherwise the body changes on the spot (a joiner's pick arriving after its brawler).
	 */
	void SetSpecies(int32 NewSpecies);
	int32 GetSpecies() const { return Species; }
	/** The species' picture for the HUD's team cards. */
	UTexture2D* GetPortrait() const;
	static FLinearColor GetTeamColor(int32 InTeam);

	bool IsAlive() const { return bAlive; }
	/** Dancing after the match (the winners in the dance line and the MVP); the gun is put away meanwhile. */
	bool IsCelebrating() const;
	/** Hanging its head after the match (the losers, but not the MVP); the gun is put away meanwhile. */
	bool IsDespairing() const;
	/** Server, when the match ends: which dance or which slump this brawler does (taken round the list it falls in). */
	void SetCelebrationPick(int32 Pick) { CelebrationPick = static_cast<uint8>(Pick & 0xFF); }
	/**
	 * F: gets on the nearest free vehicle in reach (ARRRandomVehicle::MountRange), or off the one ridden. Asks the server
	 * when this is a client. Not while knocked out, climbing, flying or before the match starts.
	 */
	void ToggleRide();
	/** The vehicle being ridden, if any (replicated). */
	ARRRandomVehicle* GetVehicle() const { return Vehicle; }
	/** At the wheel (not in the rear seat): both hands on it, so no shooting, and the gun is put away. Not a plane's pilot. */
	bool IsDriving() const;
	/**
	 * Flying a plane (round 88, ARRRandomVehicle::IsPlane): the movement in flying mode (WASD across, jump up, Ctrl down,
	 * hovering with no input), the body sitting with the arms free, and the fire button shooting (the plane's own gun, or the
	 * pilot's); the plane takes the hits. F on the ground puts it down; in the air it bails out and glides down.
	 */
	bool IsPiloting() const;
	/** On its own feet and free to take a plane now (not riding, in the robot, giant, flying, in a cannon or an ultimate). */
	bool CanBoardPlane() const;
	/** Server: into the pilot's seat of a plane just popped in under it (ARRRandomPlaneStation). False if it can't. */
	bool BoardPlane(ARRRandomVehicle* Plane);
	/** Server: out of the plane in mid-air (or shot down): thrown up and out, gliding down till it lands. */
	void BailOut();
	/** A plane's bottom near enough the ground under it to count as landed (F puts it down then, instead of bailing out). */
	bool IsPlaneLanded() const;
	/** This machine's Ctrl: held, a plane goes down. */
	void SetPlaneDescend(bool bHeld) { bPlaneDescendHeld = bHeld; }
	/**
	 * In a human cannon (round 88, ARRRandomCannon): hidden in the barrel while it loads, then flying its arc to the pad (it
	 * may shoot meanwhile, nothing else). The controlling machine moves it along the arc; the server trusts that client
	 * meanwhile, as in a climb.
	 */
	bool IsRidingCannon() const { return CannonRide != nullptr || CannonTime >= 0.f; }
	/** Out of the barrel and on the way to the pad. */
	bool IsCannonFlying() const;
	/** Server, by the cannon: into it and off to its pad. */
	void StartCannonRide(class ARRRandomCannon* Cannon);
	/**
	 * The star robot this animal friend is combining (standing still till it is done) or sitting in, and how
	 * (replicated). F does the robot's things first: starts combining the gathered cars, gets in, gets out.
	 */
	ARRRandomStarRobot* GetRobot() const { return Robot; }
	ERRRobotSeat GetRobotSeat() const { return Robot ? RobotSeat : ERRRobotSeat::None; }
	/** In one of the robot's seats: hidden inside it, its fire button working the robot. */
	bool IsInRobot() const { return Robot && RobotSeat != ERRRobotSeat::Combiner && RobotSeat != ERRRobotSeat::None; }
	/** Driving the star robot with the whole crew in, alive and in play: its jump is this one's (Jump). */
	bool CanJumpRobot() const;
	/** On its own feet, free to run (not on a vehicle, in the robot, an ultimate, climbing or the dance line). */
	bool CanSprint() const;
	/** Running with Shift right now (URRRandomMovementComponent), for the camera, HUD and animation. */
	bool IsSprinting() const;
	/**
	 * 0..1: how far over its walking speed it runs on the ground (a sprint, as every machine sees it from the speed):
	 * the forward lean and the gun held low (URRRandomBodyAnimInstance) follow it.
	 */
	float GetSprintShare() const;
	/** The made-up "gun held low" pose's share: the sprint's, unless the species' own run (rifle already down) plays. */
	float GetSprintPoseShare() const { return RunAnimation && CurrentAnimation == RunAnimation ? 0.f : GetSprintShare(); }
	/** Server, by the robot: combining it, in a seat, or done with it (null). */
	void SetRobotSeat(ARRRandomStarRobot* NewRobot, ERRRobotSeat NewSeat);
	/** Server: off the vehicle ridden, if any (the driver getting off lets the one behind out too). */
	void LeaveVehicle() { GetOffVehicle(); }
	/** The nearest free vehicle this brawler could get on now, if any. */
	ARRRandomVehicle* FindVehicleInReach() const;
	/** While dancing, the body turns to face this point: this screen's results camera (set on each machine by its ARRRandomResultsDirector). */
	void SetCelebrationViewPoint(const FVector& ViewPoint) { CelebrationViewPoint = ViewPoint; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }

	/**
	 * A vehicle driving into this brawler at this ground velocity, said just before the vehicle's damage lands: a knockout
	 * that comes from it throws the body backward through the air (FlyBackAnimation) instead of dropping it on the spot.
	 */
	void SetRamImpact(const FVector& Velocity);
	/**
	 * Server, when the match ends: the brawler's spot in the dance line round the MVP. It gets off any vehicle and loses
	 * its powers; if there is a way there on foot (FRRGridPath) it dashes along it like a speedster, as fast as it takes to
	 * arrive within VictoryGatherTime, leaving afterimages and sparks; with no way there, or if time runs out, it is
	 * teleported. Once there it dances.
	 */
	void SetFormationSpot(const FVector& Spot);
	/** Server: the MVP, already in place at the front of the line. */
	void SetFormationLeader();
	/** Standing in the dance line (the MVP from the start, the rest once they've got there). */
	bool IsInFormation() const { return bInFormation; }
	/** Has a spot in the dance line, there yet or not. */
	bool HasFormation() const { return bHasFormation; }

	/** Walking speed with no vehicle, buff or event: what the vehicles' speeds and the car's ramming are measured against. */
	float GetBaseWalkSpeed() const { return BaseWalkSpeed; }
	/** Rounds left in the magazine. */
	int32 GetAmmo() const { return Ammo; }
	/** The current gun's magazine size. */
	int32 GetMaxAmmo() const { return Weapon.MagazineSize > 0 ? Weapon.MagazineSize : MaxAmmo; }
	/** The starting pistol's magazine size, which other guns' magazines are shares of. */
	int32 GetDefaultMagazineSize() const { return MaxAmmo; }
	bool IsReloading() const { return bReloading; }
	/** Seconds to reload the current gun, before attack speed buffs. */
	float GetReloadTime() const { return ReloadTime * Weapon.ReloadTimeMultiplier; }
	/** 0 to 1 while reloading. */
	float GetReloadProgress() const { return bReloading && GetReloadTime() > 0.f ? 1.f - ReloadRemaining / GetReloadTime() : 0.f; }

	const FRRWeapon& GetWeapon() const { return Weapon; }
	/** Swaps to another gun with a full magazine. */
	void EquipWeapon(const FRRWeapon& NewWeapon);
	/** How far it fights from with the gun it holds (the bots keep to it; a foam finger only pokes this far). */
	float GetAttackRange() const { return AttackRange * Weapon.RangeMultiplier; }
	/** How far its shots fly before fizzling out: from one corner of the map to the other (a foam finger's poke: GetAttackRange). */
	float GetShotRange() const;
	float GetVisionRadius() const { return VisionRadius; }
	/**
	 * Whether a spot is inside the team's shared sight: within VisionRadius of any of its brawlers still standing,
	 * with nothing taller than half that brawler in between (see GetSightDistance).
	 * The fog of war draws exactly this, and bots only notice opponents it lets them see.
	 * In a third-person game (no fog of war) the radius is ThirdPersonSightRange instead: what is in plain sight, not just close by.
	 */
	static bool IsSeenByTeam(const UWorld* World, int32 InTeam, const FVector& Location);
	/** IsSeenByTeam at this brawler, unless it is invisible to that team (the fox's own die: URRRandomKitComponent::IsHiddenFrom). */
	bool IsNoticedByTeam(int32 InTeam) const;
	/** Where this brawler looks from for the fog of war: half its height (the capsule's middle). */
	FVector GetEyeLocation() const;
	/**
	 * How far one can see from an eye along a flat direction, up to MaxDistance: the line runs level at eye height, so
	 * world-static things taller than half the one looking cut it short (for the fox, 96: cover, hills of 2 levels and up,
	 * cliffs, the waterfall); a 1-level hill, sand and docks don't. Shots fly at about this height too, so what hides
	 * someone also stops shots at them. Used for both the fog of war's picture and the shared sight test, so they agree.
	 */
	static float GetSightDistance(const UWorld* World, const FVector& Eye, const FVector& Direction, float MaxDistance);
	float GetProjectileSpeed() const;
	/** Seconds until a knocked-out brawler gets back up. */
	float GetRespawnRemaining() const { return RespawnRemaining; }
	/** Opponents this brawler has knocked out this match. */
	int32 GetKills() const { return Kills; }
	/** Server only: counts a knockout of an opponent. */
	void AddKill() { ++Kills; }
	/** Server only: back on its feet at its starting spot right away (the match's MVP when the match ends). */
	void ReviveNow();

	bool IsGiant() const { return bGiant; }
	/**
	 * Where flying is allowed on this map (round 65): the map's GetFlightArea (Dice Town, Dice Land, the island, the studio),
	 * the feet up to its Max.Z; false with no map (then a ceiling over where this one started).
	 */
	bool GetFlightArea(FBox& OutArea) const;
	/** Cheat (RRGod): takes no damage at all, foam finger pokes in the back included, and is put back home instead of falling out. Server only to set. */
	bool IsInvincible() const { return bInvincible; }
	void SetInvincible(bool bNewInvincible) { bInvincible = bNewInvincible; }
	/** Seconds of giant left. */
	float GetGiantRemaining() const { return GiantRemaining; }
	/** Server only: turns giant for at least this many seconds (what a giant die does). */
	void StartGiant(float Seconds);
	/** Server only: can fly for at least this many seconds (what a flight die does). */
	void StartFlight(float Seconds);
	/** Server only, a cheat (RROwnDie): what this species' own die does on this face (the tiger's giant, the dragon's flight, the rest URRRandomKitComponent). */
	void CheatOwnDie(int32 Face);
	/** Damage dealt while giant, as a factor. */
	float GetGiantDamageMultiplier() const { return GiantDamageMultiplier; }
	/** Damage taken while giant, as a factor. */
	float GetGiantDamageTakenMultiplier() const { return GiantDamageTakenMultiplier; }

	/** True while the flight power lasts: holding jump rises, letting go glides down. */
	bool CanFly() const { return bFlying; }
	/**
	 * In the air on wings: flying, or gliding down (after a flight, or the dragon's held glide; other machines only know
	 * the flight). The body takes its flying pose then (URRRandomBodyAnimInstance) instead of walking in mid-air.
	 */
	bool IsInFlight() const;
	/**
	 * In the air on its own legs (round 64): a jump, a drop, a push into the air, not flying, not flung, not riding. The
	 * body takes its jumping pose then (URRRandomBodyAnimInstance) instead of walking in mid-air.
	 */
	bool IsJumping() const;
	/** Just landed from a jump or a drop: how deep the landing's crouch is now (0 to 1, gone in JumpLandTime). */
	float GetLandSquash() const { return LandSquash; }
	/** Degrees the body leans forward in flight (eased). */
	float GetFlightLean() const { return FlightLean; }
	/** Seconds of flight left. */
	float GetFlightRemaining() const { return FlightRemaining; }
	bool IsDashing() const { return bDashing; }
	/** False once this flight's dash is spent; landing gives it back. */
	bool HasDash() const { return !bDashUsed; }

	/** Ends a held dash when jump is let go. */
	virtual void StopJumping() override;
	/** Every push goes through here; the species' kit scales it (the tiger is pushed half as far). */
	virtual void LaunchCharacter(FVector LaunchVelocity, bool bXYOverride, bool bZOverride) override;
	virtual void Landed(const FHitResult& Hit) override;
	/** A jump taken on the run carries on a little faster (JumpCarryShare), so it goes further. */
	virtual void OnJumped_Implementation() override;
	const TArray<FRRDamagePopup>& GetDamagePopups() const { return DamagePopups; }

	/** True for a moment after each shot (as long as the firing pose holds), on every machine. */
	bool IsAiming() const;
	/** Seconds since this machine last saw the brawler shoot (or poke). */
	float GetTimeSinceShot() const { return GetWorld()->GetTimeSeconds() - LastShotTime; }
	/** Where shots appear, from the capsule center, turned with the brawler (forward, right, up). */
	const FVector& GetMuzzleOffset() const { return MuzzleOffset; }
	/** 0 facing ahead .. 1 turned into the fire animation's rifle stance, where the hands hold the gun on the line shots fly. */
	float GetFireStanceAlpha() const { return FireStanceAlpha; }
	/** The aiming animations (URRRandomBodyAnimInstance), or none of them when the body has no fox assets. */
	const FRRAimAnimations& GetAimAnimations() const { return AimAnimations; }
	/** True while the body should take the aiming pose: shooting lately, standing, not dancing, and not with the foam finger. */
	bool WantsAimPose() const;
	/** 0 .. 1, how far the body is in its aiming pose this frame (from its anim instance). */
	float GetAimPoseWeight() const;

	/**
	 * Third person (a cheat, ARRRandomPlayerController::RRThirdPerson): the camera goes over the right shoulder and follows the
	 * mouse, the brawler faces where the camera looks, and aiming down the sights (holding the right button) brings the
	 * camera in close. Set on the controlling machine; the server is told so it turns the brawler the same way.
	 */
	void SetThirdPerson(bool bOn);
	bool IsThirdPerson() const { return bThirdPerson; }
	void SetAimingDownSights(bool bOn) { bAimingDownSights = bOn; }
	bool IsAimingDownSights() const { return bThirdPerson && bAimingDownSights; }
	/** 0 top-down .. 1 third person, and 0 .. 1 into the sights: how far the camera has moved (eased). */
	float GetThirdPersonBlend() const { return ThirdPersonBlend; }
	float GetSightsBlend() const { return SightsBlend; }

	/** How long damage numbers stay up. */
	static constexpr float DamagePopupLifetime = 0.8f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> TopDownCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Random")
	TObjectPtr<URRRandomEventComponent> RandomEvents;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Random")
	TObjectPtr<URRRandomDiceBuffComponent> DiceBuffs;

	UPROPERTY(VisibleAnywhere, Category = "Random")
	TObjectPtr<URRRandomOreCarryComponent> OreCarry;

	/** The species' passive and own die. */
	UPROPERTY(VisibleAnywhere, Category = "Random")
	TObjectPtr<URRRandomKitComponent> Kit;

	/** The 3D die that tumbles over the head on each roll. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Random")
	TObjectPtr<URRRandomOverheadDieComponent> OverheadDie;

	/** Glowing wings on the back while the flight power lasts. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flight")
	TObjectPtr<URRRandomWingsComponent> Wings;

	/** The shadow right under the brawler, dust on take-off and landing, wind streaks in flight: height read from above. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Flight")
	TObjectPtr<URRRandomFlightEffectsComponent> FlightEffects;

	/** The rifle held between the fox's hands (hidden on the mannequin). Every gun the dice give looks like this one for now. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<URRRandomGunComponent> Gun;

	/** 0 is the blue team (starting at the player start), 1 the red team. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Team, Category = "Team")
	int32 Team = 0;

	UPROPERTY(EditAnywhere, Category = "Health")
	float MaxHealth = 100.f;

	/** Seconds without shooting or being hit before health starts coming back. */
	UPROPERTY(EditAnywhere, Category = "Health")
	float RegenDelay = 3.f;

	/** Share of max health regained per second once regen has started. */
	UPROPERTY(EditAnywhere, Category = "Health")
	float RegenPerSecond = 0.15f;

	/** Seconds spent knocked out before getting back up at the starting spot. */
	UPROPERTY(EditAnywhere, Category = "Health")
	float RespawnDelay = 4.f;

	/** Speed the ragdoll is thrown with along the final shot. */
	UPROPERTY(EditAnywhere, Category = "Health")
	float KnockdownSpeed = 500.f;

	/** The chance for each die earned to be an ultimate die (URRRandomDiceBuffComponent::UltimateDieChance). */
	UPROPERTY(EditAnywhere, Category = "Random")
	float UltimateDieChance = 0.15f;

	/**
	 * Run over: the body flies off along the vehicle's way at this share of its speed (between FlingSpeedRange), and up at
	 * FlingLift, which lands it about when the fly-back animation hits the ground (0.9 s in).
	 */
	UPROPERTY(EditAnywhere, Category = "Health")
	float FlingSpeedShare = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Health")
	FVector2D FlingSpeedRange = FVector2D(600.f, 1300.f);
	UPROPERTY(EditAnywhere, Category = "Health")
	float FlingLift = 480.f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<ARRRandomProjectile> ProjectileClass;

	/** Rounds in one magazine of the starting pistol; other guns scale from it. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 MaxAmmo = 15;

	/** Seconds to refill the whole magazine; attack speed buffs shorten it, and some guns change it. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float ReloadTime = 1.5f;

	/** Seconds between shots while the button is held (full auto); attack speed buffs shorten it, and some guns change it. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireInterval = 0.15f;

	/**
	 * How far the bots fight from, and the foam finger's reach (FRRWeapon::RangeMultiplier). Gun shots fly on across the
	 * whole map (GetShotRange); this is their reach only where there is no map.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float AttackRange = 1300.f;

	/** How far this brawler sees through the fog of war; its whole team shares what it sees (ARRRandomFogOfWar, bots). */
	UPROPERTY(EditAnywhere, Category = "Vision", meta = (ClampMin = "0"))
	float VisionRadius = 800.f;

	/** In a third-person game: how far a brawler sees, given a clear line (there is no fog of war; this is for the bots). */
	UPROPERTY(EditAnywhere, Category = "Vision", meta = (ClampMin = "0"))
	float ThirdPersonSightRange = 3000.f;

	/**
	 * Where shots appear relative to the capsule center: forward, right, up, until the gun in the hands has been seen in
	 * the aiming pose (URRRandomGunComponent::GetAimMuzzleOffset, its actual muzzle, takes over). The tip of the rifle's
	 * muzzle in the fox's fire stance (measured in game); the bots' line-of-fire sweeps use the same height and side.
	 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FVector MuzzleOffset = FVector(70.f, 12.f, 2.f);

	/** Highest ledge, above the feet, the brawler can climb onto. */
	UPROPERTY(EditAnywhere, Category = "Climb")
	float MaxClimbHeight = 200.f;

	/** How far in front of the capsule a wall can be and still be climbed. */
	UPROPERTY(EditAnywhere, Category = "Climb")
	float ClimbReach = 60.f;

	/** Seconds the climb takes: up first, then over the edge. */
	UPROPERTY(EditAnywhere, Category = "Climb")
	float ClimbDuration = 0.45f;

	/** Body size while giant. Everything grows, the capsule included, so cover hides less of a giant. At 2 the eye (capsule
	 * middle, 96 x 2 = 192) clears the 160 cover and two-level hills, so a giant sees over them (see GetEyeLocation). */
	UPROPERTY(EditAnywhere, Category = "Giant", meta = (ClampMin = "1"))
	float GiantScale = 2.f;

	/** Damage dealt while giant, as a factor (power). Stacks with attack power buffs and the gun. */
	UPROPERTY(EditAnywhere, Category = "Giant", meta = (ClampMin = "0"))
	float GiantDamageMultiplier = 1.5f;

	/** Damage taken while giant, as a factor (defense): 0.5 halves every hit. */
	UPROPERTY(EditAnywhere, Category = "Giant", meta = (ClampMin = "0"))
	float GiantDamageTakenMultiplier = 0.5f;

	/** Seconds of giant from a giant die: this plus GiantSecondsPerPip for each pip on it. */
	UPROPERTY(EditAnywhere, Category = "Giant")
	float GiantBaseDuration = 6.f;

	UPROPERTY(EditAnywhere, Category = "Giant")
	float GiantSecondsPerPip = 1.f;

	/** Seconds to grow to giant size or shrink back. */
	UPROPERTY(EditAnywhere, Category = "Giant")
	float GiantGrowTime = 0.3f;

	/** Upward speed while holding jump with the flight power. */
	UPROPERTY(EditAnywhere, Category = "Flight", meta = (ClampMin = "0"))
	float FlightRiseSpeed = 900.f;


	/**
	 * While flying, a second jump press within this many seconds starts a dash that lasts as long as that press is held.
	 * One dash per flight: after letting go, the next one comes after landing.
	 */
	UPROPERTY(EditAnywhere, Category = "Flight|Dash", meta = (ClampMin = "0"))
	float DashDoubleTapTime = 0.3f;

	/** Speed of the dash, level, toward where the brawler is steering (or facing). */
	UPROPERTY(EditAnywhere, Category = "Flight|Dash", meta = (ClampMin = "0"))
	float DashSpeed = 1200.f;

	/** Steering in the air while flying or gliding. */
	UPROPERTY(EditAnywhere, Category = "Flight", meta = (ClampMin = "0", ClampMax = "1"))
	float FlightAirControl = 0.8f;

	/** Seconds of flight from a flight die: this plus FlightSecondsPerPip for each pip on it. */
	UPROPERTY(EditAnywhere, Category = "Flight")
	float FlightBaseDuration = 6.f;

	UPROPERTY(EditAnywhere, Category = "Flight")
	float FlightSecondsPerPip = 1.f;

	/** Shots never tilt more than this many degrees up or down. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float MaxShotPitch = 50.f;

	/** The same in third person, where the player aims up and down at the crosshair. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	float ThirdPersonMaxShotPitch = 80.f;

	/** How early, in seconds, the server still accepts a client's shot; covers network jitter. */
	UPROPERTY(EditAnywhere, Category = "Network")
	float FireTimeSlack = 0.05f;

	/** Seconds after a client's climb ends during which the server keeps trusting its position. */
	UPROPERTY(EditAnywhere, Category = "Network")
	float ClimbTrustMargin = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> WalkAnimation;

	/** Loops while standing and shooting; walking while shooting keeps the walk. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> FireAnimation;

	/** Played once on knockout, holding its last frame until getting back up. Without one the body goes ragdoll. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> DeathAnimation;

	/** Played once when a vehicle's knockout throws the body (SetRamImpact), holding its last frame. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> FlyBackAnimation;

	/** Loops once the match is over, for the winning team and the MVP (the gun is put away meanwhile); each brawler gets one
	 * of the dances (CelebrationPick). The first is VictoryAnimation, kept for the code that checks for any dance. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> VictoryAnimation;
	UPROPERTY(EditAnywhere, Category = "Animation")
	TArray<TObjectPtr<UAnimSequence>> VictoryAnimations;

	/** Loops once the match is over, for the losing team: one of these slumps each (CelebrationPick). */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TArray<TObjectPtr<UAnimSequence>> DefeatAnimations;

	/** Seconds after the last shot that the aiming pose holds (and the brawler keeps facing where it shot instead of where it walks). */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float FireAnimationHold = 0.6f;

	/** Mixamo rifle aiming set: standing aim, and the aimed run forward, backward, left and right (in place). */
	UPROPERTY(EditAnywhere, Category = "Animation")
	FRRAimAnimations AimAnimations;

	/**
	 * The fire animation is a bladed rifle stance: the hands hold the rifle this many degrees left of where the body
	 * faces. While it plays, the body turns right by this much so the rifle in its hands points where the shots go.
	 */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float FireStanceYaw = 38.f;

	/** How fast the body turns into and out of that stance (1 = a full turn takes a second). */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float FireStanceSpeed = 8.f;

	/** How fast a dancer's body turns to face the results camera, in degrees a second. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float CelebrationTurnRate = 360.f;

	/** Skeletal-mesh-ready material with a "DiffuseColor" parameter for random events; the mannequin's own material has a fixed color. */
	UPROPERTY(EditAnywhere, Category = "Body")
	TObjectPtr<UMaterialInterface> BodyBaseMaterial;

	/**
	 * How much the team color tints a textured body ("TintStrength"). Off by default: the HUD's friend-or-foe marker
	 * over the head tells the teams apart, and the tint muddied the fur.
	 */
	UPROPERTY(EditAnywhere, Category = "Body", meta = (ClampMin = "0", ClampMax = "1"))
	float BodyTeamTint = 0.f;


	/** Ground speed at which the walk animation plays at its normal rate. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float WalkAnimationSpeed = 200.f;
	/** Sprinting: the species' run (FRRSpeciesLook::Run) and the ground speed it plays at its normal rate. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> RunAnimation;
	float RunAnimationSpeed = 300.f;

private:
	void UpdateLocomotionAnimation();
	void UpdateWalkSpeed();
	void UpdateAmmoAndHealth(float DeltaSeconds);
	void UpdateDamagePopups(float DeltaSeconds);
	void AddDamagePopup(float Amount, ERRHitEffect Effect, bool bBackstab);
	/** Seconds between shots with the current gun and attack speed buffs. */
	float GetFireInterval() const;
	/** Server side of FireAt. Slack lets a client's shot arrive a little before the server's cooldown ends. */
	bool ShootAt(const FVector& TargetLocation, float CooldownSlack);
	/** Turns the body toward the target and returns that flat direction. */
	FVector FaceToward(const FVector& TargetLocation);
	/** Server only: drops the brawler, scores for the other team and starts the respawn countdown. bFell: off the arena. */
	void KnockOut(const FVector& ShotDirection, ARRRandomCharacter* Attacker, bool bFell = false);
	/** Ragdoll and no movement, on every machine. */
	void ApplyKnockedOutBody();
	/** Back on its feet with movement and collision, on every machine. */
	void ApplyStandingBody();
	void ApplyTeamColor();
	void OnWeaponDie(int32 Face);
	void OnGiantDie(int32 Face);
	/** Grows toward giant size or shrinks back, on every machine. */
	void UpdateGiantSize(float DeltaSeconds);
	/** The glowing effects of the powers (ARRRandomEffects) on every machine: bursts as they start and end, auras and trails meanwhile. */
	void UpdatePowerEffects(float DeltaSeconds);
	/** Fades the body's flash after a hit. */
	void UpdateHitFlash(float DeltaSeconds);
	/** After the match: walks to the spot in the dance line, or is teleported there if it's too far (see SetFormationSpot). */
	void UpdateFormation(float DeltaSeconds);
	/** Ends giant and flight at once (for the dance line). */
	void EndPowers();

	/** Every machine, while dashing to the dance line: afterimages of the body, streaks and sparks; a boom as it sets off
	 * and as it stops. */
	void UpdateDashEffects(float DeltaSeconds);
	/** Dashing to the dance line right now (on every machine, from the replicated route). */
	bool IsDashingToFormation() const { return bHasFormation && !bInFormation && FormationDashSpeed > 0.f; }

	/** Every machine: a puff of light where a brawler vanishes on its way to the dance line and where it appears. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFormationTeleport(const FVector_NetQuantize& From, const FVector_NetQuantize& To);
	/** Scales the whole brawler, keeping its feet where they are. */
	void SetBodyScale(float NewScale);
	void OnFlightDie(int32 Face);
	void OnOwnDie(int32 Face);
	/** The species' passive numbers (health, jumps) and dice chances on this brawler; every machine. */
	void ApplyKit();
	/** The rabbit's super hop in mid-air: on the controlling machine (predicted) and the server. */
	bool TrySuperHop();
	void DoSuperHop();

	UFUNCTION(Server, Reliable)
	void ServerSuperHop();

	/** The dragon's glide: whether jump is held, told to the server by a remote player (the engine's own flag lets go early). */
	UFUNCTION(Server, Reliable)
	void ServerSetJumpHeld(bool bHeld);
	void SetJumpHeld(bool bHeld);
	bool bJumpHeld = false;

	UPROPERTY(Replicated)
	bool bDecoy = false;
	UFUNCTION(NetMulticast, Reliable)
	void MulticastDecoyPoof();
	/** Server: an ultimate die landed; the tiger calls its spirit for Face strikes. */
	void OnUltimateDie(int32 Face);

	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomUltimate> Ultimate;
	/** Jump and glide settings for the flight power, on every machine so movement prediction agrees. */
	void UpdateFlight();
	/** Starts a dash if flying and this flight's dash is unused. Only where the brawler is controlled; tells the server when that is a client. */
	bool TryDash();
	void StartDash(const FVector& Direction);
	/** Holds the dash speed level, steering with the movement input. */
	void UpdateDash();
	/** Slows to air speed and goes back to gliding. On a client, tells the server. */
	void EndDash();
	/** Starts climbing if cover is in front with a reachable top and room to stand there. Only where the brawler is controlled. */
	bool TryClimb();
	void StartClimb(const FVector& Destination, const FVector& WallFacing);
	void UpdateClimb(float DeltaSeconds);
	/**
	 * Server only: lets the owning client move the capsule itself. Character movement doesn't predict the climb,
	 * so without this the server would keep snapping a climbing client back down.
	 */
	void SetTrustClientMovement(bool bTrust);
	/** Lets brawlers step off the top of cover, but never off the edge of the arena floor. */
	void UpdateLedgeWalking();
	void Respawn();

	UFUNCTION(Server, Reliable)
	void ServerFireAt(FVector_NetQuantize TargetLocation);

	UFUNCTION(Server, Reliable)
	void ServerRobotAltFireAt(FVector_NetQuantize TargetLocation);

	UFUNCTION(Server, Reliable)
	void ServerRobotClashKey(uint8 Key);

	UFUNCTION(Server, Reliable)
	void ServerSetThirdPerson(bool bOn);

	UFUNCTION(Server, Reliable)
	void ServerStartReload();

	UFUNCTION(Server, Reliable)
	void ServerRollDice();

	UFUNCTION(Server, Reliable)
	void ServerStartClimb(FVector_NetQuantize Destination, FVector_NetQuantizeNormal WallFacing);

	UFUNCTION(Server, Reliable)
	void ServerStartDash(FVector_NetQuantizeNormal Direction);

	UFUNCTION(Server, Reliable)
	void ServerEndDash();

	UFUNCTION(Server, Reliable)
	void ServerToggleRide();

	/** Server only: puts down the vehicle being ridden (or leaves its rear seat), if any. The robot does it too. */
	void GetOffVehicle();
	friend class ARRRandomStarRobot;
	/** Hidden inside the robot, carried by it (not the driver, whose capsule carries the robot) or back out; every machine. */
	void ApplyRobotSeat();

	UFUNCTION()
	void OnRep_Robot();
	/** Seats the brawler in its vehicle's rear seat (no movement of its own, attached) or takes it out; every machine. */
	void ApplyRide();

	UFUNCTION()
	void OnRep_Ride();
	/** The ridden vehicle's handling (or walking's) on the movement, and the rider up on its deck or seat; every machine. */
	void UpdateVehicleHandling();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDamagePopup(float Amount, ERRHitEffect Effect, bool bBackstab);
	/** A plane shows the hits on its hull over its pilot (MulticastDamagePopup) and throws it out (BailOut). */
	friend class ARRRandomVehicle;

	/** The human cannon this one is in (replicated), and this machine's seconds since it climbed in (-1: not in one). */
	UPROPERTY(ReplicatedUsing = OnRep_CannonRide)
	TObjectPtr<class ARRRandomCannon> CannonRide;
	float CannonTime = -1.f;
	/** This machine: out of the barrel yet, and knocked off the arc (into a wall) and falling. */
	bool bCannonFired = false;
	bool bCannonOffArc = false;
	/** Server: seconds since the ride began (it ends a little after the flight is over, for a remote flier). */
	float CannonServerTime = 0.f;
	UFUNCTION()
	void OnRep_CannonRide();
	/** Every machine: into the barrel (hidden; where it is controlled, held at the muzzle with movement off). */
	void BeginCannonRide();
	/** Every machine, every frame of the ride: the load, the shot, the arc (moved where it is controlled), the landing. */
	void UpdateCannonRide(float DeltaSeconds);
	/** Every machine: the ride is over (landed on the pad: the thud). */
	void EndCannonRide(bool bLanded);

	/** Bailed out of a plane: gliding down till it lands (replicated, so the client's movement glides too). */
	UPROPERTY(Replicated)
	bool bBailedOut = false;
	/** This machine, piloting: jump (up) and Ctrl (down) held, and when it took off (it climbs by itself at first). */
	bool bPlaneRiseHeld = false;
	bool bPlaneDescendHeld = false;
	float PlaneBoardTime = -100.f;
	/** The plane last seen flown (a new one restarts the take-off). */
	TWeakObjectPtr<ARRRandomVehicle> PilotedPlane;
	/** Piloting, every frame: up and down, facing the way it flies, kept in the flight area (where it is controlled). */
	void UpdatePlaneFlight(float DeltaSeconds);
	/** Server: the plane's own gun at the target, if its cooldown allows; which of its two muzzles goes next. */
	bool ShootPlaneGun(const FVector& TargetLocation, float CooldownSlack);
	int32 PlaneGunSide = 0;

	UFUNCTION()
	void OnRep_Team();

	UFUNCTION()
	void OnRep_Species();

	UFUNCTION()
	void OnRep_Alive();

	UFUNCTION()
	void OnRep_ShotCount();

	/** Plays one animation on the body, from the start, at the given rate. */
	void PlayBodyAnimation(UAnimSequence* Animation, bool bLoop, float PlayRate = 1.f);

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentAnimation;

	/** Every species' body and animations (FRRSpeciesLook::GetAll), so they stay loaded. */
	UPROPERTY()
	TArray<FRRSpeciesLook> SpeciesLooks;

	UPROPERTY(ReplicatedUsing = OnRep_Species)
	uint8 Species = 0;

	/** The species the body shows. */
	int32 ShownSpecies = 0;

	/** Takes a species' mesh, material and animations (the mesh component's materials are set at begin play). */
	void UseLook(const FRRSpeciesLook& Look);
	/** Shows Species, if it isn't shown yet; after begin play also renews the body's materials and pose. */
	void ApplySpecies();

	/** 0 facing ahead .. 1 turned into the fire animation's bladed stance (FireStanceYaw). */
	float FireStanceAlpha = 0.f;

	/** Where the dancing body turns to face (see SetCelebrationViewPoint), and how far it has turned from the brawler's facing, in degrees. */
	TOptional<FVector> CelebrationViewPoint;
	float CelebrationTurn = 0.f;


	/** Counts shots so every machine can play the firing animation; only the change matters. */
	UPROPERTY(ReplicatedUsing = OnRep_ShotCount)
	uint8 ShotCount = 0;

	/** When this machine last saw the brawler shoot. */
	float LastShotTime = -UE_BIG_NUMBER;

	TArray<FRRDamagePopup> DamagePopups;

	UPROPERTY(Replicated)
	float Health = 0.f;

	UPROPERTY(Replicated)
	FRRWeapon Weapon;

	FRandomStream WeaponStream;
	bool bClimbing = false;
	float ClimbElapsed = 0.f;
	FVector ClimbStart = FVector::ZeroVector;
	FVector ClimbTarget = FVector::ZeroVector;
	/** Server: seconds left trusting the owning client's position after its climb. */
	float ClientTrustRemaining = 0.f;
	/** When jump was last pressed; a jump that reaches a wall shortly after still climbs it. */
	float JumpPressedTime = -UE_BIG_NUMBER;

	UPROPERTY(Replicated)
	int32 Ammo = 0;

	UPROPERTY(Replicated)
	bool bReloading = false;

	/** Sent to the owner only, who shows the reload progress. */
	UPROPERTY(Replicated)
	float ReloadRemaining = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Alive)
	bool bAlive = true;

	UPROPERTY(Replicated)
	int32 Kills = 0;

	/** Which way the final shot threw the body, so every machine drops the ragdoll the same way. */
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal KnockdownDirection = FVector::ForwardVector;

	/** Jumping (every machine, from the movement it sees): in the air last frame, the fall's speed then, the landing's crouch. */
	void UpdateJumpLanding(float DeltaSeconds);
	bool bWasJumping = false;
	float JumpFallSpeed = 0.f;
	float LandSquash = 0.f;

	/** The map whose flight area this one keeps to (GetFlightArea) and shots cross (GetShotRange), looked for until found. */
	mutable TWeakObjectPtr<AActor> FlightMap;
	const AActor* FindMap() const;

	/** Set on a knockout by a vehicle: the velocity the body was thrown with (zero for any other knockout). */
	UPROPERTY(Replicated)
	FVector_NetQuantize10 FlingVelocity = FVector::ZeroVector;

	/** The last vehicle impact (SetRamImpact) and when; a knockout in the same moment is a fling. */
	FVector RamImpact = FVector::ZeroVector;
	double RamImpactTime = -1.0;
	/** In the air after a fling (until it lands, or gets back up), and for how long. */
	bool bFlinging = false;
	float FlingTime = 0.f;
	FVector FlingStart = FVector::ZeroVector;
	void UpdateFling(float DeltaSeconds);
	void EndFling();
	/** How the capsule met pawns before a fling made it pass through them. */
	ECollisionResponse CapsulePawnResponse = ECR_Block;

	/** Sent to the owner only, who shows the countdown. */
	UPROPERTY(Replicated)
	float RespawnRemaining = 0.f;

	/** Every machine grows the body while this is set. */
	UPROPERTY(Replicated)
	bool bGiant = false;

	UPROPERTY(Replicated)
	float GiantRemaining = 0.f;

	/** Every machine switches the jump to flight while this is set. */
	UPROPERTY(Replicated)
	bool bFlying = false;

	UPROPERTY(Replicated)
	float FlightRemaining = 0.f;

	/** The invincibility cheat; replicated so every screen can show it. */
	UPROPERTY(Replicated)
	bool bInvincible = false;

	/** Every machine rides with this vehicle's handling while it is set; in its rear seat (carried) if bPassenger. */
	UPROPERTY(ReplicatedUsing = OnRep_Ride)
	TObjectPtr<ARRRandomVehicle> Vehicle;

	UPROPERTY(ReplicatedUsing = OnRep_Ride)
	bool bPassenger = false;

	/** Whether this machine has seated the brawler as a passenger (ApplyRide). */
	bool bSeatedAsPassenger = false;

	UPROPERTY(ReplicatedUsing = OnRep_Robot)
	TObjectPtr<ARRRandomStarRobot> Robot;

	UPROPERTY(ReplicatedUsing = OnRep_Robot)
	ERRRobotSeat RobotSeat = ERRRobotSeat::None;

	/** What this machine has done for the robot seat (ApplyRobotSeat): hidden inside it, carried by it. */
	bool bInsideRobot = false;
	bool bCarriedByRobot = false;
	/** The camera pulled back for the robot's size, 0 to 1. */
	float RobotCameraPull = 0.f;
	/** Driving the robot just combined: whether it has turned round yet (ARRRandomStarRobot::GetIntroTurn). */
	bool bRobotIntroTurned = false;
	/** Where it is controlled: turns the just-combined robot round, unless driven off first. */
	void UpdateRobotIntroTurn(float DeltaSeconds);
	/** The camera on the robot being combined, 0 to 1, and where that robot stands. */
	float CombineCameraBlend = 0.f;
	FVector CombineFocus = FVector::ZeroVector;
	/** In a sword clash in the robot: how far the camera has closed in on the blades (0 to 1), on what, from which side,
	 * how far into the clash. */
	float ClashCameraBlend = 0.f;
	FVector ClashFocus = FVector::ZeroVector;
	FVector ClashEye = FVector::ZeroVector;
	float ClashCameraTime = 0.f;
	/** Where the clash camera sits, in the robot's own frame turned to the rival (ahead, right, up from its feet), how
	 * far on from the crossed blades to the rival's head it looks (0 to 1), and its field of view. */
	float ClashCamAhead = -200.f;
	float ClashCamSide = 300.f;
	float ClashCamUp = 720.f;
	float ClashCamLookRival = 0.45f;
	float ClashCamFieldOfView = 68.f;

	/** Walking's movement settings, put back on getting off. */
	float BaseMaxAcceleration = 0.f;
	float BaseGroundFriction = 0.f;
	float BaseBrakingDeceleration = 0.f;
	float BaseTurnRate = 0.f;

	bool bDashing = false;
	/** Set when a dash starts, cleared on landing: one dash per flight. */
	bool bDashUsed = false;
	FVector DashDirection = FVector::ForwardVector;

	float BaseJumpZVelocity = 0.f;
	float BaseJumpMaxHoldTime = 0.f;
	float BaseAirControl = 0.f;
	/** MaxHealth before the species' kit scales it. */
	float BaseMaxHealth = 0.f;
	/** Still gliding down from a flight that ran out (until landing), apart from the dragon's held glide. */
	bool bFlightGlide = false;

	float LastCombatTime = -UE_BIG_NUMBER;
	float NextFireTime = 0.f;
	/** In the robot's ATTACK seat: when this client may next ask for the missiles. */
	float NextAltFireTime = 0.f;
	float BaseWalkSpeed = 0.f;

	FVector HomeLocation = FVector::ZeroVector;
	FRotator HomeRotation = FRotator::ZeroRotator;
	FVector MeshRelativeLocation = FVector::ZeroVector;
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;

	/**
	 * Riding: where the body goes from its own place (the capsule's middle, put on the seat's spot by the vehicle), eased in
	 * with the ride pose. Sitting, the hips bone over the seat's spot and RideHipsClear of the hips height above its cushion;
	 * standing (the kickboard), the feet on the deck, sunk by the bent knees. Plus the species' RideFit.
	 */
	FVector GetRideOffset() const;
	/** The species' rest hips height and riding nudge (FRRSpeciesLook). */
	float RideHipsHeight = 43.f;
	FVector RideFit = FVector::ZeroVector;
	/** Degrees the body leans forward in flight (eased), and how far the camera has pulled back for it (0 .. 1). */
	float FlightLean = 0.f;
	/** Degrees the body banks into turns in flight (eased), and the way it flew last frame. */
	float FlightBank = 0.f;
	float LastFlightYaw = 0.f;
	/** Third person: on (replicated, so the server turns the brawler with the camera too), the sights, and the camera's eased blends. */
	UPROPERTY(Replicated)
	bool bThirdPerson = false;
	bool bAimingDownSights = false;

	/** The dance line after the match: given a spot (and where it is), standing in it, and seconds since it set off. */
	/** Which dance (or slump) this brawler does after the match, picked by the server so every machine shows the same. */
	UPROPERTY(Replicated)
	uint8 CelebrationPick = 0;
	UPROPERTY(Replicated)
	bool bHasFormation = false;
	UPROPERTY(Replicated)
	bool bInFormation = false;
	UPROPERTY(Replicated)
	FVector FormationSpot = FVector::ZeroVector;
	/** The way there on foot (corners, the last one the spot) and the speed that makes it in time; 0 with no way there. */
	UPROPERTY(Replicated)
	TArray<FVector_NetQuantize> FormationPath;
	UPROPERTY(Replicated)
	float FormationDashSpeed = 0.f;
	float GatherTime = 0.f;
	/** Each machine: the corner of the route being run to, and whether the dash's start boom has played. */
	int32 FormationPathIndex = 0;
	bool bDashShown = false;
	/** Afterimages of the dashing body (pose copies in a glowing see-through material), and the time to the next one. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UPoseableMeshComponent>> Afterimages;
	TArray<float> AfterimageAges;
	int32 NextAfterimage = 0;
	float AfterimageTimer = 0.f;

	/** The powers as last seen by the effects, to burst when they change (on every machine, from the replicated state). */
	bool bWasGiant = false;
	bool bWasFlying = false;
	/** The body's flash after a hit: 1 just hit, fading to 0; and whether the material still shows some. */
	float HitFlash = 0.f;
	bool bHitFlashShown = false;
	float ThirdPersonBlend = 0.f;
	float SightsBlend = 0.f;
	void UpdateCamera(float DeltaSeconds);
	/** The camera eased over to the sword clash (ClashCameraBlend of the way), from wherever UpdateCamera put it. */
	void ApplyClashCamera();
	void ApplyThirdPersonRotation();
	float FlightCameraPull = 0.f;
	/** 0..1, eased: my own sprint, for the over-the-shoulder camera's pull back. */
	float SprintCameraBlend = 0.f;
	float BaseCameraArmLength = 0.f;
	FName MeshCollisionProfile;
};
