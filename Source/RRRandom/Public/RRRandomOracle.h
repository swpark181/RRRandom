#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomOracle.generated.h"

class ARRRandomCharacter;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;

/** One slot's roll of the oracle die: a team's animal friend (null for a slot whose friend is down or gone, rolled by itself). */
USTRUCT()
struct FRROracleSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ARRRandomCharacter> Who = nullptr;

	UPROPERTY()
	int8 Team = 0;

	UPROPERTY()
	uint8 Value = 1;
};

/**
 * Round 93, the battle royale's oracle meteors: each time the cotton candy cloud has closed in (phases 1 to 5), every
 * team still standing rolls the oracle die in each of its three slots (a slot whose animal friend is down or gone rolls by
 * itself); the team with the lowest sum is struck: round one of its living animal friends picked at random, a zone a
 * quarter of the clear area (half the ring's radius) round where it stands is shown on the maps, and meteors rain down on
 * it one after another, each landing pad glowing on the floor a moment before. Later storms fall on smaller zones and are
 * harder to dodge (FRROracleRound). The server rolls and drops the meteors; every machine shows the roll from the
 * replicated slots and the clock.
 */
UCLASS()
class RRRANDOM_API ARRRandomOracleStorm : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomOracleStorm();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static ARRRandomOracleStorm* Get(const UObject* WorldContext);
	/** Server: the storm after closing InRound (1 to GetRoundCount()), with the ring as it now stands. */
	static void RollFor(const UObject* WorldContext, int32 InRound, const FVector2D& RingCenter, float RingRadius, ARRRandomCharacter* ForcedTarget = nullptr);
	/** Server cheat (RROracle): the next storm now (on ForcedTarget, if given, whatever the dice say). */
	static void RollNext(const UObject* WorldContext, ARRRandomCharacter* ForcedTarget = nullptr);
	static int32 GetRoundCount();

	/** Seconds the dice tumble, the sums show, and the first meteor lands, after the roll starts. */
	static constexpr float RollTime = 3.f;
	static constexpr float RevealTime = 5.f;
	static constexpr float FirstImpact = 8.f;
	/** Each meteor's blast round where it lands. */
	static constexpr float BlastRadius = 600.f;

	int32 GetRound() const { return Round; }
	/** Server time the dice started rolling (0 before the first storm). */
	float GetRollStart() const { return RollStart; }
	/** Server time the last meteor lands. */
	float GetStormEnd() const { return StormEnd; }
	const TArray<FRROracleSlot>& GetSlots() const { return Slots; }
	int32 GetStruckTeam() const { return StruckTeam; }
	const ARRRandomCharacter* GetTarget() const { return Target; }
	FVector2D GetZoneCenter() const { return ZoneCenter; }
	float GetZoneRadius() const { return ZoneRadius; }
	int32 GetMeteorCount() const { return MeteorCount; }
	/** Seconds since the roll started, on the server's clock (negative or huge when there was none). */
	float GetAge() const;
	/** The zone is up (shown on the maps) and meteors are still to land. */
	bool IsStriking() const;
	/** Each team's sum this storm, by team (teams that sat it out: -1). */
	void GetSums(TArray<int32>& OutSums) const;

	/** A bot's way out of what is coming down: away from a landing pad about to be hit, else out of the zone if there is ring to run to. */
	static bool FindDodge(const UObject* WorldContext, const FVector2D& Here, const FVector2D& RingCenter, float RingRadius, FVector2D& OutSpot);

private:
	void Roll(int32 InRound, const FVector2D& RingCenter, float RingRadius, ARRRandomCharacter* ForcedTarget);
	void DropDue();
	float GetNow() const;

	UPROPERTY(Replicated) int32 Round = 0;
	UPROPERTY(Replicated) float RollStart = 0.f;
	UPROPERTY(Replicated) float StormEnd = 0.f;
	UPROPERTY(Replicated) TArray<FRROracleSlot> Slots;
	UPROPERTY(Replicated) int32 StruckTeam = -1;
	UPROPERTY(Replicated) TObjectPtr<ARRRandomCharacter> Target = nullptr;
	UPROPERTY(Replicated) FVector2D ZoneCenter = FVector2D::ZeroVector;
	UPROPERTY(Replicated) float ZoneRadius = 0.f;
	UPROPERTY(Replicated) int32 MeteorCount = 0;

	/** Server: the meteors still to drop (where, when each lands, how long its pad glows first). */
	struct FDrop
	{
		FVector Spot;
		float Impact;
		float Lead;
	};
	TArray<FDrop> Drops;
	FRandomStream Stream;
};

/**
 * One oracle meteor, standing where it lands (its actor location: on whatever it hits first from above, a roof or the
 * street). For Lead seconds before, a glowing pad marks the spot with a ring closing in on it; then a burning rock streaks
 * down, blasts everyone round it (more in the middle, whatever their team), knocks them aside and smashes the blocks it
 * lands on (a wreck that stays for the rest of the match).
 */
UCLASS()
class RRRANDOM_API ARRRandomOracleMeteor : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomOracleMeteor();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, before FinishSpawning: lands at this server time, its pad glowing InLead seconds before. */
	void Setup(float InImpactTime, float InLead);
	/** Seconds until it lands (negative after), on the server's clock. */
	float GetTimeToImpact() const;
	float GetLead() const { return Lead; }

protected:
	virtual void BeginPlay() override;

private:
	void UpdateLook(float DeltaSeconds);
	void Land();

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Ring;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Closing;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Rock;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Core;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UPointLightComponent> Light;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RingMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ClosingMaterial;

	UPROPERTY(Replicated) float ImpactTime = 0.f;
	UPROPERTY(Replicated) float Lead = 2.f;

	bool bLanded = false;
	bool bLandingShown = false;
	FVector FallFrom = FVector::ZeroVector;
};
