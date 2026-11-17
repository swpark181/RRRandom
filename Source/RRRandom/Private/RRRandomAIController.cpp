#include "RRRandomAIController.h"
#include "Misc/ScopeExit.h"
#include "RRRandomRounds.h"
#include "RRRandomKit.h"
#include "RRRandomBattleRoyale.h"
#include "RRRandomCannon.h"
#include "RRRandomMovementComponent.h"
#include "RRRandomPlaneStation.h"
#include "RRRandomCharacter.h"
#include "RRRandomCover.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomDiceTown.h"
#include "RRRandomGameMode.h"
#include "RRRandomGameState.h"
#include "RRRandomOracle.h"
#include "RRRandomLoadout.h"
#include "RRRandomUltimateMeteor.h"
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
	// Tests (round 95: do bots take a big hill's flights?): "x y z" sends every battle royale bot there instead of its own goal
	TAutoConsoleVariable<FString> CVarBotTestGoal(TEXT("rr.BotTestGoal"), TEXT(""), TEXT("Battle royale bots all head for this spot, \"x y z\" (tests; empty: their own goals)."));
}

namespace
{
	constexpr float RetargetInterval = 0.5f;
	constexpr float CoverSearchInterval = 0.5f;
	// Distance band around the preferred distance where the bot only strafes
	constexpr float DistanceSlack = 150.f;
	constexpr float ArenaCenterArriveRadius = 200.f;
	constexpr float CoverArriveRadius = 40.f;
	// Round 99: Meteor Rush ore this near is fetched before anything else (a fight, a reload)
	constexpr float OreGrabDistance = 2200.f;
	// A meteor coming down (or still throwing) is worth this much of a detour over a piece of ore lying about
	constexpr float MeteorGoalBonus = 1500.f;
	// (round 99) further than this from the cover spot, the way to it is a route
	constexpr float CoverRouteDistance = 220.f;
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
	// Round 99: the feet count as on the navmesh within this of it; how far ahead a route's walker looks for a ledge or a
	// wall; no second ledge jump within this long; a route that ended short is asked for again after this long
	const FVector NavOnMeshExtent(70.f, 70.f, 60.f);
	constexpr float LedgeLookAhead = 28.f;
	constexpr float LedgeJumpGap = 0.6f;
	// A bot climbs what rises no more than this over its middle by itself (a block and a bit: what the navmesh takes for a
	// step); a body perched in the air on something it can't stand on hops off this fast (flat, up)
	constexpr float BotClimbRise = 135.f;
	constexpr float PerchHopSpeed = 260.f;
	constexpr float PerchHopLift = 300.f;
	constexpr float RouteEndRefreshTime = 0.4f;
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

