#include "RRRandomDiceBuffComponent.h"
#include "RRRandomGameState.h"
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
	DOREPLIFETIME(URRRandomDiceBuffComponent, UltimateCharges);
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

	// The battle royale (round 97): thirty weapon dice to get ready with, none at the start, then a die a minute and no
	// ultimate dice from the gauge; the gauge waits for play
	const bool bBattle = ARRRandomGameState::IsBattleRoyale(this);
	if (bBattle)
	{
		UpdateBattleRoyale();
	}
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (!bBattle || (State && State->HasPlayStarted()))
	{
		GaugeTime += DeltaTime;
	}
	const float FillTime = bBattle ? BattleGaugeFillTime : GaugeFillTime;
	if (FillTime > 0.f && GaugeTime >= FillTime)
	{
		GaugeTime -= FillTime;
		++Charges;
		// Now and then (one at a time) it is an ultimate die
		if (!bBattle && UltimateDieChance > 0.f && UltimateCharges == 0 && Stream.FRand() < UltimateDieChance)
		{
			++UltimateCharges;
		}
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
	// The battle royale's get-ready time (round 97): every die is a weapon die, for the bag
	if (ARRRandomGameState::IsPrep(this))
	{
		LastRoll.bWeapon = true;
	}
	// A stored ultimate die goes first
	else if (UltimateCharges > 0)
	{
		--UltimateCharges;
		LastRoll.bUltimate = true;
	}
	else
	{
		// One draw picks the kind: weapon, giant, flight, the owner's own, or else a buff
		const float Kind = Stream.FRand();
		LastRoll.bWeapon = Kind < WeaponDieChance;
		LastRoll.bGiant = !LastRoll.bWeapon && Kind < WeaponDieChance + GiantDieChance;
		LastRoll.bFlight = !LastRoll.bWeapon && !LastRoll.bGiant && Kind < WeaponDieChance + GiantDieChance + FlightDieChance;
		LastRoll.bOwn = !LastRoll.bWeapon && !LastRoll.bGiant && !LastRoll.bFlight && Kind < WeaponDieChance + GiantDieChance + FlightDieChance + OwnDieChance;
		if (!LastRoll.bWeapon && !LastRoll.bGiant && !LastRoll.bFlight && !LastRoll.bOwn)
		{
			LastRoll.Stat = static_cast<ERRDiceBuffStat>(Stream.RandRange(0, 2));
		}
	}
	LastRollTime = GetWorld()->GetTimeSeconds();
	++RollCount;

	// It takes effect when it lands (TickComponent)
	bRollPending = true;
	if (GetSpinTime() <= 0.f)
	{
		bRollPending = false;
		ApplyRoll(LastRoll);
	}
	return true;
}

void URRRandomDiceBuffComponent::UpdateBattleRoyale()
{
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (!State)
	{
		return;
	}
	if (State->IsPrep() && !bPrepDiceGiven)
	{
		bPrepDiceGiven = true;
		Charges = ARRRandomGameState::PrepDice;
		UltimateCharges = 0;
		GaugeTime = 0.f;
	}
	// Whatever is left of them goes when the get-ready time ends (or at the start, without one)
	if (!bBattleStartDone && (State->HasPlayStarted() || (bPrepDiceGiven && !State->IsPrep())))
	{
		bBattleStartDone = true;
		Charges = 0;
		UltimateCharges = 0;
		GaugeTime = 0.f;
	}
}

void URRRandomDiceBuffComponent::GiveUltimateDie()
{
	if (GetOwner()->HasAuthority() && UltimateDieChance > 0.f)
	{
		++Charges;
		++UltimateCharges;
	}
}

void URRRandomDiceBuffComponent::AddCharge(bool bUltimate)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	if (bUltimate && UltimateDieChance > 0.f)
	{
		GiveUltimateDie();
		return;
	}
	++Charges;
}

void URRRandomDiceBuffComponent::ApplyRoll(const FRRDiceRoll& Roll)
{
	if (Roll.bUltimate)
	{
		OnUltimateDie.Broadcast(Roll.Face);
	}
	else if (Roll.bWeapon)
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
	else if (Roll.bOwn)
	{
		OnOwnDie.Broadcast(Roll.Face);
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

float URRRandomDiceBuffComponent::GetGaugeProgress() const
{
	// (the battle royale's fills once a minute: round 97)
	const float FillTime = ARRRandomGameState::IsBattleRoyale(this) ? BattleGaugeFillTime : GaugeFillTime;
	return FillTime > 0.f ? FMath::Clamp(GaugeTime / FillTime, 0.f, 1.f) : 1.f;
}
