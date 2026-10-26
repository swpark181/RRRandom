#include "RRRandomAIController.h"
#include "RRRandomCharacter.h"
#include "RRRandomCover.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomDiceTown.h"
#include "RRRandomGameMode.h"
#include "RRRandomGameState.h"
#include "RRRandomMeteor.h"
#include "RRRandomOre.h"
#include "RRRandomProjectile.h"
#include "RRRandomStarRobot.h"
#include "RRRandomVehicle.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "NavigationSystem.h"

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
	// Seconds a bot keeps heading for where it last saw an opponent that slipped into the fog of war
	constexpr float SearchTime = 4.f;
	// With nobody in sight, bots roam between random spots this far from the arena center, picking the next one on
	// arrival or after this many seconds (cover taller than half a brawler hides the other side, so standing still in the
	// middle could leave both teams staring at the same block)
	constexpr float PatrolRadiusMin = 300.f;
	constexpr float PatrolRadiusMax = 1800.f;
	constexpr float PatrolTimeMin = 3.f;
	constexpr float PatrolTimeMax = 6.f;
	// On a map with walking routes (Dice Town, much bigger): roam between spots this far from the middle, a new one on
	// arrival or after this long; a new route this often or when the goal moves this far; a corner counts as reached this
	// close; stopped this long at a ledge, jump
	constexpr float RoutePatrolRadius = 5500.f;
	constexpr float RoutePatrolTime = 25.f;
	constexpr float RouteRefreshTime = 1.5f;
	constexpr float RouteRefreshDistance = 300.f;
	constexpr float RouteArriveRadius = 35.f;
	constexpr float RouteStuckJumpTime = 0.5f;
	// Onto the navmesh: the bot's feet from this close; a goal (maybe up in the air: a flyer, a jump) from this far round it
	const FVector NavFeetExtent(200.f, 200.f, 400.f);
	const FVector NavGoalExtent(400.f, 400.f, 1500.f);
	// Between bursts with this few rounds left, the bot reloads instead of running dry mid-fight
	constexpr int32 LowAmmo = 3;
	// BurstSize is meant for a magazine this big; other guns scale it
	constexpr int32 BaseMagazineForBursts = 15;
	// Opponents behind cover count as this much further away when picking a target
	constexpr float HiddenOpponentPenalty = 600.f;

	// Vehicles: a car counts as stopped (to hop in the back) below this speed; a vehicle more than this far above or
	// below isn't reachable; the bot gives up walking to one after this long, looks for one this often, and gets off
	// after this long barely moving at the wheel (wedged against cover)
	constexpr float StoppedSpeed = 40.f;
	constexpr float HeightReach = 120.f;
	constexpr float VehicleWalkTime = 8.f;
	constexpr float VehicleSearchInterval = 0.5f;
	constexpr float StuckSpeed = 60.f;
	constexpr float StuckTime = 1.5f;
	// A bot heads for its team's star robot with a seat free from this far away at most
	constexpr float RobotCrewSearchRadius = 12000.f;
	// Driving the team's robot: at the rival robot from this far away, up to this close (its sword reaches it there)
	constexpr float RobotRivalSearchRadius = 9000.f;
	constexpr float RobotDuelDistance = 520.f;

	// Shots fly this far above the capsule center, this far to the shooter's right (the rifle's muzzle in the fox's fire
	// stance) and are this wide (see the character's MuzzleOffset and the projectile)
	constexpr float ShotHeight = 2.f;
	constexpr float ShotSide = 12.f;
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
	// A bot that means to walk but gets less than this far in this long is stalled: out of a fight it gives its goal up and
	// heads off another way (and it is logged with rr.Bot.LogStalls)
	constexpr float StallDistance = 80.f;
	constexpr float StallTime = 3.f;
	// Wedged this long (meaning to walk, slower than StuckSpeed): go round for DetourTime; this many times without getting
	// anywhere, give the goal up (and keep away from a cover spot that couldn't be reached for this long)
	constexpr float WedgedTime = 0.6f;
	constexpr float DetourTime = 0.5f;
	constexpr int32 WedgesToGiveUp = 3;
	constexpr float BadCoverMemory = 8.f;
	constexpr float BadCoverRadius = 200.f;
	// How much of the push away from teammates is kept while following a route
	constexpr float RouteSeparationShare = 0.25f;
	TAutoConsoleVariable<int32> CVarLogStalls(TEXT("rr.Bot.LogStalls"), 0,
		TEXT("1: log each time a bot means to walk but gets nowhere for a while (where, why, what's in the way)."));

	/** Whether cover (anything world static) is between two capsule centers, along the line a shot from From would fly. */
	bool SweepHitsCover(const UWorld* World, const FVector& From, const FVector& To, float Radius)
	{
		// Level shots fly parallel to the line between the brawlers, from the muzzle beside the shooter
		const FVector Ahead = (To - From).GetSafeNormal2D();
		const FVector Offset = FVector(-Ahead.Y, Ahead.X, 0.f) * ShotSide + FVector(0.f, 0.f, ShotHeight);
		return World->SweepTestByObjectType(From + Offset, To + Offset, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic),
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
		PreferredShare = Stream.FRandRange(PreferredRangeShare.X, PreferredRangeShare.Y);
		LastHealth = Brawler->GetMaxHealth();
	}
}

void ARRRandomAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ARRRandomCharacter* Self = Cast<ARRRandomCharacter>(GetPawn());
#if !UE_BUILD_SHIPPING
	// Scripted tests (-RRIdleEnemies): the other team's bots just stand there
	static const bool bIdleEnemies = FParse::Param(FCommandLine::Get(), TEXT("RRIdleEnemies"));
	if (bIdleEnemies && Self && Self->GetTeam() != 0)
	{
		return;
	}
#endif
	// The studio (a test floor for animation work): the other team's bots stand still there too
	const ARRRandomGameMode* Mode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>();
	if (Mode && Mode->IsStudio() && Self && Self->GetTeam() != 0)
	{
		return;
	}
	// Counting down to the start, or the match is over: stand still
	if (Self && ARRRandomGameState::IsPlayLocked(this))
	{
		Self->StopJumping();
		return;
	}
	if (!Self || !Self->IsAlive())
	{
		Opponent = nullptr;
		VehicleTarget = nullptr;
		StuckTimer = 0.f;
		bRetreating = false;
		bHadSight = false;
		bHasCoverSpot = false;
		BurstRemaining = 0;
		UnderFireTimer = 0.f;
		FlightHoldRemaining = 0.f;
		LastHealth = Self ? Self->GetMaxHealth() : 0.f;
		return;
	}

	// Bots go by the fog of war like players: an opponent out of the team's sight is lost (remembered where last seen)
	if (Opponent.IsValid() && !Opponent->IsNoticedByTeam(Self->GetTeam()))
	{
		Opponent = nullptr;
	}
	RetargetTimer -= DeltaSeconds;
	if (RetargetTimer <= 0.f || !Opponent.IsValid() || !Opponent->IsAlive())
	{
		Opponent = FindBestOpponent(Self);
		RetargetTimer = RetargetInterval;
	}
	// Meteor Rush: an opponent carrying a crystal shows on every map; go after it first
	const bool bRush = ARRRandomGameState::IsMeteorRush(this);
	if (bRush)
	{
		if (ARRRandomCharacter* Carrier = FindCrystalCarrier(Self))
		{
			Opponent = Carrier;
		}
		UpdateRushGoal(Self, DeltaSeconds);
	}
	else
	{
		bHasRushGoal = false;
		bGuardingOre = false;
	}
	if (Opponent.IsValid())
	{
		LastSeenLocation = Opponent->GetActorLocation();
		LastSeenTimer = SearchTime;
	}
	LastSeenTimer -= DeltaSeconds;

	// A map with walking routes, if this is one
	if (!bLookedForTown)
	{
		bLookedForTown = true;
		TActorIterator<ARRRandomDiceTown> It(GetWorld());
		Town = It && It->IsBuilt() ? *It : nullptr;
		bLookedForTown = Town.IsValid() || !It;
	}
	const ARRRandomDiceTown* Map = Town.Get();
	const bool bRoutes = HasRoutes();

	// Nobody in sight and no trail to follow: roam the middle until someone shows up (on a map with routes, anywhere on
	// its ground, upstairs and rooftops included)
	PatrolTimer -= DeltaSeconds;
	if (!Opponent.IsValid() && LastSeenTimer <= 0.f && bHasArenaCenter
		&& (PatrolTimer <= 0.f || FVector::Dist2D(Self->GetActorLocation(), PatrolGoal) <= ArenaCenterArriveRadius))
	{
		FNavLocation NavSpot;
		const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (Map)
		{
			PatrolGoal = Map->GetRandomWalkSpot(Stream, RoutePatrolRadius);
			PatrolTimer = RoutePatrolTime;
		}
		else if (bRoutes && Nav && Nav->GetRandomPointInNavigableRadius(ArenaCenter, PatrolRadiusMax, NavSpot))
		{
			PatrolGoal = NavSpot.Location;
			PatrolTimer = Stream.FRandRange(PatrolTimeMin, PatrolTimeMax) * 2.f;
		}
		else
		{
			PatrolGoal = ArenaCenter + FVector(Stream.FRandRange(PatrolRadiusMin, PatrolRadiusMax), 0.f, 0.f).RotateAngleAxis(Stream.FRandRange(0.f, 360.f), FVector::UpVector);
			PatrolTimer = Stream.FRandRange(PatrolTimeMin, PatrolTimeMax);
		}
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
	UpdateFlight(Self, Distance, DeltaSeconds);

	// Hide while reloading or hurt (or keeping the team's winning ore safe); otherwise fight
	const bool bWantsCover = Target && (bRetreating || bGuardingOre || Self->IsReloading() || (UnderFireTimer > 0.f && HealthShare < UnderFireCoverHealthShare));
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
	// Meteor Rush: the ore (or home with it) unless an opponent is in sight and in range
	const bool bRushing = bHasRushGoal && !bWantsCover && !(Target && bCanSee && Distance <= Self->GetAttackRange() * ShootRangeShare);
	if (bRushing && !bRoutes)
	{
		const FVector ToGoal = RushGoal - (SelfLocation - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		Wanted = ToGoal.Size2D() > CoverArriveRadius ? ToGoal.GetSafeNormal2D() : FVector::ZeroVector;
	}
	// On a map with routes, out of a close fight: along the route to the opponent, where it was last seen, or the roaming
	// spot (feet heights)
	bool bOnRoute = false;
	if (bRoutes && !bWantsCover && (bRushing ||!(Target && bCanSee && Distance <= Self->GetAttackRange() * PreferredShare + DistanceSlack)))
	{
		const FVector Goal = bRushing ? RushGoal
			: Target ? Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
			: LastSeenTimer > 0.f ? LastSeenLocation - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight())
			: PatrolGoal;
		Wanted = FollowRoute(Self, Goal, DeltaSeconds);
		bOnRoute = true;
	}
	else
	{
		RouteStuckTimer = 0.f;
	}

	// Vehicles: only out of a fight (an opponent in sight near its range, or being hit)
	const bool bInFight = bWantsCover || UnderFireTimer > 0.f || (Target && Distance <= Self->GetAttackRange() * EngageRangeShare);
	if (!UpdateStarRobot(Self, bInFight, Wanted, bOnRoute, DeltaSeconds))
	{
		UpdateVehicles(Self, Target, bInFight, Wanted, DeltaSeconds);
	}

	// (a route already goes round walls, and its stairs would look like walls to the steering's probe)
	// (On a route the push away from teammates is weak: in a lane between walls it would only shove them into the walls)
	const FVector Move = Unstick(Self, (bOnRoute ? Wanted : SteerAroundObstacles(Self, Wanted)) + GetSeparation(Self) * (bOnRoute ? RouteSeparationShare : 1.f), DeltaSeconds);
	BadCoverTimer -= DeltaSeconds;
	if (!Move.IsNearlyZero())
	{
		Self->AddMovementInput(Move.GetSafeNormal2D());
	}
	WatchProgress(Self, Move, bOnRoute ? TEXT("route") : bWantsCover ? TEXT("cover") : bRushing ? TEXT("rush") : Target ? TEXT("fight") : TEXT("roam"), DeltaSeconds);

	if (bCanSee && ReactionTimer <= 0.f)
	{
		TryShoot(Self, Target, Distance);
	}
}

