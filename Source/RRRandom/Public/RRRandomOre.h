#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "RRRandomOre.generated.h"

class ARRRandomCharacter;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class USphereComponent;
class UStaticMeshComponent;

/** The dice ore of Meteor Rush, from smallest to the overtime's golden crystal. */
UENUM()
enum class ERROreKind : uint8
{
	/** A mint glass chip with one die inside: 1 point. */
	Shard,
	/** A pink glass lump with three dice inside: 3 points. */
	Ore,
	/** An amber crystal round a glowing six: 6 points. */
	Big,
	/** Overtime's crystal: holding it wins. Counts like a big one. */
	Golden,
};

namespace RROre
{
	int32 GetPoints(ERROreKind Kind);
	/** Big and golden crystals stay whole when dropped, glow, and show on every map. */
	inline bool IsCrystal(ERROreKind Kind) { return Kind == ERROreKind::Big || Kind == ERROreKind::Golden; }
	FLinearColor GetColor(ERROreKind Kind);
}

/**
 * A piece of dice ore: thrown out of a meteor or a knocked-out brawler, it arcs through the air (the server moves it,
 * clients follow), comes to rest hovering over the floor, and is picked up by the first brawler to touch it (after a
 * moment, so it isn't caught mid-throw). Shards and ore are see-through candy glass with real dice tumbling inside;
 * crystals glow. Falls off the island and it is gone.
 */
UCLASS()
class RRRANDOM_API ARRRandomOre : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomOre();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, before FinishSpawning: what it is and how it is thrown. */
	void Setup(ERROreKind InKind, const FVector& InVelocity);
	/**
	 * Server, before FinishSpawning, instead of Setup: a falling star, streaking straight down from where it spawns to
	 * Target (on the floor) in a trail of stars, a ring marking the spot meanwhile.
	 */
	void SetupFallingStar(ERROreKind InKind, const FVector& Target);
	bool IsFallingStar() const { return bFallingStar && !bResting; }

	/** A falling star's speed. */
	static constexpr float FallingStarSpeed = 2200.f;

	ERROreKind GetKind() const { return Kind; }
	bool IsResting() const { return bResting; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnRep_Kind();
	void ApplyLook();
	/** Every machine: the trail behind it while it flies, the marker under a falling star, a pop on landing. */
	void UpdateEffects(float DeltaSeconds);
	/** Server: flight under gravity until it lands on the floor. */
	void UpdateFlight(float DeltaSeconds);
	/** Server: the first living brawler touching it takes it. */
	void TryPickUp();

	UFUNCTION()
	void OnTouch(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Touch;

	/** Bobs and turns; the shell and dice ride on it. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Float;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Shell;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> Dice;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Glow;

	/** Where a falling star lands, a ring on the floor while it falls. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Marker;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MarkerMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_Kind)
	ERROreKind Kind = ERROreKind::Shard;

	UPROPERTY(Replicated)
	bool bResting = false;

	UPROPERTY(Replicated)
	bool bFallingStar = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize StarTarget = FVector::ZeroVector;

	/** Each machine: where it was last frame (its speed for the trail), and whether the landing pop has played. */
	FVector LastSeen = FVector::ZeroVector;
	bool bHasLastSeen = false;
	/** The way it was last seen going (the landing splashes on along it). */
	FVector TrailWay = FVector::ZeroVector;
	bool bLandingShown = false;

	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
	/** Below this height it has fallen off the map. */
	float LostZ = -UE_BIG_NUMBER;
	/** Random phase so a pile doesn't bob in step. */
	float Phase = 0.f;
	TArray<FRotator> DieSpin;
	bool bLooked = false;
};

/**
 * The dice ore a brawler carries in Meteor Rush, and how it shows: a count over the head (the HUD) and, while holding a
 * crystal, that crystal circling the waist in a warm light. The server adds ore as it is picked up and throws it all
 * back out when the brawler is knocked out (crystals whole, the rest as one-point shards); off the edge it is lost.
 */
UCLASS()
class RRRANDOM_API URRRandomOreCarryComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URRRandomOreCarryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	int32 GetPoints() const { return Points; }
	int32 GetCrystals() const { return Crystals; }
	int32 GetShards() const { return Shards; }
	int32 GetOres() const { return Ores; }
	bool HasGolden() const { return bGolden; }
	bool HasCrystal() const { return Crystals > 0 || bGolden; }
	/** Points picked up over the whole match (the MVP). */
	int32 GetCollected() const { return Collected; }

	/** Server. */
	void Add(ERROreKind Kind);
	/** Server: throws everything out around the owner (or loses it, off the edge). */
	void DropAll(bool bLost);

private:
	UPROPERTY(Replicated)
	int32 Points = 0;

	UPROPERTY(Replicated)
	int32 Crystals = 0;

	/** How many of each smaller kind it carries (for the owner's panel). */
	UPROPERTY(Replicated)
	int32 Shards = 0;

	UPROPERTY(Replicated)
	int32 Ores = 0;

	UPROPERTY(Replicated)
	bool bGolden = false;

	UPROPERTY(Replicated)
	int32 Collected = 0;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Orbiter;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> OrbiterMaterial;

	float OrbitAngle = 0.f;
};
