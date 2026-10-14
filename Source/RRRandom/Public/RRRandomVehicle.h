#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomVehicle.generated.h"

class ARRRandomCharacter;
class UStaticMesh;
class UStaticMeshComponent;

UENUM()
enum class ERRVehicleKind : uint8
{
	Kickboard,
	Motorbike,
	Car,
};

/** How a kind of vehicle rides: the brawler's movement while on it, and where it stands or sits. */
struct FRRVehicleHandling
{
	/** Top speed, as a multiple of walking (dice and other buffs still apply on top). */
	float SpeedMultiplier = 1.f;
	/** How fast it gets up to speed (the movement's MaxAcceleration). */
	float Acceleration = 2048.f;
	/**
	 * Grip: how fast the velocity swings round to where the rider steers (the movement's GroundFriction). Low grip with
	 * a quick turn rate is a drift: the vehicle faces the new way while still sliding the old one.
	 */
	float Grip = 8.f;
	/** How fast it turns to face the steering, degrees a second. */
	float TurnRate = 640.f;
	/** How hard it slows with no input, units a second each second (the movement's BrakingDecelerationWalking). */
	float Braking = 2048.f;
	/** Whether it drifts at all; drifting leans the body (bike) or rolls it a little (car). */
	bool bDrifts = false;
	/** How high the rider's feet are above the ground on it (on the deck, in the seat). */
	float SeatHeight = 0.f;
	/** Degrees of lean per degree of drift, and the most. */
	float LeanPerDrift = 0.f;
	float MaxLean = 0.f;
	const TCHAR* Name = TEXT("");
	/** Where the driver sits, forward of the vehicle's middle; and whether there is a rear seat for a passenger, and where. */
	float DriverSeatX = 0.f;
	bool bRearSeat = false;
	float RearSeatX = 0.f;
	/**
	 * Tyre marks while drifting (ARRRandomSkidMarks): how wide at full speed, and where the rear wheels are, as shares
	 * of the mesh's half length (behind the middle) and half width (either side; 0 for one wheel in the middle).
	 */
	float SkidWidth = 0.f;
	float RearAxleShare = 0.6f;
	float TrackShare = 0.f;
};

/**
 * Something to ride: a kickboard, a motorbike or a car (Tripo models, char-pipeline/scripts/blender_vehicles.py). Parked,
 * it stands on the floor where the game mode put it (or where its last rider got off), with no collision: brawlers and
 * shots pass through it. A brawler next to it presses F to get on (ARRRandomCharacter::ToggleRide); then it rides along
 * under them, attached to their capsule, and their movement takes on its handling (FRRVehicleHandling): faster with
 * each kind, and the motorbike and the car slide into drifts in tight turns. The car has a rear seat: a second brawler
 * pressing F next to it while it has a driver gets in behind and is carried along (it can still shoot, not move).
 * F again, a knockout or the flight power puts it back down (the driver getting off lets the passenger out too).
 * The car is a weapon too: an enemy it drives into faster than walking is knocked out (the driver gets the point),
 * slower it hurts them by its speed and throws them aside.
 * Replicated: who drives it, who sits behind, and its kind.
 */
UCLASS(NotPlaceable)
class RRRANDOM_API ARRRandomVehicle : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomVehicle();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Set before the vehicle finishes spawning. */
	void SetKind(ERRVehicleKind NewKind);
	ERRVehicleKind GetKind() const { return Kind; }
	static const FRRVehicleHandling& GetHandling(ERRVehicleKind InKind);
	const FRRVehicleHandling& GetHandling() const { return GetHandling(Kind); }

	/** The driver, who steers it; and the one in the rear seat, along for the ride. */
	ARRRandomCharacter* GetRider() const { return Rider; }
	ARRRandomCharacter* GetPassenger() const { return Passenger; }
	/** Half the vehicle's width, for stepping off beside it. */
	float GetHalfWidth() const;
	/** A seat free: the driver's, or the rear one with a driver in front. */
	bool HasRoom() const { return !Rider || (GetHandling().bRearSeat && !Passenger); }
	/** Whether the next one to get on would ride in the rear seat. */
	bool IsNextSeatRear() const { return Rider != nullptr; }

	/**
	 * Server only: the driver's seat if free (the vehicle goes under the brawler's feet, riding along with it), else
	 * the rear seat (the brawler is carried along; see ARRRandomCharacter). Returns 0 for the driver, 1 for the rear
	 * seat, -1 if there was no room.
	 */
	int32 Mount(ARRRandomCharacter* NewRider);
	/** Server only: the driver gets off and the vehicle stays parked where it is, level. */
	void Dismount();
	/** Server only: empties the rear seat. */
	void RemovePassenger(ARRRandomCharacter* Who);
	/** Where a passenger's capsule middle sits, relative to the vehicle's pivot (the middle of its bottom). */
	FVector GetRearSeatLocation(float CapsuleHalfHeight) const { return FVector(GetHandling().RearSeatX, 0.f, CapsuleHalfHeight); }

	/** Brawlers this close (flat) to a parked vehicle can get on. */
	static constexpr float MountRange = 170.f;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void ApplyKind();

	UFUNCTION()
	void OnRep_Kind();

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Kind, Category = "Vehicle")
	ERRVehicleKind Kind = ERRVehicleKind::Kickboard;

	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomCharacter> Rider;

	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomCharacter> Passenger;

	/** The meshes for each kind, by ERRVehicleKind. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMesh>> KindMeshes;

	/** The body's lean into a drift right now, degrees. */
	float Lean = 0.f;

	/** Server, the car: an enemy it drives into faster than walking is knocked out, slower takes damage by the speed. */
	void UpdateRam();

	/** Who the car has just hit and when, so one bump hits once. */
	TArray<TPair<TWeakObjectPtr<ARRRandomCharacter>, float>> RamTimes;

	/** Lays tyre marks behind the rear wheels while drifting on the ground, every machine for itself. */
	void UpdateSkidMarks(bool bDrifting, float Speed);

	/** Where each rear wheel's mark last ended; empty when not marking. */
	TArray<FVector> SkidSpots;
};
