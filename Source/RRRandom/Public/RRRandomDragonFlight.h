#pragma once

#include "CoreMinimal.h"
#include "RRRandomUltimate.h"
#include "RRRandomDragonFlight.generated.h"

class UMaterialInstanceDynamic;
class UPoseableMeshComponent;
class UStaticMeshComponent;

/**
 * The dragon's ultimate, "Dragon Flight" (드래곤 비행): the little dragon bursts into flame and its true form, a big classic
 * four-legged dragon in a streamlined high-speed pose (/Game/Characters/DragonFlight: char-pipeline input/dragon_flight,
 * blender_creature_base.py + rig_chains.sh + ue_import_creature.py), shows itself once its player has clicked the way to
 * go (ARRRandomUltimate: anywhere; a bot's dragon heads for where most enemies are; meanwhile an arrow shows the way). The
 * fire's line runs from where the little dragon stood that way to the edge of the map: the dragon appears in a burst of
 * flame on the line behind its start, flies one quick lap beside the start back onto the line and, without a break, on
 * along it breathing fire from the start to the end, and climbs away; the strip it burnt flames up and then glows with
 * small embers until BurnSeconds after the breath went by. Enemies the breath
 * reaches are hurt (FrontDamage, and thrown aside), and hurt again every BurnTick while they stand in the embers. The die's
 * face widens the fire to the left and right (BreathRadius) and makes the dragon bigger (Scale). The little dragon stands
 * hidden and untouchable meanwhile, and comes back where it was.
 *
 * Every machine flies the same flight from MulticastFly (the line), each on its own clock; the server alone decides and
 * deals the damage. The dragon is a poseable mesh (FlightBody) whose wings flap and neck and tail move by code (PoseBody),
 * no animation assets.
 */
UCLASS()
class RRRANDOM_API ARRRandomDragonFlight : public ARRRandomUltimate
{
	GENERATED_BODY()

public:
	ARRRandomDragonFlight();

	virtual bool WantsSpots() const override { return true; }
	virtual bool IsBusy() const override;
	virtual bool HoldsBrawler() const override { return true; }
	virtual bool HidesBrawler() const override { return true; }
	virtual FString GetTitle() const override { return TEXT("DRAGON FLIGHT"); }
	virtual FString GetStatusLine() const override;
	virtual void ShowCursorSpot(bool bShow, const FVector& Spot) override;

	/**
	 * The dragon's size (the model is about 100 long and wide at 1): BaseScale, and ScalePerFace more for each pip (10 at a
	 * one, 20 at a six).
	 */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float BaseScale = 8.f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float ScalePerFace = 2.f;

	/** How far to each side of the line the breath reaches: BaseBreathRadius, and BreathRadiusPerFace more for each pip. */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float BaseBreathRadius = 140.f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float BreathRadiusPerFace = 55.f;

	/** How high it flies over the floor: BaseHeight, and HeightPerScale more for each step of Scale. */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float BaseHeight = 400.f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float HeightPerScale = 70.f;

	/** Speed along the line (the fire spreads along it as fast). */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float FlySpeed = 2800.f;

	/** The line runs to the map's edge, but no shorter or longer than these. */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float MinLineLength = 1500.f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float MaxLineLength = 7000.f;

	/**
	 * Once the way is picked the dragon appears and flies one quick lap beside where the little dragon stood: CircleTime
	 * seconds (and CirclePerScale more per step of Scale, so a bigger dragon doesn't spin), its radius this much more than
	 * the distance its breath reaches ahead of it.
	 */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float CircleTime = 1.2f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float CirclePerScale = 0.05f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float CircleExtraRadius = 600.f;

	/** Damage when the breath reaches an enemy, and how hard it throws them out of it (aside, up). */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float FrontDamage = 25.f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	FVector2D FrontKnock = FVector2D(450.f, 320.f);

	/**
	 * What the breath leaves on the ground: flames for BlazeSeconds where it just went by, dying down into small embers that
	 * glow on until BurnSeconds after it passed; an enemy standing in either takes BurnDamage every BurnTick.
	 */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float BlazeSeconds = 0.7f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float BurnSeconds = 3.f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float BurnDamage = 4.f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float BurnTick = 0.25f;

	/**
	 * The breath smashes the structures it sweeps (ARRRandomWreckage) as its front goes by, in stretches this long, this share
	 * of BreathRadius to each side.
	 */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float WreckStep = 450.f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float WreckShare = 0.85f;

	/** Seconds flying away at the end. */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float LeaveTime = 1.2f;

	/** While it flies the line, its player watches from the side, this far from it (and FollowPerScale more per step of Scale). */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float FollowDistance = 2600.f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float FollowPerScale = 220.f;

	/** Wing beats a second while it flaps (setting off and climbing away). */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float FlapRate = 2.2f;

	/**
	 * Gliding (the lap and the fire run): wings held spread and still, and every so many seconds (min, max) one big beat
	 * lasting StrokeSeconds, StrokeDegrees down and back up at the root; the body surges nose up a little with it.
	 */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	FVector2D StrokeEvery = FVector2D(1.1f, 1.9f);
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float StrokeSeconds = 0.55f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float StrokeDegrees = 46.f;

	/** Banking into turns: degrees per degree a second it turns, and at most. */
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float BankPerTurn = 0.5f;
	UPROPERTY(EditAnywhere, Category = "Dragon Flight")
	float MaxBank = 55.f;

protected:
	virtual void BeginPlay() override;
	virtual void OnStarted() override;
	virtual void TickUltimate(float DeltaSeconds) override;
	virtual void StartAction(const FVector& Target) override;
	virtual FVector GetDefaultSpot() const override;
	virtual FVector PickTarget() const override;
	virtual void OnEnding() override;

private:
	enum class EPhase : uint8 { Hover, Circle, Pass, Leave, Gone };

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFly(FVector_NetQuantize From, FVector_NetQuantize To);

