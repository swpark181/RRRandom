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
 */
UCLASS()
class RRRANDOM_API ARRRandomWreckage : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomWreckage();

	/** Server: smashes what's within Radius of the line From - To (a column through every height, or a ball round the line). */
	static void Wreck(const UObject* WorldContext, const FVector& From, const FVector& To, float Radius, bool bColumn);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seconds a wreck stays at least (rr.Wreck.RestoreSeconds); then it mends once its ground is clear. */
	static float GetRestoreSeconds();

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
	};

	UFUNCTION()
	void OnRep_Wrecks();

	/** This machine's copy of the map, smashed and mended to match Wrecks; rubble and sparkles for what changed. */
	void Apply();
	/** The pieces of Dice Land and the island's cover, found the first time anything is smashed (false: not built yet). */
	bool FindPieces();
	/** Server: true while somebody is in the wreck (with some room round them): an animal friend, a ridden car, the crewed robot. */
	bool IsOccupied(const FRRWreck& Wreck) const;
	void ThrowRubble(const TArray<FRRBrokenBit>& Bits);
	void TickRubble(float DeltaSeconds);
	void TickGrowing(float DeltaSeconds);

	UPROPERTY(ReplicatedUsing = OnRep_Wrecks)
	TArray<FRRWreck> Wrecks;

	/** Server: when each wreck (by Id) was made. */
	TMap<int32, float> MadeAt;
	int32 NextId = 1;
	float CheckTimer = 0.f;

	/** The wrecks this machine has applied (by Id). */
	TSet<int32> Applied;
	/** Waiting for the map to be built before applying. */
	bool bPending = false;

	TArray<FPiece> Pieces;
	bool bFoundPieces = false;

	/** The rubble: a pool of the town's block, lit like the town. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Rubble;
	TArray<FChunk> Chunks;
	TArray<int32> FreeSlots;
	TArray<FGrowing> Growing;
};
