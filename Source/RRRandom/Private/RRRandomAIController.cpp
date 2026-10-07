#include "RRRandomAIController.h"
#include "RRRandomCharacter.h"
#include "RRRandomCover.h"
#include "RRRandomDiceBuffComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	constexpr float RetargetInterval = 0.5f;
	constexpr float CoverSearchInterval = 0.5f;
	// Distance band around the preferred distance where the bot only strafes
	constexpr float DistanceSlack = 150.f;
	constexpr float ArenaCenterArriveRadius = 200.f;
	constexpr float CoverArriveRadius = 40.f;
	// Pause before rolling dice, so bots don't react on the very same frame
	constexpr float DiceReactionMin = 0.2f;
	constexpr float DiceReactionMax = 0.8f;
	// Fights count as started a little outside attack range
	constexpr float EngageRangeShare = 1.2f;
	// Bots don't shoot at the very edge of their range, where shots fade out
	constexpr float ShootRangeShare = 0.9f;
	// A retreating bot still shoots at opponents this close, as a share of attack range
	constexpr float RetreatShootRangeShare = 0.5f;
	// Seconds a hit keeps the bot thinking it is under fire
	constexpr float UnderFireMemory = 1.5f;
	// Between bursts with this few rounds left, the bot reloads instead of running dry mid-fight
	constexpr int32 LowAmmo = 3;
	// BurstSize is meant for a magazine this big; other guns scale it
	constexpr int32 BaseMagazineForBursts = 15;
	// Opponents behind cover count as this much further away when picking a target
	constexpr float HiddenOpponentPenalty = 600.f;

	// Shots fly this far above the capsule center and are this wide (see the character's MuzzleOffset and the projectile)
	constexpr float ShotHeight = 20.f;
	constexpr float ShotRadius = 12.f;
	// A cover spot must stay hidden for a shot this wide, so the bot's body doesn't stick out
	constexpr float HiddenMargin = 30.f;
	// Gap between the cover and the bot's capsule at a cover spot
	constexpr float CoverSpotGap = 30.f;
	// Cover spots closer than this to the threat are no use
	constexpr float MinCoverDistanceFromThreat = 400.f;
	// Extra score for a cover spot a teammate already stands at
	constexpr float CrowdedCoverPenalty = 400.f;
	// How far ahead the bot checks for cover in its way, and how far it turns aside to get around it
	constexpr float AvoidProbeDistance = 120.f;
	constexpr float AvoidAngles[] = { 35.f, 70.f, 105.f, 140.f };

	/** Whether cover (anything world static) is between two capsule centers, at the height shots fly. */
	bool SweepHitsCover(const UWorld* World, const FVector& From, const FVector& To, float Radius)
	{
		const FVector Up(0.f, 0.f, ShotHeight);
		return World->SweepTestByObjectType(From + Up, To + Up, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
			FCollisionShape::MakeSphere(Radius), FCollisionQueryParams(SCENE_QUERY_STAT(RRRandomCoverSweep), false));
	}
}

ARRRandomAIController::ARRRandomAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ARRRandomAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (Seed != 0)
	{
		Stream.Initialize(Seed);
	}
	else
	{
		Stream.GenerateNewSeed();
	}
	DiceHoldLimit = Stream.RandRange(DiceHoldRange.X, DiceHoldRange.Y);
	StrafeSign = Stream.FRand() < 0.5f ? -1.f : 1.f;
	AvoidSign = Stream.FRand() < 0.5f ? -1.f : 1.f;

	// Each bot gets its own fighting distance
	if (const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(InPawn))
	{
		PreferredDistance = Brawler->GetAttackRange() * Stream.FRandRange(PreferredRangeShare.X, PreferredRangeShare.Y);
		LastHealth = Brawler->GetMaxHealth();
	}
}

void ARRRandomAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ARRRandomCharacter* Self = Cast<ARRRandomCharacter>(GetPawn());
	if (!Self || !Self->IsAlive())
	{
		Opponent = nullptr;
		bRetreating = false;
		bHadSight = false;
		bHasCoverSpot = false;
		BurstRemaining = 0;
		UnderFireTimer = 0.f;
		LastHealth = Self ? Self->GetMaxHealth() : 0.f;
		return;
	}

	RetargetTimer -= DeltaSeconds;
	if (RetargetTimer <= 0.f || !Opponent.IsValid() || !Opponent->IsAlive())
	{
		Opponent = FindBestOpponent(Self);
		RetargetTimer = RetargetInterval;
	}

	// Getting hit: dodge the other way right away, and stay wary for a moment
	const float Health = Self->GetHealth();
	if (Health < LastHealth)
	{
		UnderFireTimer = UnderFireMemory;
		StrafeSign = -StrafeSign;
		StrafeTimer = Stream.FRandRange(0.6f, 1.8f);
	}
	LastHealth = Health;
	UnderFireTimer -= DeltaSeconds;

	StrafeTimer -= DeltaSeconds;
	if (StrafeTimer <= 0.f)
	{
		StrafeSign = Stream.FRand() < 0.5f ? -1.f : 1.f;
		StrafeTimer = Stream.FRandRange(0.6f, 1.8f);
	}

	const float HealthShare = Health / FMath::Max(1.f, Self->GetMaxHealth());
	if (HealthShare < RetreatHealthShare)
	{
		bRetreating = true;
	}
	else if (HealthShare >= ReturnHealthShare)
	{
		bRetreating = false;
	}

	const ARRRandomCharacter* Target = Opponent.Get();
	const FVector SelfLocation = Self->GetActorLocation();
	const float Distance = Target ? FVector::Dist2D(SelfLocation, Target->GetActorLocation()) : UE_BIG_NUMBER;
	const bool bCanSee = Target && HasLineOfFire(SelfLocation, Target->GetActorLocation());

	// Take a moment to react whenever an opponent comes into view
	if (bCanSee && !bHadSight)
	{
		ReactionTimer = Stream.FRandRange(ReactionTime.X, ReactionTime.Y);
		BurstRemaining = 0;
	}
	bHadSight = bCanSee;
	ReactionTimer -= DeltaSeconds;
	ShotTimer -= DeltaSeconds;

	UpdateReload(Self, bCanSee && Distance <= Self->GetAttackRange());
	UpdateDice(Self, Distance, DeltaSeconds);

	// Hide while reloading or hurt; otherwise fight
	const bool bWantsCover = Target && (bRetreating || Self->IsReloading() || (UnderFireTimer > 0.f && HealthShare < UnderFireCoverHealthShare));
	FVector Wanted = FVector::ZeroVector;
	if (bWantsCover)
	{
		CoverSearchTimer -= DeltaSeconds;
		if (!bHasCoverSpot || CoverSearchTimer <= 0.f)
		{
			bHasCoverSpot = FindCoverSpot(Self, Target->GetActorLocation(), CoverSpot);
			CoverSearchTimer = CoverSearchInterval;
		}
		if (bHasCoverSpot)
		{
			const FVector ToSpot = CoverSpot - SelfLocation;
			Wanted = ToSpot.Size2D() > CoverArriveRadius ? ToSpot.GetSafeNormal2D() : FVector::ZeroVector;
		}
		else
		{
			// Nowhere to hide: back away while sidestepping
			const FVector Toward = (Target->GetActorLocation() - SelfLocation).GetSafeNormal2D();
			Wanted = -Toward + FVector::CrossProduct(FVector::UpVector, Toward) * StrafeSign * 0.4f;
		}
	}
	else
	{
		bHasCoverSpot = false;
		Wanted = ChooseFightDirection(Self, Target, Distance, bCanSee);
	}

	const FVector Move = SteerAroundObstacles(Self, Wanted) + GetSeparation(Self);
	if (!Move.IsNearlyZero())
	{
		Self->AddMovementInput(Move.GetSafeNormal2D());
	}

	if (bCanSee && ReactionTimer <= 0.f)
	{
		TryShoot(Self, Target, Distance);
	}
}

bool ARRRandomAIController::HasLineOfFire(const FVector& From, const FVector& To) const
{
	return !SweepHitsCover(GetWorld(), From, To, ShotRadius);
}

ARRRandomCharacter* ARRRandomAIController::FindBestOpponent(const ARRRandomCharacter* Self) const
{
	// The nearest opponent, but one in plain sight beats one hiding a little closer
	ARRRandomCharacter* Best = nullptr;
	float BestScore = UE_BIG_NUMBER;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetTeam() == Self->GetTeam() || !It->IsAlive())
		{
			continue;
		}
		float Score = FVector::Dist2D(Self->GetActorLocation(), It->GetActorLocation());
		if (!HasLineOfFire(Self->GetActorLocation(), It->GetActorLocation()))
		{
			Score += HiddenOpponentPenalty;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = *It;
		}
	}
	return Best;
}

