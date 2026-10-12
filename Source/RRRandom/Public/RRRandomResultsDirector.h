#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomResultsDirector.generated.h"

class ARRRandomCharacter;
class UCameraComponent;
class ULightComponent;
class UPointLightComponent;
class USpotLightComponent;

/**
 * The results screen's show, on one machine: a camera that the player's view blends to, at the MVP's height in front of
 * it (from the first angle with nothing in the way) and swaying gently from side to side, fireworks (ARRRandomFirework)
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

	void Setup(ARRRandomCharacter* InMVP);

	/** Camera distance from the MVP (grows with a giant), its pitch (a little down, leaving sky above for the fireworks) and field of view. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	float Distance = 900.f;
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
	void LaunchFirework();
	void UpdateCamera();
	void UpdateLights(float DeltaSeconds);
	void OnFireworkBurst(const FLinearColor& Color);
	/** How far the camera can back off from the MVP along a view before something is in the way. */
	float GetClearReach(const FRotator& View, float Reach) const;
	/** How open a view is, 0 to 1: its middle and sides near the camera checked for cover and hills. */
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
	float Elapsed = 0.f;
	float StartYaw = 0.f;
	float FireworkTimer = 0.f;
	FRandomStream Stream;
	/** Dimmed lights and their own brightness, to put back. */
	TArray<TPair<TWeakObjectPtr<ULightComponent>, float>> DimmedLights;
	TArray<TPair<TWeakObjectPtr<class USkyLightComponent>, float>> DimmedSkies;
};
