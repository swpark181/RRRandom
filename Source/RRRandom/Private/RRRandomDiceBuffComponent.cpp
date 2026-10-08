#include "RRRandomDiceBuffComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "RRRandomDice"

URRRandomDiceBuffComponent::URRRandomDiceBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void URRRandomDiceBuffComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URRRandomDiceBuffComponent, ActiveBuffs);
	DOREPLIFETIME(URRRandomDiceBuffComponent, LastRoll);
	DOREPLIFETIME(URRRandomDiceBuffComponent, RollCount);
	DOREPLIFETIME(URRRandomDiceBuffComponent, GaugeTime);
	DOREPLIFETIME(URRRandomDiceBuffComponent, Charges);
}

void URRRandomDiceBuffComponent::OnRep_RollCount()
{
	LastRollTime = GetWorld()->GetTimeSeconds();
}

void URRRandomDiceBuffComponent::BeginPlay()
{
	Super::BeginPlay();

	if (Seed != 0)
	{
		Stream.Initialize(Seed);
	}
	else
	{
		Stream.GenerateNewSeed();
	}
}

void URRRandomDiceBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	GaugeTime += DeltaTime;
	if (GaugeFillTime > 0.f && GaugeTime >= GaugeFillTime)
	{
		GaugeTime -= GaugeFillTime;
		++Charges;
	}

	for (FRRDiceBuff& Buff : ActiveBuffs)
	{
		Buff.RemainingTime -= DeltaTime;
	}
	ActiveBuffs.RemoveAll([](const FRRDiceBuff& Buff) { return Buff.RemainingTime <= 0.f; });
}

void URRRandomDiceBuffComponent::RollDice()
{
	if (Charges <= 0 || !GetOwner()->HasAuthority())
	{
		return;
	}

	LastRoll.Reset();
	LastRollTime = GetWorld()->GetTimeSeconds();
	++RollCount;
	int32 BestWeaponFace = 0;
	int32 BestGiantFace = 0;
	int32 BestFlightFace = 0;
	for (; Charges > 0; --Charges)
	{
		FRRDiceRoll& Roll = LastRoll.AddDefaulted_GetRef();
		Roll.Face = Stream.RandRange(1, 6);
		// One draw picks the kind: weapon, giant, flight, or else a buff
		const float Kind = Stream.FRand();
		Roll.bWeapon = Kind < WeaponDieChance;
		Roll.bGiant = !Roll.bWeapon && Kind < WeaponDieChance + GiantDieChance;
		Roll.bFlight = !Roll.bWeapon && !Roll.bGiant && Kind < WeaponDieChance + GiantDieChance + FlightDieChance;
		if (Roll.bWeapon)
		{
			BestWeaponFace = FMath::Max(BestWeaponFace, Roll.Face);
			continue;
		}
		if (Roll.bGiant)
		{
			BestGiantFace = FMath::Max(BestGiantFace, Roll.Face);
			continue;
		}
		if (Roll.bFlight)
		{
			BestFlightFace = FMath::Max(BestFlightFace, Roll.Face);
			continue;
		}
		Roll.Stat = static_cast<ERRDiceBuffStat>(Stream.RandRange(0, 2));

		FRRDiceBuff& Buff = ActiveBuffs.AddDefaulted_GetRef();
		Buff.Stat = Roll.Stat;
		Buff.Bonus = Roll.Face * BonusPerPip;
		Buff.RemainingTime = BuffDuration;
	}

	// Several weapon dice still give one gun, the best one
	if (BestWeaponFace > 0)
	{
		OnWeaponDie.Broadcast(BestWeaponFace);
	}
	// Likewise one giant spell and one flight, as long as the best die of each gives
	if (BestGiantFace > 0)
	{
		OnGiantDie.Broadcast(BestGiantFace);
	}
	if (BestFlightFace > 0)
	{
		OnFlightDie.Broadcast(BestFlightFace);
	}
}

float URRRandomDiceBuffComponent::GetTimeSinceLastRoll() const
{
	return LastRollTime < 0.f ? UE_BIG_NUMBER : GetWorld()->GetTimeSeconds() - LastRollTime;
}

float URRRandomDiceBuffComponent::GetMultiplier(ERRDiceBuffStat Stat) const
{
	float Multiplier = 1.f;
	for (const FRRDiceBuff& Buff : ActiveBuffs)
	{
		if (Buff.Stat == Stat)
		{
			Multiplier += Buff.Bonus;
		}
	}
	return Multiplier;
}

FText URRRandomDiceBuffComponent::GetStatName(ERRDiceBuffStat Stat)
{
	switch (Stat)
	{
	case ERRDiceBuffStat::AttackPower: return LOCTEXT("AttackPower", "ATK");
	case ERRDiceBuffStat::AttackSpeed: return LOCTEXT("AttackSpeed", "ATK SPD");
	default: return LOCTEXT("MoveSpeed", "MOVE SPD");
	}
}

FString URRRandomDiceBuffComponent::GetStatShortName(ERRDiceBuffStat Stat)
{
	switch (Stat)
	{
	case ERRDiceBuffStat::AttackPower: return TEXT("ATK");
	case ERRDiceBuffStat::AttackSpeed: return TEXT("ASPD");
	default: return TEXT("MOVE");
	}
}

FLinearColor URRRandomDiceBuffComponent::GetStatColor(ERRDiceBuffStat Stat)
{
	switch (Stat)
	{
	case ERRDiceBuffStat::AttackPower: return FLinearColor(1.f, 0.45f, 0.2f);
	case ERRDiceBuffStat::AttackSpeed: return FLinearColor(0.8f, 0.55f, 1.f);
	default: return FLinearColor(0.35f, 1.f, 0.5f);
	}
}

#undef LOCTEXT_NAMESPACE
