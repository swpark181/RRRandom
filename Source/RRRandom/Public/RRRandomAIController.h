#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Math/RandomStream.h"
#include "RRRandomAIController.generated.h"

class ARRRandomCharacter;
class ARRRandomDiceTown;
class ARRRandomVehicle;

/**
 * Careful brawler bot. Goes after the nearest opponent it can see, keeps its own preferred distance while strafing,
 * waits a moment after spotting someone before shooting, fires in short bursts with some aim error,
 * and only shoots when no cover is in the way. Ducks behind cover to reload or heal, reloads early while
 * nobody is in sight, and rolls its stored dice when a fight starts. Out of a fight it rides vehicles for long trips
 * and hops in the back of a teammate's stopped car (RideTripDistance).
 * Steers directly (sliding around cover) instead of pathfinding, so it needs no navmesh; on a map with walking routes
 * (Dice Town: houses, stairs, canals) it follows the map's routes instead whenever it isn't fighting up close.
 */
UCLASS()
class RRRANDOM_API ARRRandomAIController : public AAIController
{
	GENERATED_BODY()

public:
	ARRRandomAIController();

	virtual void Tick(float DeltaSeconds) override;

	/** Where to wait when no opponent is up. */
	void SetArenaCenter(const FVector& Center) { ArenaCenter = Center; bHasArenaCenter = true; }

protected:
	virtual void OnPossess(APawn* InPawn) override;

	/** Each bot picks a preferred fighting distance in this range, as a share of its attack range. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FVector2D PreferredRangeShare = FVector2D(0.6f, 0.85f);

	/** Below this share of health the bot backs off to heal behind cover... */
	UPROPERTY(EditAnywhere, Category = "AI")
	float RetreatHealthShare = 0.45f;

	/** ...and comes back once healed to this share. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float ReturnHealthShare = 0.9f;

	/** Recently hit and below this share of health, the bot looks for cover even with ammo left. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float UnderFireCoverHealthShare = 0.7f;

	/** Seconds after spotting an opponent before the first shot. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FVector2D ReactionTime = FVector2D(0.25f, 0.55f);

	/** Shots are thrown off by up to this many degrees either way. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float AimErrorDegrees = 6.f;

	/** Shots per burst, picked anew for each burst. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FIntPoint BurstSize = FIntPoint(3, 5);

	/** Seconds between bursts. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FVector2D BurstPause = FVector2D(0.4f, 0.9f);

	/** With nobody in sight, the bot reloads once the magazine is down to this share. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float TacticalReloadShare = 0.6f;

	/** Cover further than this from the bot is not considered. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float CoverSearchRadius = 1200.f;

	/** Out of a fight, the bot holds this many dice at most before rolling anyway. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FIntPoint DiceHoldRange = FIntPoint(2, 4);

	/** Teammates closer than this push each other apart so bots don't stack up. */
	UPROPERTY(EditAnywhere, Category = "AI")
	float SeparationDistance = 220.f;

	/** With the flight power and an opponent near, the bot takes off this often (seconds after landing)... */
	UPROPERTY(EditAnywhere, Category = "AI")
	FVector2D FlightHopDelay = FVector2D(0.3f, 1.5f);

	/** ...holding jump this long, so it rises to a random height and glides down shooting. */
	UPROPERTY(EditAnywhere, Category = "AI")
	FVector2D FlightHoldTime = FVector2D(0.25f, 0.75f);

	/**
	 * Out of a fight, bots ride for trips longer than this (to where they are heading: an opponent far off, where one
	 * was last seen, or the next roaming spot), walking to a free vehicle at most VehicleSearchRadius away (and no further
	 * than half the trip); they get off within RideArriveDistance of where they are going, when stuck, or when a fight
	 * starts. A teammate's car standing still within PassengerJoinRadius with its rear seat free: they hop in the back,
	 * trip or not, and get off when the driver does or a fight starts.
	 */
	UPROPERTY(EditAnywhere, Category = "AI|Vehicles")
	float RideTripDistance = 1400.f;

	UPROPERTY(EditAnywhere, Category = "AI|Vehicles")
	float RideArriveDistance = 500.f;

	UPROPERTY(EditAnywhere, Category = "AI|Vehicles")
	float VehicleSearchRadius = 1000.f;

	UPROPERTY(EditAnywhere, Category = "AI|Vehicles")
	float PassengerJoinRadius = 600.f;

