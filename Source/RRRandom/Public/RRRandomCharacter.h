#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Math/RandomStream.h"
#include "RRRandomWeapon.h"
#include "RRRandomAimAnimations.h"
#include "RRRandomCharacter.generated.h"

class ARRRandomProjectile;
class ARRRandomVehicle;
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
};

/**
 * Brawler seen from a fixed top-down camera, used by both the player and the bots.
 * Shoots full auto from a magazine that reloads all at once (spare ammo is unlimited), heals after staying out of combat,
 * swaps to a random gun when a weapon die comes up (back to the pistol on knockout),
 * turns giant for a while when a giant die comes up (bigger, more damage dealt, less taken),
 * flies for a while when a flight die comes up (hold jump to rise, let go to glide down), jumps and climbs onto cover,
 * and is knocked out at 0 health, getting back up at its starting spot after a delay.
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
	URRRandomOverheadDieComponent* GetOverheadDie() const { return OverheadDie; }

	/**
	 * Turns toward a target and shoots a projectile at it if the magazine has a round and the fire cooldown allows.
	 * TargetLocation is where the target's capsule center is (or would be), so shots stay level between brawlers on
	 * the same floor and angle up or down at ones on another level. Returns whether it fired.
	 * On a client this only predicts the cooldown and asks the server to shoot.
	 */
	bool FireAt(const FVector& TargetLocation);

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
	static FLinearColor GetTeamColor(int32 InTeam);

	bool IsAlive() const { return bAlive; }
	/** Dancing the victory animation after the match (the winners and the MVP); the gun is put away meanwhile. */
	bool IsCelebrating() const;
	/**
	 * F: gets on the nearest free vehicle in reach (ARRRandomVehicle::MountRange), or off the one ridden. Asks the server
	 * when this is a client. Not while knocked out, climbing, flying or before the match starts.
	 */
	void ToggleRide();
	/** The vehicle being ridden, if any (replicated). */
	ARRRandomVehicle* GetVehicle() const { return Vehicle; }
	/** The nearest free vehicle this brawler could get on now, if any. */
	ARRRandomVehicle* FindVehicleInReach() const;
	/** While dancing, the body turns to face this point: this screen's results camera (set on each machine by its ARRRandomResultsDirector). */
	void SetCelebrationViewPoint(const FVector& ViewPoint) { CelebrationViewPoint = ViewPoint; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
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
	/** How far its shots reach with the gun it holds (a foam finger only pokes). */
	float GetAttackRange() const { return AttackRange * Weapon.RangeMultiplier; }
	float GetVisionRadius() const { return VisionRadius; }
	/**
	 * Whether a spot is inside the team's shared sight: within VisionRadius of any of its brawlers still standing,
	 * with nothing taller than half that brawler in between (see GetSightDistance).
	 * The fog of war draws exactly this, and bots only notice opponents it lets them see.
	 * In a third-person game (no fog of war) the radius is ThirdPersonSightRange instead: what is in plain sight, not just close by.
	 */
	static bool IsSeenByTeam(const UWorld* World, int32 InTeam, const FVector& Location);
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
	/** Cheat (RRGod): takes no damage at all, foam finger pokes in the back included, and is put back home instead of falling out. Server only to set. */
	bool IsInvincible() const { return bInvincible; }
	void SetInvincible(bool bNewInvincible) { bInvincible = bNewInvincible; }
	/** Seconds of giant left. */
	float GetGiantRemaining() const { return GiantRemaining; }
	/** Server only: turns giant for at least this many seconds (what a giant die does). */
	void StartGiant(float Seconds);
	/** Server only: can fly for at least this many seconds (what a flight die does). */
	void StartFlight(float Seconds);
	/** Damage dealt while giant, as a factor. */
	float GetGiantDamageMultiplier() const { return GiantDamageMultiplier; }
	/** Damage taken while giant, as a factor. */
	float GetGiantDamageTakenMultiplier() const { return GiantDamageTakenMultiplier; }

	/** True while the flight power lasts: holding jump rises, letting go glides down. */
	bool CanFly() const { return bFlying; }
	/** Seconds of flight left. */
	float GetFlightRemaining() const { return FlightRemaining; }
	bool IsDashing() const { return bDashing; }
	/** False once this flight's dash is spent; landing gives it back. */
	bool HasDash() const { return !bDashUsed; }

	/** Ends a held dash when jump is let go. */
	virtual void StopJumping() override;
	virtual void Landed(const FHitResult& Hit) override;
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

	/** How far shots fly before disappearing. */
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

	/** Longest a held jump keeps rising with the flight power; with FlightRiseSpeed this sets how high one lift goes (about 700). */
	UPROPERTY(EditAnywhere, Category = "Flight", meta = (ClampMin = "0"))
	float FlightMaxHoldTime = 0.75f;

	/** Hard ceiling for flying: the feet never go higher above the arena floor than this many body heights (normal size). */
	UPROPERTY(EditAnywhere, Category = "Flight", meta = (ClampMin = "0"))
	float FlightMaxHeightInBodies = 5.f;

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

	/** Loops once the match is over, for the winning team and the MVP (the gun is put away meanwhile). */
	UPROPERTY(EditAnywhere, Category = "Animation")
	TObjectPtr<UAnimSequence> VictoryAnimation;

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

private:
	void UpdateLocomotionAnimation();
	void UpdateWalkSpeed();
	void UpdateAmmoAndHealth(float DeltaSeconds);
	void UpdateDamagePopups(float DeltaSeconds);
	void AddDamagePopup(float Amount, ERRHitEffect Effect);
	/** Seconds between shots with the current gun and attack speed buffs. */
	float GetFireInterval() const;
	/** Server side of FireAt. Slack lets a client's shot arrive a little before the server's cooldown ends. */
	bool ShootAt(const FVector& TargetLocation, float CooldownSlack);
	/** Turns the body toward the target and returns that flat direction. */
	FVector FaceToward(const FVector& TargetLocation);
	/** Server only: drops the brawler, scores for the other team and starts the respawn countdown. */
	void KnockOut(const FVector& ShotDirection, ARRRandomCharacter* Attacker);
	/** Ragdoll and no movement, on every machine. */
	void ApplyKnockedOutBody();
	/** Back on its feet with movement and collision, on every machine. */
	void ApplyStandingBody();
	void ApplyTeamColor();
	void OnWeaponDie(int32 Face);
	void OnGiantDie(int32 Face);
	/** Grows toward giant size or shrinks back, on every machine. */
	void UpdateGiantSize(float DeltaSeconds);
	/** Scales the whole brawler, keeping its feet where they are. */
	void SetBodyScale(float NewScale);
	void OnFlightDie(int32 Face);
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

	/** Server only: puts down the vehicle being ridden (or leaves its rear seat), if any. */
	void GetOffVehicle();
	/** Seats the brawler in its vehicle's rear seat (no movement of its own, attached) or takes it out; every machine. */
	void ApplyRide();

	UFUNCTION()
	void OnRep_Ride();
	/** The ridden vehicle's handling (or walking's) on the movement, and the rider up on its deck or seat; every machine. */
	void UpdateVehicleHandling();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDamagePopup(float Amount, ERRHitEffect Effect);

	UFUNCTION()
	void OnRep_Team();

	UFUNCTION()
	void OnRep_Alive();

	UFUNCTION()
	void OnRep_ShotCount();

	/** Plays one animation on the body, from the start, at the given rate. */
	void PlayBodyAnimation(UAnimSequence* Animation, bool bLoop, float PlayRate = 1.f);

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentAnimation;

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

	float LastCombatTime = -UE_BIG_NUMBER;
	float NextFireTime = 0.f;
	float BaseWalkSpeed = 0.f;

	FVector HomeLocation = FVector::ZeroVector;
	FRotator HomeRotation = FRotator::ZeroRotator;
	FVector MeshRelativeLocation = FVector::ZeroVector;
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;
	/** Degrees the body leans forward in flight (eased), and how far the camera has pulled back for it (0 .. 1). */
	float FlightLean = 0.f;
	/** Third person: on (replicated, so the server turns the brawler with the camera too), the sights, and the camera's eased blends. */
	UPROPERTY(Replicated)
	bool bThirdPerson = false;
	bool bAimingDownSights = false;
	float ThirdPersonBlend = 0.f;
	float SightsBlend = 0.f;
	void UpdateCamera(float DeltaSeconds);
	void ApplyThirdPersonRotation();
	float FlightCameraPull = 0.f;
	float BaseCameraArmLength = 0.f;
	FName MeshCollisionProfile;
};
