#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraTypes.h"
#include "RRRandomResultsDirector.generated.h"

class ARRRandomCharacter;
class UCameraComponent;
class ULightComponent;
class UPointLightComponent;
class USpotLightComponent;

/**
 * The results screen's show, on one machine: a camera at the MVP's height in front of it (straight at the winners' dance
 * line in front of the map's centerpiece, when there is one; else from the first angle with nothing in the way) and swaying gently from side to side, which it moves to from the player's view by itself, rounding
 * the MVP and keeping it in sight (a plain view blend swung up to the sky on the way), the dancers turned to face it, fireworks (ARRRandomFirework)
 * going up behind it, and the sun and sky dimmed so their lights stand out. The MVP stays readable against the bright
 * bursts: a soft key light from above the camera, and a glow in front of it that flashes in each burst's color, as if
 * the fireworks were reflecting off it (the toon materials have no specular reflections of their own). Spawned by the local player controller when the match ends; puts the lights back when it goes away.
 */
UCLASS(NotPlaceable)
class RRRANDOM_API ARRRandomResultsDirector : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomResultsDirector();

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Starts the show from the view the player has now (its place, aim, field of view and post process), moving in over BlendTime seconds. */
	void Setup(ARRRandomCharacter* InMVP, const FMinimalViewInfo& InFromView, float InBlendTime);

	/** Camera distance from the MVP (grows with a giant; far enough to take in the dance line behind it), its pitch (a little down, leaving sky above for the fireworks) and field of view. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float Distance = 1100.f;
	UPROPERTY(EditAnywhere, Category = "Camera")
	float Pitch = -2.f;
	UPROPERTY(EditAnywhere, Category = "Camera")
	float FieldOfView = 42.f;
	/** Side to side sway: degrees each way and seconds for a full swing. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float SwayAngle = 6.f;
	UPROPERTY(EditAnywhere, Category = "Camera")
	float SwayPeriod = 14.f;

	/** Seconds between fireworks, and how far behind the MVP (as the camera sees it) and to the sides they go up. */
	UPROPERTY(EditAnywhere, Category = "Fireworks")
	FVector2D FireworkInterval = FVector2D(0.35f, 0.8f);
	UPROPERTY(EditAnywhere, Category = "Fireworks")
	FVector2D FireworkDepth = FVector2D(700.f, 1400.f);
	UPROPERTY(EditAnywhere, Category = "Fireworks")
	float FireworkSpread = 900.f;
	UPROPERTY(EditAnywhere, Category = "Fireworks")
	FVector2D FireworkHeight = FVector2D(260.f, 480.f);

	/** The sun and sky are turned down to this share, over DimTime seconds. */
	UPROPERTY(EditAnywhere, Category = "Lights")
	float DimTo = 0.3f;
	UPROPERTY(EditAnywhere, Category = "Lights")
	float DimTime = 2.f;

	/** The key light on the MVP: a spot from this far above the camera, and how bright. */
	UPROPERTY(EditAnywhere, Category = "Lights")
	float KeyLightHeight = 250.f;
	UPROPERTY(EditAnywhere, Category = "Lights")
	float KeyLightIntensity = 40000.f;

	/** The burst glow: how bright at a burst, how long it takes to fade, and where it sits (toward the camera, up). */
	UPROPERTY(EditAnywhere, Category = "Lights")
	float GlowIntensity = 25000.f;
	UPROPERTY(EditAnywhere, Category = "Lights")
	float GlowFadeTime = 0.9f;
	UPROPERTY(EditAnywhere, Category = "Lights")
	FVector2D GlowOffset = FVector2D(220.f, 120.f);

private:
	/** What the camera looks at: the middle of the dance line in front of the map's centerpiece, or else the MVP. */
	FVector GetTarget() const;
	void LaunchFirework();
	void UpdateCamera();
	void UpdateLights(float DeltaSeconds);
	void OnFireworkBurst(const FLinearColor& Color);
	/** How far the camera can back off from the MVP along a view before something is in the way. */
	float GetClearReach(const FRotator& View, float Reach) const;
	/** How far the camera can back off from the MVP along a view (up to Reach) before another brawler is in the way: hiding the MVP, or in the frame well in front of it. */
	float GetBrawlerReach(const FRotator& View, float Reach) const;
	/** How open a view is, 0 to 1: its middle and sides near the camera checked for cover, hills and other brawlers. */
	float GetViewOpenness(const FRotator& View) const;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(VisibleAnywhere, Category = "Lights")
	TObjectPtr<USpotLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, Category = "Lights")
	TObjectPtr<UPointLightComponent> Glow;

	/** Seconds since the last burst, and its color. */
	float SinceBurst = 100.f;
	FLinearColor GlowColor = FLinearColor::White;

	TWeakObjectPtr<ARRRandomCharacter> MVP;
	/** The winners' dance line (ARRRandomGameState::GetDanceLine): its middle, where the MVP is headed. */
	bool bDanceLine = false;
	FVector DanceLineCenter = FVector::ZeroVector;
	/** Where the camera ends up (before the sway), to keep fireworks from going up behind the centerpiece. */
	FVector ShotPoint = FVector::ZeroVector;
	float Elapsed = 0.f;
	float StartYaw = 0.f;
	/** The camera stays this close to the MVP, in front of another brawler no angle could avoid (huge when none). */
	float BrawlerReach = UE_BIG_NUMBER;
	/** The player's view the camera moves in from, and over how many seconds. */
	FMinimalViewInfo FromView;
	float BlendTime = 0.f;
	float FireworkTimer = 0.f;
	FRandomStream Stream;
	/** Dimmed lights and their own brightness, to put back. */
	TArray<TPair<TWeakObjectPtr<ULightComponent>, float>> DimmedLights;
	TArray<TPair<TWeakObjectPtr<class USkyLightComponent>, float>> DimmedSkies;
};
