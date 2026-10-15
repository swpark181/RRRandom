#pragma once

#include "CoreMinimal.h"
#include "RRRandomUltimate.h"
#include "RRRandomTigerSpirit.generated.h"

class UAnimSequence;
class UMaterialInstanceDynamic;
class USkeletalMeshComponent;

/**
 * The tiger's ultimate, "Tiger Spirit" (호랑이 기운): the tiger grows as big as the arena (GiantHeight) with a roar, then
 * strikes the ground with both forepaws as many times as the die showed. Each strike lands wherever its player clicks
 * (ARRRandomUltimate: anywhere, people or not; a bot's tiger goes where most enemies are): the giant leaps so its paws
 * come down right there, and everyone of the other team within AreaRadius (about ten brawlers across) is hurt (more near
 * the middle) and thrown out. After the last strike it shrinks back into the tiger, which stood hidden and untouchable.
 *
 * Every machine plays the growth and shrinking from its own clock and each strike from MulticastStrike. Mixamo motions on
 * the tiger's skeleton: A_Tiger_Roar ("Mutant Roaring"), A_Tiger_JumpAttack ("Mutant Jump Attack", both paws slamming
 * down 77 cm ahead of the rig at 1.7 s), A_Tiger_Idle.
 */
UCLASS()
class RRRANDOM_API ARRRandomTigerSpirit : public ARRRandomUltimate
{
	GENERATED_BODY()

public:
	ARRRandomTigerSpirit();

	virtual bool WantsSpots() const override { return true; }
	virtual bool IsBusy() const override { return Phase != EPhase::Idle; }
	virtual bool HoldsBrawler() const override { return true; }
	virtual bool HidesBrawler() const override { return true; }
	virtual FString GetTitle() const override { return TEXT("TIGER SPIRIT"); }

	/** How tall the giant stands, the arena's size about. */
	UPROPERTY(EditAnywhere, Category = "Tiger Spirit")
	float GiantHeight = 3500.f;

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

protected:
	virtual void BeginPlay() override;
	virtual void OnStarted() override;
	virtual void TickUltimate(float DeltaSeconds) override;
	virtual void StartAction(const FVector& Target) override;
	virtual FVector GetDefaultSpot() const override;
	virtual void OnEnding() override;

private:
	enum class EPhase : uint8 { Grow, Idle, Strike, Shrink, Gone };

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStrike(FVector_NetQuantize Target, float NewYaw);

	/** The paws come down: effects here, damage on the server. */
	void Impact();
	void PlayBody(UAnimSequence* Animation, bool bLoop, float Rate = 1.f, float From = 0.f);
	/** Where the giant stands (Feet, Yaw) and how big (Scale) onto the actor and the body. */
	void Place(float Scale);
	/** This machine's camera is in or right by the giant: show it as its glowing spirit, so the view isn't all fur. */
	void UpdateGhost();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Body;

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
};
