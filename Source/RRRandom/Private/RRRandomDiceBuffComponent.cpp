#include "RRRandomDiceBuffComponent.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "RRRandomDice"

URRRandomDiceBuffComponent::URRRandomDiceBuffComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
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
	if (Charges <= 0)
	{
		return;
	}

	LastRoll.Reset();
	LastRollTime = GetWorld()->GetTimeSeconds();
	int32 BestWeaponFace = 0;
	for (; Charges > 0; --Charges)
	{
		FRRDiceRoll& Roll = LastRoll.AddDefaulted_GetRef();
		Roll.Face = Stream.RandRange(1, 6);
		Roll.bWeapon = Stream.FRand() < WeaponDieChance;
		if (Roll.bWeapon)
		{
			BestWeaponFace = FMath::Max(BestWeaponFace, Roll.Face);
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
