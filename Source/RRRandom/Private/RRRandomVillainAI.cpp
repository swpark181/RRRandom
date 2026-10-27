#include "RRRandomVillainAI.h"
#include "RRRandomCharacter.h"
#include "RRRandomDefense.h"
#include "RRRandomDiceTown.h"
#include "RRRandomGameState.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	// An animal friend this close is bitten first (a rat goes back to the tower once it is this far off its way); the cat
	// boss chases them from much further. Height apart past this and it's on another floor.
	constexpr float RatNotice = 650.f;
	constexpr float BossNotice = 2600.f;
	constexpr float FloorApart = 260.f;
	constexpr float RetargetInterval = 0.45f;
	// Its route: worked out again this often (staggered), or when the goal moves this far; a corner counts as reached this close
	constexpr float RouteRefreshTime = 2.f;
	constexpr float RouteRefreshDistance = 300.f;
	constexpr float RouteArrive = 70.f;
	// Stopped this long while walking (not biting): a step aside for a moment
	constexpr float StuckSpeed = 30.f;
	constexpr float StuckTime = 1.2f;
	constexpr float SidestepTime = 0.6f;
	// The cat slams once a friend is this deep inside its slam's reach
	constexpr float SlamShare = 0.75f;
	constexpr float SlamBaseRadius = 380.f;
	// Stuck: checked this often, moving less than this far in that time
	constexpr float ProgressCheckTime = 4.f;
	constexpr float ProgressMinMove = 80.f;
	// Waiting this close to the tower or a gate (beyond its reach) is queueing, not being stuck
	constexpr float QueueRoom = 600.f;
}

ARRRandomVillainAI::ARRRandomVillainAI()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = false;
}

void ARRRandomVillainAI::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	Stream.Initialize(GetUniqueID());
	// Staggered, so a crowd of rats doesn't plan all at once
	RouteTimer = Stream.FRandRange(0.f, RouteRefreshTime);
	RetargetTimer = Stream.FRandRange(0.f, RetargetInterval);
	if (ACharacter* Body = Cast<ACharacter>(InPawn))
	{
		// A crowd: steer round each other instead of piling up
		Body->GetCharacterMovement()->bUseRVOAvoidance = true;
		Body->GetCharacterMovement()->AvoidanceConsiderationRadius = 300.f;
	}
	for (TActorIterator<ARRRandomDiceTown> It(GetWorld()); It; ++It)
	{
		Town = *It;
		break;
	}
}

float ARRRandomVillainAI::GetGap(const ARRRandomCharacter* Self, const AActor* Target)
{
	const FVector Middle = Self->GetActorLocation();
	// A brawler: capsule edge to capsule edge
	if (const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(Target))
	{
		return FVector::Dist2D(Middle, Brawler->GetActorLocation()) - Self->GetCapsuleComponent()->GetScaledCapsuleRadius() - Brawler->GetCapsuleComponent()->GetScaledCapsuleRadius();
	}
	float Best = FVector::Dist2D(Middle, Target->GetActorLocation());
	// The nearest point of whatever on it collides (the tower's box, a wall's), else its middle
	TInlineComponentArray<UPrimitiveComponent*> Parts(Target);
	for (const UPrimitiveComponent* Part : Parts)
	{
		FVector Nearest;
		if (Part->IsCollisionEnabled() && Part->GetClosestPointOnCollision(Middle, Nearest) >= 0.f)
		{
			Best = FMath::Min(Best, (float)FVector::Dist2D(Middle, Nearest));
		}
	}
	return Best - Self->GetCapsuleComponent()->GetScaledCapsuleRadius();
}

