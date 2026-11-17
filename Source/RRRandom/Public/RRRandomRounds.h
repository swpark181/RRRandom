#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomRounds.generated.h"

class UBoxComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Round 98, the main mode (ERRMatchRule::Rounds, the user's answers in Docs/TPSOverhaulQuestions.md): three rounds, the
 * first team to win two takes the match. At each round's start the rule die picks how that round is won (a rule not
 * played yet in the match): a DEATHMATCH (first to KillTarget knockouts), MOST KO (the team with more knockouts when the
 * clock runs out) or METEOR RUSH (the ore rule, shortened to a round). A tie on the clock goes to sudden death (the next
 * knockout wins, or the golden meteor), and a sudden death that runs out is settled by each team's die.
 */
UENUM()
enum class ERRRoundRule : uint8
{
	Deathmatch,
	MostKills,
	MeteorRush,
	Count UMETA(Hidden),
};

/**
 * Round 98: a personal card. In the main mode a rolled die deals three different cards at its face's strength and the
 * roller keeps one (1 / 2 / 3, or a click); the bots pick by their kit. They reuse the old dice powers.
 */
UENUM()
enum class ERRCard : uint8
{
	/** Attack power + 6% a pip, CardSeconds. */
	Power,
	/** Attack speed (fire interval and reload) + 5% a pip. */
	Rapid,
	/** Move speed + 5% a pip. */
	Swift,
	/** Giant for 3 + a pip seconds (bigger, x1.5 damage dealt, x0.5 taken). */
	Giant,
	/** Wings for 4 + a pip seconds (hold jump to rise). */
	Wings,
	/** A shield of 12 a pip for CardSeconds. */
	Shield,
	/** Invisible for 1.5 seconds a pip (the fox's look); shooting ends it with an ambush bonus. */
	Cloak,
	/** Health back at once: 15 + 10 a pip. */
	Heal,
	Count UMETA(Hidden),
};

/** Round 98: each animal friend's Q ability (FRRKit::Ability), on a cooldown. */
UENUM()
enum class ERRAbility : uint8
{
	None,
	/** The fox: invisible for AbilityPower seconds. */
	Cloak,
	/** The rabbit: AbilityPower super hops (a very high second jump). */
	SuperHop,
	/** The squirrel: an acorn mine at its feet (ARRRandomAcornTrap), up to two out. */
	AcornTrap,
	/** The tiger: giant for AbilityPower seconds. */
	Giant,
	/** The dragon: flies for AbilityPower seconds (and shoots meanwhile). */
	Flight,
	/** The red fox: looks like the other team for AbilityPower seconds (their bots and screens take her for one of theirs). */
	Disguise,
};

namespace RRRounds
{
	constexpr int32 RoundsToWin = 2;
	constexpr int32 MaxRounds = 3;
	/** Seconds of play in a round, before sudden death. */
	constexpr float RoundLength = 180.f;
	/** The round's start: the rule die tumbles this long, then the 3-2-1. */
	constexpr float RuleRollTime = 3.2f;
	constexpr float IntroCountdown = 3.f;
	/** The round's end: the banner and the score this long before the next round (or the results). */
	constexpr float RoundOverTime = 5.f;
	/** Sudden death this long at most; then each team rolls a die and the higher wins. */
	constexpr float SuddenDeathLength = 45.f;
	/** A deathmatch round goes to the first team with this many knockouts. */
	constexpr int32 KillTarget = 7;
	/** A meteor round: hold this much ore (and more than the other team) for MeteorHold seconds; a meteor MeteorInterval apart. */
	constexpr int32 MeteorGoal = 30;
	constexpr float MeteorHold = 8.f;
	constexpr float MeteorFirst = 8.f;
	constexpr float MeteorInterval = 40.f;
	/** Dice to start each round with, and how long the gauge takes for the next. */
	constexpr int32 StartDice = 1;
	constexpr float GaugeFillTime = 18.f;
	/** A personal card's timed power lasts this long. */
	constexpr float CardSeconds = 20.f;
	/** Cards shown for a roll (one is kept). */
	constexpr int32 CardsDealt = 3;