void ARRRandomAIController::UpdateRushGoal(const ARRRandomCharacter* Self, float DeltaSeconds)
{
	RushTimer -= DeltaSeconds;
	if (RushTimer > 0.f)
	{
		return;
	}
	RushTimer = 0.3f;
	bHasRushGoal = false;
	bGuardingOre = false;
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	const URRRandomOreCarryComponent* Carry = Self->GetOreCarry();
	if (!State || !Carry)
	{
		return;
	}
	const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	// The team is counting down to a win: carrying a share of it, keep it safe at home
	if (State->GetHoldTeam() == Self->GetTeam() && (Carry->GetPoints() >= 3 || Carry->HasGolden()))
	{
		bGuardingOre = true;
		bHasRushGoal = true;
		RushGoal = Self->GetHomeLocation() - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		return;
	}

	// The best ore in reach: near, and worth more (a crystal is worth a detour; overtime's golden one, any)
	const bool bBigMap = Town.IsValid();
	const float Reach = bBigMap ? 6500.f : 3500.f;
	float Best = UE_BIG_NUMBER;
	for (TActorIterator<ARRRandomOre> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(Feet, It->GetActorLocation());
		if (Distance > Reach && It->GetKind() != ERROreKind::Golden)
		{
			continue;
		}
		// Off a map with walking routes, bots don't climb: ore up on a block or a hill is left to the players
		if (!bBigMap && It->IsResting() && It->GetKind() != ERROreKind::Golden && It->GetActorLocation().Z - Feet.Z > 90.f)
		{
			continue;
		}
		const float Score = Distance - (It->GetKind() == ERROreKind::Golden ? 10000.f : RROre::IsCrystal(It->GetKind()) ? 900.f : It->GetKind() == ERROreKind::Ore ? 250.f : 0.f);
		if (Score < Best)
		{
			Best = Score;
			RushGoal = It->GetActorLocation();
			bHasRushGoal = true;
		}
	}
	if (bHasRushGoal)
	{
		return;
	}

	// Else a meteor on its way or still throwing: wait just outside where it lands
	for (TActorIterator<ARRRandomMeteor> It(GetWorld()); It; ++It)
	{
		if (!It->IsActive())
		{
			continue;
		}
		const FVector Spot = It->GetActorLocation();
		const float Distance = FVector::Dist2D(Feet, Spot);
		if (Distance > Reach * 1.5f || Distance >= Best)
		{
			continue;
		}
		Best = Distance;
		const FVector Out = Distance > 1.f ? (Feet - Spot).GetSafeNormal2D() : FVector(1.f, 0.f, 0.f);
		RushGoal = It->GetTimeToImpact() > 0.3f ? Spot + Out * (ARRRandomMeteor::ImpactRadius + 120.f) : Spot;
		bHasRushGoal = true;
	}
}

ARRRandomCharacter* ARRRandomAIController::FindCrystalCarrier(const ARRRandomCharacter* Self) const
{
	ARRRandomCharacter* Best = nullptr;
	float BestDistance = Town.IsValid() ? 5000.f : 3500.f;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		const URRRandomOreCarryComponent* Carry = It->GetOreCarry();
		if (It->GetTeam() == Self->GetTeam() || !It->IsAlive() || It->IsDecoy() || !Carry || !Carry->HasCrystal())
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Self->GetActorLocation(), It->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	return Best;
}

