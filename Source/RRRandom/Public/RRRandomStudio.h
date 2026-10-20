#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomStudio.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;

/**
 * "Studio" (round 61), a test map for animation work: nothing but a wide flat floor in a soft grey with a grid drawn on
 * it, a line every GridStep, darker every MajorEvery lines, the two lines through the middle in color (X pink, Y mint),
 * so steps, strides and sliding feet can be read off it. No cover, no walls, no props; the game mode parks only the star
 * team's cars (far out, for RRStarCars) and the other team's bots stand still. Third person only: -RRThirdPerson
 * -RRMap=Studio (the title doesn't list it). Look at a brawler or the robot from a fixed side with RRStudioCam.
 * Every machine builds the same floor; the game mode spawns it on the host.
 */
UCLASS()
class RRRANDOM_API ARRRandomStudio : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomStudio();

	/** The teams start this far apart, facing each other across the middle. */
	static constexpr float TeamSpawnDistance = 3000.f;

	bool IsBuilt() const { return bBuilt; }

protected:
	virtual void BeginPlay() override;

private:
	void Build();

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Floor;

	/** The grid's lines: thin ones, the darker every MajorEvery-th, and the two through the middle. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> MinorLines;
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> MajorLines;
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> AxisX;
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> AxisY;

	bool bBuilt = false;
};