	/**
	 * Getting back up (the user's: "죽을 때 마다 부활 시간이 늘어나고. 죽음 사이의 갭이 짧으면 부활 시간을 더 늘림"): RespawnBase,
	 * plus RespawnPerDeath for each knockout before it this round (up to RespawnStackMax), plus up to QuickPenaltyMax when
	 * this knockout came within QuickGap seconds of the one before (more the quicker), at most RespawnMax in all.
	 */
	constexpr float RespawnBase = 3.f;
	constexpr float RespawnPerDeath = 1.5f;
	constexpr float RespawnStackMax = 6.f;
	constexpr float QuickGap = 20.f;
	constexpr float QuickPenaltyMax = 4.f;
	constexpr float RespawnMax = 13.f;
	/** Seconds to wait for this knockout (the KnockoutsBefore this round, the seconds since the last one), and the quick part. */
	float GetRespawnDelay(int32 KnockoutsBefore, float SinceLastKnockout, float& OutQuickPenalty);

	/**
	 * The arena cut out of Dice Town (the user's "주사위 마을에서 작은 아레나 잘라내기"): everything inside the ring road, a
	 * square this far from the middle each way, fenced (ARRRandomArenaFence).
	 */
	constexpr float ArenaHalf = 4600.f;
	constexpr float FenceHeight = 6000.f;

	/** The results screen goes on to the next match by itself after this. */
	constexpr float NextMatchDelay = 10.f;

	/** The rule die's faces: 1-2 a deathmatch, 3-4 most KO, 5-6 a meteor round. */
	ERRRoundRule GetRuleForFace(int32 Face);
	FString GetRuleName(ERRRoundRule Rule);
	FString GetRuleLine(ERRRoundRule Rule);
	FLinearColor GetRuleColor(ERRRoundRule Rule);

	FString GetCardName(ERRCard Card);
	/** What the card does at a face, short ("+30% ATK 20s"). */
	FString GetCardLine(ERRCard Card, int32 Face);
	FLinearColor GetCardColor(ERRCard Card);

	FString GetAbilityName(ERRAbility Ability);
}

/**
 * Round 98: the main mode's arena fence, a square ArenaHalf from the middle of Dice Town: see-through candy glass panels
 * with glowing posts and a top rail (M_FXCloak), and hidden walls behind them that stop brawlers (not shots). The game mode
 * spawns it on the host; it replicates its size so every machine builds the same.
 */
UCLASS()
class RRRANDOM_API ARRRandomArenaFence : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomArenaFence();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, before it finishes spawning: the half size of the square (round its location) and the floor's height. */
	void Setup(float InHalfExtent, float InFloorZ);
	float GetHalfExtent() const { return HalfExtent; }
	/** Whether a spot is inside the fence (2D), Margin in from it. */
	bool IsInside(const FVector& Where, float Margin = 0.f) const;
	/** The fence in the world (null when there is none). */
	static ARRRandomArenaFence* Get(const UObject* WorldContext);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnRep_Shape();
	void Build();

	UPROPERTY(ReplicatedUsing = OnRep_Shape)
	float HalfExtent = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Shape)
	float FloorZ = 0.f;

	bool bBuilt = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> Walls;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Panels;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Posts;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PanelMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> PostMaterial;
};

/**
 * Round 98, the squirrel's Q (ERRAbility::AcornTrap, its trapper role): an acorn half buried in the ground at its feet. It
 * arms after ArmTime; the first opponent to come within TriggerRadius sets it off: it pops up and bursts, hurting and
 * throwing everyone of the other team within BlastRadius. It goes by itself after Lifetime. Server decides; replicated.
 */
UCLASS()
class RRRANDOM_API ARRRandomAcornTrap : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomAcornTrap();

	virtual void Tick(float DeltaSeconds) override;

	static constexpr float ArmTime = 0.8f;
	static constexpr float TriggerRadius = 170.f;
	static constexpr float BlastRadius = 300.f;
	static constexpr float BlastDamage = 38.f;
	static constexpr float Lifetime = 30.f;
	static constexpr int32 MaxPerOwner = 2;

	int32 GetTeam() const;

protected:
	virtual void BeginPlay() override;

private:
	void Burst();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UStaticMeshComponent> Visual;

	float Age = 0.f;
	bool bBurst = false;

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBurst(FVector_NetQuantize Where);
};