FVector ARRRandomAIController::GetTravelGoal(const ARRRandomCharacter* Self, const ARRRandomCharacter* Target) const
{
	if (bHasRushGoal && !Target)
	{
		return RushGoal;
	}
	if (Target)
	{
		return Target->GetActorLocation();
	}
	if (LastSeenTimer > 0.f)
	{
		return LastSeenLocation;
	}
	return bHasArenaCenter ? PatrolGoal : Self->GetActorLocation();
}

FVector ARRRandomAIController::FollowRoute(ARRRandomCharacter* Self, const FVector& Goal, float DeltaSeconds)
{
	const ARRRandomDiceTown* Map = Town.Get();
	const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	RouteTimer -= DeltaSeconds;
	if (HasRoutes() && (RouteTimer <= 0.f || RouteIndex >= Route.Num() || FVector::Dist(Goal, RouteGoal) > RouteRefreshDistance))
	{
		RouteTimer = RouteRefreshTime;
		RouteGoal = Goal;
		RouteIndex = 0;
		// The navmesh first (it knows what's smashed); Dice Town's walking graph while the navmesh is still being made
		bNavRoute = FindNavRoute(Feet, Goal, Route);
		if (!bNavRoute && !(Map && Map->FindPath(Feet, Goal, Route)))
		{
			Route.Reset();
		}
	}
	// Past the corners already reached
	while (Route.IsValidIndex(RouteIndex) && FVector::Dist2D(Feet, Route[RouteIndex]) < RouteArriveRadius && FMath::Abs(Feet.Z - Route[RouteIndex].Z) < 150.f)
	{
		++RouteIndex;
	}
	const FVector Next = Route.IsValidIndex(RouteIndex) ? Route[RouteIndex] : Goal;
	const FVector Toward = Next - Feet;
	if (Toward.Size2D() < RouteArriveRadius)
	{
		RouteStuckTimer = 0.f;
		return FVector::ZeroVector;
	}
	// Stopped against something: jump (at a ledge a jump climbs it), and soon look for another route
	const bool bStopped = Self->GetVelocity().Size2D() < StuckSpeed && !Self->GetVehicle();
	RouteStuckTimer = bStopped ? RouteStuckTimer + DeltaSeconds : 0.f;
	if (RouteStuckTimer >= RouteStuckJumpTime)
	{
		Self->Jump();
		RouteStuckTimer = -RouteStuckJumpTime;
		RouteTimer = FMath::Min(RouteTimer, 0.5f);
	}
	return Toward.GetSafeNormal2D();
}

bool ARRRandomAIController::HasRoutes() const
{
	if (Town.IsValid())
	{
		return true;
	}
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	return Nav && Nav->GetDefaultNavDataInstance() != nullptr;
}

bool ARRRandomAIController::FindNavRoute(const FVector& Feet, const FVector& Goal, TArray<FVector>& OutPoints) const
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* Data = Nav ? Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	FNavLocation Start;
	FNavLocation End;
	if (!Data || !Nav->ProjectPointToNavigation(Feet, Start, NavFeetExtent, Data) || !Nav->ProjectPointToNavigation(Goal, End, NavGoalExtent, Data))
	{
		return false;
	}
	FPathFindingQuery Query(this, *Data, Start.Location, End.Location);
	Query.SetAllowPartialPaths(true);
	const FPathFindingResult Result = Nav->FindPathSync(Query);
	if (!Result.IsSuccessful() || !Result.Path.IsValid())
	{
		return false;
	}
	const TArray<FNavPathPoint>& Points = Result.Path->GetPathPoints();
	OutPoints.Reset(Points.Num());
	for (int32 Index = 1; Index < Points.Num(); ++Index)
	{
		OutPoints.Add(Points[Index].Location);
	}
	return OutPoints.Num() > 0;
}

bool ARRRandomAIController::IsStillWorthRiding(const ARRRandomCharacter* Self, const ARRRandomVehicle* Ride) const
{
	// Not the star team's cars: gathering those for the robot is for players (a bot would only drive them apart)
	if (!Ride || !Ride->HasRoom() || Ride->GetStarPart() >= 0)
	{
		return false;
	}
	const ARRRandomCharacter* Driver = Ride->GetRider();
	// The driver's seat free, or the back of a teammate's car that is still standing for it
	return !Driver || (Driver->GetTeam() == Self->GetTeam() && Driver->GetVelocity().Size2D() <= StoppedSpeed);
}

