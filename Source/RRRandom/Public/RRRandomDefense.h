#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomDefense.generated.h"

class ARRRandomCharacter;
class ARRRandomDiceTown;
class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * The defense mode (round 90, ERRMatchRule::Defense): everyone on one team (blue, team 0) holds the giant dice tower in the
 * middle of a defense stage (ARRRandomDiceTown with its stage) against 7 waves of villains (red, team 1: rats, and cat
 * bosses at waves 4 and 7) coming in along the stage's roads from its gates. A ring of walls round the tower stops the
 * villains (only them: the animal friends, their shots and their rides pass) until they break it; the tower falling loses,
 * the seventh wave beaten wins and opens the next stage.
 */
namespace RRDefense
{
	/** The tower's pedestal reaches this many cells either way from the middle; the wall ring stands this far out (cells). */
	constexpr int32 TowerHalfCells = 5;
	constexpr int32 WallRingCells = 14;
	/**
	 * The castle wall (round 90, built into the stage's grid, ARRRandomDiceTown::LayOutDefense): this many cells thick from
	 * WallRingCells out, this many levels high (low enough to shoot down from) with a walkway on top and battlements along its
	 * outer edge, unbreakable, with stairs up on the inside. Two gateways through it, east (+X) and west (-X), this many cells
	 * either side of the road's middle, open to the sky (a giant boss fits) between two taller posts, each shut by a gate door
	 * (ARRRandomDefenseWall) GateLevels high that the villains must break.
	 */
	constexpr int32 WallThickCells = 3;
	constexpr int32 WallLevels = 2;
	constexpr int32 GateHalfCells = 3;
	constexpr int32 GateLevels = WallLevels;
	/**
	 * The jump ramps (ARRRandomJumpRamp), on the north side: one outside leaping riders in over the wall onto the plaza, one
	 * inside leaping them out onto the north road. Each: where its foot and lip are (X, Y from the middle, in the world's units).
	 */
	constexpr float OuterRampX = -1100.f;
	constexpr float OuterRampFootY = 2900.f;
	constexpr float OuterRampLipY = 2500.f;
	constexpr float InnerRampX = 0.f;
	constexpr float InnerRampFootY = 900.f;
	constexpr float InnerRampLipY = 1300.f;
	// How far on past its lip a ramp's leap comes down (round 91: 1500 -> 2200 with a lower arc, ARRRandomJumpRamp::Apex 520 ->
	// 350 and a heavier fall, so the leap flies faster than the bike drives instead of floating)
	constexpr float RampLandDistance = 2200.f;
	/** Gates (where villains come in) the stages have room for: one at each of 8 compass points, the first ones first. */
	constexpr int32 MaxGates = 8;
	/** Where the animal friends start and get back up: in front of the tower on its -X side, inside the ring. */
	constexpr float FriendStartOut = 900.f;
	/** The villains' capsules move as this object type: what the gate doors stop (the friends pass through them). */
	constexpr ECollisionChannel VillainChannel = ECC_Vehicle;
}

/**
 * The giant dice tower in the middle of a defense stage: a die six blocks across turning slowly over a stepped pedestal.
 * Villains bite it; its health is everyone's (replicated) and the HUD shows it. At zero it bursts, and the game is lost.
 */
UCLASS()
class RRRANDOM_API ARRRandomDiceTower : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomDiceTower();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** The world's tower, if there is one. */
	static ARRRandomDiceTower* Get(const UObject* WorldContext);

	/** Server, before it finishes spawning. */
	void SetMaxHealth(float InMaxHealth) { MaxHealth = InMaxHealth; Health = InMaxHealth; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsDestroyed() const { return Health <= 0.f; }
	/** The top of the die (for the HUD's marker and the villains' aim). */
	FVector GetTop() const;
	/** Half the pedestal's width: how far its sides are from the middle. */
	static float GetHalfWidth();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Health();
	void Build();
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastHit(FVector_NetQuantize Where);
	UFUNCTION(NetMulticast, Reliable)
	void MulticastBurst();

	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Blocks;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Die;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Solid;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 2000.f;
	UPROPERTY(Replicated)
	float MaxHealth = 2000.f;
	/** Each machine's: the die shaking and flashing after a hit, and its turn. */
	float HitShake = 0.f;
	float LastHealth = 0.f;
	float Spin = 0.f;
	bool bBurst = false;
	FVector DieBase = FVector::ZeroVector;
};

/**
 * A gate door of the castle round the tower (round 90; the wall itself is the stage's grid, unbreakable): planks of the town's
 * blocks filling a gateway, a row per level, dark and light wood with an iron band and a gold top. It stops the villains
 * (their capsules move as RRDefense::VillainChannel) and shots both ways; the animal friends walk through it (their way out
 * and back in). Villains bite it down block by block: every bite throws sparks and grit, and as its health goes planks break
 * off (the top row first) and tumble in toward the tower; at zero what is left caves in and the way is open. Between waves
 * it is mended part of the way, the planks popping back up. Every machine works the look out from the replicated health,
 * with the same order of planks.
 */
