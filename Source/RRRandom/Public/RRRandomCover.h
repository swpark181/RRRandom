#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomCover.generated.h"

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
 * Uses the engine's basic cube so it needs no project assets.
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

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cover")
	TObjectPtr<UStaticMeshComponent> Block;

	UPROPERTY(EditAnywhere, Category = "Cover")
	FVector Size = FVector(100.f, 300.f, 160.f);

	UPROPERTY(EditAnywhere, Category = "Cover")
	FLinearColor Color = FLinearColor(0.35f, 0.3f, 0.25f);
};
