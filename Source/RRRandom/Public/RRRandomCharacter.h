#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Math/RandomStream.h"
#include "RRRandomWeapon.h"
#include "RRRandomCharacter.generated.h"

class ARRRandomProjectile;
class UAnimSequence;
class UCameraComponent;
class UMaterialInterface;
class USpringArmComponent;
class URRRandomEventComponent;
class URRRandomDiceBuffComponent;
class URRRandomOverheadDieComponent;
class URRRandomWingsComponent;
class URRRandomGunComponent;

/** A damage number floating over a brawler's head, drawn by the HUD. */
struct FRRDamagePopup
{
	float Amount = 0.f;
	float Age = 0.f;
	/** Sideways screen offset so numbers from quick hits don't overlap. */
	float OffsetX = 0.f;
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
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	/** Rounds left in the magazine. */
	int32 GetAmmo() const { return Ammo; }
	/** The current gun's magazine size. */
	int32 GetMaxAmmo() const { return Weapon.MagazineSize > 0 ? Weapon.MagazineSize : MaxAmmo; }
	bool IsReloading() const { return bReloading; }
	/** Seconds to reload the current gun, before attack speed buffs. */
	float GetReloadTime() const { return ReloadTime * Weapon.ReloadTimeMultiplier; }
	/** 0 to 1 while reloading. */
	float GetReloadProgress() const { return bReloading && GetReloadTime() > 0.f ? 1.f - ReloadRemaining / GetReloadTime() : 0.f; }

	const FRRWeapon& GetWeapon() const { return Weapon; }
	/** Swaps to another gun with a full magazine. */
	void EquipWeapon(const FRRWeapon& NewWeapon);
	float GetAttackRange() const { return AttackRange; }
	float GetProjectileSpeed() const;
	/** Seconds until a knocked-out brawler gets back up. */
	float GetRespawnRemaining() const { return RespawnRemaining; }

	bool IsGiant() const { return bGiant; }
	/** Seconds of giant left. */
	float GetGiantRemaining() const { return GiantRemaining; }
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
	/** Where shots appear, from the capsule center, turned with the brawler (forward, right, up). */
	const FVector& GetMuzzleOffset() const { return MuzzleOffset; }
	/** 0 facing ahead .. 1 turned into the fire animation's rifle stance, where the hands hold the gun on the line shots fly. */
	float GetFireStanceAlpha() const { return FireStanceAlpha; }

	/** How long damage numbers stay up. */
	static constexpr float DamagePopupLifetime = 0.8f;

protected:
	virtual void BeginPlay() override;

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

	/**
	 * Where shots appear relative to the capsule center: forward, right, up. The tip of the rifle's muzzle in the fox's
	 * fire stance (measured in game), so shots leave the barrel; the bots' line-of-fire sweeps use the same height and side.
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

	/** Body size while giant. Everything grows, the capsule included, so cover hides less of a giant. */
	UPROPERTY(EditAnywhere, Category = "Giant", meta = (ClampMin = "1"))
	float GiantScale = 1.5f;

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

	/** Seconds after the last shot that the firing animation keeps playing. */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float FireAnimationHold = 0.35f;

	/**
	 * The fire animation is a bladed rifle stance: the hands hold the rifle this many degrees left of where the body
	 * faces. While it plays, the body turns right by this much so the rifle in its hands points where the shots go.
	 */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float FireStanceYaw = 38.f;

	/** How fast the body turns into and out of that stance (1 = a full turn takes a second). */
	UPROPERTY(EditAnywhere, Category = "Animation")
	float FireStanceSpeed = 8.f;

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
	void AddDamagePopup(float Amount);
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
	void ServerStartReload();

	UFUNCTION(Server, Reliable)
	void ServerRollDice();

	UFUNCTION(Server, Reliable)
	void ServerStartClimb(FVector_NetQuantize Destination, FVector_NetQuantizeNormal WallFacing);

	UFUNCTION(Server, Reliable)
	void ServerStartDash(FVector_NetQuantizeNormal Direction);

	UFUNCTION(Server, Reliable)
	void ServerEndDash();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDamagePopup(float Amount);

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
	FName MeshCollisionProfile;
};