FVector ARRRandomAIController::ChooseFightDirection(const ARRRandomCharacter* Self, const ARRRandomCharacter* Target, float Distance, bool bCanSee) const
{
	if (!Target)
	{
		const FVector ToCenter = (bHasArenaCenter ? ArenaCenter : Self->GetActorLocation()) - Self->GetActorLocation();
		return ToCenter.Size2D() > ArenaCenterArriveRadius ? ToCenter.GetSafeNormal2D() : FVector::ZeroVector;
	}

	const FVector Toward = (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Toward) * StrafeSign;

	// Cover in the way: work around it to get a clear shot
	if (!bCanSee)
	{
		return Toward + Side * 0.5f;
	}
	if (Distance > PreferredDistance + DistanceSlack)
	{
		return Toward + Side * 0.35f;
	}
	if (Distance < PreferredDistance - DistanceSlack)
	{
		return -Toward + Side * 0.6f;
	}
	return Side;
}

bool ARRRandomAIController::FindCoverSpot(const ARRRandomCharacter* Self, const FVector& Threat, FVector& OutSpot) const
{
	UWorld* World = GetWorld();
	const FVector SelfLocation = Self->GetActorLocation();
	const UCapsuleComponent* Capsule = Self->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float ThreatDistance = FVector::Dist2D(SelfLocation, Threat);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomCoverSpot), false);

	bool bFound = false;
	float BestScore = UE_BIG_NUMBER;
	for (TActorIterator<ARRRandomCover> It(World); It; ++It)
	{
		const FVector CoverCenter(It->GetActorLocation().X, It->GetActorLocation().Y, SelfLocation.Z);
		if (FVector::Dist2D(CoverCenter, SelfLocation) > CoverSearchRadius)
		{
			continue;
		}

		// Right behind the block, on the side away from the threat
		const FVector Away = (CoverCenter - Threat).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			continue;
		}
		const FVector Spot = CoverCenter + Away * (It->GetExtentAlong(Away) + Radius + CoverSpotGap);
		if (FVector::Dist2D(Spot, Threat) < MinCoverDistanceFromThreat)
		{
			continue;
		}

		// Somewhere a bot can stand: clear of other cover, with floor under it, and out of the threat's sight
		if (World->OverlapAnyTestByObjectType(Spot, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(Radius), Params)
			|| !World->LineTraceTestByChannel(Spot, Spot - FVector(0.f, 0.f, HalfHeight + 50.f), ECC_Visibility, Params)
			|| !SweepHitsCover(World, Threat, Spot, HiddenMargin))
		{
			continue;
		}

		// Prefer close spots, and don't run toward the threat to reach one
		float Score = FVector::Dist2D(SelfLocation, Spot) + FMath::Max(0.f, ThreatDistance - FVector::Dist2D(Spot, Threat)) * 1.5f;
		for (TActorIterator<ARRRandomCharacter> Mate(World); Mate; ++Mate)
		{
			if (*Mate != Self && Mate->GetTeam() == Self->GetTeam() && Mate->IsAlive() && FVector::Dist2D(Mate->GetActorLocation(), Spot) < SeparationDistance)
			{
				Score += CrowdedCoverPenalty;
			}
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			OutSpot = Spot;
			bFound = true;
		}
	}
	return bFound;
}

FVector ARRRandomAIController::SteerAroundObstacles(const ARRRandomCharacter* Self, const FVector& Wanted)
{
	if (Wanted.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	// A slightly thin probe, so a bot already pressed against a wall isn't stuck inside its own check
	UWorld* World = GetWorld();
	const FVector Location = Self->GetActorLocation();
	const FCollisionShape Probe = FCollisionShape::MakeSphere(Self->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.8f);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomSteer), false);
	auto IsClear = [&](const FVector& Direction)
	{
		return !World->SweepTestByObjectType(Location, Location + Direction * AvoidProbeDistance, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Probe, Params);
	};

	const FVector Direction = Wanted.GetSafeNormal2D();
	if (IsClear(Direction))
	{
		return Direction;
	}

	// Turn further and further aside, keeping to the side that worked last time so the bot doesn't dither
	for (const float Angle : AvoidAngles)
	{
		for (const float Sign : { AvoidSign, -AvoidSign })
		{
			const FVector Turned = Direction.RotateAngleAxis(Angle * Sign, FVector::UpVector);
			if (IsClear(Turned))
			{
				AvoidSign = Sign;
				return Turned;
			}
		}
	}
	return Direction;
}

