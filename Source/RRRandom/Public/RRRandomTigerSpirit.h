#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomTigerSpirit.generated.h"

class ARRRandomCharacter;
class UAnimSequence;
class UCameraComponent;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;
class UStaticMeshComponent;

/**
 * The tiger's ultimate, "Tiger Spirit" (호랑이 기운): rolled with an ultimate die, the tiger grows as big as the arena
 * (GiantHeight) with a roar, then strikes the ground with both forepaws as many times as the die showed. Each strike
 * lands wherever its tiger's player clicks (anywhere round it, people or not; a click during a strike is kept for the
 * next; left too long it strikes straight ahead; a bot's tiger picks the spot with the most enemies): the giant leaps so its paws come down right there, and everyone of the other team within StrikeRadius
 * (about ten brawlers across) is hurt (more near the middle) and thrown out. After the last strike it shrinks back into
 * the tiger, which stood hidden and untouchable meanwhile.
 *
 * Spawned by the server (ARRRandomCharacter::OnUltimateDie) and replicated: every machine plays the giant's growth and
 * shrinking from its own clock, and each strike from MulticastStrike; the server alone deals the damage. Mixamo motions on
 * the tiger's skeleton: A_Tiger_Roar ("Mutant Roaring"), A_Tiger_JumpAttack ("Mutant Jump Attack", both paws slamming
 * down 77 cm ahead of the rig at 1.7 s), A_Tiger_Idle. Its tiger's player looks through Overview, a camera high over the
 * arena, with the cursor out to pick the spots.
 */
UCLASS()
class RRRANDOM_API ARRRandomTigerSpirit : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomTigerSpirit();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, between SpawnActorDeferred and FinishSpawning. */
	void Setup(ARRRandomCharacter* InTiger, int32 Strikes);

	/** Server: the next strike at this spot (anywhere: where the player clicked); waits for the current strike to end. */
	bool RequestStrike(const FVector& Target);

	/** Grown, standing between strikes, with strikes left. */
	bool IsReady() const;
	int32 GetStrikesLeft() const { return StrikesLeft; }
	ARRRandomCharacter* GetTiger() const { return Tiger; }
	float GetStrikeRadius() const { return StrikeRadius; }

	/** The floor's height where the tiger stood: a picked spot is found on the floor, not on what sticks up from it. */
	float GetGroundZ() const { return GroundZ; }
	/** The ground at a spot: the first walkable world surface going down from a little above it (or the spot itself). */
	FVector FindGround(const FVector& Spot) const;
	/** This machine's player is pointing at a spot: the strike area's hologram there (none with bShow false). */
	void ShowCursorSpot(bool bShow, const FVector& Spot);

	/** How tall the giant stands, the arena's size about. */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float GiantHeight = 3500.f;

	/** Everyone of the other team this close to a strike's spot gets hit. */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float StrikeRadius = 500.f;

	/** Damage in the middle of a strike, and the share of it left at its edge. */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float StrikeDamage = 70.f;
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float EdgeDamageShare = 0.5f;

	/** How hard the ones left standing are thrown out (along the ground, and up). */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	FVector2D StrikeKnock = FVector2D(900.f, 550.f);

	/** Seconds growing (with the roar) and shrinking back. */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float GrowTime = 2.f;
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float ShrinkTime = 1.2f;

	/** The jump attack's speed, and where in it a strike starts (the crouch before the leap) and ends. */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float StrikeRate = 1.3f;
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float StrikeFrom = 0.35f;
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float StrikeTo = 2.9f;

	/** A view within this of the space the giant takes up sees it see-through. */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float GhostMargin = 300.f;

	/** How tall the strike area's hologram wall stands. */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float HoloHeight = 260.f;

	/** Seconds a player may take to pick the next spot before the giant picks one; a bot's tiger picks after BotDelay. */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float PlayerDelay = 6.f;
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float BotDelay = 0.8f;

protected:
	virtual void BeginPlay() override;

private:
	enum class EPhase : uint8 { Grow, Idle, Strike, Shrink, Gone };

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStrike(FVector_NetQuantize Target, float NewYaw);

	UFUNCTION()
	void OnRep_Ending();

	/** Grows out of its tiger, once the tiger is known here. */
	void TryStart();
	bool bStarted = false;

	/** Server: the spot with the most enemies in reach of a strike (or ahead of the giant without any). */
	FVector PickTarget() const;
	/** The paws come down: effects here, damage on the server. */
	void Impact();
	void PlayBody(UAnimSequence* Animation, bool bLoop, float Rate = 1.f, float From = 0.f);
	/** Where the giant stands (Feet, Yaw) and how big (Scale) onto the actor and the body. */
	void Place(float Scale);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Overview;

	/**
	 * The strike area as a hologram on the floor (M_FXHolo on the engine cylinder, StrikeRadius across): one following
	 * the cursor of the tiger's player (that machine only), and one on every machine where the giant is about to strike.
	 */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CursorHolo;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TargetHolo;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CursorHoloMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TargetHoloMaterial;
	/** Seconds the target hologram has shown (-1: hidden), and how far it has faded after the paws landed. */
	float TargetHoloTime = -1.f;
	float ImpactHoloFade = 0.f;
	float GroundZ = 0.f;
	/** Places a hologram on the floor at a spot. */
	void PlaceHolo(UStaticMeshComponent* Holo, const FVector& Spot) const;

	UPROPERTY()
	TObjectPtr<UAnimSequence> RoarAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> AttackAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAnimation;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	/** The see-through glowing look (M_FXGhost) for a view the giant would fill, one per slot. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> GhostMaterials;
	bool bGhost = false;
	/** This machine's camera is in or right by the giant: show it as its glowing spirit, so the view isn't all fur. */
	void UpdateGhost();

	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomCharacter> Tiger;

	UPROPERTY(Replicated)
	int32 StrikesLeft = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Ending)
	bool bEnding = false;

	EPhase Phase = EPhase::Grow;
	float PhaseTime = 0.f;
	/** The tiger's size when it started, the giant's full one (mesh scales). */
	float StartScale = 1.f;
	float FullScale = 1.f;
	FVector Feet = FVector::ZeroVector;
	float Yaw = 0.f;
	/** This strike: where the giant leaps from and to, its turn, its spot, and whether the paws are down yet. */
	FVector LeapFrom = FVector::ZeroVector;
	FVector LeapTo = FVector::ZeroVector;
	float YawFrom = 0.f;
	float YawTo = 0.f;
	FVector StrikeSpot = FVector::ZeroVector;
	bool bImpactDone = false;
	/** Server: seconds standing ready. */
	float ReadyTime = 0.f;
	/** Server: a spot asked for while the giant was busy, struck as soon as it's ready. */
	FVector QueuedTarget = FVector::ZeroVector;
	bool bHasQueued = false;
};