	/**
	 * Round 99: what really stops this body going Way, asked the way its movement asks (the capsule's own channel and
	 * responses: walls, the arena's fence, rides, other animal friends), from a step up so the floor and low steps don't
	 * count. Reach: how far past the capsule to look.
	 */
	bool FindBlocker(const ARRRandomCharacter* Self, const FVector& Way, float Reach, FHitResult& OutHit)
	{
		const UCapsuleComponent* Capsule = Self->GetCapsuleComponent();
		const float Lift = Self->GetCharacterMovement()->MaxStepHeight + 5.f;
		const float Radius = Capsule->GetScaledCapsuleRadius() * 0.95f;
		const float HalfHeight = FMath::Max(Radius, Capsule->GetScaledCapsuleHalfHeight() - Lift * 0.5f);
		const FVector From = Self->GetActorLocation() + FVector(0.f, 0.f, Lift * 0.5f);
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomBlocker), false, Self);
		return Self->GetWorld()->SweepSingleByChannel(OutHit, From, From + Way.GetSafeNormal2D() * Reach, FQuat::Identity, Capsule->GetCollisionObjectType(),
			FCollisionShape::MakeCapsule(Radius, HalfHeight), Params, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels())) && OutHit.bBlockingHit;
	}

	FString DescribeBlocker(const ARRRandomCharacter* Self, const FVector& Way)
	{
		FHitResult Hit;
		if (!FindBlocker(Self, Way, 70.f, Hit))
		{
			return TEXT("nothing");
		}
		return FString::Printf(TEXT("%s.%s normal %s %s"), *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()), *Hit.ImpactNormal.ToCompactString(),
			Hit.bStartPenetrating ? TEXT("(inside it)") : TEXT(""));
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
	// The battle royale (round 92): the map is huge; rides are worth a longer walk, and for shorter trips too
	bBattle = ARRRandomGameState::IsBattleRoyale(this);
	if (bBattle)
	{
		VehicleSearchRadius = 2500.f;
		RideTripDistance = 2500.f;
		RideArriveDistance = 900.f;
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
	// (-RRIdleBots: every bot, my team's too, e.g. filming my animal friend alone, char-pipeline/scripts/rec_grip.ps1)
	static const bool bIdleBots = FParse::Param(FCommandLine::Get(), TEXT("RRIdleBots"));
	if (bIdleBots)
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
	// The battle royale's get-ready time (round 97): no fighting; the bot stands at its team's place on the plaza rolling its
	// thirty weapon dice one after another, and keeps its bag in order as the guns come in
	if (Self && ARRRandomGameState::IsPrep(this))
	{
		Self->StopJumping();
		StopMovement();
		if (Self->IsAlive())
		{
			DiceTimer -= DeltaSeconds;
			if (DiceTimer <= 0.f && Self->GetDiceBuffs()->RollDice())
			{
				DiceTimer = Stream.FRandRange(0.3f, 1.2f);
			}
			URRRandomLoadoutComponent* Bag = Self->GetLoadout();
			if (Bag && Bag->GetAddCount() != SeenBagAdds && !Self->GetDiceBuffs()->IsRolling())
			{
				SeenBagAdds = Bag->GetAddCount();
				Bag->AutoManage();
			}
		}
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
	// (the battle map: the tile underfoot, looked for again now and then, for its walking graph)
	if (bBattle)
	{
		TownTimer -= DeltaSeconds;
		if (TownTimer <= 0.f || !Town.IsValid())
		{
			TownTimer = 1.f;
			Town = ARRRandomDiceTown::FindAt(this, Self->GetActorLocation());
		}
		UpdateBattleGoal(Self, DeltaSeconds);
	}
	const ARRRandomDiceTown* Map = Town.Get();
	const bool bRoutes = HasRoutes();

	// Nobody in sight and no trail to follow: roam the middle until someone shows up (on a map with routes, anywhere on
	// its ground, upstairs and rooftops included)
	PatrolTimer -= DeltaSeconds;
	// (the battle royale: its own goal, UpdateBattleGoal)
	if (bBattle)
	{
		PatrolGoal = bHasBattleGoal ? BattleGoal : Self->GetActorLocation();
		TArray<FString> TestGoal;
		CVarBotTestGoal.GetValueOnGameThread().ParseIntoArrayWS(TestGoal);
		if (TestGoal.Num() == 3)
		{
			PatrolGoal = FVector(FCString::Atof(*TestGoal[0]), FCString::Atof(*TestGoal[1]), FCString::Atof(*TestGoal[2]));
		}
	}
	else if (!Opponent.IsValid() && LastSeenTimer <= 0.f && bHasArenaCenter
		&& (PatrolTimer <= 0.f || FVector::Dist2D(Self->GetActorLocation(), PatrolGoal) <= ArenaCenterArriveRadius))
	{
		FNavLocation NavSpot;
		const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (Map)
		{
			// (round 98: the main mode keeps inside its arena's fence)
			PatrolGoal = Map->GetRandomWalkSpot(Stream, ARRRandomGameState::IsRounds(this) ? RRRounds::ArenaHalf - 400.f : RoutePatrolRadius);
			PatrolTimer = ARRRandomGameState::IsRounds(this) ? RoutePatrolTime * 0.5f : RoutePatrolTime;
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
	// The light sword (round 97): shot at from further than a swing reaches, on guard (while focus lasts) to send the shots back
	// at the shooter; down again up close (to swing) or out of focus
	// (round 98: the hammer's shield too, the same way)
	if (Self->CanGuard() && Target && bCanSee)
	{
		const bool bWant = Distance > Self->GetWeapon().GetMeleeReach() * 1.6f && UnderFireTimer > 0.f
			&& Self->GetDeflectFocus() >= (Self->IsDeflecting() ? 0.3f : 1.5f);
		if (bWant != Self->IsDeflecting())
		{
			Self->SetDeflecting(bWant, Target->GetActorLocation());
		}
		else if (bWant)
		{
			Self->SetDeflectAim(Target->GetActorLocation());
		}
	}
	else if (Self->IsDeflecting())
	{
		Self->SetDeflecting(false, SelfLocation);
	}
	UpdateDice(Self, Distance, DeltaSeconds);
	// Round 98, the main mode: the card to keep, and the Q ability
	if (ARRRandomGameState::IsRounds(this))
	{
		UpdateCards(Self, Distance, DeltaSeconds);
		UpdateAbility(Self, Target, Distance, bCanSee, DeltaSeconds);
	}
	UpdateFlight(Self, Distance, DeltaSeconds);

	// Hide while reloading or hurt (or keeping the team's winning ore safe); otherwise fight
	// Round 99 (the user: "메테오 게임에서 봇이 운석 드랍을 줍는 우선순위 추가. 지금은 먹으러 안 가는것 같다"): ore lying within
	// OreGrabDistance comes before the fight: the bot goes and gets it, shooting on the way, where it used to stand and
	// trade shots with whoever was in sight (and hide to reload) while the meteor's drops lay there. Only a bot that is
	// badly hurt, or keeping the team's winning ore safe, still looks after itself first
	const bool bOreNear = bHasRushGoal && bRushOre && FVector::Dist2D(SelfLocation, RushGoal) <= OreGrabDistance;
	const bool bHurtUnderFire = UnderFireTimer > 0.f && HealthShare < UnderFireCoverHealthShare;
	const bool bWantsCover = Target && (bRetreating || bGuardingOre || bHurtUnderFire || (Self->IsReloading() && !bOreNear));
	FVector Wanted = FVector::ZeroVector;
	bool bCoverRoute = false;
	if (bWantsCover)
	{
		CoverSearchTimer -= DeltaSeconds;
		if (!bHasCoverSpot || CoverSearchTimer <= 0.f)
		{
			const bool bHadCoverSpot = bHasCoverSpot;
			bHasCoverSpot = FindCoverSpot(Self, Target->GetActorLocation(), CoverSpot);
			CoverSearchTimer = CoverSearchInterval;
			if (bHasCoverSpot && !bHadCoverSpot && ARRRandomGameState::IsBattleRoyale(this))
			{
				UE_LOG(LogTemp, Verbose, TEXT("RRRandom: %s takes cover at %s (%.0f up)."), *Self->GetName(), *CoverSpot.ToCompactString(),
					CoverSpot.Z - (ARRRandomBattleZone::Get(this) ? ARRRandomBattleZone::Get(this)->GetFloorZ() : 0.f));
			}
		}
		if (bHasCoverSpot)
		{
			const FVector ToSpot = CoverSpot - SelfLocation;
			Wanted = ToSpot.Size2D() > CoverArriveRadius ? ToSpot.GetSafeNormal2D() : FVector::ZeroVector;
			// Round 99: round the walls to it where the map has routes (it used to be walked at in a straight line, the
			// cover itself often in the way); the last stretch straight
			if (bRoutes && ToSpot.Size2D() > CoverRouteDistance)
			{
				Wanted = FollowRoute(Self, CoverSpot - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), DeltaSeconds, true);
				bCoverRoute = true;
			}
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
	const bool bRushing = bHasRushGoal && !bWantsCover && (bOreNear || !(Target && bCanSee && Distance <= Self->GetAttackRange() * ShootRangeShare));
	if (bRushing && !bRoutes)
	{
		const FVector ToGoal = RushGoal - (SelfLocation - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		Wanted = ToGoal.Size2D() > CoverArriveRadius ? ToGoal.GetSafeNormal2D() : FVector::ZeroVector;
	}
	// On a map with routes, out of a close fight: along the route to the opponent, where it was last seen, or the roaming
	// spot (feet heights)
	bool bOnRoute = bCoverRoute;
	if (bRoutes && !bWantsCover && (bRushing ||!(Target && bCanSee && Distance <= Self->GetAttackRange() * PreferredShare + DistanceSlack)))
	{
		// (the battle royale: into the ring first when it is urgent, whoever is about)
		const FVector Goal = bRushing ? RushGoal
			: bBattle && bRingUrgent && bHasBattleGoal ? BattleGoal
			: Target ? Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
			: LastSeenTimer > 0.f ? LastSeenLocation - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight())
			: PatrolGoal;
		Wanted = FollowRoute(Self, Goal, DeltaSeconds);
		bOnRoute = true;
	}
	else if (!bCoverRoute)
	{
		RouteStuckTimer = 0.f;
	}

	// Vehicles: only out of a fight (an opponent in sight near its range, or being hit)
	const bool bInFight = bWantsCover || UnderFireTimer > 0.f || (Target && Distance <= Self->GetAttackRange() * EngageRangeShare);
	if (bBattle && UpdateBattleTravel(Self, bInFight, Wanted, bOnRoute, DeltaSeconds))
	{
	}
	else if (!UpdateStarRobot(Self, bInFight, Wanted, bOnRoute, DeltaSeconds))
	{
		UpdateVehicles(Self, Target, bInFight, Wanted, bOnRoute, DeltaSeconds);
	}

	MoveMode = bOnRoute ? TEXT("route") : bWantsCover ? TEXT("cover") : bRushing ? TEXT("rush") : Target ? TEXT("fight") : TEXT("roam");
	// (a route already goes round walls, and its stairs would look like walls to the steering's probe)
	// (On a route the push away from teammates is weak: in a lane between walls it would only shove them into the walls)
	// (round 99: unless the route itself says it can't be walked blind this frame, FollowRoute's bRouteSteer)
	const bool bBlind = bOnRoute && !bRouteSteer;
	const FVector Move = Unstick(Self, (bBlind ? Wanted : SteerAroundObstacles(Self, Wanted)) + GetSeparation(Self) * (bOnRoute ? RouteSeparationShare : 1.f), DeltaSeconds);
	if (bBattle)
	{
		UpdateSprint(Self, bInFight || (Target && bCanSee), Move);
	}
	ClimbLedgeAhead(Self, Move);
	BadCoverTimer -= DeltaSeconds;
	if (!Move.IsNearlyZero())
	{
		Self->AddMovementInput(Move.GetSafeNormal2D());
	}
	WatchProgress(Self, Move, MoveMode, DeltaSeconds);

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
	bRushOre = false;
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
	UNavigationSystemV1* RushNav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* RushNavData = RushNav ? RushNav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	const bool bBigMap = Town.IsValid();
	// (round 99: anywhere in the main mode's arena, 9200 across)
	const float Reach = bBigMap ? 9500.f : 3500.f;
	float Best = UE_BIG_NUMBER;
	for (TActorIterator<ARRRandomOre> It(GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(Feet, It->GetActorLocation());
		if (Distance > Reach && It->GetKind() != ERROreKind::Golden)
		{
			continue;
		}
		// (round 99: nor ore that came to rest where there is no walking to it, on a map with a navmesh: a bot under it
		// only hops; one in the air, on its way down, is fine)
		if (RushNav && RushNavData && It->IsResting() && It->GetKind() != ERROreKind::Golden)
		{
			FNavLocation OnMesh;
			if (!RushNav->ProjectPointToNavigation(It->GetActorLocation(), OnMesh, FVector(110.f, 110.f, 130.f), RushNavData))
			{
				continue;
			}
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
			bRushOre = true;
		}
	}
	// A meteor on its way or still throwing: wait just outside where it lands. Round 99: it counts for more than a stray
	// piece far off (it used to be gone to only when no ore lay anywhere in reach, so the bots were rarely there when it
	// came down and the drops went to whoever was)
	for (TActorIterator<ARRRandomMeteor> It(GetWorld()); It; ++It)
	{
		if (!It->IsActive())
		{
			continue;
		}
		const FVector Spot = It->GetActorLocation();
		const float Distance = FVector::Dist2D(Feet, Spot);
		if (Distance > Reach * 1.5f || Distance - MeteorGoalBonus >= Best)
		{
			continue;
		}
		Best = Distance - MeteorGoalBonus;
		bRushOre = true;
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

void ARRRandomAIController::SwapErrandRoute()
{
	Swap(Route, ErrandRoute.Route);
	Swap(RouteIndex, ErrandRoute.RouteIndex);
	Swap(RouteGoal, ErrandRoute.RouteGoal);
	Swap(RouteTimer, ErrandRoute.RouteTimer);
	Swap(RouteStuckTimer, ErrandRoute.RouteStuckTimer);
	Swap(bNavRoute, ErrandRoute.bNavRoute);
	Swap(bRouteOffMesh, ErrandRoute.bRouteOffMesh);
	Swap(bRoutePartial, ErrandRoute.bRoutePartial);
}

FVector ARRRandomAIController::FollowRoute(ARRRandomCharacter* Self, const FVector& Goal, float DeltaSeconds, bool bErrand)
{
	// (an errand's route has its own memory: swapped in for this call)
	if (bErrand)
	{
		SwapErrandRoute();
	}
	ON_SCOPE_EXIT
	{
		if (bErrand)
		{
			SwapErrandRoute();
		}
	};
	const ARRRandomDiceTown* Map = Town.Get();
	const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	RouteTimer -= DeltaSeconds;
	// (round 99: at the end of a route that stops short of the goal a new one is asked for a couple of times a second, not
	// every frame)
	if (HasRoutes() && (RouteTimer <= 0.f || FVector::Dist(Goal, RouteGoal) > RouteRefreshDistance
		|| (RouteIndex >= Route.Num() && RouteTimer <= RouteRefreshTime - RouteEndRefreshTime)))
	{
		RouteTimer = RouteRefreshTime;
		RouteGoal = Goal;
		RouteIndex = 0;
		// The navmesh first (it knows what's smashed); Dice Town's walking graph while the navmesh is still being made
		// (the battle map: the navmesh is only made round the animal friends, so a far goal is aimed at through a spot this far
		// along the way to it; the route is asked for again as the bot gets on)
		FVector NavGoal = Goal;
		if (bBattle && FVector::Dist2D(Feet, Goal) > 5000.f)
		{
			NavGoal = Feet + (Goal - Feet).GetSafeNormal2D() * 5000.f;
		}
		bRouteOffMesh = false;
		bRoutePartial = false;
		bNavRoute = FindNavRoute(Feet, NavGoal, Route, bRouteOffMesh, bRoutePartial);
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
	// Round 99: where the route can't be walked blind, the steering takes it: no route at all, a start off the navmesh (the
	// first corner may be on the floor below, a wall between), the stretch past the end of a route that stops short
	bRouteSteer = Route.Num() == 0 || bRouteOffMesh || (RouteIndex >= Route.Num() && bRoutePartial);
	if (Toward.Size2D() < RouteArriveRadius)
	{
		RouteStuckTimer = 0.f;
		return FVector::ZeroVector;
	}
	const UCharacterMovementComponent* Movement = Self->GetCharacterMovement();
	const bool bOnFoot = !Self->GetVehicle() && !Self->GetRobot() && !Self->IsClimbing() && Movement->IsMovingOnGround();
	// Something solid right ahead on the way (not another animal friend: the push apart sees to those): a ledge the route
	// goes up is climbed at once (it used to be pushed at for half a second first); anything else is steered round, and
	// another route asked for soon
	FHitResult Ahead;
	if (bOnFoot && FindBlocker(Self, Toward, LedgeLookAhead, Ahead) && !Cast<ARRRandomCharacter>(Ahead.GetActor()))
	{
		// (a ledge it can climb is climbed, by the tick: ClimbLedgeAhead)
		if (!Self->CanClimbToward(Toward, 0.f, BotClimbRise))
		{
			bRouteSteer = true;
			RouteTimer = FMath::Min(RouteTimer, RouteEndRefreshTime);
		}
	}
	// Stopped against something: jump (at a ledge a jump climbs it), and soon look for another route
	const bool bStopped = Self->GetVelocity().Size2D() < StuckSpeed && !Self->GetVehicle() && !Self->IsClimbing();
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

bool ARRRandomAIController::FindNavRoute(const FVector& Feet, const FVector& Goal, TArray<FVector>& OutPoints, bool& bOutOffMesh, bool& bOutPartial) const
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const ANavigationData* Data = Nav ? Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
	FNavLocation Start;
	FNavLocation End;
	if (!Data)
	{
		return false;
	}
	// (round 99: right under the feet first. The wide look that follows finds the navmesh from a roof's trim or a block's
	// top too, but perhaps a floor down or the far side of a wall: the route is then marked as starting off the navmesh)
	bOutOffMesh = !Nav->ProjectPointToNavigation(Feet, Start, NavOnMeshExtent, Data);
	if ((bOutOffMesh && !Nav->ProjectPointToNavigation(Feet, Start, NavFeetExtent, Data)) || !Nav->ProjectPointToNavigation(Goal, End, NavGoalExtent, Data))
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
	bOutPartial = Result.IsPartial();
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
				Wanted = FollowRoute(Self, Rival->GetActorLocation(), DeltaSeconds, true);
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
	// (the battle map, round 92: any team's sets, the nearest; only one not far off: bots shouldn't all cross the map for it)
	const float CrewReach = bBattle ? 4000.f : RobotCrewSearchRadius;
	if (!bInFight && !Self->IsGiant() && !Self->CanFly() && ARRRandomStarRobot::FindGathered(Self, Self->GetTeam(), Cars, Gathered))
	{
		const FVector ToCars = (Gathered - Self->GetActorLocation()) * FVector(1.f, 1.f, 0.f);
		if (ToCars.Size() <= CrewReach && ToCars.Size() > ARRRandomStarRobot::CombineReach * 0.6f)
		{
			if (HasRoutes())
			{
				Wanted = FollowRoute(Self, Gathered, DeltaSeconds, true);
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
	if (Distance > (bBattle ? 6000.f : RobotCrewSearchRadius))
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
		Wanted = FollowRoute(Self, Robot->GetActorLocation(), DeltaSeconds, true);
		bOnRoute = true;
		return true;
	}
	Wanted = ToRobot.GetSafeNormal2D();
	bOnRoute = false;
	return true;
}

bool ARRRandomAIController::UpdateVehicles(ARRRandomCharacter* Self, const ARRRandomCharacter* Target, bool bInFight, FVector& Wanted, bool& bOnRoute, float DeltaSeconds)
{
	const FVector Location = Self->GetActorLocation();
	// Defense (round 90): villains are about all the time, so the bots stay on foot by the tower (the rides are the players')
	if (ARRRandomGameState::IsDefense(this) && !Self->GetVehicle())
	{
		VehicleTarget = nullptr;
		return false;
	}
	// (a plane is flown by UpdateBattleTravel)
	if (Self->IsPiloting())
	{
		VehicleTarget = nullptr;
		return false;
	}
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
	// Round 99: over to it along a route where the map has them (it used to walk straight at it, through whatever wall
	// stood between, for up to eight seconds); the last stretch straight, steered
	if (HasRoutes() && ToRide.Size2D() > ARRRandomVehicle::MountRange * 2.f)
	{
		Wanted = FollowRoute(Self, Ride->GetActorLocation() - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), DeltaSeconds, true);
		bOnRoute = true;
		return true;
	}
	Wanted = ToRide.GetSafeNormal2D();
	bOnRoute = false;
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
	// A spot right behind a piece of cover (its middle at this bot's height, Along its half size the way from the threat),
	// if it is any good: scored, the best kept
	auto Consider = [&](const FVector& CoverCenter, const FVector& Away, float Along, bool bCheckRim)
	{
		const FVector Spot = CoverCenter + Away * (Along + Radius + CoverSpotGap);
		if (FVector::Dist2D(Spot, Threat) < MinCoverDistanceFromThreat || (BadCoverTimer > 0.f && FVector::Dist2D(Spot, BadCoverSpot) < BadCoverRadius))
		{
			return;
		}
		// Not past a wall that stops brawlers only (the island's rim, the water's edge): a spot there can't be reached
		FCollisionQueryParams RimParams(SCENE_QUERY_STAT(RRRandomCoverRim), false, Self);
		if (bCheckRim && World->SweepTestByObjectType(CoverCenter, Spot, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldDynamic), FCollisionShape::MakeSphere(Radius * 0.5f), RimParams))
		{
			return;
		}

		// Somewhere a bot can stand: clear of other cover, with floor under it, and out of the threat's sight
		if (World->OverlapAnyTestByObjectType(Spot, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionShape::MakeSphere(Radius), Params)
			|| !World->LineTraceTestByChannel(Spot, Spot - FVector(0.f, 0.f, HalfHeight + 50.f), ECC_Visibility, Params)
			|| !SweepHitsCover(World, Threat, Spot, HiddenMargin))
		{
			return;
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
	};
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
		if (!Away.IsNearlyZero())
		{
			Consider(CoverCenter, Away, It->GetExtentAlong(Away), true);
		}
	}
	// Round 94: the cover inside the big buildings, on the floor this bot is on (a smashed piece no longer hides the spot,
	// so the sight test drops it)
	if (const ARRRandomDiceTown* Here = ARRRandomDiceTown::FindAt(Self, SelfLocation))
	{
		const float Feet = SelfLocation.Z - HalfHeight;
		for (const ARRRandomDiceTown::FCoverPiece& Piece : Here->GetCoverPieces())
		{
			const FVector CoverCenter(Piece.Center.X, Piece.Center.Y, SelfLocation.Z);
			if (FMath::Abs(Piece.Center.Z - Feet) > 150.f || FVector::Dist2D(CoverCenter, SelfLocation) > CoverSearchRadius)
			{
				continue;
			}
			const FVector Away = (CoverCenter - Threat).GetSafeNormal2D();
			if (!Away.IsNearlyZero())
			{
				Consider(CoverCenter, Away, FMath::Abs(Away.X) * Piece.Extent.X + FMath::Abs(Away.Y) * Piece.Extent.Y, false);
			}
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
	// Round 99: a ledge ahead that a jump will climb is walked straight at (ClimbLedgeAhead takes it), not gone round: in
	// a crater dug by a blast a bot off the navmesh ran round and round the pit's wall instead of climbing out
	if (Self->GetCharacterMovement()->IsMovingOnGround() && Self->CanClimbToward(Direction, AvoidProbeDistance, BotClimbRise))
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
	// (round 99: asked the way the body's own movement asks, by its channel and responses: the main mode's fence, which
	// stops animal friends only, and parked rides were not seen before, and bots walked into them for good)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomSteer), false, Self);
	FHitResult Hit;
	const bool bHit = GetWorld()->SweepSingleByChannel(Hit, From, From + Direction.GetSafeNormal2D() * Distance, FQuat::Identity, Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Radius, HalfHeight), Params, FCollisionResponseParams(Capsule->GetCollisionResponseToChannels()));
	// (Shots fly as world dynamic too; one passing by isn't a wall. Nor is another animal friend: they step apart)
	const bool bBlocked = bHit && !(Hit.GetActor() && (Hit.GetActor()->IsA<ARRRandomProjectile>() || Hit.GetActor()->IsA<ARRRandomCharacter>()));
	if (OutHit)
	{
		*OutHit = Hit;
	}
	return !bBlocked;
}

void ARRRandomAIController::ClimbLedgeAhead(ARRRandomCharacter* Self, const FVector& Move)
{
	// Round 99 (the user: "벽으로 계속 이동하려는 문제"): walking into something a jump climbs (a block, a ledge the route goes
	// up, a crater's rim), whatever it is walking for, the bot jumps now: its jump is the climb (ARRRandomCharacter::Jump).
	// It used to push at the ledge for half a second, till it counted as stopped, before trying
	const float Now = GetWorld()->GetTimeSeconds();
	if (Move.IsNearlyZero() || Now < NextLedgeJumpTime || Self->GetVehicle() || Self->GetRobot() || Self->IsClimbing() || Self->IsUsingUltimate()
		|| !Self->GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	FHitResult Ahead;
	if (FindBlocker(Self, Move, LedgeLookAhead, Ahead) && !Cast<ARRRandomCharacter>(Ahead.GetActor()) && Self->CanClimbToward(Move, 0.f, BotClimbRise))
	{
		NextLedgeJumpTime = Now + LedgeJumpGap;
		// (the climb looks along the last movement input: this frame's, so it is the way the ledge was found)
		Self->AddMovementInput(Move.GetSafeNormal2D());
		Self->Jump();
	}
}

FVector ARRRandomAIController::Unstick(ARRRandomCharacter* Self, const FVector& Move, float DeltaSeconds)
{
	// Round 99: perched in the air on something it can't stand on (a roof's trim, a prop's top: the movement keeps it
	// "falling" there, where neither walking nor jumping works), barely moving: a little hop off, the way it wants to go
	const UCharacterMovementComponent* Movement = Self->GetCharacterMovement();
	if (Movement->IsFalling() && !Self->IsClimbing() && !Self->IsInFlight() && !Self->GetVehicle() && !Self->GetRobot() && !Move.IsNearlyZero()
		&& Self->GetVelocity().Size() < StuckSpeed)
	{
		PerchedTimer += DeltaSeconds;
		if (PerchedTimer >= WedgedTime)
		{
			PerchedTimer = 0.f;
			Self->LaunchCharacter(Move.GetSafeNormal2D() * PerchHopSpeed + FVector(0.f, 0.f, PerchHopLift), true, true);
			UE_CLOG(CVarLogStalls.GetValueOnGameThread() != 0, LogTemp, Log, TEXT("RRRandom: bot perched: %s at %s hops off."), *Self->GetName(), *Self->GetActorLocation().ToCompactString());
		}
	}
	else
	{
		PerchedTimer = 0.f;
	}
	if (DetourTimer > 0.f)
	{
		DetourTimer -= DeltaSeconds;
		return DetourWay;
	}
	// (In the air too: a bot hopping against a wall it can't climb is as stuck as one pushing it; not flying, though)
	// (round 99: nor climbing: the climb moves the body itself, at no speed the movement knows of, and every one of them
	// counted as a wedge, sending the bot off sideways as it got to the top)
	const bool bTrying = !Move.IsNearlyZero() && !Self->GetVehicle() && !Self->GetRobot() && !Self->IsUsingUltimate() && !Self->IsInFlight() && !Self->IsClimbing()
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
	// (round 99: what the body's own movement runs into, the arena's fence and parked rides too, not only world static)
	if (FindBlocker(Self, Want, 60.f, Hit) && !Hit.bStartPenetrating)
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
	UE_CLOG(CVarLogStalls.GetValueOnGameThread() != 0, LogTemp, Log, TEXT("RRRandom: bot unstick: %s (%s) at %s wanted %s goes %s for %.1f s (try %d, blocked by %s; really %s; %s, route %d/%d %s next %s)."),
		*Self->GetName(), MoveMode, *Self->GetActorLocation().ToCompactString(), *Want.ToCompactString(), *DetourWay.ToCompactString(), DetourTime, WedgeCount, *GetNameSafe(Hit.GetActor()),
		*DescribeBlocker(Self, Want), Self->GetCharacterMovement()->IsMovingOnGround() ? TEXT("on ground") : TEXT("in the air"), RouteIndex, Route.Num(),
		bNavRoute ? TEXT("navmesh") : TEXT("graph"), Route.IsValidIndex(RouteIndex) ? *Route[RouteIndex].ToCompactString() : TEXT("-"));
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
	// Round 99: how much of its walking the bot spends getting nowhere, and how much of that pushing at a wall (logged now
	// and then with the stalls, to compare builds)
	if (CVarLogStalls.GetValueOnGameThread() != 0)
	{
		WalkSeconds += DeltaSeconds;
		if (Self->GetVelocity().Size2D() < StuckSpeed && !Self->IsClimbing())
		{
			StoppedSeconds += DeltaSeconds;
			FHitResult Wall;
			if (FindBlocker(Self, Move, 30.f, Wall) && !Cast<ARRRandomCharacter>(Wall.GetActor()))
			{
				PushSeconds += DeltaSeconds;
			}
		}
		if (WalkSeconds >= NextWalkReport)
		{
			NextWalkReport = WalkSeconds + 20.f;
			UE_LOG(LogTemp, Log, TEXT("RRRandom: bot walk: %s walked %.1f s, stopped %.1f s, pushing at a wall %.1f s."), *Self->GetName(), WalkSeconds, StoppedSeconds, PushSeconds);
		}
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
	UE_LOG(LogTemp, Log, TEXT("RRRandom: bot stall: %s (%s) at %s for %.1f s, wants %s, speed %.0f (max %.0f), %s, route %d/%d (%s) next %s, blocked by %s (%s) normal %s, really %s, nearest %s %.0f away (team %d)."),
		*Self->GetName(), Mode, *Location.ToCompactString(), ProgressTimer, *Way.ToCompactString(), Self->GetVelocity().Size2D(), Self->GetCharacterMovement()->MaxWalkSpeed,
		Self->GetCharacterMovement()->IsMovingOnGround() ? TEXT("on ground") : TEXT("in the air"), RouteIndex, Route.Num(),
		bNavRoute ? TEXT("navmesh") : TEXT("walking graph"), *Next.ToCompactString(),
		Hit.bBlockingHit ? *GetNameSafe(Hit.GetActor()) : TEXT("nothing"), Hit.bBlockingHit ? *GetNameSafe(Hit.GetComponent()) : TEXT(""),
		*Hit.ImpactNormal.ToCompactString(), *DescribeBlocker(Self, Way), *GetNameSafe(Nearest), NearestDistance, Nearest ? Nearest->GetTeam() : -1);
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
	// (round 98: a lobbed acorn hangs in the air longer than its speed says: lead it by most of its arc)
	const float LeadTime = ProjectileSpeed <= 0.f ? 0.f : Self->GetWeapon().bLob ? Distance / (ProjectileSpeed * 0.8f) * Stream.FRandRange(0.6f, 1.f)
		: Distance / ProjectileSpeed * Stream.FRandRange(0.f, 0.8f);
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
	// The battle royale's bag (round 97): a new gun sorted into the hands, and the close gun in hand up close (not too often)
	if (URRRandomLoadoutComponent* Bag = Self->GetLoadout(); Bag && Bag->IsInUse())
	{
		if (Bag->GetAddCount() != SeenBagAdds)
		{
			SeenBagAdds = Bag->GetAddCount();
			Bag->AutoManage();
		}
		HandTimer -= DeltaSeconds;
		const int32 Want = Bag->ChooseHandFor(Distance);
		if (Want != Bag->GetActiveHand() && HandTimer <= 0.f)
		{
			Bag->SelectHand(Want);
			HandTimer = 1.5f;
		}
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

// ---------------------------------------------------------------------------------------------------------------------
// The battle royale (round 92)

namespace
{
	// Into the ring: inside the next one by this share of its radius when it closes within this many seconds
	constexpr float RingSoonSeconds = 45.f;
	constexpr float RingInsideShare = 0.8f;
	// Roaming inside the ring: a spot this far from where the bot is at most, a new one on arrival or after this long
	constexpr float RoamReach = 4000.f;
	constexpr float RoamTimeMin = 18.f;
	constexpr float RoamTimeMax = 30.f;
	constexpr float RoamArrive = 400.f;
	// Round 96: now and then (this share of the roaming spots) a hill's top this near and inside the ring instead, held this
	// long (the way there included)
	constexpr float HillGoalChance = 0.4f;
	constexpr float HillGoalReach = 7000.f;
	constexpr float HillHoldMin = 35.f;
	constexpr float HillHoldMax = 55.f;
	// Round 97: the ultimate die's meteor is worth this long a way
	constexpr float UltimateGoalReach = 8000.f;
	// Rides for the long ways: a cannon this near whose pad cuts the way by this much at least; a plane station this
	// near for a way this long; looked for this often
	constexpr float CannonWalk = 2500.f;
	constexpr float CannonGain = 3000.f;
	constexpr float StationWalk = 3000.f;
	constexpr float PlaneTrip = 9000.f;
	constexpr float LaunchLookInterval = 2.f;
	// A plane: flown this high over the floor, landed within this of the goal
	constexpr float PlaneCruise = 2500.f;
	constexpr float PlaneLandDistance = 1500.f;
	// Sprinting out of a fight for a way at least this long (or into the ring)
	constexpr float SprintTrip = 1500.f;
}

void ARRRandomAIController::UpdateBattleGoal(const ARRRandomCharacter* Self, float DeltaSeconds)
{
	BattleLookTimer -= DeltaSeconds;
	BattleGoalTimer -= DeltaSeconds;
	if (BattleLookTimer > 0.f)
	{
		return;
	}
	BattleLookTimer = 0.5f;
	const ARRRandomBattleZone* Zone = ARRRandomBattleZone::Get(this);
	const ARRRandomBattleMap* Battle = ARRRandomBattleMap::Get(this);
	if (!Zone || !Battle || Zone->GetPhase() <= 0)
	{
		bHasBattleGoal = false;
		bHillGoal = false;
		bRingUrgent = false;
		return;
	}
	const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector2D Here(Feet.X, Feet.Y);
	const FVector2D Center = Zone->GetCenter();
	const float Radius = Zone->GetRadius();
	const FVector2D Next = Zone->GetNextCenter();
	const float NextRadius = Zone->GetNextRadius();
	const bool bInsideNow = FVector2D::Distance(Here, Center) < Radius - 300.f;
	const bool bInsideNext = FVector2D::Distance(Here, Next) < NextRadius * RingInsideShare;
	const bool bSoon = Zone->IsClosing() || Zone->GetSecondsToChange() < RingSoonSeconds;
	const float FloorZ = Battle->GetFloorZ();
	// The oracle's meteors (round 93): off a landing pad about to be hit, and out of the struck zone while there is ring
	// to run to (checked every look, before anything else)
	FVector2D Dodge;
	if (bInsideNow && ARRRandomOracleStorm::FindDodge(this, Here, Center, Radius, Dodge))
	{
		if (!bHasBattleGoal || FVector2D::Distance(FVector2D(BattleGoal), Dodge) > 300.f)
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s dodges the oracle's meteors (%.0f to go)."), *Self->GetName(), FVector2D::Distance(Here, Dodge));
		}
		bHillGoal = false;
		bRingUrgent = false;
		bHasBattleGoal = true;
		BattleGoal = FVector(Dodge, FloorZ);
		BattleGoalTimer = 2.f;
		return;
	}
	// Outside the ring (or the next one, closing soon): into the next one, toward its middle, half way in from its edge
	if (!bInsideNow || (bSoon && !bInsideNext))
	{
		// (a spot that stops getting nearer for a while, behind walls or up on something: another one round the middle)
		if (!bRingUrgent)
		{
			RingAngle = FMath::Atan2(Here.Y - Next.Y, Here.X - Next.X);
			RingBest = UE_BIG_NUMBER;
			RingStuckTimer = 0.f;
		}
		const float Left = FVector2D::Distance(Here, Next);
		if (Left < RingBest - 100.f)
		{
			RingBest = Left;
			RingStuckTimer = 0.f;
		}
		else if ((RingStuckTimer += 0.5f) >= 4.f)
		{
			RingAngle += Stream.FRandRange(0.8f, 2.4f) * (Stream.FRand() < 0.5f ? -1.f : 1.f);
			RingBest = UE_BIG_NUMBER;
			RingStuckTimer = 0.f;
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s gets no nearer the ring; another way in."), *Self->GetName());
		}
		const FVector2D Out(FMath::Cos(RingAngle), FMath::Sin(RingAngle));
		const FVector2D Spot = Next + Out * NextRadius * 0.45f;
		if (!bRingUrgent)
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s heads into the ring (%.0f to go)."), *Self->GetName(), FVector2D::Distance(Here, Spot));
		}
		bHillGoal = false;
		bRingUrgent = true;
		bHasBattleGoal = true;
		BattleGoal = FVector(Spot, FloorZ);
		BattleGoalTimer = 0.f;
		return;
	}
	bRingUrgent = false;
	// Round 97: the ultimate die's meteor, landing or landed in reach, for a bot that can take it
	const ARRRandomUltimateMeteor* Meteor = ARRRandomUltimateMeteor::Get(this);
	if (Meteor && ARRRandomUltimateMeteor::CanTake(Self) && Meteor->GetTimeToImpact() < 2.f
		&& FVector2D::Distance(Here, FVector2D(Meteor->GetActorLocation())) < UltimateGoalReach)
	{
		if (!bUltimateGoal)
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s heads for the ultimate die (%.0f away)."), *Self->GetName(), FVector2D::Distance(Here, FVector2D(Meteor->GetActorLocation())));
		}
		bUltimateGoal = true;
		bHillGoal = false;
		bHasBattleGoal = true;
		BattleGoal = Meteor->GetActorLocation();
		BattleGoalTimer = 2.f;
		return;
	}
	if (bUltimateGoal)
	{
		bUltimateGoal = false;
		bHasBattleGoal = false;
	}
	// Roaming inside it: a spot not far off, drawn a little toward the ring's middle, kept inside the ring that matters
	// (a hill's top: kept till its time is up, there or on the way)
	if (bHasBattleGoal && BattleGoalTimer > 0.f && (bHillGoal || FVector2D::Distance(Here, FVector2D(BattleGoal)) > RoamArrive))
	{
		return;
	}
	bHillGoal = false;
	const FVector2D KeepCenter = bSoon ? Next : Center;
	const float KeepRadius = (bSoon ? NextRadius : Radius) * RingInsideShare;
	// Round 96: now and then a hill's top in reach and inside the ring (one of them at random: the floor under a pavilion's
	// roof or the deck on it), to hold for a while
	if (Stream.FRand() < HillGoalChance)
	{
		TArray<FVector, TInlineAllocator<8>> Tops;
		for (TActorIterator<ARRRandomDiceTown> It(GetWorld()); It; ++It)
		{
			for (const FVector& Top : It->GetHillTops())
			{
				if (FVector2D::Distance(Here, FVector2D(Top)) < HillGoalReach && FVector2D::Distance(FVector2D(Top), KeepCenter) <= KeepRadius)
				{
					Tops.Add(Top);
				}
			}
		}
		if (Tops.Num() > 0)
		{
			const FVector Top = Tops[Stream.RandHelper(Tops.Num())];
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s heads for a hill's top (%.0f away, %.0f up)."), *Self->GetName(), FVector2D::Distance(Here, FVector2D(Top)), Top.Z - FloorZ);
			bHillGoal = true;
			bHasBattleGoal = true;
			BattleGoal = Top;
			BattleGoalTimer = Stream.FRandRange(HillHoldMin, HillHoldMax);
			return;
		}
	}
	FVector2D Spot = Here;
	for (int32 Try = 0; Try < 12; ++Try)
	{
		const float Angle = Stream.FRandRange(0.f, UE_TWO_PI);
		FVector2D Want = Here + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Stream.FRandRange(RoamReach * 0.3f, RoamReach);
		Want = FMath::Lerp(Want, KeepCenter, 0.2f);
		if (FVector2D::Distance(Want, KeepCenter) <= KeepRadius && RRBattle::IsOnBattleIsland(Battle->GetActorLocation(), Battle->GetReach(), FVector(Want, 0.f)))
		{
			Spot = Want;
			break;
		}
	}
	bHasBattleGoal = true;
	BattleGoal = FVector(Spot, FloorZ);
	BattleGoalTimer = Stream.FRandRange(RoamTimeMin, RoamTimeMax);
}

bool ARRRandomAIController::UpdateBattleTravel(ARRRandomCharacter* Self, bool bInFight, FVector& Wanted, bool& bOnRoute, float DeltaSeconds)
{
	const ARRRandomBattleMap* Battle = ARRRandomBattleMap::Get(this);
	const FVector Goal = bHasBattleGoal ? BattleGoal : Self->GetActorLocation();
	const FVector Location = Self->GetActorLocation();
	const float Trip = FVector::Dist2D(Location, Goal);

	// Flying a plane: high over the roofs toward the goal, then down; off it on the ground (it goes). Shots as usual.
	if (Self->IsPiloting())
	{
		const float FloorZ = Battle ? Battle->GetFloorZ() : Location.Z;
		const FVector Toward = (Goal - Location).GetSafeNormal2D();
		if (Trip > PlaneLandDistance)
		{
			Self->SetPlaneDescend(false);
			if (Location.Z < FloorZ + PlaneCruise)
			{
				Self->Jump();
			}
			else
			{
				Self->StopJumping();
			}
			Wanted = Toward;
		}
		else
		{
			Self->StopJumping();
			Self->SetPlaneDescend(true);
			Wanted = Toward * 0.4f;
			if (Self->IsPlaneLanded())
			{
				Self->SetPlaneDescend(false);
				Self->ToggleRide();
				UE_LOG(LogTemp, Log, TEXT("RRRandom: %s lands its plane and gets out."), *Self->GetName());
			}
		}
		bOnRoute = false;
		return true;
	}
	// Shot from a cannon: nothing to steer
	if (Self->IsRidingCannon())
	{
		Wanted = FVector::ZeroVector;
		bOnRoute = false;
		return true;
	}
	if (bInFight || Self->GetVehicle() || Self->GetRobot() || Self->IsGiant() || Self->CanFly() || !bHasBattleGoal)
	{
		LaunchTarget = nullptr;
		return false;
	}

	// Now and then, for a long way: a cannon near whose pad cuts the way well short, or a plane station for a very long way
	LaunchTimer -= DeltaSeconds;
	if (!LaunchTarget.IsValid() && LaunchTimer <= 0.f)
	{
		LaunchTimer = LaunchLookInterval;
		auto IsTaken = [this](const AActor* Launcher)
		{
			for (TActorIterator<ARRRandomAIController> Other(GetWorld()); Other; ++Other)
			{
				if (*Other != this && Other->GetLaunchTarget() == Launcher)
				{
					return true;
				}
			}
			return false;
		};
		// (the longer the way, the further off a launcher is still worth the walk)
		const float CannonReachNow = FMath::Clamp(Trip * 0.35f, CannonWalk, 6000.f);
		const float StationReachNow = FMath::Clamp(Trip * 0.3f, StationWalk, 7000.f);
		float BestGain = CannonGain;
		for (TActorIterator<ARRRandomCannon> It(GetWorld()); It; ++It)
		{
			const float Walk = FVector::Dist2D(Location, It->GetActorLocation());
			if (!It->IsReady() || Walk > CannonReachNow)
			{
				continue;
			}
			const float Gain = Trip - FVector::Dist2D(It->GetPadLocation(), Goal) - Walk;
			if (Gain > BestGain && !IsTaken(*It))
			{
				BestGain = Gain;
				LaunchTarget = *It;
			}
		}
		if (!LaunchTarget.IsValid() && Trip > PlaneTrip)
		{
			for (TActorIterator<ARRRandomPlaneStation> It(GetWorld()); It; ++It)
			{
				if (It->GetPlane() || It->IsRolling() || It->GetRechargeRemaining() > 0.f || FVector::Dist2D(Location, It->GetActorLocation()) > StationReachNow || IsTaken(*It))
				{
					continue;
				}
				LaunchTarget = *It;
				break;
			}
		}
		if (const AActor* Picked = LaunchTarget.Get())
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s heads for %s (%.0f away) for the long way (%.0f)."), *Self->GetName(), *Picked->GetName(),
				FVector::Dist2D(Location, Picked->GetActorLocation()), Trip);
		}
	}
	const AActor* Launcher = LaunchTarget.Get();
	if (!Launcher)
	{
		return false;
	}
	const ARRRandomCannon* Cannon = Cast<ARRRandomCannon>(Launcher);
	const ARRRandomPlaneStation* Station = Cast<ARRRandomPlaneStation>(Launcher);
	if ((Cannon && !Cannon->IsReady()) || (Station && (Station->GetPlane() || Station->IsRolling() || Station->GetRechargeRemaining() > 0.f)))
	{
		LaunchTarget = nullptr;
		return false;
	}
	const float Reach = Cannon ? ARRRandomCannon::UseRange * 0.7f : ARRRandomPlaneStation::UseRange * 0.6f;
	if (FVector::Dist2D(Location, Launcher->GetActorLocation()) <= Reach)
	{
		// In (the cannon fires it to the pad; the station's die gives a plane, boarded where it stands)
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s uses %s."), *Self->GetName(), *Launcher->GetName());
		Self->ToggleRide();
		LaunchTarget = nullptr;
		LaunchTimer = LaunchLookInterval;
		Wanted = FVector::ZeroVector;
		bOnRoute = false;
		return true;
	}
	Wanted = FollowRoute(Self, Launcher->GetActorLocation(), DeltaSeconds);
	bOnRoute = true;
	return true;
}

void ARRRandomAIController::UpdateSprint(ARRRandomCharacter* Self, bool bInFight, const FVector& Move)
{
	URRRandomMovementComponent* Movement = Cast<URRRandomMovementComponent>(Self->GetCharacterMovement());
	if (!Movement)
	{
		return;
	}
	// Out of a fight, on foot, on a long way (or into the ring): Shift held, as a player would; shooting needs it let go
	const float Trip = bHasBattleGoal ? FVector::Dist2D(Self->GetActorLocation(), BattleGoal) : 0.f;
	Movement->bWantsToSprint = !bInFight && !Move.IsNearlyZero() && Self->CanSprint() && !Self->IsReloading() && (bRingUrgent || Trip > SprintTrip);
}

// ---------------------------------------------------------------------------------------------------------------------
// Round 98, the main mode: cards and abilities

void ARRRandomAIController::UpdateCards(ARRRandomCharacter* Self, float Distance, float DeltaSeconds)
{
	URRRandomDiceBuffComponent* Dice = Self->GetDiceBuffs();
	if (!Dice->HasCards())
	{
		CardTimer = Stream.FRandRange(0.4f, 1.1f);
		return;
	}
	// A moment to "read" them, then the one that suits: hurt, health or a shield; fighting, power or speed of attack;
	// else moving fast, flying or growing
	CardTimer -= DeltaSeconds;
	if (CardTimer > 0.f)
	{
		return;
	}
	const float HealthShare = Self->GetHealth() / FMath::Max(1.f, Self->GetMaxHealth());
	const bool bFighting = Distance <= Self->GetAttackRange() * EngageRangeShare;
	int32 Best = 0;
	float BestScore = -1.f;
	const TArray<ERRCard>& Cards = Dice->GetCards();
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		float Score = Stream.FRandRange(0.f, 0.6f);
		switch (Cards[Index])
		{
		case ERRCard::Heal: Score += HealthShare < 0.5f ? 3.f : HealthShare < 0.8f ? 1.f : 0.f; break;
		case ERRCard::Shield: Score += HealthShare < 0.6f ? 2.f : bFighting ? 1.2f : 0.6f; break;
		case ERRCard::Power: Score += bFighting ? 2.f : 1.2f; break;
		case ERRCard::Rapid: Score += bFighting ? 1.8f : 1.f; break;
		case ERRCard::Giant: Score += bFighting ? 1.7f : 0.7f; break;
		case ERRCard::Swift: Score += bFighting ? 0.6f : 1.3f; break;
		case ERRCard::Wings: Score += bFighting ? 0.8f : 1.1f; break;
		case ERRCard::Cloak: Score += bRetreating ? 2.f : 0.9f; break;
		default: break;
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Index;
		}
	}
	Self->ChooseCard(Best);
}

void ARRRandomAIController::UpdateAbility(ARRRandomCharacter* Self, const ARRRandomCharacter* Target, float Distance, bool bCanSee, float DeltaSeconds)
{
	// The rabbit's super hop: the second jump of a double jump goes super high (it was set off just before)
	if (HopTimer > 0.f)
	{
		HopTimer -= DeltaSeconds;
		if (HopTimer <= 0.f)
		{
			Self->StopJumping();
			Self->Jump();
			HopReleaseTimer = 0.3f;
		}
	}
	if (HopReleaseTimer > 0.f)
	{
		HopReleaseTimer -= DeltaSeconds;
		if (HopReleaseTimer <= 0.f)
		{
			Self->StopJumping();
		}
	}
	const URRRandomKitComponent* Kit = Self->GetKit();
	const FRRKit& Own = RRKit::Get(Self->GetSpecies());
	AbilityThinkTimer -= DeltaSeconds;
	if (!Kit || Kit->GetAbilityCooldown() > 0.f || Own.Ability == ERRAbility::None || AbilityThinkTimer > 0.f)
	{
		return;
	}
	AbilityThinkTimer = Stream.FRandRange(0.4f, 0.9f);
	const bool bSeen = Target && bCanSee;
	const float Range = Self->GetAttackRange();
	bool bUse = false;
	switch (Own.Ability)
	{
	case ERRAbility::Cloak:
		// In to fight unseen, or out of a losing one
		bUse = (bSeen && Distance < Range * 1.4f && Distance > 400.f) || (bRetreating && UnderFireTimer > 0.f);
		break;
	case ERRAbility::SuperHop:
		// Up onto the roofs when pressed, or to see over cover
		bUse = UnderFireTimer > 0.f || (bSeen && Distance < 700.f) || (!bSeen && Stream.FRand() < 0.08f);
		break;
	case ERRAbility::AcornTrap:
		bUse = (bSeen && Distance < 1100.f) || UnderFireTimer > 0.f;
		break;
	case ERRAbility::Giant:
		bUse = bSeen && Distance < 900.f;
		break;
	case ERRAbility::Flight:
		bUse = bSeen && Distance < Range;
		break;
	case ERRAbility::Disguise:
		// Walking in among them unseen before a stab
		bUse = !bRetreating && (!bSeen || Distance > 500.f) && Target != nullptr;
		break;
	default:
		break;
	}
	if (!bUse)
	{
		return;
	}
	Self->UseAbility();
	if (Own.Ability == ERRAbility::SuperHop && !Self->GetCharacterMovement()->IsFalling())
	{
		Self->Jump();
		HopTimer = 0.28f;
	}
}