ARRRandomVehicle* ARRRandomAIController::FindVehicleToRide(const ARRRandomCharacter* Self, const ARRRandomCharacter* Target) const
{
	const FVector Location = Self->GetActorLocation();
	const float Trip = FVector::Dist2D(Location, GetTravelGoal(Self, Target));
	ARRRandomVehicle* Best = nullptr;
	float BestScore = UE_BIG_NUMBER;
	for (TActorIterator<ARRRandomVehicle> It(GetWorld()); It; ++It)
	{
		ARRRandomVehicle* Ride = *It;
		if (!IsStillWorthRiding(Self, Ride) || FMath::Abs(Ride->GetActorLocation().Z - Location.Z) > HeightReach)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Location, Ride->GetActorLocation());
		float Score = Distance;
		if (Ride->GetRider())
		{
			// A lift with a teammate, trip or not; first choice
			if (Distance > PassengerJoinRadius)
			{
				continue;
			}
			Score -= PassengerJoinRadius;
		}
		else
		{
			// Driving only pays for a long trip, with the vehicle not far out of the way
			if (Trip < RideTripDistance || Distance > VehicleSearchRadius || Distance > Trip * 0.5f)
			{
				continue;
			}
			// Another bot already going for it
			bool bTaken = false;
			for (TActorIterator<ARRRandomAIController> Other(GetWorld()); Other; ++Other)
			{
				bTaken |= *Other != this && Other->GetVehicleTarget() == Ride;
			}
			if (bTaken)
			{
				continue;
			}
		}
		if (Score < BestScore)
		{
			Best = Ride;
			BestScore = Score;
		}
	}
	return Best;
}

bool ARRRandomAIController::UpdateStarRobot(ARRRandomCharacter* Self, bool bInFight, FVector& Wanted, bool& bOnRoute, float DeltaSeconds)
{
	// Combining it (standing still) or in it: the robot is all there is. Driving it with the rival robot about: at it, up
	// to sword's reach, then circling it a little
	if (const ARRRandomStarRobot* Mine = Self->GetRobot())
	{
		const ARRRandomStarRobot* Rival = Mine->IsAssembled() && Self->GetRobotSeat() == ERRRobotSeat::Move ? Mine->GetRival() : nullptr;
		const FVector ToRival = Rival ? (Rival->GetActorLocation() - Mine->GetActorLocation()) * FVector(1.f, 1.f, 0.f) : FVector::ZeroVector;
		if (Rival && ToRival.Size() <= RobotRivalSearchRadius)
		{
			const FVector Toward = ToRival.GetSafeNormal();
			if (HasRoutes() && ToRival.Size() > RobotDuelDistance * 3.f)
			{
				Wanted = FollowRoute(Self, Rival->GetActorLocation(), DeltaSeconds);
				bOnRoute = true;
			}
			else
			{
				Wanted = ToRival.Size() > RobotDuelDistance ? Toward : FVector::CrossProduct(FVector::UpVector, Toward) * StrafeSign * 0.25f;
				bOnRoute = false;
			}
		}
		return true;
	}
	// Next to the gathered cars out of a fight: combine them, unless a player on the team is there too (most likely the
	// one who drove them together: theirs to press)
	if (!bInFight && ARRRandomStarRobot::CanStartCombine(Self))
	{
		bool bPlayerThere = false;
		for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It && !bPlayerThere; ++It)
		{
			bPlayerThere = It->IsPlayerControlled() && It->GetTeam() == Self->GetTeam() && ARRRandomStarRobot::CanStartCombine(*It);
		}
		if (!bPlayerThere)
		{
			ARRRandomStarRobot::StartCombine(Self);
			return true;
		}
	}
	// The team's own cars gathered with no robot yet, not too far, out of a fight: over to them (to combine, above)
	ARRRandomVehicle* Cars[3];
	FVector Gathered;
	if (!bInFight && !Self->IsGiant() && !Self->CanFly() && ARRRandomStarRobot::FindGathered(this, Self->GetTeam(), Cars, Gathered))
	{
		const FVector ToCars = (Gathered - Self->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (ToCars.Size() <= RobotCrewSearchRadius && ToCars.Size() > ARRRandomStarRobot::CombineReach * 0.6f)
		{
			if (HasRoutes())
			{
				Wanted = FollowRoute(Self, Gathered, DeltaSeconds);
				bOnRoute = true;
			}
			else
			{
				Wanted = ToCars.GetSafeNormal();
				bOnRoute = false;
			}
			return true;
		}
	}
	// The team's robot with a seat free, not too far: over to it and in
	const ARRRandomStarRobot* Robot = ARRRandomStarRobot::Get(this, Self->GetTeam());
	if (!Robot || !Robot->IsAssembled() || Robot->GetTeam() != Self->GetTeam() || Robot->GetFreeSeat() == ERRRobotSeat::None || Self->IsGiant() || Self->CanFly())
	{
		return false;
	}
	const FVector ToRobot = Robot->GetActorLocation() - Self->GetActorLocation();
	const float Distance = ToRobot.Size2D();
	if (Distance > RobotCrewSearchRadius)
	{
		return false;
	}
	if (Distance <= ARRRandomStarRobot::BoardReach * 0.8f)
	{
		// Off any vehicle first (F does that), then in
		Self->ToggleRide();
		return true;
	}
	// A map with walking routes: along them, round the walls (to its feet), the last stretch straight at it
	if (HasRoutes() && Distance > ARRRandomStarRobot::BoardReach * 1.6f)
	{
		Wanted = FollowRoute(Self, Robot->GetActorLocation(), DeltaSeconds);
		bOnRoute = true;
		return true;
	}
	Wanted = ToRobot.GetSafeNormal2D();
	bOnRoute = false;
	return true;
}