FVector ARRRandomAIController::GetSeparation(const ARRRandomCharacter* Self) const
{
	FVector Push = FVector::ZeroVector;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (*It == Self || It->GetTeam() != Self->GetTeam() || !It->IsAlive())
		{
			continue;
		}
		const FVector Away = Self->GetActorLocation() - It->GetActorLocation();
		const float Dist = Away.Size2D();
		if (Dist < SeparationDistance)
		{
			Push += Away.GetSafeNormal2D() * (1.f - Dist / SeparationDistance);
		}
	}
	return Push;
}

void ARRRandomAIController::TryShoot(ARRRandomCharacter* Self, const ARRRandomCharacter* Target, float Distance)
{
	const float MaxShootDistance = Self->GetAttackRange() * (bRetreating ? RetreatShootRangeShare : ShootRangeShare);
	if (ShotTimer > 0.f || Distance > MaxShootDistance || Self->IsReloading() || Self->GetAmmo() <= 0)
	{
		return;
	}
	if (BurstRemaining <= 0)
	{
		// Big magazines (minigun) fire longer bursts, tiny ones (cannon) shorter
		const float MagazineScale = Self->GetMaxAmmo() / static_cast<float>(BaseMagazineForBursts);
		BurstRemaining = FMath::Max(1, FMath::RoundToInt(Stream.RandRange(BurstSize.X, BurstSize.Y) * MagazineScale));
	}

	// Lead the target by a random part of its travel time, then throw the aim off a little
	const FVector SelfLocation = Self->GetActorLocation();
	const float ProjectileSpeed = Self->GetProjectileSpeed();
	const float LeadTime = ProjectileSpeed > 0.f ? Distance / ProjectileSpeed * Stream.FRandRange(0.f, 0.8f) : 0.f;
	// Only sideways motion is led; a jump would otherwise tilt the shot
	const FVector Predicted = Target->GetActorLocation() + FVector(Target->GetVelocity().X, Target->GetVelocity().Y, 0.f) * LeadTime;
	// Swing the aim point around the bot by the error, keeping the target's height so shots tilt toward other levels
	const FVector Aim = SelfLocation + (Predicted - SelfLocation).RotateAngleAxis(Stream.FRandRange(-AimErrorDegrees, AimErrorDegrees), FVector::UpVector);

	// The character's fire interval paces shots within a burst
	if (Self->FireAt(Aim) && --BurstRemaining <= 0)
	{
		ShotTimer = Stream.FRandRange(BurstPause.X, BurstPause.Y);
	}
}

void ARRRandomAIController::UpdateReload(ARRRandomCharacter* Self, bool bOpponentInSight)
{
	if (Self->IsReloading() || Self->GetAmmo() >= Self->GetMaxAmmo())
	{
		return;
	}

	// Top up while nobody is around, or between bursts before the magazine runs dry
	const bool bQuiet = !bOpponentInSight && Self->GetAmmo() <= FMath::FloorToInt(Self->GetMaxAmmo() * TacticalReloadShare);
	const bool bNearlyEmpty = Self->GetAmmo() <= FMath::Min(LowAmmo, Self->GetMaxAmmo() / 5) && BurstRemaining <= 0;
	if (bQuiet || bNearlyEmpty)
	{
		Self->StartReload();
	}
}

void ARRRandomAIController::UpdateDice(ARRRandomCharacter* Self, float Distance, float DeltaSeconds)
{
	URRRandomDiceBuffComponent* Dice = Self->GetDiceBuffs();
	if (Dice->GetCharges() <= 0)
	{
		return;
	}

	// Buffs run out, so save dice for a fight unless too many have piled up
	const bool bEngaging = !bRetreating && Distance <= Self->GetAttackRange() * EngageRangeShare;
	if (!bEngaging && Dice->GetCharges() < DiceHoldLimit)
	{
		DiceTimer = Stream.FRandRange(DiceReactionMin, DiceReactionMax);
		return;
	}

	DiceTimer -= DeltaSeconds;
	if (DiceTimer <= 0.f)
	{
		Dice->RollDice();
		DiceHoldLimit = Stream.RandRange(DiceHoldRange.X, DiceHoldRange.Y);
		DiceTimer = Stream.FRandRange(DiceReactionMin, DiceReactionMax);
	}
}
