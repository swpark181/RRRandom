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
	// The first replication of a brawler that joins mid-game brings an old roll along; don't throw it again
	if (GetOwner()->HasActorBegunPlay())
	{
		LastRollTime = GetWorld()->GetTimeSeconds();
	}
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

	if (GetOwner()->HasAuthority())
	{
		Charges = StartingCharges;
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

	if (bRollPending && !IsRolling())
	{
		bRollPending = false;
		ApplyRoll(LastRoll);
	}
}

bool URRRandomDiceBuffComponent::RollDice()
{
	if (Charges <= 0 || IsRolling() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	// Everything is decided now so every machine can tumble the die onto the face it will show
	--Charges;
	LastRoll = FRRDiceRoll();
	LastRoll.Face = Stream.RandRange(1, 6);
	// One draw picks the kind: weapon, giant, flight, or else a buff
	const float Kind = Stream.FRand();
	LastRoll.bWeapon = Kind < WeaponDieChance;
	LastRoll.bGiant = !LastRoll.bWeapon && Kind < WeaponDieChance + GiantDieChance;
	LastRoll.bFlight = !LastRoll.bWeapon && !LastRoll.bGiant && Kind < WeaponDieChance + GiantDieChance + FlightDieChance;
	if (!LastRoll.bWeapon && !LastRoll.bGiant && !LastRoll.bFlight)
	{
		LastRoll.Stat = static_cast<ERRDiceBuffStat>(Stream.RandRange(0, 2));
	}
	LastRollTime = GetWorld()->GetTimeSeconds();
	++RollCount;

	// It takes effect when it lands (TickComponent)
	bRollPending = true;
	if (RollSpinTime <= 0.f)
	{
		bRollPending = false;
		ApplyRoll(LastRoll);
	}
	return true;
}

void URRRandomDiceBuffComponent::ApplyRoll(const FRRDiceRoll& Roll)
{
	if (Roll.bWeapon)
	{
		OnWeaponDie.Broadcast(Roll.Face);
	}
	else if (Roll.bGiant)
	{
		OnGiantDie.Broadcast(Roll.Face);
	}
	else if (Roll.bFlight)
	{
		OnFlightDie.Broadcast(Roll.Face);
	}
	else
	{
		FRRDiceBuff& Buff = ActiveBuffs.AddDefaulted_GetRef();
		Buff.Stat = Roll.Stat;
		Buff.Bonus = Roll.Face * BonusPerPip;
		Buff.RemainingTime = Stream.FRandRange(BuffDurationRange.X, BuffDurationRange.Y);
	}
}

void URRRandomDiceBuffComponent::ClearBuffs()
{
	ActiveBuffs.Reset();
	bRollPending = false;
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