bool ARRRandomAIController::UpdateVehicles(ARRRandomCharacter* Self, const ARRRandomCharacter* Target, bool bInFight, FVector& Wanted, float DeltaSeconds)
{
	const FVector Location = Self->GetActorLocation();
	if (const ARRRandomVehicle* Ridden = Self->GetVehicle())
	{
		VehicleTarget = nullptr;
		// A fight: on foot (F again gets off either seat)
		if (bInFight)
		{
			StuckTimer = 0.f;
			Self->ToggleRide();
			return false;
		}
		// At the wheel: steered by the usual heading; off near the goal, or when it can't get on
		if (Ridden->GetRider() == Self)
		{
			const bool bArrived = FVector::Dist2D(Location, GetTravelGoal(Self, Target)) <= RideArriveDistance;
			StuckTimer = Self->GetVelocity().Size2D() < StuckSpeed ? StuckTimer + DeltaSeconds : 0.f;
			if (bArrived || StuckTimer >= StuckTime)
			{
				StuckTimer = 0.f;
				Self->ToggleRide();
			}
		}
		// In the back: along for the ride until the driver gets off
		return false;
	}
	StuckTimer = 0.f;
	// No seat fits a giant or a flyer
	if (bInFight || Self->IsGiant() || Self->CanFly())
	{
		VehicleTarget = nullptr;
		return false;
	}

	VehicleSearchTimer -= DeltaSeconds;
	if (!VehicleTarget.IsValid() && VehicleSearchTimer <= 0.f)
	{
		VehicleSearchTimer = VehicleSearchInterval;
		VehicleTarget = FindVehicleToRide(Self, Target);
		VehicleTargetTimer = VehicleWalkTime;
	}
	const ARRRandomVehicle* Ride = VehicleTarget.Get();
	if (!Ride)
	{
		return false;
	}
	VehicleTargetTimer -= DeltaSeconds;
	if (VehicleTargetTimer <= 0.f || !IsStillWorthRiding(Self, Ride))
	{
		VehicleTarget = nullptr;
		return false;
	}
	// Walk over and get on (the nearest vehicle with room once in reach, which this one is by then)
	const FVector ToRide = Ride->GetActorLocation() - Location;
	if (ToRide.Size2D() <= ARRRandomVehicle::MountRange * 0.8f)
	{
		VehicleTarget = nullptr;
		Self->ToggleRide();
		return false;
	}
	Wanted = ToRide.GetSafeNormal2D();
	return true;
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
		// Only opponents the team can see through the fog of war
		if (It->GetTeam() == Self->GetTeam() || !It->IsAlive() || !It->IsNoticedByTeam(Self->GetTeam()))
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
		// Lost in the fog: look where it was last seen for a while, then roam the middle again
		const FVector Goal = LastSeenTimer > 0.f ? LastSeenLocation : (bHasArenaCenter ? PatrolGoal : Self->GetActorLocation());
		const FVector ToGoal = Goal - Self->GetActorLocation();
		return ToGoal.Size2D() > ArenaCenterArriveRadius ? ToGoal.GetSafeNormal2D() : FVector::ZeroVector;
	}

	const FVector Toward = (Target->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D();
	const FVector Side = FVector::CrossProduct(FVector::UpVector, Toward) * StrafeSign;

	// Its own distance, whatever gun it holds now (a foam finger wants to be right up close)
	const float PreferredDistance = Self->GetAttackRange() * PreferredShare;
	const float Slack = FMath::Min(DistanceSlack, PreferredDistance * 0.4f);

	// Cover in the way: work around it to get a clear shot
	if (!bCanSee)
	{
		return Toward + Side * 0.5f;
	}
	if (Distance > PreferredDistance + Slack)
	{
		return Toward + Side * 0.35f;
	}
	if (Distance < PreferredDistance - Slack)
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
		// (Not one smashed for now: ARRRandomWreckage hides it)
		if (It->IsHidden())
		{
			continue;
		}
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
		if (FVector::Dist2D(Spot, Threat) < MinCoverDistanceFromThreat || (BadCoverTimer > 0.f && FVector::Dist2D(Spot, BadCoverSpot) < BadCoverRadius))
		{
			continue;
		}
		// Not past a wall that stops brawlers only (the island's rim, the water's edge): a spot there can't be reached
		FCollisionQueryParams RimParams(SCENE_QUERY_STAT(RRRandomCoverRim), false, Self);
		if (World->SweepTestByObjectType(CoverCenter, Spot, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldDynamic), FCollisionShape::MakeSphere(Radius * 0.5f), RimParams))
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

	auto IsClear = [&](const FVector& Direction) { return IsWayClear(Self, Direction, AvoidProbeDistance); };

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

bool ARRRandomAIController::IsWayClear(const ARRRandomCharacter* Self, const FVector& Direction, float Distance, FHitResult* OutHit) const
{
	// The body from just over a step up to the head, a little thinner (so a bot already pressed against a wall isn't stuck
	// inside its own check): low things it can't step onto count, the floor and steps don't. What stops walking: cover and
	// walls (world static) and the island's rim walls, which stop brawlers only (world dynamic)
	const UCapsuleComponent* Capsule = Self->GetCapsuleComponent();
	const float Radius = Capsule->GetScaledCapsuleRadius() * 0.8f;
	const float Lift = Self->GetCharacterMovement()->MaxStepHeight + 5.f;
	const float HalfHeight = FMath::Max(Radius, Capsule->GetScaledCapsuleHalfHeight() - Lift * 0.5f);
	const FVector From = Self->GetActorLocation() + FVector(0.f, 0.f, Lift * 0.5f);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomSteer), false, Self);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByObjectType(Hit, From, From + Direction.GetSafeNormal2D() * Distance, FQuat::Identity, Objects,
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params);
	// (Shots fly as world dynamic too; one passing by isn't a wall)
	const bool bBlocked = bHit && !(Hit.GetActor() && Hit.GetActor()->IsA<ARRRandomProjectile>());
	if (OutHit)
	{
		*OutHit = Hit;
	}
	return !bBlocked;
}

