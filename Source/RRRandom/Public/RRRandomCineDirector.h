#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomCineDirector.generated.h"

class ALevelSequenceActor;

UENUM()
enum class ERRCineEffect : uint8
{
	/** The fox's smoke-and-leaves poof at A (Size 1 = one brawler's worth): Fox Clones springing up. */
	FoxPoof,
	/** Dice Town smashed within Size of the line A - B (a ball round it), never mended during the shot. */
	Wreck,
	/** The ultimate die tumbling at the die actor (or A when there is none), from Time for Duration, its owner's feet at B. */
	DieSpin,
	/** The set's own blocks (ARRRandomCineBlocks) within Size of A knocked flying away from B (the blow), with a puff of dust. */
	Smash,
	/** The ultimate die landing at the die actor (or A): rings of gold, a flash, a pillar of light (Size = the die's half size). */
	DieBurst,
};

USTRUCT()
struct FRRCineCue
{
	GENERATED_BODY()

	/** Seconds into the sequence. */
	UPROPERTY(EditAnywhere, Category = "Cine")
	float Time = 0.f;

	/** Seconds it lasts (DieSpin). */
	UPROPERTY(EditAnywhere, Category = "Cine")
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, Category = "Cine")
	ERRCineEffect Kind = ERRCineEffect::FoxPoof;

	UPROPERTY(EditAnywhere, Category = "Cine")
	FVector A = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Cine")
	FVector B = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Cine")
	float Size = 1.f;
};

/**
 * Cinematic (2026-11-19): plays the game's own effects in step with a Level Sequence. The effects (ARRRandomEffects, the
 * Dice Town wreckage) are game code that only runs in a game world, so they show when the sequence plays (PIE, Movie
 * Render Queue), not while scrubbing in the editor. Each frame this reads the sequence's time and fires the cues it went
 * past (a jump of over a second, a scrub, fires nothing).
 */
UCLASS()
class RRRANDOM_API ARRRandomCineDirector : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomCineDirector();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Cine")
	TObjectPtr<ALevelSequenceActor> Sequence;

	/** The golden die the sequence moves (DieSpin / DieBurst follow it). */
	UPROPERTY(EditAnywhere, Category = "Cine")
	TObjectPtr<AActor> GoldDie;

	UPROPERTY(EditAnywhere, Category = "Cine")
	TArray<FRRCineCue> Cues;

private:
	void Fire(const FRRCineCue& Cue) const;
	FVector DieSpot(const FRRCineCue& Cue) const;

	float LastTime = -1.f;
};
