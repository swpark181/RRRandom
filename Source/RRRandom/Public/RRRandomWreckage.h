#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomWreckage.generated.h"

class ARRRandomCover;
class UInstancedStaticMeshComponent;
class UStaticMesh;

/**
 * A stretch of the map smashed: everything within Radius of the line From - To. A column (a paw or a breath from
 * overhead) takes everything over the floor at any height, measured flat; otherwise it is a ball round the line (a fist).
 */
USTRUCT()
struct FRRWreck
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	UPROPERTY()
	FVector_NetQuantize From = FVector::ZeroVector;

	UPROPERTY()
	FVector_NetQuantize To = FVector::ZeroVector;

	UPROPERTY()
	float Radius = 0.f;

	UPROPERTY()
	bool bColumn = false;

	/**
	 * Layers of ground it digs where it reaches the floor (round 89): the tiger's paw 1, the robot's slam and a crashing
	 * plane 2, a cannon shot 1, the rest 0 (they stop at the floor). Wrecks over the same ground add up, so blow after
	 * blow digs a crater deeper, until it breaks through to the mantle under the island (lava).
	 */
	UPROPERTY()
	uint8 Dig = 0;

	/**
	 * Round 100 (the dragon's breath): a roof this wreck takes off takes this many layers of the walls under it too,
	 * over the roof's whole footprint. A roof is one model and went whole at a touch, while walls only break cell by
	 * cell inside the wreck, so a building at the breath's edge used to lose its roof and nothing else.
	 */
	UPROPERTY()
	uint8 Crush = 0;

	/** How far a spot is from the line (flat for a column), less Radius: negative inside. */
	float Distance(const FVector& Spot) const;
	bool Contains(const FVector& Spot, float Margin = 0.f) const { return Distance(Spot) <= Margin; }
	/** A box round it, from Low to High for a column. */
	FBox GetBounds(float Low, float High) const;
};

/** A piece that broke, for the flying rubble: where it was, its colors and its size (1 = a 100 block). */
struct FRRBrokenBit
{
	FVector Where;
	FLinearColor Body;
	FLinearColor Accent;
	float Size = 1.f;
};

/** Something shown again as a wreck mends: grown back into place. */
struct FRRRegrow
{
	TWeakObjectPtr<UInstancedStaticMeshComponent> Instances;
	int32 Index = INDEX_NONE;
	FTransform Target;
	/** Seconds it waits before it starts growing (a crater fills from the bottom up). */
	float Delay = 0.f;
};

/**
 * Round 99 (the user: "프레임드랍 … 건물 파괴나 재생성 될 때"). A piece of a big instanced mesh that moves for a moment
 * (a part of a building coming down, a block growing back) is played by a copy of it in a small instanced mesh of its own.
 * Moving an instance of the big mesh makes the renderer take the whole mesh in again (the town's blocks are tens of
 * thousands of instances, all in the distance field scene: some 30 ms on the render thread, measured), and the shows did
 * that every frame for up to a second and a half; this way the big mesh is touched once when the show starts or ends.
 * The stand-ins are made on first use, one per big mesh (its mesh, materials and custom data), with nothing that costs:
 * no collision, no navigation, no distance field. Each machine keeps its own.
 */
struct FRRStandIns
{
	struct FPiece
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> StandIn;
		int32 Slot = INDEX_NONE;
	};

	/** A copy of Source's instance Index (its colors too) at Transform (world), on a stand-in belonging to Owner. */
	FPiece Add(AActor* Owner, UInstancedStaticMeshComponent* Source, int32 Index, const FTransform& Transform);
	void Move(const FPiece& Piece, const FTransform& Transform);
	/** The copy goes (its place kept for the next one). */
	void Remove(FPiece& Piece);
	/** Every stand-in destroyed (the owner ending play). */
	void Clear();

private:
	struct FSet
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> StandIn;
		TArray<int32> Free;
	};
	TMap<TWeakObjectPtr<UInstancedStaticMeshComponent>, FSet> Sets;
};

/**
 * Structures the big blows smash (the tiger's spirit, the dragon's breath, the star robot's fists), mended a while later
 * once nobody stands where they were (round 60).
 * The server keeps the list of wrecks (Wreck adds one) and replicates it; every machine smashes and mends its own copy of
 * the map from that list, so all agree and a late joiner sees the same holes:
 * - Dice Town: the blocks above the floor inside a wreck go, block by block (ARRRandomDiceTown::ApplyWrecks), the hidden
 *   collision boxes are cut round the holes, and what stands on what went (roofs, trees, stairs, lamps) goes with it;
 *   giant dice stay.
 * - Dice Land: each piece of cover (a hidden box and the dice, balls and trunks drawn over it) goes whole when a wreck
 *   reaches its box; the plaza's pedestal stays.
 * - The island's cover blocks (ARRRandomCover) go whole the same way; the hills and cliffs stay.
 * Each wreck mends RestoreSeconds after it was made, as soon as nobody is in it (an animal friend, alive or down, a car
 * with someone in it, the robot while it has crew; parked cars don't count): the blocks grow back into place with a
 * sparkle. Rubble flies when it breaks (each machine its own, from the same list).
 * Round 89: what the wrecks make between them in Dice Town (a building whose upper floors lost their support coming down,
 * a crater broken through to the lava) is big: the wrecks that made it wait BigRestoreSeconds from then, and mend only
 * once nobody is anywhere in it either.
 */