FVector ARRRandomAIController::Unstick(ARRRandomCharacter* Self, const FVector& Move, float DeltaSeconds)
{
	if (DetourTimer > 0.f)
	{
		DetourTimer -= DeltaSeconds;
		return DetourWay;
	}
	// (In the air too: a bot hopping against a wall it can't climb is as stuck as one pushing it; not flying, though)
	const bool bTrying = !Move.IsNearlyZero() && !Self->GetVehicle() && !Self->GetRobot() && !Self->IsUsingUltimate() && !Self->IsInFlight()
		&& Self->GetCharacterMovement()->MaxWalkSpeed > 1.f;
	WedgedTimer = bTrying && Self->GetVelocity().Size2D() < StuckSpeed ? WedgedTimer + DeltaSeconds : 0.f;
	if (WedgedTimer < WedgedTime)
	{
		return Move;
	}
	WedgedTimer = 0.f;
	++WedgeCount;

	// Along whatever is in the way (the way that keeps closest to where it wanted to go) and a little away from it; nothing
	// to see (another animal friend in the way, or something odd): off to one side
	const FVector Want = Move.GetSafeNormal2D();
	FHitResult Hit;
	FVector Away = FVector::ZeroVector;
	if (!IsWayClear(Self, Want, Self->GetCapsuleComponent()->GetScaledCapsuleRadius() + 60.f, &Hit) && Hit.bBlockingHit)
	{
		Away = Hit.ImpactNormal.GetSafeNormal2D();
	}
	const FVector Along = Away.IsNearlyZero() ? FVector::CrossProduct(FVector::UpVector, Want) : (Want - Away * FVector::DotProduct(Want, Away));
	const FVector Side = Along.Size2D() > 0.2f ? Along.GetSafeNormal2D() : FVector::CrossProduct(FVector::UpVector, Want) * AvoidSign;
	// Each try the other way round, if the first didn't do it
	const float Turn = WedgeCount % 2 == 0 ? -1.f : 1.f;
	FVector Pick = FVector::ZeroVector;
	for (const FVector& Try : { (Side * Turn + Away * 0.6f), (-Side * Turn + Away * 0.6f), Away.IsNearlyZero() ? -Want : Away, -Want })
	{
		if (!Try.IsNearlyZero() && IsWayClear(Self, Try, AvoidProbeDistance))
		{
			Pick = Try.GetSafeNormal2D();
			break;
		}
	}
	DetourWay = Pick.IsNearlyZero() ? -Want : Pick;
	DetourTimer = DetourTime;
	// Wedged again and again on the way there: give the goal up
	if (WedgeCount >= WedgesToGiveUp)
	{
		WedgeCount = 0;
		if (bHasCoverSpot)
		{
			BadCoverSpot = CoverSpot;
			BadCoverTimer = BadCoverMemory;
			bHasCoverSpot = false;
			CoverSearchTimer = 0.f;
		}
		PatrolTimer = 0.f;
		RouteTimer = 0.f;
		StrafeSign = -StrafeSign;
	}
	UE_CLOG(CVarLogStalls.GetValueOnGameThread() != 0, LogTemp, Log, TEXT("RRRandom: bot unstick: %s at %s goes %s for %.1f s (try %d, blocked by %s)."),
		*Self->GetName(), *Self->GetActorLocation().ToCompactString(), *DetourWay.ToCompactString(), DetourTime, WedgeCount, *GetNameSafe(Hit.GetActor()));
	return DetourWay;
}

