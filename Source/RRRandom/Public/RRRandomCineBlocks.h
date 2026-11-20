#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomCineBlocks.generated.h"

class UInstancedStaticMeshComponent;

USTRUCT()
struct FRRCineBlock
{
	GENERATED_BODY()

	/** The block's middle in the world. */
	UPROPERTY(EditAnywhere, Category = "Cine")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Cine")
	FColor Body = FColor::White;

	/** The top edge's color on a plain block, the pips on a die block. */
	UPROPERTY(EditAnywhere, Category = "Cine")
	FColor Accent = FColor::White;

	UPROPERTY(EditAnywhere, Category = "Cine")
	bool bDie = false;

	/** 1 = a town block (100 across); 0.5 = a small die for a pile of clutter. */
	UPROPERTY(EditAnywhere, Category = "Cine")
	float Scale = 1.f;

	/** Turned about the up axis, degrees. */
	UPROPERTY(EditAnywhere, Category = "Cine")
	float Yaw = 0.f;
};

/**
 * Cinematic (2026-11-19): extra Dice Town blocks for a set (the fox vs tiger dead end, the clutter the tiger smashes),
 * drawn and colored the way the town draws its own (ARRRandomDiceTown::AddColored: SM_BTBlock / SM_BTDieBlock instances,
 * custom floats 0-2 the body's linear color, 3-5 the accent's), so they sit in the town's look. Rebuilt from Blocks
 * whenever the actor is constructed.
 *
 * In a game world (play, Movie Render Queue) SmashAt knocks every block within reach flying: each one leaves the spot
 * away from the blow and upward, spinning, falls, bounces on the floor and settles (worked out here every frame, no
 * physics engine, so a render comes out the same every time). The town's own blocks are broken by ARRRandomWreckage.
 */
UCLASS()
class RRRANDOM_API ARRRandomCineBlocks : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomCineBlocks();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Game world: knocks the blocks within Radius of Center flying away from From (where the blow comes from). */
	void SmashAt(const FVector& Center, float Radius, const FVector& From);

	UPROPERTY(EditAnywhere, Category = "Cine")
	TArray<FRRCineBlock> Blocks;

	UPROPERTY(VisibleAnywhere, Category = "Cine")
	TObjectPtr<UInstancedStaticMeshComponent> Plain;

	UPROPERTY(VisibleAnywhere, Category = "Cine")
	TObjectPtr<UInstancedStaticMeshComponent> Dice;

private:
	struct FFlying
	{
		int32 Block = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FQuat Rotation = FQuat::Identity;
		FVector Spin = FVector::ZeroVector;
		float Age = 0.f;
		bool bResting = false;
	};

	/** Each block's instance in Plain or Dice (worked out from Blocks' order: the instances were added in it). */
	TArray<int32> InstanceOf;
	TArray<bool> Broken;
	TArray<FFlying> Flying;
	FRandomStream Stream = FRandomStream(1119);
};
