#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomEffects.generated.h"

class UInstancedStaticMeshComponent;

/**
 * Bursts of glowing particles for the brawlers' powers and hits: balls, rings, stars and little dice (/Game/Effects/DiceFX,
 * char-pipeline/scripts/ue_make_fx.py; the star is the wings' sparkle), each an instance tinted through its custom data,
 * flying, growing and fading here. One per machine, made on first use and never replicated: every machine plays the same
 * effects from the replicated state (a brawler turning giant, getting wings, taking a hit), so nothing is sent for them.
 */
UCLASS()
class RRRANDOM_API ARRRandomEffects : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomEffects();

	/** This machine's effects, made the first time they are needed (null without a world). */
	static ARRRandomEffects* Get(const UObject* WorldContext);

	virtual void Tick(float DeltaSeconds) override;

	/** Growing into a giant: a shockwave along the ground, a dust ring, stars and dice thrown up, a flash. Size is the new scale. */
	void GiantBurst(const FVector& Feet, float Size, const FLinearColor& Tint);
	/** Shrinking back: a soft puff and a few stars. */
	void GiantShrink(const FVector& Feet, float Size);
	/** While giant, every frame: a glowing ring pulsing under the feet and motes rising round the body. */
	void GiantAura(const FVector& Feet, float Size, float Height, const FLinearColor& Tint, float DeltaSeconds);
	/** Wings opening: a ring of light and a spray of stars from the back. */
	void WingsBurst(const FVector& Back, const FLinearColor& Tint);
	/** While flying or gliding, every frame: a trail of stars and soft glows from the back, more the faster it goes. */
	void FlightTrail(const FVector& Back, const FVector& Velocity, const FLinearColor& Tint, float DeltaSeconds);
	/** While the die over a head tumbles, every frame: rainbow glitter shed round it (Radius: the die's half size). */
	void DiceSpinSparkle(const FVector& Where, float Radius, float DeltaSeconds);
	/** The die landing on its face: a ring turned to the camera, a flash and a pop of stars. */
	void DiceLandBurst(const FVector& Where, float Radius);
	/**
	 * The tiger's ultimate die tumbling, every frame (Progress 0 .. 1 through its tumble): gold sparks swirling round it
	 * faster and faster, a beam of gold light up from its owner's feet, sparks rising up the beam, rings pulsing out
	 * along the ground.
	 */
	void UltimateDieSpin(const FVector& Where, float Radius, const FVector& Feet, float Progress, float DeltaSeconds);
	/** The ultimate die landing: rings of gold, a flash, a fountain of stars and gold dice, a pillar of light, a shockwave. */
	void UltimateDieBurst(const FVector& Where, float Radius, const FVector& Feet);
	/** A speedster's dash, every frame: streaks of light along the way it runs, crackling sparks and dust at its feet. */
	void SpeedTrail(const FVector& Middle, const FVector& Feet, const FVector& Velocity, const FLinearColor& Tint, float DeltaSeconds);
	/** Setting off or stopping at speed: a ring across the way it goes, a flash and a spray of sparks. */
	void SonicBoom(const FVector& Where, const FVector& Direction, const FLinearColor& Tint);
	/** A brawler vanishing or appearing (teleported into the dance line): a flash, a ring and a pop of stars at its middle. */
	void TeleportPoof(const FVector& Where, const FLinearColor& Tint);
	/** The tiger spirit's paws hitting the ground: shockwaves out to Radius, a flash, a ring of dust, stars and dice thrown up. */
	void TigerStrike(const FVector& Where, float Radius, const FLinearColor& Tint);
	/** A hit: sparks and stars thrown back toward where the shot came from, a quick ring and a flash; Strength 1 is a pistol shot. */
	void HitBurst(const FVector& Where, const FVector& FromDirection, float Strength, const FLinearColor& Tint);

private:
	enum class EShape : uint8 { Ball, Ring, Star, Die, Count };

	struct FParticle
	{
		EShape Shape = EShape::Ball;
		int32 Slot = INDEX_NONE;
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		/** Units per second squared, down. */
		float Gravity = 0.f;
		/** Share of the speed lost per second. */
		float Drag = 0.f;
		float Age = 0.f;
		float Life = 1.f;
		/** Size (scale of the 100 unit mesh, the star's 10) at the start and the end, eased between. */
		FVector StartSize = FVector::OneVector;
		FVector EndSize = FVector::OneVector;
		FRotator Turn = FRotator::ZeroRotator;
		/** Degrees per second. */
		FRotator Spin = FRotator::ZeroRotator;
		FLinearColor Color = FLinearColor::White;
		float Glow = 1.f;
		float Opacity = 1.f;
		/** Seconds to pop in before fading over the rest of the life. */
		float FadeIn = 0.05f;
		/** Turned toward the camera every frame (stars). */
		bool bFacesCamera = false;
	};

	void Spawn(const FParticle& Particle);
	/** Little glowing particles thrown out from a spot. */
	void Spray(EShape Shape, const FVector& Where, int32 Count, const FVector& Direction, float Cone, FVector2D Speed, FVector2D Size, FVector2D Life,
		const FLinearColor& Color, float Glow, float Gravity);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;

	/** Each pool's unused slots (hidden, no size). */
	TArray<TArray<int32>> FreeSlots;
	TArray<FParticle> Live;
};
