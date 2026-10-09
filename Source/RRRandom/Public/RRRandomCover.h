#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomCover.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

/** Where the game mode puts one block of cover, relative to the player start. */
USTRUCT()
struct FRRCoverPlacement
{
	GENERATED_BODY()

	/** Center on the floor, from the player start: X is screen up, Y is screen right. */
	UPROPERTY(EditAnywhere, Category = "Cover")
	FVector2D Offset = FVector2D::ZeroVector;

	/** Full width (X), depth (Y) and height. */
	UPROPERTY(EditAnywhere, Category = "Cover")
	FVector Size = FVector(100.f, 300.f, 160.f);
};

/**
 * Solid block that stops shots and brawlers. Taller than the height shots fly at, so standing behind it is safe.
 * Its collision is the engine's basic cube, hidden: what shows is a row of grass-topped soil blocks
 * (/Game/Environment/Grass/SM_GrassBlock_*, the Tripo grass blocks) laid over the same box, about BlockCellSize each,
 * a random one of the set per cell and turned at random, picked the same way on every machine from where the cover
 * stands. Without those assets the cube shows instead.
 */
UCLASS()
class RRRANDOM_API ARRRandomCover : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomCover();

	/** Resizes the block; its pivot is at the bottom center, so it stands on the spot it is placed at. */
	void SetSize(const FVector& NewSize);

	/** Distance from the center to the block's outline along a flat direction. */
	float GetExtentAlong(const FVector& Direction) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cover")
	TObjectPtr<UStaticMeshComponent> Block;

	/** Replicated so cover the server lays out has the same shape on every client. */
	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Size, Category = "Cover")
	FVector Size = FVector(100.f, 300.f, 160.f);

	UPROPERTY(EditAnywhere, Category = "Cover")
	FLinearColor Color = FLinearColor(0.35f, 0.3f, 0.25f);

	/** The grass blocks the cover is built of, each a 100 cube with its pivot in the middle of the bottom. */
	UPROPERTY(EditAnywhere, Category = "Cover|Look")
	TArray<TObjectPtr<UStaticMesh>> BlockMeshes;

	/** Roughly how wide each block is; the cover is split into whole cells near this size. */
	UPROPERTY(EditAnywhere, Category = "Cover|Look")
	float BlockCellSize = 130.f;

	/** Blocks are this much bigger than their cell so neighbors' grass tops meet. */
	UPROPERTY(EditAnywhere, Category = "Cover|Look")
	float BlockOverlap = 1.08f;

	/** One instance list per block mesh. */
	UPROPERTY(VisibleAnywhere, Category = "Cover|Look")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> BlockLooks;

private:
	UFUNCTION()
	void OnRep_Size();

	/** Lays the grass blocks over the current size. */
	void RebuildLook();
};
