#pragma once

#include "CoreMinimal.h"
#include "RRRandomUltimate.h"
#include "RRRandomMoonHop.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;

/**
 * The rabbit's ultimate, "Moon Hop" (달토끼 낙하): as many hops as the die showed, each one to wherever its player clicks
 * (ARRRandomUltimate: anywhere; a bot's rabbit goes where most enemies are). The rabbit leaps sky high in a front flip,
 * hangs a moment against a big glowing moon, and comes down bottom first: everyone of the other team within AreaRadius is
 * hurt (more near the middle) and bounced out. The real rabbit stands hidden and untouchable meanwhile, and is where the
 * last hop came down when it's over.
 *
 * Every machine plays each hop from MulticastHop. Mixamo motions on the rabbit's own rig (Rabbit/Anims): A_Rabbit_JumpUp,
 * A_Rabbit_FrontFlip ("Big Front Flip"), A_Rabbit_FallIdle, A_Rabbit_HardLanding, A_Rabbit_Idle.
 */
UCLASS()
class RRRANDOM_API ARRRandomMoonHop : public ARRRandomUltimate
{
	GENERATED_BODY()

public:
	ARRRandomMoonHop();

	virtual bool WantsSpots() const override { return true; }
	virtual bool IsBusy() const override { return Phase != EPhase::Ready; }
	virtual bool HoldsBrawler() const override { return true; }
	virtual bool HidesBrawler() const override { return true; }
	virtual FString GetTitle() const override { return TEXT("MOON HOP"); }

	/** How high a hop goes, how long it takes, and how much bigger than the rabbit it is drawn. */
	UPROPERTY(EditAnywhere, Category = "Moon Hop")
	float HopHeight = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Moon Hop")
	float HopTime = 1.35f;
	UPROPERTY(EditAnywhere, Category = "Moon Hop")
	float BodyScale = 1.4f;

	/** Damage in the middle of a landing, and the share of it left at its edge; how hard the ones left standing bounce. */
	UPROPERTY(EditAnywhere, Category = "Moon Hop")
	float StompDamage = 45.f;
	UPROPERTY(EditAnywhere, Category = "Moon Hop")
	float EdgeDamageShare = 0.55f;
	UPROPERTY(EditAnywhere, Category = "Moon Hop")
	FVector2D StompKnock = FVector2D(700.f, 500.f);

	/** Seconds getting ready (a crouch), and settling after each landing. */
	UPROPERTY(EditAnywhere, Category = "Moon Hop")
	float ReadyTime = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Moon Hop")
	float LandTime = 0.45f;

protected:
	virtual void BeginPlay() override;
	virtual void OnStarted() override;
	virtual void TickUltimate(float DeltaSeconds) override;
	virtual void StartAction(const FVector& Target) override;
	virtual FVector GetDefaultSpot() const override;
	virtual void OnEnding() override;

private:
	enum class EPhase : uint8 { GetReady, Ready, Hop, Land, Gone };

	UFUNCTION(NetMulticast, Reliable)
	void MulticastHop(FVector_NetQuantize Target);

	void PlayBody(UAnimSequence* Animation, bool bLoop, float Rate = 1.f, float From = 0.f);
	/** Down: effects here, damage on the server. */
	void Land();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Body;

	UPROPERTY()
	TObjectPtr<UAnimSequence> JumpAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> FlipAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> FallAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> LandAnimation;
	UPROPERTY()
	TObjectPtr<UAnimSequence> IdleAnimation;

	EPhase Phase = EPhase::GetReady;
	float PhaseTime = 0.f;
	FVector Feet = FVector::ZeroVector;
	float Yaw = 0.f;
	FVector HopFrom = FVector::ZeroVector;
	FVector HopTo = FVector::ZeroVector;
	bool bFalling = false;
	bool bMoonShown = false;
};
