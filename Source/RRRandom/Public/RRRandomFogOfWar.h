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
 * /Game/PostProcess/M_PP_Fog through a texture: never-seen ground is nearly black under slowly drifting cloud,
 * explored ground dark and grey under a thin haze, the circles clear, their edges breaking up like parting cloud.
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

	/**
	 * How much of a spot the team sees right now (0 to 1, smoothly between the grid's cells) and, in OutExplored, whether
	 * it has ever been seen. Everything counts as seen until the fog has set itself up, and once the match is over.
	 */
	float GetSightAt(const FVector& WorldLocation, bool* OutExplored = nullptr) const;

	/** Shows or hides the fog's shading on screen (the third-person camera looks across the ground, which the shading isn't made
	 * for); who can be seen stays the same either way. */
	void SetShadingVisible(bool bVisible);

	/** When the match ends the fog lifts at this rate (its whole strength per second). */
	UPROPERTY(EditAnywhere, Category = "Fog")
	float ResultsFadeSpeed = 3.f;

	/** The vision circle fades out over this much of its edge (the material breaks the fade up into billowing cloud). */
	UPROPERTY(EditAnywhere, Category = "Fog")
	float EdgeSoftness = 300.f;

	/** The shade behind something blocking the view fades in over this distance, so it has no stair steps along the grid. */
	UPROPERTY(EditAnywhere, Category = "Fog")
	float ShadowSoftness = 120.f;

	/** Directions each teammate looks in for things blocking its view (traces per teammate per frame). */
	UPROPERTY(EditAnywhere, Category = "Fog", meta = (ClampMin = "8"))
	int32 SightRays = 240;

	/**
	 * Small blurs on the grids before they go to the screen (each about two cells wide): the sight cones meet in
	 * creases that the sharpened fade would show as straight lines. Gameplay (GetSightAt) reads the sharp grids.
	 */
	UPROPERTY(EditAnywhere, Category = "Fog", meta = (ClampMin = "0"))
	int32 UploadBlurPasses = 2;

	/** Cells along each side of the fog grid. */
	UPROPERTY(EditAnywhere, Category = "Fog")
	int32 Resolution = 256;

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
	/** A soft copy of a grid for the screen (UploadBlurPasses of a small binomial blur). */
	void BlurForUpload(const TArray<float>& Source, TArray<float>& Out);
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
	/** Scratch: the grids as blurred for the screen. */
	TArray<float> BlurredVisible;
	TArray<float> BlurredExplored;
	TArray<float> BlurScratch;
	/** Opponents currently hidden on this screen, with the parts that were hidden for it. */
	TMap<TWeakObjectPtr<ARRRandomCharacter>, TArray<TWeakObjectPtr<UPrimitiveComponent>>> Hidden;
};
