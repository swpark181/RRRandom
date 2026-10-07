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
 * swaps to a random gun when a weapon die comes up (back to the pistol on knockout), jumps and climbs onto cover,
 * and is knocked out at 0 health, getting back up at its starting spot after a delay.
 * Uses the engine's mannequin so it needs no project assets.
 *
 * Networking: the server (host) owns health, ammo, the gun, dice and knockouts and replicates them.
 * The owning client predicts its fire cooldown and climbs locally, asking the server through RPCs.
 */
UCLASS()
class RRRANDOM_API ARRRandomCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ARRRandomCharacter();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	URRRandomEventComponent* GetRandomEvents() const { return RandomEvents; }
	URRRandomDiceBuffComponent* GetDiceBuffs() const { return DiceBuffs; }

	/**
	 * Turns toward a target and shoots a projectile at it if the magazine has a round and the fire cooldown allows.
	 * TargetLocation is where the target's capsule center is (or would be), so shots stay level between brawlers on
	 * the same floor and angle up or down at ones on another level. Returns whether it fired.
	 * On a client this only predicts the cooldown and asks the server to shoot.
	 */
	bool FireAt(const FVector& TargetLocation);

	/** Spends every stored die. On a client, asks the server to roll. */
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
	const TArray<FRRDamagePopup>& GetDamagePopups() const { return DamagePopups; }

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

	/** Where shots appear relative to the capsule center: forward, right, up. */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FVector MuzzleOffset = FVector(70.f, 0.f, 20.f);

	/** Highest ledge, above the feet, the brawler can climb onto. */
	UPROPERTY(EditAnywhere, Category = "Climb")
	float MaxClimbHeight = 200.f;

	/** How far in front of the capsule a wall can be and still be climbed. */
	UPROPERTY(EditAnywhere, Category = "Climb")
	float ClimbReach = 60.f;

	/** Seconds the climb takes: up first, then over the edge. */
	UPROPERTY(EditAnywhere, Category = "Climb")
	float ClimbDuration = 0.45f;

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

	/** Skeletal-mesh-ready material with a "DiffuseColor" parameter for random events; the mannequin's own material has a fixed color. */
	UPROPERTY(EditAnywhere, Category = "Body")
	TObjectPtr<UMaterialInterface> BodyBaseMaterial;

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

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDamagePopup(float Amount);

	UFUNCTION()
	void OnRep_Team();

	UFUNCTION()
	void OnRep_Alive();

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequence> CurrentAnimation;

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

	float LastCombatTime = -UE_BIG_NUMBER;
	float NextFireTime = 0.f;
	float BaseWalkSpeed = 0.f;

	FVector HomeLocation = FVector::ZeroVector;
	FRotator HomeRotation = FRotator::ZeroRotator;
	FVector MeshRelativeLocation = FVector::ZeroVector;
	FRotator MeshRelativeRotation = FRotator::ZeroRotator;
	FName MeshCollisionProfile;
};