	/** 0 picks a new seed each play session. */
	UPROPERTY(EditAnywhere, Category = "AI")
	int32 Seed = 0;

private:
	ARRRandomCharacter* FindBestOpponent(const ARRRandomCharacter* Self) const;
	FVector ChooseFightDirection(const ARRRandomCharacter* Self, const ARRRandomCharacter* Opponent, float Distance, bool bCanSee) const;
	/** Picks a spot behind cover, hidden from Threat. Returns false if there is none nearby. */
	bool FindCoverSpot(const ARRRandomCharacter* Self, const FVector& Threat, FVector& OutSpot) const;
	/** Turns a wanted direction aside so the bot slides around cover instead of walking into it. */
	FVector SteerAroundObstacles(const ARRRandomCharacter* Self, const FVector& Wanted);
	FVector GetSeparation(const ARRRandomCharacter* Self) const;
	/** Whether a shot from one spot would reach the other without hitting cover. Both are capsule centers. */
	bool HasLineOfFire(const FVector& From, const FVector& To) const;
	void TryShoot(ARRRandomCharacter* Self, const ARRRandomCharacter* Opponent, float Distance);
	void UpdateReload(ARRRandomCharacter* Self, bool bOpponentInSight);
	void UpdateDice(ARRRandomCharacter* Self, float Distance, float DeltaSeconds);
	/** Takes off and lets go of jump while the flight power lasts and a fight is near. */
	void UpdateFlight(ARRRandomCharacter* Self, float Distance, float DeltaSeconds);
	/** Where the bot is heading when not fighting. */
	FVector GetTravelGoal(const ARRRandomCharacter* Self, const ARRRandomCharacter* Target) const;
	/**
	 * Meteor Rush: what to go for when not fighting up close (RushGoal, at foot height): back home with the ore while the
	 * team holds the goal, else the most worthwhile ore lying about (crystals first), else a meteor coming down (waiting
	 * at the edge of where it lands).
	 */
	void UpdateRushGoal(const ARRRandomCharacter* Self, float DeltaSeconds);
	/** Meteor Rush: the nearest opponent carrying a crystal (seen on every map), if near enough to chase. */
	ARRRandomCharacter* FindCrystalCarrier(const ARRRandomCharacter* Self) const;
	/**
	 * The star robot (ARRRandomStarRobot): combines the gathered cars when next to them out of a fight, walks over to its
	 * team's robot with a seat free and gets in. In it, the usual fighting works it (the driver's steering walks it,
	 * the fire button its fists or shield). Returns true when the robot has this bot's attention (no vehicles then);
	 * sets Wanted while walking over (along the map's routes, if it has them: then bOnRoute).
	 */
	bool UpdateStarRobot(ARRRandomCharacter* Self, bool bInFight, FVector& Wanted, bool& bOnRoute, float DeltaSeconds);
	/** Gets on and off vehicles (see RideTripDistance). Returns true and sets Wanted while walking over to one. */
	bool UpdateVehicles(ARRRandomCharacter* Self, const ARRRandomCharacter* Target, bool bInFight, FVector& Wanted, float DeltaSeconds);
	/** The vehicle worth walking to now, if any: a teammate's stopped car with room behind, or a free one for a long trip. */
	ARRRandomVehicle* FindVehicleToRide(const ARRRandomCharacter* Self, const ARRRandomCharacter* Target) const;
	/** Whether a vehicle can still be ridden the way the bot set out to (driver's seat free, or the car still stopped for it). */
	bool IsStillWorthRiding(const ARRRandomCharacter* Self, const ARRRandomVehicle* Ride) const;
	/**
	 * On a map with walking routes: the way toward Goal (feet height) along the route to it, asking for a new route now
	 * and then or when the goal moves; jumps when it stops moving against a ledge (a jump climbs it).
	 */
	FVector FollowRoute(ARRRandomCharacter* Self, const FVector& Goal, float DeltaSeconds);

public:
	/** The vehicle this bot is walking over to, so teammates don't all head for the same one. */
	ARRRandomVehicle* GetVehicleTarget() const { return VehicleTarget.Get(); }

private:
	TWeakObjectPtr<ARRRandomVehicle> VehicleTarget;
	/** Seconds left to reach VehicleTarget before giving up; until the next look for one; spent barely moving at the wheel. */
	float VehicleTargetTimer = 0.f;
	float VehicleSearchTimer = 0.f;
	float StuckTimer = 0.f;

	TWeakObjectPtr<ARRRandomCharacter> Opponent;
	FRandomStream Stream;
	FVector ArenaCenter = FVector::ZeroVector;
	bool bHasArenaCenter = false;
	bool bRetreating = false;
	bool bHadSight = false;
	bool bHasCoverSpot = false;
	FVector CoverSpot = FVector::ZeroVector;
	/** This bot's fighting distance, as a share of its attack range (which a gun such as the foam finger may change). */
	float PreferredShare = 0.7f;
	float RetargetTimer = 0.f;
	float CoverSearchTimer = 0.f;
	float StrafeTimer = 0.f;
	float StrafeSign = 1.f;
	float AvoidSign = 1.f;
	float ReactionTimer = 0.f;
	float ShotTimer = 0.f;
	int32 BurstRemaining = 0;
	float UnderFireTimer = 0.f;
	float LastHealth = 0.f;
	/** Where the opponent was last in the team's sight, and for how much longer to go look there. */
	FVector LastSeenLocation = FVector::ZeroVector;
	float LastSeenTimer = 0.f;
	/** Where the bot roams to while nobody is in sight, and how long until it picks another spot. */
	FVector PatrolGoal = FVector::ZeroVector;
	float PatrolTimer = 0.f;
	float DiceTimer = 0.f;
	int32 DiceHoldLimit = 3;
	/** Throwing stored dice one by one until none are left. */
	bool bSpendingDice = false;
	float FlightHopTimer = 0.f;
	/** Seconds left holding jump; 0 when not holding. */
	float FlightHoldRemaining = 0.f;

	/** The map with walking routes, if this one has them (looked for once). */
	TWeakObjectPtr<ARRRandomDiceTown> Town;
	bool bLookedForTown = false;
	/** The route being followed (corners at foot height), the next corner, the goal it was found for, until the next one. */
	TArray<FVector> Route;
	int32 RouteIndex = 0;
	FVector RouteGoal = FVector::ZeroVector;
	float RouteTimer = 0.f;
	/** Seconds following a route without moving. */
	float RouteStuckTimer = 0.f;

	/** Meteor Rush (UpdateRushGoal): where to go, whether there is anywhere, keeping the ore safe, until the next look. */
	FVector RushGoal = FVector::ZeroVector;
	bool bHasRushGoal = false;
	bool bGuardingOre = false;
	float RushTimer = 0.f;
};