UCLASS()
class RRRANDOM_API ARRRandomWreckage : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomWreckage();

	/**
	 * Server: smashes what's within Radius of the line From - To (a column through every height, or a ball round the line),
	 * digging Dig layers into the ground where it reaches the floor. bLasting (round 93, the oracle meteors): never mended.
	 */
	static void Wreck(const UObject* WorldContext, const FVector& From, const FVector& To, float Radius, bool bColumn, int32 Dig = 0, bool bLasting = false, int32 Crush = 0);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seconds a wreck stays at least (rr.Wreck.RestoreSeconds); then it mends once its ground is clear. */
	static float GetRestoreSeconds();
	/** Seconds a collapse or a lava pool stays at least (rr.Wreck.BigRestoreSeconds), from when it happened. */
	static float GetBigRestoreSeconds();

	/** This world's wreckage, if anything was ever smashed. */
	static ARRRandomWreckage* Find(const UObject* WorldContext);
	/** Flying rubble for what broke, on this machine's screen (a collapse landing throws its own). */
	void ThrowRubble(const TArray<FRRBrokenBit>& Bits);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** A piece of the map that goes whole: a hidden box that stops things and the looks drawn over it, or a cover block. */
	struct FPiece
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Box;
		int32 BoxIndex = INDEX_NONE;
		FTransform BoxTransform;
		TWeakObjectPtr<ARRRandomCover> Cover;
		struct FLook
		{
			TWeakObjectPtr<UInstancedStaticMeshComponent> Instances;
			int32 Index;
			FTransform Transform;
		};
		TArray<FLook> Looks;
		bool bBroken = false;
	};

	/** A bit of flying rubble. */
	struct FChunk
	{
		int32 Slot;
		FVector Location;
		FVector Velocity;
		FRotator Turn;
		FRotator Spin;
		float Size;
		float Age;
		float Life;
		float FloorZ;
	};

	struct FGrowing
	{
		FRRRegrow Regrow;
		float Age;
		/** Its copy that does the growing (the big mesh's own instance stays hidden until the whole batch is back). */
		FRRStandIns::FPiece Stand;
	};

	UFUNCTION()
	void OnRep_Wrecks();

	/** This machine's copy of the map, smashed and mended to match Wrecks; rubble and sparkles for what changed. */
	void Apply();
	/** The pieces of Dice Land and the island's cover, found the first time anything is smashed (false: not built yet). */
	bool FindPieces();
	/** Server: true while somebody is in the wreck (with some room round them): an animal friend, a ridden car, the crewed robot. */
	bool IsOccupied(const FRRWreck& Wreck) const;
	/** Server: the wrecks that made the big things Dice Town reports after an Apply wait longer (and keep its ground). */
	void MarkBigWrecks();
	void TickRubble(float DeltaSeconds);
	void TickGrowing(float DeltaSeconds);

	UPROPERTY(ReplicatedUsing = OnRep_Wrecks)
	TArray<FRRWreck> Wrecks;

	/** Round 92: the battle map tiles a wreck reached last time (mended once none does). */
	TArray<TWeakObjectPtr<class ARRRandomDiceTown>> WreckedTiles;

	/** Server: when each wreck (by Id) was made. */
	TMap<int32, float> MadeAt;
	/** Server: the wrecks never mended (the oracle meteors'). */
	TSet<int32> Lasting;
	/** Server: wrecks that made something big: not mended before this time, nor while anybody is in its box. */
	TMap<int32, float> BigUntil;
	TMap<int32, FBox> BigArea;
	int32 NextId = 1;
	float CheckTimer = 0.f;

	/** The wrecks this machine has applied (by Id). */
	TSet<int32> Applied;
	/** To apply at the next tick: wrecks came in (all of a frame's at once), or the map was not built yet. */
	bool bPending = false;
	/** Round 99: changes are applied no more often than this (seconds), a volley's worth at once. */
	static constexpr float ApplyGap = 0.2f;
	float LastApplyTime = -100.f;

	TArray<FPiece> Pieces;
	bool bFoundPieces = false;

	/** The rubble: a pool of the town's block, lit like the town. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Rubble;
	TArray<FChunk> Chunks;
	TArray<int32> FreeSlots;
	TArray<FGrowing> Growing;
	FRRStandIns GrowStandIns;
	/** Every piece growing back is in place: the big meshes take them over and the stand-ins go. */
	void FinishGrowing();
};