UCLASS()
class RRRANDOM_API ARRRandomDefenseWall : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomDefenseWall();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Server, before it finishes spawning: how long it is (along its +Y), how tough, and its crest color. */
	void Setup(float InLength, float InMaxHealth, const FColor& InColor) { Length = InLength; MaxHealth = InMaxHealth; Health = InMaxHealth; Color = InColor; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsBroken() const { return Health <= 0.f; }
	float GetLength() const { return Length; }
	/**
	 * Server, between waves: back up by this share of its full health, never past Cap of it (round 90: broken walls come back
	 * only part of the way). Returns whether it changed.
	 */
	bool Mend(float Share, float Cap);
	/**
	 * Server: the break between waves opens the gates (the door sinks into the floor and everyone passes), the next wave
	 * shuts them (it rises again as its health stands).
	 */
	void SetOpen(bool bInOpen);
	bool IsOpen() const { return bOpen; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Health();
	UFUNCTION()
	void OnRep_Open();
	/** A plank's place now: its spot, sunk as far as the door is open, at this share of its size. */
	FTransform GetBlockTransform(int32 Block, float Scale) const;
	UPROPERTY(ReplicatedUsing = OnRep_Open)
	bool bOpen = false;
	/** Each machine's: how far the door has sunk (0 shut, 1 open). */
	float OpenShown = 0.f;
	/** Lays out its blocks (every machine the same) and draws them as the health stands. */
	void Build();
	/** Shows the blocks the health keeps and the box that stops villains; with bShow, what goes breaks off and what comes back grows. */
	void Refresh(bool bShow);
	/** A block breaking off: a copy of it tumbling in toward the tower. */
	void ThrowBlock(int32 Block, float Strength);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBitten(FVector_NetQuantize Where);

	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Blocks;
	/** The pieces in the air. */
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Rubble;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Stopper;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 500.f;
	UPROPERTY(Replicated)
	float MaxHealth = 500.f;
	UPROPERTY(Replicated)
	float Length = 700.f;
	UPROPERTY(Replicated)
	FColor Color = FColor::White;

	/** Each block's place (world) and colors, and the order they break off in (battlements first, then the top row...). */
	TArray<FTransform> Spots;
	TArray<FColor> Bodies;
	TArray<FColor> Crests;
	TArray<int32> BreakOrder;
	/** Each block shown now, and how far it has grown back (1 standing). */
	TArray<bool> Shown;
	TArray<float> Growth;
	struct FPiece
	{
		int32 Instance;
		FVector Location;
		FQuat Rotation;
		FVector Scale;
		FVector Velocity;
		FVector Spin;
		float Age;
	};
	TArray<FPiece> Pieces;
	bool bBuilt = false;
};

/**
 * A jump ramp (round 90): a striped wedge of the town's blocks a rider takes at speed to leap the castle wall, one outside
 * leaping in, one inside leaping out. Whoever drives a ride (not a plane) up it toward its lip faster than MinSpeed is
 * thrown off the lip on a set arc: up Apex over it, landing LandDistance further on whatever its speed (the ride comes with
 * its driver, the one behind too). Every machine checks its own: the server for everyone, a player's own machine for its
 * player (the movement is predicted there), so the leap looks the same everywhere.
 */
UCLASS()
class RRRANDOM_API ARRRandomJumpRamp : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomJumpRamp();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, before it finishes spawning (the actor stands at the foot on the floor, facing the lip): how long it is and how far it throws. */
	void Setup(float InLength, float InLandDistance) { Length = InLength; LandDistance = InLandDistance; }

	static constexpr float Rise = 150.f;
	static constexpr float Width = 320.f;
	// (round 91: 520 -> 350, lower and faster, falling LeapGravity times heavier while it flies: about 1.3 s in the air at
	// about 1,700 a second, against 2.2 s at 680 before; RRDefense::RampLandDistance)
	static constexpr float Apex = 350.f;
	static constexpr float LeapGravity = 2.f;
	static constexpr float MinSpeed = 200.f;
	/** How bright its lights glow (the block material's Glow), and pulses up it a second. */
	static constexpr float RampGlow = 1.4f;
	static constexpr float PulseRate = 1.2f;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Looks;
	/** The lights' pulse and the glitter, every frame on every machine. */
	void Animate(float DeltaSeconds);
	/**
	 * What shines (round 90, a speed-boost look): chevrons on the slope and the run-up pointing at the lip, neon rails along
	 * its edges, the gold lip; a pulse of light runs up them toward the lip over and over (Tick), glitter at the lip.
	 */
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> Glows;
	/** Each glowing piece's color and where it lies along the run (0 the foot, 1 the lip; the run-up below 0), for the pulse. */
	TArray<FColor> GlowColors;
	TArray<float> GlowPlaces;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Slope;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Support;
	UPROPERTY(Replicated) float Length = 400.f;
	UPROPERTY(Replicated) float LandDistance = 1500.f;
	/** Who leapt lately (not again till they're down). */
	TMap<TWeakObjectPtr<AActor>, float> LastLeap;
};

