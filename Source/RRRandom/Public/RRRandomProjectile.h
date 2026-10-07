#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomWeapon.h"
#include "RRRandomProjectile.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class UPointLightComponent;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * Straight-flying shot that deals a random amount of damage to whatever it hits, then disappears.
 * Passes through the shooter's teammates and other shots, and fades out after MaxRange.
 * Shots from stronger guns (higher Tier) glow, leave a trail and burst bigger on impact.
 *
 * Networking: only the server spawns shots and decides hits. Clients get a replicated copy that flies without
 * collision, just for show, and play the impact effects when the server reports where the shot landed.
 */
UCLASS()
class RRRANDOM_API ARRRandomProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomProjectile();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

	float GetSpeed() const;

	/** Scales the flying speed; call before the shot finishes spawning. */
	void SetSpeedMultiplier(float Multiplier);

	/** Damage is rolled between these on every hit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	int32 MinDamage = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	int32 MaxDamage = 15;

	/** Scales the rolled damage; the shooter sets it from its gun and attack power buffs. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float DamageMultiplier = 1.f;

	/** Distance flown before the shot disappears; 0 keeps InitialLifeSpan. The shooter sets it from its attack range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float MaxRange = 0.f;

	/** Push given to physics bodies it hits, such as a knocked-down dummy. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float ImpactImpulse = 300.f;

	/** Color of the viewer's own shots, so they stand out; everyone else's take their team color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile")
	FLinearColor ShotColor = FLinearColor(1.f, 0.85f, 0.1f);

	/** How flashy the shot looks; the shooter sets it from its gun before the shot begins play. */
	UPROPERTY(Replicated)
	ERRWeaponTier Tier = ERRWeaponTier::Common;

	/** How long a shot that hit something stays around, hidden, so clients hear about the hit before it goes. */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	float ImpactLingerTime = 0.2f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/** Colored light carried by rare and better shots, lighting up the floor as they fly. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effects")
	TObjectPtr<UPointLightComponent> Glow;

	/** Small spark burst: impacts of rare shots, muzzle flash of epic ones. */
	UPROPERTY(EditAnywhere, Category = "Effects")
	TObjectPtr<UNiagaraSystem> SparkEffect;

	/** Bigger directional burst: impacts of epic shots, muzzle flash of legendary ones. */
	UPROPERTY(EditAnywhere, Category = "Effects")
	TObjectPtr<UNiagaraSystem> BurstEffect;

	/** Legendary impacts. */
	UPROPERTY(EditAnywhere, Category = "Effects")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	/** Ring of sparks added to legendary impacts. */
	UPROPERTY(EditAnywhere, Category = "Effects")
	TObjectPtr<UNiagaraSystem> RingEffect;

	/** Particles left behind epic and legendary shots. */
	UPROPERTY(EditAnywhere, Category = "Effects")
	TObjectPtr<UNiagaraSystem> TrailEffect;

private:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void SpawnImpactEffects(const FVector& Location, const FVector& Normal);

	/** Stops and hides the shot where it landed; the server destroys it a moment later. */
	void HideAfterImpact();

	UFUNCTION()
	void OnRep_Impact();

	/** Not attached, so its particles linger where they were left after the shot is gone. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Trail;

	/** Set by the server when the shot hits something; clients play the impact from it. */
	UPROPERTY(ReplicatedUsing = OnRep_Impact)
	bool bImpacted = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize ImpactLocation;

	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal ImpactNormal;
};
