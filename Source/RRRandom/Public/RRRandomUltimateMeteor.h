#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomUltimateMeteor.generated.h"

class ARRRandomCharacter;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Round 97, the battle royale's ultimate dice (the user: "궁극기 주사위는 하늘에서 운석으로 한개씩 떨어지고 줏으면 e키 눌렀을때
 * 궁극기 주사위가 굴려짐"): no gauge makes them there; one at a time a golden meteor falls somewhere inside the ring (its
 * landing pad glowing gold for a few seconds first, shown on every map), lands harmlessly (a knock aside) and leaves a big
 * golden die spinning over the crater under a beam of light. The first animal friend with an ultimate (and no ultimate
 * die yet) to touch it takes it: a stored ultimate die, rolled first by the next E. Then the next one falls after a while.
 * Spawned by the server (UpdateSky, from the game mode), replicated; every machine plays the fall from the server's clock.
 */
UCLASS()
class RRRANDOM_API ARRRandomUltimateMeteor : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomUltimateMeteor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, every second or so while a battle royale is under way: drops the next meteor when it is due (NextDropTime, server
	 * time; 0 before the first is planned). */
	static void UpdateSky(UWorld* World, float& NextDropTime);
	/** The meteor on the map now (falling or waiting to be taken), or null. */
	static ARRRandomUltimateMeteor* Get(const UObject* WorldContext);

	/** Server, before FinishSpawning: lands at this server time. */
	void Setup(float InImpactTime) { ImpactTime = InImpactTime; }
	float GetTimeToImpact() const;
	bool HasLanded() const { return GetTimeToImpact() <= 0.f; }
	bool IsTaken() const { return bTaken; }
	/** Whether this animal friend may take it (alive, has an ultimate, holds no ultimate die yet). */
	static bool CanTake(const ARRRandomCharacter* Who);

	/** Seconds the pad glows before it lands; the first one falls this long after play starts, each next one this long after the last is taken. */
	static constexpr float Lead = 6.f;
	static constexpr float FirstDrop = 60.f;
	static constexpr float NextDrop = 40.f;
	/** How near a body must come to take it. */
	static constexpr float PickupRadius = 170.f;
	static FLinearColor GetGold() { return FLinearColor(1.f, 0.75f, 0.15f); }

protected:
	virtual void BeginPlay() override;

private:
	void UpdateLook(float DeltaSeconds);
	void Land();
	void TryPickUp();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ring;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Rock;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Die;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Beam;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RingMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> BeamMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> GoldMaterial;

	UPROPERTY(Replicated) float ImpactTime = 0.f;
	UPROPERTY(ReplicatedUsing = OnRep_Taken) bool bTaken = false;
	UFUNCTION() void OnRep_Taken();
	/** Every machine: the taking's pop, once. */
	void ShowTaken();
	bool bTakenShown = false;

	bool bLanded = false;
	/** Server: when it was taken (this machine's clock). */
	float TakenAt = 0.f;
	bool bLandingShown = false;
	FVector FallFrom = FVector::ZeroVector;
};