/**
 * A die a villain drops (round 90): it pops out, and the nearest animal friend close by pulls it in, storing one more die
 * (URRRandomDiceBuffComponent::AddCharge; a boss's are ultimate dice). Gone after a while if nobody comes.
 */
UCLASS()
class RRRANDOM_API ARRRandomDiePickup : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomDiePickup();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, before it finishes spawning. */
	void SetUltimate(bool bOn) { bUltimate = bOn; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere) TObjectPtr<UInstancedStaticMeshComponent> Mesh;
	UPROPERTY(Replicated) bool bUltimate = false;
	FVector Velocity = FVector::ZeroVector;
	float Age = 0.f;
	TWeakObjectPtr<ARRRandomCharacter> Taker;
};

/**
 * The defense mode's waves (server only): the first comes after a short wait, each one's villains walk in from the stage's
 * open gates (more of them than there are players), a few at a time; once all of a wave are down, a 5 second break mends
 * the walls part of the way and the next comes, bigger and stronger. Waves 4 and 7 bring a cat boss (and the star team's
 * three cars, for the robot). The seventh beaten wins; the tower falling loses (ARRRandomGameMode::EndDefense).
 */
UCLASS()
class RRRANDOM_API ARRRandomDefenseDirector : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomDefenseDirector();

	virtual void Tick(float DeltaSeconds) override;

	/** The world's director (the host's), if any. */
	static ARRRandomDefenseDirector* Get(const UObject* WorldContext);

	/** Server, right after spawning: the stage, its map, and the tower and walls it set up. */
	void Begin(int32 InStage, ARRRandomDiceTown* InTown, ARRRandomDiceTower* InTower, const TArray<ARRRandomDefenseWall*>& InWalls);

	/** A villain stuck where it can't get out (ARRRandomVillainAI): back to the nearest open gate to walk in again. */
	void ReturnToGate(ARRRandomCharacter* Villain);
	/** A villain went down (ARRRandomGameMode::OnBrawlerKnockedOut): maybe a die for its killer's team, and it is cleared away. */
	void OnVillainDown(ARRRandomCharacter* Villain, ARRRandomCharacter* Attacker);
	/** The tower fell: the game is lost. */
	void OnTowerDestroyed();

	/** Cheats: straight to the break before this wave (RRWave), and every villain out now down (RRClearWave). */
	void CheatSkipToWave(int32 Wave);
	void CheatClearWave();

	/** The gates villains come in by this wave (more than the players), for the minimap. */
	const TArray<FVector>& GetOpenGates() const { return OpenGates; }

	/** Seconds before the first wave, of each break, and between villains walking in at a gate. */
	static constexpr float FirstWaveDelay = 6.f;
	static constexpr float BreakSeconds = 5.f;

private:
	/** One villain to come: its kind and whether it is a wave's boss. */
	struct FQueued
	{
		int32 Species;
		bool bBoss;
	};
	void StartWave(int32 Wave);
	/** The castle's gates open for a break, shut for a wave. */
	void SetGatesOpen(bool bOpen);
	void EndWave();
	void SpawnNext();
	ARRRandomCharacter* SpawnVillain(const FQueued& What, const FVector& Gate);
	/** Opens the gates for this wave: one more than the players, at least as many as the stage's own count. */
	void OpenGatesFor(int32 Wave);
	/** The star team's three cars by the tower for a boss wave, and gone again after it. */
	void SpawnStarCars();
	void RemoveStarCars();
	int32 CountPlayers() const;

	int32 Stage = 1;
	TWeakObjectPtr<ARRRandomDiceTown> Town;
	TWeakObjectPtr<ARRRandomDiceTower> Tower;
	TArray<TWeakObjectPtr<ARRRandomDefenseWall>> Walls;
	TArray<FVector> AllGates;
	TArray<FVector> OpenGates;
	TArray<FQueued> Queue;
	TArray<TWeakObjectPtr<ARRRandomCharacter>> Alive;
	TArray<TWeakObjectPtr<AActor>> StarCars;
	int32 Wave = 0;
	int32 NextGate = 0;
	float SpawnTimer = 0.f;
	float NextWaveAt = 0.f;
	bool bBreak = true;
	bool bOver = false;
	bool bBegun = false;
	int32 SpawnSerial = 0;
	float StatusTimer = 0.f;
	FRandomStream Stream;
};
