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
	/** The star team: three cars that combine into one robot (ARRRandomStarRobot): its chest and head, its arms, its legs. */
	StarChest,
	StarArms,
	StarLegs,
	/**
	 * The planes (round 88), given by a plane station's die (ARRRandomPlaneStation): one seat, flown and fought from, the
	 * plane taking the hits. Never parked: put down, a plane is gone; shot down, it falls and blows up.
	 */
	PlaneWood,
	PlaneFighter,
	PlaneSpaceship,
};

/** What a plane shoots when its pilot fires: the pilot's own gun, or the plane's. */
enum class ERRPlaneGun : uint8
{
	/** The wooden plane: the pilot leans out with its own gun. */
	Pilot,
	/** The fighter: twin machine guns in the nose, fast and light. */
	Guns,
	/** The spaceship: heavy laser bolts from its wing tips, slow. */
	Laser,
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
	/**
	 * Above the vehicle's bottom: the deck the rider stands on (the kickboard), or the top of the driver's seat cushion,
	 * which the rider sits on with its hips over the seat's spot (ARRRandomCharacter::GetRideOffset).
	 */
	float SeatHeight = 0.f;
	/** Degrees of lean per degree of drift, and the most. */
	float LeanPerDrift = 0.f;
	float MaxLean = 0.f;
	const TCHAR* Name = TEXT("");
	/** Where the driver's hips go, forward of the vehicle's middle; and whether there is a rear seat for a passenger, and where. */
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
	/** How far right of the middle the driver's and the other seat are: side by side seats are left and right of it. */
	float DriverSeatY = 0.f;
	float RearSeatY = 0.f;
	/** Whether it runs enemies over like a car (UpdateRam). */
	bool bRams = false;
	/** The top of the rear seat's cushion, like SeatHeight. */
	float RearSeatHeight = 0.f;
	/** A plane: flies (its rider's movement in flying mode), takes hits on its hull for its pilot, and shoots Gun. */
	bool bFlies = false;
	float MaxHealth = 0.f;
	ERRPlaneGun Gun = ERRPlaneGun::Pilot;
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
 * The star team's three cars (chest, arms, legs) ride like cars, two seats each, and combine into a robot
 * (ARRRandomStarRobot) once gathered: they are hidden inside it meanwhile (SetCombined).
 * Replicated: who drives it, who sits behind, its kind and whether it is in the robot.
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
	/** How tall it is (the star team's cars stand tall round their seats: the camera rises over them). */
	float GetHeight() const;
	/** A seat free: the driver's, or the rear one with a driver in front. None while it is part of the robot. */
	bool HasRoom() const { return !bCombined && !IsPlane() && (!Rider || (GetHandling().bRearSeat && !Passenger)); }

	/** One of the star team's three cars, and which (0 chest, 1 arms, 2 legs), or -1. */
	int32 GetStarPart() const { return Kind >= ERRVehicleKind::StarChest && Kind <= ERRVehicleKind::StarLegs ? static_cast<int32>(Kind) - static_cast<int32>(ERRVehicleKind::StarChest) : -1; }

