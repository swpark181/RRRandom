#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "RRRandomDiceBuffComponent.generated.h"

UENUM(BlueprintType)
enum class ERRDiceBuffStat : uint8
{
	AttackPower,
	AttackSpeed,
	MoveSpeed,
};

/** One die's reward: a percentage bonus to a stat that runs out after a while. */
USTRUCT(BlueprintType)
struct FRRDiceBuff
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RRRandom")
	ERRDiceBuffStat Stat = ERRDiceBuffStat::AttackPower;

	/** 0.25 means +25%. */
	UPROPERTY(BlueprintReadOnly, Category = "RRRandom")
	float Bonus = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "RRRandom")
	float RemainingTime = 0.f;
};

/** One die from the latest roll, kept briefly so it can be shown over the roller's head. */
USTRUCT(BlueprintType)
struct FRRDiceRoll
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RRRandom")
	int32 Face = 1;

	UPROPERTY(BlueprintReadOnly, Category = "RRRandom")
	ERRDiceBuffStat Stat = ERRDiceBuffStat::AttackPower;

	/** A weapon die gives a new gun instead of a buff; Stat is unused then. */
	UPROPERTY(BlueprintReadOnly, Category = "RRRandom")
	bool bWeapon = false;

	/** A giant die makes its owner a giant for a while instead of a buff; Stat is unused then. */
	UPROPERTY(BlueprintReadOnly, Category = "RRRandom")
	bool bGiant = false;
};

/** Fired once per roll that had weapon dice, with the best weapon die's face. */
DECLARE_MULTICAST_DELEGATE_OneParam(FRRWeaponDieSignature, int32 /*Face*/);

/** Fired once per roll that had giant dice, with the best giant die's face. */
DECLARE_MULTICAST_DELEGATE_OneParam(FRRGiantDieSignature, int32 /*Face*/);

/**
 * A gauge fills over time; each full gauge stores one die. Rolling spends every stored die,
 * and each die grants a timed bonus to attack power, attack speed or move speed sized by its face,
 * or now and then comes up as a weapon die that hands its owner a new random gun,
 * or a giant die that makes its owner bigger, harder hitting and harder to hurt for a while.
 */
UCLASS(ClassGroup = (RRRandom), meta = (BlueprintSpawnableComponent))
class RRRANDOM_API URRRandomDiceBuffComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URRRandomDiceBuffComponent();

	/** Rolls one die per stored charge and applies the results. */
	UFUNCTION(BlueprintCallable, Category = "RRRandom")
	void RollDice();

	/** 1 plus the summed bonuses of every active buff on that stat. */
	UFUNCTION(BlueprintPure, Category = "RRRandom")
	float GetMultiplier(ERRDiceBuffStat Stat) const;

	UFUNCTION(BlueprintPure, Category = "RRRandom")
	int32 GetCharges() const { return Charges; }

	/** 0..1 progress toward the next charge. */
	UFUNCTION(BlueprintPure, Category = "RRRandom")
	float GetGaugeProgress() const { return GaugeFillTime > 0.f ? GaugeTime / GaugeFillTime : 1.f; }

	const TArray<FRRDiceBuff>& GetActiveBuffs() const { return ActiveBuffs; }

	/** Every die from the most recent roll. */
	const TArray<FRRDiceRoll>& GetLastRoll() const { return LastRoll; }

	/** Seconds since the most recent roll; very large if there hasn't been one. */
	float GetTimeSinceLastRoll() const;

	/** Drops every active buff, keeping stored dice. Used when the owner is knocked out. */
	void ClearBuffs() { ActiveBuffs.Reset(); }

	static FText GetStatName(ERRDiceBuffStat Stat);

	/** Compact label for over-the-head indicators. */
	static FString GetStatShortName(ERRDiceBuffStat Stat);

	static FLinearColor GetStatColor(ERRDiceBuffStat Stat);

	/** The owner listens to this to swap guns. */
	FRRWeaponDieSignature OnWeaponDie;

	/** The owner listens to this to turn giant. */
	FRRGiantDieSignature OnGiantDie;

	/** Chance for each die to be a weapon die. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RRRandom", meta = (ClampMin = "0", ClampMax = "1"))
	float WeaponDieChance = 1.f / 6.f;

	/** Chance for each die to be a giant die (on top of the weapon die chance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RRRandom", meta = (ClampMin = "0", ClampMax = "1"))
	float GiantDieChance = 0.1f;

	static FLinearColor GetGiantColor() { return FLinearColor(0.3f, 0.95f, 1.f); }

	/** Seconds for the gauge to fill once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RRRandom")
	float GaugeFillTime = 5.f;

	/** How long each die's buff lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RRRandom")
	float BuffDuration = 15.f;

	/** Bonus per pip on the die: a 6 with 0.05 gives +30%. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RRRandom")
	float BonusPerPip = 0.05f;

	/** 0 picks a new seed each play session. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RRRandom")
	int32 Seed = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	/** A new roll arrived: start its popup from this machine's clock. */
	UFUNCTION()
	void OnRep_RollCount();

	// The server fills the gauge, rolls and runs the buffs down; clients only show the replicated results

	FRandomStream Stream;

	UPROPERTY(Replicated)
	TArray<FRRDiceBuff> ActiveBuffs;

	UPROPERTY(Replicated)
	TArray<FRRDiceRoll> LastRoll;

	/** Counts rolls so clients notice a new one even when it matches the last. */
	UPROPERTY(ReplicatedUsing = OnRep_RollCount)
	int32 RollCount = 0;

	/** Local clock; clients set it when a roll arrives. */
	float LastRollTime = -1.f;

	UPROPERTY(Replicated)
	float GaugeTime = 0.f;

	UPROPERTY(Replicated)
	int32 Charges = 0;
};