void ARRRandomAIController::WatchProgress(const ARRRandomCharacter* Self, const FVector& Move, const TCHAR* Mode, float DeltaSeconds)
{
	// Only while it means to walk on its own feet
	const FVector Location = Self->GetActorLocation();
	if (Move.IsNearlyZero() || Self->GetVehicle() || Self->GetRobot() || Self->IsUsingUltimate())
	{
		ProgressFrom = Location;
		ProgressTimer = 0.f;
		bStallLogged = false;
		return;
	}
	ProgressTimer += DeltaSeconds;
	if (FVector::Dist2D(Location, ProgressFrom) >= StallDistance)
	{
		ProgressFrom = Location;
		ProgressTimer = 0.f;
		bStallLogged = false;
		// Getting somewhere again: the tries at getting unstuck start over
		WedgeCount = 0;
		return;
	}
	if (ProgressTimer < StallTime || bStallLogged)
	{
		return;
	}
	bStallLogged = true;
	// Getting nowhere for a while without being quite stopped (dithering in a nook): give the goal up as a run of wedges
	// would, and head off another way for a moment. (Not in a fight: sidestepping back and forth there is meant)
	if (FCString::Strcmp(Mode, TEXT("fight")) != 0)
	{
		if (bHasCoverSpot)
		{
			BadCoverSpot = CoverSpot;
			BadCoverTimer = BadCoverMemory;
			bHasCoverSpot = false;
			CoverSearchTimer = 0.f;
		}
		PatrolTimer = 0.f;
		RouteTimer = 0.f;
		const FVector Off = Move.GetSafeNormal2D().RotateAngleAxis(Stream.FRandRange(100.f, 260.f), FVector::UpVector);
		DetourWay = IsWayClear(Self, Off, AvoidProbeDistance) ? Off : -Move.GetSafeNormal2D();
		DetourTimer = DetourTime * 2.f;
	}
	if (CVarLogStalls.GetValueOnGameThread() == 0)
	{
		return;
	}
	// What's in the way, straight along the way it wants to go
	FHitResult Hit;
	const FVector Way = Move.GetSafeNormal2D();
	IsWayClear(Self, Way, Self->GetCapsuleComponent()->GetScaledCapsuleRadius() + 60.f, &Hit);
	// And the nearest other animal friend (they block each other)
	const ARRRandomCharacter* Nearest = nullptr;
	float NearestDistance = UE_BIG_NUMBER;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		const float Distance = *It != Self && It->IsAlive() ? FVector::Dist2D(It->GetActorLocation(), Location) : UE_BIG_NUMBER;
		if (Distance < NearestDistance)
		{
			Nearest = *It;
			NearestDistance = Distance;
		}
	}
	const FVector Next = Route.IsValidIndex(RouteIndex) ? Route[RouteIndex] : FVector::ZeroVector;
	UE_LOG(LogTemp, Log, TEXT("RRRandom: bot stall: %s (%s) at %s for %.1f s, wants %s, speed %.0f (max %.0f), %s, route %d/%d (%s) next %s, blocked by %s (%s) normal %s, nearest %s %.0f away (team %d)."),
		*Self->GetName(), Mode, *Location.ToCompactString(), ProgressTimer, *Way.ToCompactString(), Self->GetVelocity().Size2D(), Self->GetCharacterMovement()->MaxWalkSpeed,
		Self->GetCharacterMovement()->IsMovingOnGround() ? TEXT("on ground") : TEXT("in the air"), RouteIndex, Route.Num(),
		bNavRoute ? TEXT("navmesh") : TEXT("walking graph"), *Next.ToCompactString(),
		Hit.bBlockingHit ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"), Hit.bBlockingHit ? *GetNameSafe(Hit.GetComponent()) : TEXT(""),
		*Hit.ImpactNormal.ToCompactString(), *GetNameSafe(Nearest), NearestDistance, Nearest ? Nearest->GetTeam() : -1);
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

void ARRRandomAIController::UpdateFlight(ARRRandomCharacter* Self, float Distance, float DeltaSeconds)
{
	if (FlightHoldRemaining > 0.f)
	{
		FlightHoldRemaining -= DeltaSeconds;
		if (FlightHoldRemaining <= 0.f || !Self->CanFly())
		{
			FlightHoldRemaining = 0.f;
			Self->StopJumping();
		}
		return;
	}

	// Take off from the ground when a fight is near: over cover to get a shot, out of reach of the ground
	const bool bFightNear = !bRetreating && Distance <= Self->GetAttackRange() * EngageRangeShare;
	if (!Self->CanFly() || !bFightNear || !Self->GetCharacterMovement()->IsMovingOnGround())
	{
		FlightHopTimer = FMath::Min(FlightHopTimer, FlightHopDelay.Y);
		return;
	}
	FlightHopTimer -= DeltaSeconds;
	if (FlightHopTimer <= 0.f)
	{
		Self->Jump();
		FlightHoldRemaining = Stream.FRandRange(FlightHoldTime.X, FlightHoldTime.Y);
		FlightHopTimer = Stream.FRandRange(FlightHopDelay.X, FlightHopDelay.Y);
	}
}

void ARRRandomAIController::UpdateDice(ARRRandomCharacter* Self, float Distance, float DeltaSeconds)
{
	// In the star robot (combining it or inside): no dice, no ultimates, till it's out again
	if (Self->GetRobot())
	{
		return;
	}
	URRRandomDiceBuffComponent* Dice = Self->GetDiceBuffs();
	if (Dice->GetCharges() <= 0)
	{
		// Spent them all: hold some back again next time
		if (bSpendingDice)
		{
			bSpendingDice = false;
			DiceHoldLimit = Stream.RandRange(DiceHoldRange.X, DiceHoldRange.Y);
		}
		return;
	}

	// Buffs run out, so save dice for a fight unless too many have piled up.
	// Once it starts, the bot throws them one after another until none are left.
	const bool bEngaging = !bRetreating && Distance <= Self->GetAttackRange() * EngageRangeShare;
	if (!bSpendingDice && !bEngaging && Dice->GetCharges() < DiceHoldLimit)
	{
		DiceTimer = Stream.FRandRange(DiceReactionMin, DiceReactionMax);
		return;
	}
	bSpendingDice = true;

	// Each throw waits for the previous die to land, then a short reaction
	DiceTimer -= DeltaSeconds;
	if (DiceTimer <= 0.f && Dice->RollDice())
	{
		DiceTimer = Stream.FRandRange(DiceReactionMin, DiceReactionMax);
	}
}