	/**
	 * A plane (round 88): boarded only where it pops in (ARRRandomPlaneStation, ARRRandomCharacter::BoardPlane), flown and
	 * fought from by its one pilot. Its hull takes the hits meant for the pilot (TakeDamage, from shots on the hull or on the
	 * pilot): at 0 the pilot is thrown out to glide down, and the plane drops straight down, spinning and smoking, and blows
	 * up where it lands, smashing the blocks there (ARRRandomWreckage) and hurting the pilot's opponents round it. Put down
	 * on the ground, it is gone in a puff. Either way its station is told (SetStation).
	 */
	bool IsPlane() const { return GetHandling().bFlies; }
	float GetPlaneHealth() const { return PlaneHealth; }
	float GetPlaneMaxHealth() const { return GetHandling().MaxHealth; }
	bool IsCrashing() const { return bCrashing; }
	/** Server, before it finishes spawning: the station it came from (told when it is gone). */
	void SetStation(class ARRRandomPlaneStation* NewStation);
	/** Server: the hits on the hull (or passed on by its pilot); shots from the pilot's own team do nothing. */
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	/** Server: down it goes (the pilot thrown out first); Killer gets the credit for what its blast does. */
	void ShootDown(AController* Killer);
	/**
	 * A plane's own gun (ERRPlaneGun): the spots its shots come out of, in the world, this frame; seconds between shots, and
	 * the shot's damage (a share of a pistol's), size and speed (shares too) and look.
	 */
	void GetGunMuzzles(TArray<FVector>& OutMuzzles) const;
	static void GetGunStats(ERRPlaneGun Gun, float& OutInterval, float& OutDamage, float& OutScale, float& OutSpeed, uint8& OutTier);
	/** Which team's star cars these are (each team has its own three; only that team can combine them). Set before it
	 * finishes spawning. */
	void SetStarTeam(int32 NewTeam) { StarTeam = NewTeam; }
	int32 GetStarTeam() const { return StarTeam; }
	/**
	 * Server: taken into the robot (hidden, no seats, out of the way) or back out of it, parked at Where facing Yaw on
	 * the floor below it. Its riders must be off first.
	 */
	void SetCombined(bool bNewCombined, const FVector& Where = FVector::ZeroVector, float Yaw = 0.f);
	bool IsCombined() const { return bCombined; }
	/** The mesh of a kind of vehicle (the robot flies these round while combining). */
	UStaticMesh* GetKindMesh(ERRVehicleKind InKind) const;
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
	FVector GetRearSeatLocation(float CapsuleHalfHeight) const { return FVector(GetHandling().RearSeatX, GetHandling().RearSeatY, CapsuleHalfHeight); }

	/** The kickboard is stood on; everything else is sat in (FRRVehicleHandling::SeatHeight). */
	bool IsStoodOn() const { return Kind == ERRVehicleKind::Kickboard; }
	/** The deck's or the cushion's height above the vehicle's bottom, at the driver's seat or the rear one. */
	float GetSeatHeight(bool bRear) const { return bRear ? GetHandling().RearSeatHeight : GetHandling().SeatHeight; }

	/** Brawlers this close (flat) to a parked vehicle can get on. */
	static constexpr float MountRange = 170.f;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void ApplyKind();

	/** A plane, every frame on every machine: banking into turns and nosing up or down with the climb, the propeller turning; shot down, spinning down with a trail of smoke (the server moving it and blowing it up where it lands). */
	void UpdatePlane(float DeltaSeconds);
	/** Server: the shot-down plane hits the ground at Where. */
	void Explode(const FVector& Where);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastExplode(const FVector_NetQuantize& Where);

	/** A plane's hull (replicated: its pilot's HUD shows it). */
	UPROPERTY(Replicated)
	float PlaneHealth = 0.f;

	/** Shot down and falling (replicated: every machine spins it and trails smoke). */
	UPROPERTY(Replicated)
	bool bCrashing = false;

	/** Blown up: no puff as it goes (each machine). */
	bool bExploded = false;
	/** Server: its station, who shot it down, and how fast it falls. */
	TWeakObjectPtr<class ARRRandomPlaneStation> Station;
	TWeakObjectPtr<AController> CrashKiller;
	float CrashFallSpeed = 0.f;
	/** Server: the pilot's team when it was shot down (its side is hurt by the blast). */
	int32 CrashTeam = INDEX_NONE;
	/** Each machine: how far it has spun going down, its bank and pitch now, and the way it flew last frame. */
	float CrashSpin = 0.f;
	float PlaneBank = 0.f;
	float PlanePitch = 0.f;
	float LastPlaneYaw = 0.f;

	/** The wooden plane's propeller: a thin blade turning in front of its nose (engine cube). */
	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	TObjectPtr<UStaticMeshComponent> Propeller;
	float PropellerTurn = 0.f;
	/** The engine cube: the propeller, and a plane's stand-in while its model is missing. */
	UPROPERTY()
	TObjectPtr<UStaticMesh> Cube;

	UFUNCTION()
	void OnRep_Kind();

	UFUNCTION()
	void OnRep_Combined();

	/** Part of the robot: hidden, no seats (replicated). */
	UPROPERTY(ReplicatedUsing = OnRep_Combined)
	bool bCombined = false;

	UPROPERTY(VisibleAnywhere, Category = "Vehicle")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Kind, Category = "Vehicle")
	ERRVehicleKind Kind = ERRVehicleKind::Kickboard;

	/** A star car: whose team's (SetStarTeam). */
	UPROPERTY(Replicated)
	int32 StarTeam = 0;

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
