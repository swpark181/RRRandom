#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomCannon.generated.h"

class ARRRandomCharacter;
class ARRRandomDiceTown;
class UCapsuleComponent;
class UStaticMeshComponent;

/**
 * A human cannon (round 88): a jump pad that shoots an animal friend across the map. Always a set with its own landing pad,
 * far off, and one-way (no cannon stands at the pad). An animal friend next to it presses F (ARRRandomCharacter::ToggleRide):
 * it climbs into the barrel (hidden, the barrel squashing down for LoadTime), then BOOM! out of the muzzle along an arc
 * that comes down on the middle of the pad (GetArcPoint, over everything in the way, under the map's flight ceiling). It may
 * shoot all the way; something in the way ends the arc and it just falls. The cannon reloads for ReloadTime after a shot.
 * The arc is the flier's own (ARRRandomCharacter's cannon ride): the controlling machine moves it, the server trusts that
 * client meanwhile, like a climb. This actor only holds the set: where the pad is, when it last fired, and the show.
 * Dice Town has four sets, each team two from beside its base out to the far flanks (SpawnDiceTownSets).
 * Looks: /Game/Props/Cannon/SM_Cannon (barrel up and forward along +X) and SM_CannonPad, or engine shapes without them.
 */
UCLASS(NotPlaceable)
class RRRANDOM_API ARRRandomCannon : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomCannon();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Seconds inside the barrel before the shot, and before the cannon can take the next one after it. */
	static constexpr float LoadTime = 0.5f;
	static constexpr float ReloadTime = 2.f;
	/** Animal friends this close (flat, from its feet) can climb in. */
	static constexpr float UseRange = 180.f;

	/** Server, before it finishes spawning: where its pad lies (on the floor; the pad's pivot is the middle of its bottom). */
	void SetPad(const FVector& Where);
	const FVector& GetPadLocation() const { return PadLocation; }
	/** Where the flier sets off from: the barrel's mouth. */
	FVector GetMuzzleLocation() const;
	/** Whether it can take someone now (loaded and not busy). */
	bool IsReady() const;
	/** 0 just fired .. 1 ready again, on this machine's clock. */
	float GetReloadShare() const;
	/** Its number among the cannons (spawn order), for RRCannon and the HUD. */
	int32 GetIndex() const { return Index; }

	/**
	 * The arc for a flier whose capsule middle is HalfHeight over its feet: where it is a share Alpha (0 .. 1) of the way
	 * from the muzzle to just over the pad, and the seconds the whole way takes.
	 */
	FVector GetArcPoint(float Alpha, float HalfHeight) const;
	/** The arc's speed (units a second) at Alpha, for the flier's lean and trail. */
	FVector GetArcVelocity(float Alpha, float HalfHeight) const;
	float GetFlightTime() const;

	/** Server: Who climbs in and is shot to the pad. False if it can't now. */
	bool Fire(ARRRandomCharacter* Who);

	/** The ready cannon nearest to Who that it could climb into now, or null. */
	static ARRRandomCannon* FindInReach(const ARRRandomCharacter* Who);
	/** Server: Dice Town's four sets, on open ground near the spots they are meant for. */
	static void SpawnDiceTownSets(UWorld* World, const ARRRandomDiceTown* Town);
	/** Server: a set whose cannon stands at From, aimed at a pad at To (both on the floor). */
	static ARRRandomCannon* SpawnSet(UWorld* World, const FVector& From, const FVector& To, int32 InIndex);
	/**
	 * Open ground in Dice Town near Want (world X, Y): the nearest spot whose ground within Clear is all bare level-0 floor
	 * (no blocks, stairs, roofs or canals) with nothing overhead; its spot on the floor. False with none within Reach. Accept: any
	 * other test a spot must pass (a cannon's: its arc clear).
	 */
	static bool FindOpenGround(UWorld* World, const ARRRandomDiceTown* Town, const FVector2D& Want, float Clear, FVector& Out,
		TFunctionRef<bool(const FVector&)> Accept = [](const FVector&) { return true; }, float Reach = 2000.f);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_PadLocation();
	UFUNCTION()
	void OnRep_FireCount();
	void PlacePad();

	UPROPERTY(VisibleAnywhere, Category = "Cannon")
	TObjectPtr<UStaticMeshComponent> Body;

	/** The landing pad, out where it lies (absolute, not carried by the cannon). */
	UPROPERTY(VisibleAnywhere, Category = "Cannon")
	TObjectPtr<UStaticMeshComponent> Pad;

	/** What stops animal friends and shots at the cannon (hidden): a squat capsule round its base. */
	UPROPERTY(VisibleAnywhere, Category = "Cannon")
	TObjectPtr<UCapsuleComponent> Blocker;

	UPROPERTY(ReplicatedUsing = OnRep_PadLocation)
	FVector_NetQuantize PadLocation = FVector::ZeroVector;

	/** Counts shots so every machine plays each one (only the change matters). */
	UPROPERTY(ReplicatedUsing = OnRep_FireCount)
	uint8 FireCount = 0;

	UPROPERTY(Replicated)
	int32 Index = 0;

	/** This machine: when it last saw a shot begin (the load), and whether that shot's BOOM has played. */
	float LoadStartTime = -100.f;
	bool bBoomShown = true;
	/** Server: when the last one climbed in. */
	float LastLoadTime = -100.f;
	/** The body's size as built (the barrel's squash is relative to it). */
	FVector BodyScale = FVector::OneVector;
};