	/** The dragon's mesh: the bones the code moves, parents first, with their parent in this list and their
	 * reference pose (component space), how far each is turned now, the snout tip from the head bone and the hips' height. */
	struct FRig
	{
		UPoseableMeshComponent* Mesh = nullptr;
		TArray<FName> Bones;
		TArray<int32> Parents;
		TArray<FQuat> RefRotations;
		TArray<FQuat> Turns;
		FVector SnoutOffset = FVector::ZeroVector;
		float HipsHeight = 0.f;
		int32 Head = INDEX_NONE;
	};

	/** Fills a rig from its mesh's asset (the component's own pose may not be set up yet). Snout: the snout tip in the
	 * mesh's component space. Bones named ...2, 3, 4 follow the one before; the others hang off the first (the hips). */
	void SetupRig(FRig& Rig, UPoseableMeshComponent* Mesh, const TArray<FName>& Bones, const FVector& Snout);
	/** Turns each bone of a rig by Want (about the reference pose's axes, carried along by its parent's turn). */
	void ApplyPose(FRig& Rig, const TMap<FName, FQuat>& Want);
	/** Wings, neck and tail for this moment; bBreathing bends the head down at the fire. */
	void PoseBody(float Time, bool bBreathing);
	/** How far the line goes from From the way Direction points: to the map's edge, within the allowed lengths. */
	float GetLineLength(const FVector& From, const FVector& Direction) const;
	/** The way from the little dragon to a spot (the way it faces, for a spot right on it). */
	FVector GetWayTo(const FVector& Spot) const;
	/** The player's view once it appears: swinging round to the side of the line, taking in the lap, following it along. */
	void FollowWithView(float DeltaSeconds);
	/** Puts the dragon here, facing along Velocity (banking into turns). */
	void PlaceBody(const FVector& Where, const FVector& Velocity, float DeltaSeconds);
	/** Where the fire comes out: in front of the snout. */
	FVector GetMouth() const;
	/** A point on the line, Along units from its start, on the floor. */
	FVector OnLine(float Along) const;
	/** Server, every frame of the pass and while the ground burns: the breath's front and the burning strip hurt enemies. */
	void BurnEnemies(float DeltaSeconds);
	/** Lays a hologram strip on the floor from one spot to another, Radius to each side. */
	void PlaceStrip(UStaticMeshComponent* Strip, const FVector& From, const FVector& To, float Radius) const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPoseableMeshComponent> FlightBody;

	/** The line while picking (this machine's player only), and the warning on every machine until the fire is out. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PathHolo;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WarnHolo;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PathHoloMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WarnHoloMaterial;

	/** The ground the breath has scorched, with embers glowing in it (one thin strip just over the floor along the whole
	 * line, the material working out how far the fire has got and how old each spot is: char-pipeline/scripts/ue_make_scorch.py). */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Scorch;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ScorchMaterial;

	FRig FlightRig;

	EPhase Phase = EPhase::Hover;
	float PhaseTime = 0.f;
	float FlightTime = 0.f;
	float Scale = 1.f;
	float BreathRadius = 200.f;
	float Height = 800.f;
	/** Ahead of the dragon along the line, where its breath meets the floor. */
	float Lead = 400.f;

	FVector Hover = FVector::ZeroVector;
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	float Bank = 0.f;

	FVector LineStart = FVector::ZeroVector;
	FVector LineEnd = FVector::ZeroVector;
	FVector LineDirection = FVector::ForwardVector;
	float LineLength = 0.f;
	/** Seconds since the pass began (the breath's front is FlySpeed times that along the line). */
	float PassTime = -1.f;
	/** Server: how far along the line the breath has smashed so far. */
	float WreckedTo = 0.f;
	/**
	 * The lap: where it begins and ends (on the line, Lead behind its start, at height), its middle (beside the start),
	 * radius, seconds, and the angle (radians) of its end round the middle.
	 */
	FVector LapExit = FVector::ZeroVector;
	FVector CircleCenter = FVector::ZeroVector;
	float CircleRadius = 1000.f;
	float CircleSeconds = 1.f;
	float CircleEnd = 0.f;
	/** Extra distance the side view keeps while it laps (eased back in for the dash). */
	float ViewExtra = 0.f;
	FVector LeaveFrom = FVector::ZeroVector;
	/** The view over the arena when the flight began, and how far it has swung to follow the dragon (0 .. 1). */
	FTransform ViewFrom;
	float ViewBlend = -1.f;
	FVector ViewFocus = FVector::ZeroVector;
	/** The map's area (from the island or Dice Land), for the line's end. */
	FBox2D MapBounds = FBox2D(ForceInit);
	/** How fast the wings beat now (0 hovering .. 1 in flight), eased toward the phase's. */
	float Dash = 0.f;
	/** The wing beat's phase (radians), run on at the beat's rate of the moment. */
	float FlapPhase = 0.f;
	/** 0 .. 1 how much the wings flap (setting off, climbing away) rather than glide; the glide's next big beat in so many
	 * seconds, and how far through the one going on (0 .. 1; above 1 none). */
	float Flapping = 1.f;
	float StrokeWait = 0.8f;
	float StrokeAlpha = 2.f;

	/** Server: enemies the breath's front has already hit, and the burn's clock. */
	TSet<TWeakObjectPtr<AActor>> FrontHit;
	float BurnClock = 0.f;
	int32 Hits = 0;
	/** Server: burn ticks dealt, and the longest after the breath went by that one hurt (for the log). */
	int32 BurnHits = 0;
	float LastBurnAge = 0.f;
};
