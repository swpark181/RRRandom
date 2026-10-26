#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomVehicle.h"
#include "RRRandomPlaneStation.generated.h"

class ARRRandomCharacter;
class ARRRandomDiceTown;
class UStaticMeshComponent;

/**
 * A plane station (round 88): where an animal friend gets a plane. F next to it (ARRRandomCharacter::ToggleRide) throws a
 * big die over the station; when it lands (RollTime) its face decides the plane that pops in under the one who threw it,
 * already boarded: 1-2 the wooden plane, 3-4 the fighter, 5-6 the spaceship (KindForFace; each its own weapon and
 * toughness, FRRVehicleHandling). One plane per station at a time: the station waits till that plane is gone (put down,
 * or shot down) and RechargeTime more before the next throw. Anyone may use either team's station.
 * The die's tumble is every machine's own show from the replicated roll (like the die over a head); the plane is the
 * server's. Dice Town has one by each team's base (SpawnDiceTownStations).
 * Looks: /Game/Vehicles/SM_PlaneStation (or a flat engine cylinder without it), the die /Game/Props/Dice/SM_Dice.
 */
UCLASS(NotPlaceable)
class RRRANDOM_API ARRRandomPlaneStation : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomPlaneStation();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Animal friends this close (flat, from its middle; the deck is 600 across) can throw the die. */
	static constexpr float UseRange = 450.f;
	/** Seconds the die tumbles before it lands; after its plane is gone, seconds before the next throw. */
	static constexpr float RollTime = 1.f;
	static constexpr float RechargeTime = 8.f;
	/** How high over the station's deck the die tumbles, and its size (SM_Dice is 100 across). */
	static constexpr float DieHeight = 260.f;
	static constexpr float DieScale = 1.2f;

	/** The plane each face of the die gives. */
	static ERRVehicleKind KindForFace(int32 Face);

	/** Whether Who could throw the die here now (the station free and charged, Who on foot and free to board). */
	bool CanRoll(const ARRRandomCharacter* Who) const;
	/** Server: Who throws the die; the plane comes when it lands. */
	void StartRoll(ARRRandomCharacter* Who);
	/** Server, by its plane: the plane is gone; the station charges up for the next one. */
	void OnPlaneGone();

	ARRRandomVehicle* GetPlane() const { return Plane; }
	bool IsRolling() const { return bRolling; }
	/** Seconds till the next throw is allowed (0 when ready; while its plane is out, RechargeTime and more). */
	float GetRechargeRemaining() const;
	/** The deck's top over the station's pivot (where a plane pops in when the cheat calls one there). */
	float GetDeckHeight() const;

	/** The station nearest to Who that it could throw the die at now, or null. */
	static ARRRandomPlaneStation* FindInReach(const ARRRandomCharacter* Who);
	/** Any station near Who (in reach), ready or not, for the HUD to say why not. */
	static ARRRandomPlaneStation* FindNear(const ARRRandomCharacter* Who);
	/** Server: Dice Town's two stations, one by each team's base. */
	static void SpawnDiceTownStations(UWorld* World, const ARRRandomDiceTown* Town);
	/**
	 * Server: the plane for Face, popped in under Who and boarded (Station: whose it is, told when it is gone; null for the
	 * cheat). Null if Who can't board one now.
	 */
	static ARRRandomVehicle* SpawnPlaneFor(ARRRandomCharacter* Who, int32 Face, ARRRandomPlaneStation* Station);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Roll();
	/** Every machine: the die tumbling over the station, landing on its face, staying a while and shrinking away. */
	void UpdateDie(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Station")
	TObjectPtr<UStaticMeshComponent> Die;

	/** Its plane while that is out (replicated, for the HUD). */
	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomVehicle> Plane;

	/** The game state's server clock when the next throw is allowed. */
	UPROPERTY(Replicated)
	float ReadyAt = 0.f;

	/** The throw: counted so every machine shows each one, its face, and whether it is still in the air (the server's word). */
	UPROPERTY(ReplicatedUsing = OnRep_Roll)
	uint8 RollCount = 0;
	UPROPERTY(Replicated)
	uint8 RollFace = 1;
	UPROPERTY(Replicated)
	bool bRolling = false;

	/** Server: who threw it, and when it lands. */
	TWeakObjectPtr<ARRRandomCharacter> Roller;
	float RollLandTime = 0.f;

	/** This machine: when it saw the throw, the tumble's axis, and whether the landing's burst and word have played. */
	float RollSeenTime = -100.f;
	FVector SpinAxis = FVector::ForwardVector;
	bool bLandShown = true;
};
