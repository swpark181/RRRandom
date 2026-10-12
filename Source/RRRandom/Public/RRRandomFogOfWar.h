#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomFogOfWar.generated.h"

class APlayerController;
class APostProcessVolume;
class ARRRandomCharacter;
class UMaterialInstanceDynamic;
class UTexture2D;

/**
 * StarCraft-style fog of war for one local player's screen. Not replicated: each machine spawns its own for its own
 * player (ARRRandomPlayerController), so it only changes what that player sees; shots, bots and everything else in
 * the game stay as they were.
 * The arena floor is covered by a Resolution x Resolution grid. Every frame each living brawler on the viewer's team
 * (the player and allied bots) sees a circle of its VisionRadius around itself (the edge softened over EdgeSoftness,
 * centered on the radius), cut short behind anything taller than half of it (cover, hills of 2 levels and up, cliffs,
 * the waterfall); cells seen now are visible, cells ever seen stay explored. The grid goes to
 * /Game/PostProcess/M_PP_Fog through a texture: never-seen ground is nearly black, explored ground dark and grey,
 * the circles clear.
 * Opponents outside the team's sight (ARRRandomCharacter::IsSeenByTeam, the same test the bots go by) are hidden on
 * this screen (their body, gun, wings, die and the HUD over them).
 */
UCLASS()
class RRRANDOM_API ARRRandomFogOfWar : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomFogOfWar();

	virtual void Tick(float DeltaSeconds) override;

	/** The vision circle fades out over this much of its edge. */
	/**
	 * How much of a spot the team sees right now (0 to 1) and, in OutExplored, whether it has ever been seen. Everything
	 * counts as seen until the fog has set itself up, and once the match is over.
	 */
	float GetSightAt(const FVector& WorldLocation, bool* OutExplored = nullptr) const;

	/** When the match ends the fog lifts at this rate (its whole strength per second). */
	UPROPERTY(EditAnywhere, Category = "Fog")
	float ResultsFadeSpeed = 3.f;

	UPROPERTY(EditAnywhere, Category = "Fog")
	float EdgeSoftness = 200.f;

	/** Directions each teammate looks in for things blocking its view (traces per teammate per frame). */
	UPROPERTY(EditAnywhere, Category = "Fog", meta = (ClampMin = "8"))
	int32 SightRays = 120;

	/** Cells along each side of the fog grid. */
	UPROPERTY(EditAnywhere, Category = "Fog")
	int32 Resolution = 128;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Finds the floor under the viewer, sizes the grid to it and puts the fog on screen. False until there is a floor. */
	bool Setup(const FVector& Near);
	/**
	 * Sight around one teammate: full inside its VisionRadius - EdgeSoftness / 2, gone at VisionRadius + EdgeSoftness / 2,
	 * and cut off along SightRays directions wherever something taller than half of it stands
	 * (ARRRandomCharacter::GetSightDistance).
	 */
	void Stamp(const ARRRandomCharacter& Brawler);
	void Upload();
	/**
	 * Hides an opponent on this screen only (components, so nothing replicates), or shows it again. Only the parts that
	 * were showing get hidden and come back: the collision capsule and other parts hidden in game stay hidden (shown, the
	 * capsule draws as a wireframe).
	 */
	void HideBrawler(ARRRandomCharacter* Brawler);
	void ShowBrawler(ARRRandomCharacter* Brawler);
	void ShowAll();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> FogTexture;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FogMaterial;

	UPROPERTY(Transient)
	TObjectPtr<APostProcessVolume> Volume;

	/** Lower corner of the grid and its side, in world units. */
	FVector2D Origin = FVector2D::ZeroVector;
	float Size = 0.f;
	bool bReady = false;

	TArray<float> Visible;
	TArray<float> Explored;
	/** Scratch: how far the teammate being stamped sees along each ray. */
	TArray<float> SightRayLengths;
	/** Opponents currently hidden on this screen, with the parts that were hidden for it. */
	TMap<TWeakObjectPtr<ARRRandomCharacter>, TArray<TWeakObjectPtr<UPrimitiveComponent>>> Hidden;
};