ARRRandomCharacter* ARRRandomVillainAI::FindFriend(const ARRRandomCharacter* Self) const
{
	const float Notice = Self->IsVillainBoss() ? BossNotice : RatNotice;
	ARRRandomCharacter* Best = nullptr;
	float BestDistance = Notice;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Other = *It;
		if (Other->GetTeam() == Self->GetTeam() || !Other->IsAlive() || Other->IsVillain() || Other->IsPiloting() || Other->IsRidingCannon()
			|| FMath::Abs(Other->GetActorLocation().Z - Self->GetActorLocation().Z) > FloorApart * FMath::Max(1.f, Self->GetVillainBodyScale()))
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Other->GetActorLocation(), Self->GetActorLocation());
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Other;
		}
	}
	return Best;
}

ARRRandomDefenseWall* ARRRandomVillainAI::FindBlockingWall(const ARRRandomCharacter* Self, const FVector& TowerAt) const
{
	const FVector Here = Self->GetActorLocation();
	for (TActorIterator<ARRRandomDefenseWall> It(GetWorld()); It; ++It)
	{
		ARRRandomDefenseWall* Wall = *It;
		if (Wall->IsBroken() || Wall->IsOpen())
		{
			continue;
		}
		// On the other side of it from the tower, and within reach
		const FVector Across = Wall->GetActorForwardVector().GetSafeNormal2D();
		const float Side = FVector::DotProduct(Here - Wall->GetActorLocation(), Across);
		const float TowerSide = FVector::DotProduct(TowerAt - Wall->GetActorLocation(), Across);
		if (Side * TowerSide < 0.f && GetGap(Self, Wall) <= Self->GetVillainReach() + 25.f)
		{
			return Wall;
		}
	}
	return nullptr;
}

void ARRRandomVillainAI::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ARRRandomCharacter* Self = Cast<ARRRandomCharacter>(GetPawn());
	if (!Self || !Self->IsAlive() || ARRRandomGameState::IsPlayLocked(this))
	{
		return;
	}

	RetargetTimer -= DeltaSeconds;
	if (RetargetTimer <= 0.f || (Friend.IsValid() && !Friend->IsAlive()))
	{
		RetargetTimer = RetargetInterval;
		Friend = FindFriend(Self);
	}
	const ARRRandomDiceTower* Tower = ARRRandomDiceTower::Get(this);
	const FVector TowerAt = Tower ? Tower->GetActorLocation() : Self->GetActorLocation();
	const float Reach = Self->GetVillainReach();

	// The cat boss: friends crowding round it get the slam
	if (Self->IsVillainBoss() && Self->GetSlamCooldown() <= 0.f)
	{
		const float SlamReach = SlamBaseRadius * FMath::Max(1.f, Self->GetVillainBodyScale() * 0.6f) * SlamShare;
		for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
		{
			if (It->GetTeam() != Self->GetTeam() && It->IsAlive() && FVector::Dist2D(It->GetActorLocation(), Self->GetActorLocation()) < SlamReach
				&& FMath::Abs(It->GetActorLocation().Z - Self->GetActorLocation().Z) < FloorApart)
			{
				Self->VillainSlam();
				break;
			}
		}
	}

	// Stuck somewhere (a crater a CANNON dug, a corner) for a few seconds without biting: hop (a jump climbs a ledge in
	// reach); three checks in a row, and the director puts it back at a gate
	ProgressTimer += DeltaSeconds;
	if (ProgressTimer >= ProgressCheckTime)
	{
		// (crowding round the tower or a gate, waiting for room to bite, isn't being stuck)
		bool bQueueing = Tower && FVector::Dist2D(Self->GetActorLocation(), TowerAt) < ARRRandomDiceTower::GetHalfWidth() + QueueRoom;
		for (TActorIterator<ARRRandomDefenseWall> It(GetWorld()); It && !bQueueing; ++It)
		{
			bQueueing = !It->IsBroken() && FVector::Dist2D(Self->GetActorLocation(), It->GetActorLocation()) < It->GetLength() * 0.5f + QueueRoom;
		}
		const bool bStuck = !bBitLately && !bQueueing && FVector::Dist(Self->GetActorLocation(), ProgressFrom) < ProgressMinMove;
		StuckChecks = bStuck ? StuckChecks + 1 : 0;
		ProgressTimer = 0.f;
		ProgressFrom = Self->GetActorLocation();
		bBitLately = false;
		if (StuckChecks >= 3)
		{
			StuckChecks = 0;
			if (ARRRandomDefenseDirector* Director = ARRRandomDefenseDirector::Get(this))
			{
				Director->ReturnToGate(Self);
			}
		}
		else if (bStuck)
		{
			Self->Jump();
		}
	}

	// What to bite now: a friend in reach, a wall in the way, the tower
	AActor* Bite = nullptr;
	if (Friend.IsValid() && GetGap(Self, Friend.Get()) <= Reach)
	{
		Bite = Friend.Get();
	}
	else if (ARRRandomDefenseWall* Wall = FindBlockingWall(Self, TowerAt))
	{
		Bite = Wall;
	}
	else if (Tower && !Tower->IsDestroyed() && GetGap(Self, Tower) <= Reach + 30.f)
	{
		Bite = const_cast<ARRRandomDiceTower*>(Tower);
	}
	if (Bite)
	{
		bBitLately = true;
		Self->VillainStrike(Bite);
		StuckTimer = 0.f;
		// Standing up to it (a friend may still dodge away: keep after it)
		if (Bite == Friend.Get())
		{
			Self->AddMovementInput((Bite->GetActorLocation() - Self->GetActorLocation()).GetSafeNormal2D(), 0.3f);
		}
		return;
	}

	// Walking: to the friend it is after, or the tower
	const FVector Goal = Friend.IsValid() ? Friend->GetActorLocation() : TowerAt;
	FVector Move = FollowRoute(Self, Goal, DeltaSeconds);
	const bool bStopped = Self->GetVelocity().Size2D() < StuckSpeed;
	StuckTimer = bStopped ? StuckTimer + DeltaSeconds : 0.f;
	if (StuckTimer >= StuckTime)
	{
		// A step aside, then a fresh route
		StuckTimer = 0.f;
		SidestepTimer = SidestepTime;
		Sidestep = FVector::CrossProduct(FVector::UpVector, Move.IsNearlyZero() ? Self->GetActorForwardVector() : Move).GetSafeNormal2D() * (Stream.FRand() < 0.5f ? -1.f : 1.f);
		RouteTimer = 0.f;
	}
	if (SidestepTimer > 0.f)
	{
		SidestepTimer -= DeltaSeconds;
		Move = (Sidestep + Move * 0.3f).GetSafeNormal2D();
	}
	if (!Move.IsNearlyZero())
	{
		Self->AddMovementInput(Move);
	}
}

FVector ARRRandomVillainAI::FollowRoute(ARRRandomCharacter* Self, const FVector& Goal, float DeltaSeconds)
{
	const FVector Feet = Self->GetActorLocation() - FVector(0.f, 0.f, Self->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	RouteTimer -= DeltaSeconds;
	if (RouteTimer <= 0.f || RouteIndex >= Route.Num() || FVector::Dist(Goal, RouteGoal) > RouteRefreshDistance)
	{
		RouteTimer = RouteRefreshTime + Stream.FRandRange(-0.3f, 0.3f);
		RouteGoal = Goal;
		RouteIndex = 0;
		if (!Town.IsValid() || !Town->FindPath(Feet, Goal, Route))
		{
			Route.Reset();
		}
	}
	while (Route.IsValidIndex(RouteIndex) && FVector::Dist2D(Feet, Route[RouteIndex]) < RouteArrive && FMath::Abs(Feet.Z - Route[RouteIndex].Z) < 150.f)
	{
		++RouteIndex;
	}
	const FVector Next = Route.IsValidIndex(RouteIndex) ? Route[RouteIndex] : Goal;
	const FVector Toward = (Next - Feet) * FVector(1.f, 1.f, 0.f);
	return Toward.Size() < 10.f ? FVector::ZeroVector : Toward.GetSafeNormal();
}
