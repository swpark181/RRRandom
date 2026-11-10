#include "RRRandomOracle.h"
#include "RRRandomBattleRoyale.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "RRRandomWreckage.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** A storm, by when it comes (after closing phase 1, 2, ...): meteors, seconds they fall over, seconds each pad glows first. */
	struct FRROracleRound
	{
		int32 Meteors;
		float Span;
		float Lead;
	};
	// The zone is a quarter of the clear area each time, so it shrinks with the ring: fewer meteors packed closer. Standing
	// still in the zone, the chance of being in a blast goes 8%, 18%, 34%, 57%, 87% (one blast covers 600 round)
	const FRROracleRound OracleRounds[] =
	{
		{ 30, 10.f, 3.f },
		{ 28, 9.f, 2.5f },
		{ 20, 8.f, 2.f },
		{ 12, 6.f, 2.f },
		{ 4, 4.f, 1.5f },
	};
	// The zone's radius as a share of the ring's (a quarter of the area), and never smaller than this
	constexpr float ZoneShare = 0.5f;
	constexpr float ZoneMin = 800.f;

	// The blast: this much in the middle, this much at the edge; the knock aside; the blocks smashed (a ball this big,
	// digging this many layers)
	constexpr float BlastFull = 40.f;
	constexpr float BlastEdge = 20.f;
	constexpr float KnockSpeed = 700.f;
	constexpr float KnockLift = 460.f;
	constexpr float WreckRadius = 450.f;
	constexpr int32 WreckDig = 1;

	// The rock: this tall (the Meteor Rush meteor's crust, a burning core in its ring), falling this long from this far
	// off to one side and this high
	constexpr float RockHeight = 260.f;
	constexpr float FallTime = 0.9f;
	constexpr float FallSide = 1600.f;
	constexpr float FallRise = 3000.f;
	constexpr float SinkTime = 0.9f;
	constexpr float GoneAfter = 1.6f;

	const FLinearColor PadColor = FLinearColor::FromSRGBColor(FColor(255, 70, 110));
	const FLinearColor FireColor = FLinearColor::FromSRGBColor(FColor(255, 140, 70));

	float ServerNow(const UWorld* World)
	{
		const AGameStateBase* State = World ? World->GetGameState() : nullptr;
		return State ? State->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.f);
	}

	float EaseIn(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * (0.35f + 0.65f * T);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The storm

ARRRandomOracleStorm::ARRRandomOracleStorm()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Stream.GenerateNewSeed();
}

void ARRRandomOracleStorm::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomOracleStorm, Round);
	DOREPLIFETIME(ARRRandomOracleStorm, RollStart);
	DOREPLIFETIME(ARRRandomOracleStorm, StormEnd);
	DOREPLIFETIME(ARRRandomOracleStorm, Slots);
	DOREPLIFETIME(ARRRandomOracleStorm, StruckTeam);
	DOREPLIFETIME(ARRRandomOracleStorm, Target);
	DOREPLIFETIME(ARRRandomOracleStorm, ZoneCenter);
	DOREPLIFETIME(ARRRandomOracleStorm, ZoneRadius);
	DOREPLIFETIME(ARRRandomOracleStorm, MeteorCount);
}

ARRRandomOracleStorm* ARRRandomOracleStorm::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ARRRandomOracleStorm> It(World);
	return It ? *It : nullptr;
}

int32 ARRRandomOracleStorm::GetRoundCount()
{
	return UE_ARRAY_COUNT(OracleRounds);
}

void ARRRandomOracleStorm::RollFor(const UObject* WorldContext, int32 InRound, const FVector2D& RingCenter, float RingRadius, ARRRandomCharacter* ForcedTarget)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || World->GetNetMode() == NM_Client || InRound < 1 || InRound > GetRoundCount())
	{
		return;
	}
	ARRRandomOracleStorm* Storm = Get(World);
	if (!Storm)
	{
		Storm = World->SpawnActor<ARRRandomOracleStorm>(ARRRandomOracleStorm::StaticClass(), FTransform::Identity);
	}
	if (Storm)
	{
		Storm->Roll(InRound, RingCenter, RingRadius, ForcedTarget);
	}
}

void ARRRandomOracleStorm::RollNext(const UObject* WorldContext, ARRRandomCharacter* ForcedTarget)
{
	const ARRRandomBattleZone* Zone = ARRRandomBattleZone::Get(WorldContext);
	if (!Zone || Zone->GetPhase() <= 0)
	{
		return;
	}
	const ARRRandomOracleStorm* Storm = Get(WorldContext);
	const int32 Next = Storm ? Storm->GetRound() % GetRoundCount() + 1 : 1;
	RollFor(WorldContext, Next, Zone->GetCenter(), Zone->GetRadius(), ForcedTarget);
}

float ARRRandomOracleStorm::GetNow() const
{
	return ServerNow(GetWorld());
}

float ARRRandomOracleStorm::GetAge() const
{
	return Round > 0 ? GetNow() - RollStart : UE_BIG_NUMBER;
}

bool ARRRandomOracleStorm::IsStriking() const
{
	const float Age = GetAge();
	return Round > 0 && Age >= RollTime && GetNow() < StormEnd + 0.5f;
}

void ARRRandomOracleStorm::GetSums(TArray<int32>& OutSums) const
{
	OutSums.Reset();
	for (const FRROracleSlot& Slot : Slots)
	{
		if (Slot.Team < 0)
		{
			continue;
		}
		while (OutSums.Num() <= Slot.Team)
		{
			OutSums.Add(-1);
		}
		OutSums[Slot.Team] = FMath::Max(OutSums[Slot.Team], 0) + Slot.Value;
	}
}

void ARRRandomOracleStorm::Roll(int32 InRound, const FVector2D& RingCenter, float RingRadius, ARRRandomCharacter* ForcedTarget)
{
	const ARRRandomBattleZone* Zone = ARRRandomBattleZone::Get(this);
	// Who is standing, team by team
	TMap<int32, TArray<ARRRandomCharacter*>> Living;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && !It->IsDecoy() && It->GetTeam() >= 0)
		{
			Living.FindOrAdd(It->GetTeam()).Add(*It);
		}
	}
	if (Living.Num() == 0)
	{
		return;
	}
	TArray<int32> Teams;
	Living.GetKeys(Teams);
	Teams.Sort();
	// Every slot of every team still standing rolls (the living first, then the slots whose friends are down or gone);
	// the lowest sum is struck, a tie rolled again between the tied teams only
	Slots.Reset();
	TArray<int32> Tied = Teams;
	TMap<int32, int32> Sums;
	for (int32 Pass = 0; Pass < 6 && Tied.Num() != 1; ++Pass)
	{
		Slots.RemoveAll([&Tied](const FRROracleSlot& Slot) { return Tied.Contains(Slot.Team); });
		for (const int32 Team : Tied)
		{
			const TArray<ARRRandomCharacter*>& Members = Living[Team];
			int32 Sum = 0;
			for (int32 Seat = 0; Seat < FMath::Max(RRBattle::TeamSize, Members.Num()); ++Seat)
			{
				FRROracleSlot& Slot = Slots.AddDefaulted_GetRef();
				Slot.Who = Members.IsValidIndex(Seat) ? Members[Seat] : nullptr;
				Slot.Team = int8(Team);
				Slot.Value = uint8(Stream.RandRange(1, 6));
				Sum += Slot.Value;
			}
			Sums.Add(Team, Sum);
		}
		int32 Lowest = MAX_int32;
		for (const int32 Team : Tied)
		{
			Lowest = FMath::Min(Lowest, Sums[Team]);
		}
		Tied.RemoveAll([&](int32 Team) { return Sums[Team] != Lowest; });
	}
	Slots.Sort([](const FRROracleSlot& A, const FRROracleSlot& B) { return A.Team < B.Team; });
	StruckTeam = Tied[Stream.RandRange(0, Tied.Num() - 1)];
	const TArray<ARRRandomCharacter*>& Struck = Living[StruckTeam];
	Target = Struck[Stream.RandRange(0, Struck.Num() - 1)];
	// (the cheat's: on this one, whatever the dice said)
	if (ForcedTarget && ForcedTarget->IsAlive())
	{
		Target = ForcedTarget;
		StruckTeam = ForcedTarget->GetTeam();
	}

	const FRROracleRound& Plan = OracleRounds[InRound - 1];
	Round = InRound;
	RollStart = GetNow();
	ZoneCenter = FVector2D(Target->GetActorLocation());
	ZoneRadius = FMath::Max(RingRadius * ZoneShare, ZoneMin);
	MeteorCount = Plan.Meteors;
	StormEnd = RollStart + FirstImpact + Plan.Span;

	// Where they land: the first right where the target stands, the rest anywhere in the zone, each on the first thing
	// in its way from above (a roof, a wall top, the street); off the island, nothing to land on: drawn again
	const float FloorZ = Zone ? Zone->GetFloorZ() : Target->GetActorLocation().Z - 100.f;
	Drops.Reset();
	FCollisionObjectQueryParams Statics(ECC_WorldStatic);
	FCollisionQueryParams Query(TEXT("OracleMeteor"), false, this);
	for (int32 Index = 0; Index < Plan.Meteors; ++Index)
	{
		for (int32 Try = 0; Try < 8; ++Try)
		{
			const FVector2D Where = Index == 0 && Try == 0 ? ZoneCenter
				: ZoneCenter + FVector2D(1.f, 0.f).GetRotated(Stream.FRandRange(0.f, 360.f)) * ZoneRadius * FMath::Sqrt(Stream.FRand());
			FHitResult Hit;
			if (GetWorld()->LineTraceSingleByObjectType(Hit, FVector(Where, FloorZ + 20000.f), FVector(Where, FloorZ - 3000.f), Statics, Query))
			{
				FDrop& Drop = Drops.AddDefaulted_GetRef();
				Drop.Spot = Hit.ImpactPoint;
				Drop.Impact = RollStart + FirstImpact + (Plan.Meteors > 1 ? Plan.Span * Index / (Plan.Meteors - 1) : 0.f);
				Drop.Lead = Plan.Lead;
				break;
			}
		}
	}
	ForceNetUpdate();

	FString Rolled;
	TArray<int32> TeamSums;
	GetSums(TeamSums);
	for (int32 Team = 0; Team < TeamSums.Num(); ++Team)
	{
		if (TeamSums[Team] >= 0)
		{
			Rolled += FString::Printf(TEXT(" T%d=%d"), Team, TeamSums[Team]);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: oracle %d of %d rolled:%s; team %d is struck round %s (zone %.0f round %s, ring %.0f), %d meteors over %.0f s (%d spots found)."),
		Round, GetRoundCount(), *Rolled, StruckTeam, *GetNameSafe(Target), ZoneRadius, *ZoneCenter.ToString(), RingRadius, Plan.Meteors, Plan.Span, Drops.Num());
}

void ARRRandomOracleStorm::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority())
	{
		DropDue();
	}
}

void ARRRandomOracleStorm::DropDue()
{
	if (Drops.Num() == 0)
	{
		return;
	}
	if (ARRRandomGameState::IsMatchOver(this))
	{
		Drops.Reset();
		return;
	}
	const float Now = GetNow();
	for (int32 Index = 0; Index < Drops.Num(); )
	{
		const FDrop& Drop = Drops[Index];
		// Spawned as its pad starts to glow (a beat early, for the trip to the other machines)
		if (Now < Drop.Impact - Drop.Lead - 0.15f)
		{
			++Index;
			continue;
		}
		const FTransform Spot(Drop.Spot);
		if (ARRRandomOracleMeteor* Meteor = GetWorld()->SpawnActorDeferred<ARRRandomOracleMeteor>(ARRRandomOracleMeteor::StaticClass(), Spot, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Meteor->Setup(Drop.Impact, Drop.Lead);
			Meteor->FinishSpawning(Spot);
		}
		Drops.RemoveAt(Index);
	}
}

bool ARRRandomOracleStorm::FindDodge(const UObject* WorldContext, const FVector2D& Here, const FVector2D& RingCenter, float RingRadius, FVector2D& OutSpot)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return false;
	}
	// A pad about to be hit: straight out of its blast (along the ring's way if standing in its middle)
	for (TActorIterator<ARRRandomOracleMeteor> It(World); It; ++It)
	{
		const float Left = It->GetTimeToImpact();
		const FVector2D Spot(It->GetActorLocation());
		const float Distance = FVector2D::Distance(Here, Spot);
		if (Left <= 0.f || Left > It->GetLead() + 0.5f || Distance > BlastRadius + 150.f)
		{
			continue;
		}
		const FVector2D Away = Distance > 10.f ? (Here - Spot) / Distance : (RingCenter - Here).GetSafeNormal();
		OutSpot = Spot + Away * (BlastRadius + 350.f);
		return true;
	}
	// In the zone while meteors are still to come: out the nearest way, if that is still in the ring
	const ARRRandomOracleStorm* Storm = Get(World);
	if (!Storm || !Storm->IsStriking())
	{
		return false;
	}
	const float FromMiddle = FVector2D::Distance(Here, Storm->ZoneCenter);
	if (FromMiddle > Storm->ZoneRadius + 200.f)
	{
		return false;
	}
	const FVector2D Out = FromMiddle > 10.f ? (Here - Storm->ZoneCenter) / FromMiddle : (RingCenter - Here).GetSafeNormal();
	for (const float Turn : { 0.f, 40.f, -40.f, 80.f, -80.f, 130.f, -130.f })
	{
		const FVector2D Spot = Storm->ZoneCenter + Out.GetRotated(Turn) * (Storm->ZoneRadius + 700.f);
		if (FVector2D::Distance(Spot, RingCenter) < RingRadius - 300.f)
		{
			OutSpot = Spot;
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// A meteor

ARRRandomOracleMeteor::ARRRandomOracleMeteor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CrustMesh(TEXT("/Game/MeteorRush/SM_MRMeteor.SM_MRMeteor"));
	auto MakeMesh = [this](const TCHAR* Name, USceneComponent* Parent, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Parent);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		Part->SetStaticMesh(Mesh);
		return Part;
	};
	Ring = MakeMesh(TEXT("Ring"), Root, Plane.Object);
	Closing = MakeMesh(TEXT("Closing"), Root, Plane.Object);
	Rock = MakeMesh(TEXT("Rock"), Root, CrustMesh.Object);
	Rock->SetUsingAbsoluteLocation(true);
	Rock->SetUsingAbsoluteRotation(true);
	Rock->SetUsingAbsoluteScale(true);
	Core = MakeMesh(TEXT("Core"), Rock, Sphere.Object);
	// In the crust's ring (its middle 0.47 of the way up, 0.37 of the height round: the model is 100 tall)
	Core->SetRelativeLocation(FVector(0.f, 0.f, 47.f));
	Core->SetRelativeScale3D(FVector(0.74f));
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Rock);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 47.f));
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(1200.f);
	Light->SetLightColor(FireColor);
}

void ARRRandomOracleMeteor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomOracleMeteor, ImpactTime);
	DOREPLIFETIME(ARRRandomOracleMeteor, Lead);
}

void ARRRandomOracleMeteor::Setup(float InImpactTime, float InLead)
{
	ImpactTime = InImpactTime;
	Lead = InLead;
}

float ARRRandomOracleMeteor::GetTimeToImpact() const
{
	return ImpactTime - ServerNow(GetWorld());
}

void ARRRandomOracleMeteor::BeginPlay()
{
	Super::BeginPlay();
	// The landing pad: the Meteor Rush hologram (a band at the edge of a flat plane), red-pink, as big as the blast
	if (UMaterialInterface* Marker = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRRing.M_MRRing")))
	{
		RingMaterial = UMaterialInstanceDynamic::Create(Marker, this);
		RingMaterial->SetVectorParameterValue(TEXT("Color"), PadColor);
		RingMaterial->SetScalarParameterValue(TEXT("Glow"), 3.f);
		RingMaterial->SetScalarParameterValue(TEXT("Thickness"), 0.06f);
		RingMaterial->SetScalarParameterValue(TEXT("Fill"), 0.3f);
		ClosingMaterial = UMaterialInstanceDynamic::Create(Marker, this);
		ClosingMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(FColor(255, 190, 210)));
		ClosingMaterial->SetScalarParameterValue(TEXT("Glow"), 2.2f);
		ClosingMaterial->SetScalarParameterValue(TEXT("Fill"), 0.f);
		for (int32 Slot = 0; Slot < Ring->GetNumMaterials(); ++Slot)
		{
			Ring->SetMaterial(Slot, RingMaterial);
			Closing->SetMaterial(Slot, ClosingMaterial);
		}
	}
	// The rock burns: its crust glowing, a fire-orange core
	if (UMaterialInterface* CrustMaterial = Rock->GetMaterial(0))
	{
		UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(CrustMaterial, this);
		Paint->SetScalarParameterValue(TEXT("Glow"), 2.5f);
		Rock->SetMaterial(0, Paint);
	}
	if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRGlass.M_MRGlass")))
	{
		UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Glass, this);
		Paint->SetVectorParameterValue(TEXT("Tint"), FireColor);
		Paint->SetScalarParameterValue(TEXT("Opacity"), 0.85f);
		Paint->SetScalarParameterValue(TEXT("EdgeOpacity"), 1.f);
		Paint->SetScalarParameterValue(TEXT("Glow"), 6.f);
		Core->SetMaterial(0, Paint);
	}
	// It comes in from one side (its own, the same on every machine) and high up
	const float Yaw = float(GetActorLocation().X * 0.013f + GetActorLocation().Y * 0.007f);
	FallFrom = FVector(FallSide, 0.f, 0.f).RotateAngleAxis(FMath::Fmod(Yaw * 57.3f, 360.f), FVector::UpVector) + FVector(0.f, 0.f, FallRise);
	UpdateLook(0.f);
}

void ARRRandomOracleMeteor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLook(DeltaSeconds);
	if (HasAuthority())
	{
		const float T = -GetTimeToImpact();
		if (T >= 0.f && !bLanded)
		{
			bLanded = true;
			Land();
		}
		if (T > GoneAfter)
		{
			Destroy();
		}
	}
}

void ARRRandomOracleMeteor::UpdateLook(float DeltaSeconds)
{
	const float T = -GetTimeToImpact();
	const FVector Target = GetActorLocation();

	// The pad: glowing round the spot, pulsing quicker as it nears, a ring closing in from twice as far
	const bool bPad = T < 0.f && T >= -Lead - 0.2f;
	Ring->SetVisibility(bPad);
	Closing->SetVisibility(bPad);
	if (bPad)
	{
		const float Left = FMath::Clamp(-T / FMath::Max(Lead, 0.1f), 0.f, 1.f);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * (9.f + 12.f * (1.f - Left)));
		Ring->SetWorldLocationAndRotation(Target + FVector(0.f, 0.f, 8.f), FRotator::ZeroRotator);
		Ring->SetWorldScale3D(FVector(ARRRandomOracleStorm::BlastRadius / 50.f, ARRRandomOracleStorm::BlastRadius / 50.f, 1.f));
		const float Closer = ARRRandomOracleStorm::BlastRadius * (1.f + Left);
		Closing->SetWorldLocationAndRotation(Target + FVector(0.f, 0.f, 7.f), FRotator::ZeroRotator);
		Closing->SetWorldScale3D(FVector(Closer / 50.f, Closer / 50.f, 1.f));
		if (RingMaterial)
		{
			RingMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.55f + 0.45f * Pulse);
			ClosingMaterial->SetScalarParameterValue(TEXT("Thickness"), 22.f / Closer);
			ClosingMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.5f + 0.5f * (1.f - Left));
		}
	}

	// The rock: out of sight until it falls, streaking down the last FallTime with a fiery tail, then sinking into the
	// hole it made
	const bool bFalling = T >= -FallTime && T < 0.f;
	FVector Where = Target;
	float Size = 1.f;
	if (bFalling)
	{
		Where = FMath::Lerp(Target + FallFrom, Target, EaseIn((T + FallTime) / FallTime));
	}
	else if (T >= 0.f)
	{
		Size = FMath::Clamp(1.f - T / SinkTime, 0.f, 1.f);
		Where = Target - FVector(0.f, 0.f, RockHeight * 0.5f * (1.f - Size));
	}
	const bool bShown = (bFalling || T >= 0.f) && Size > 0.02f;
	Rock->SetVisibility(bShown, true);
	Light->SetVisibility(bShown);
	if (bShown)
	{
		Rock->SetWorldLocationAndRotation(Where, FRotator(T * 220.f, T * 300.f, 0.f));
		Rock->SetWorldScale3D(FVector(RockHeight / 100.f * Size));
		Light->SetIntensity(14000.f * Size);
	}
	if (bFalling && DeltaSeconds > 0.f)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			const FVector Before = FMath::Lerp(Target + FallFrom, Target, EaseIn((T - DeltaSeconds + FallTime) / FallTime));
			Effects->StarTail(Before, Where, RockHeight * 0.5f, 2400.f, FireColor, DeltaSeconds);
		}
	}
	// Landing, once on each machine: the blast ring, sparks, dust and a comic word
	if (T >= 0.f && !bLandingShown)
	{
		bLandingShown = true;
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->TigerStrike(Target, ARRRandomOracleStorm::BlastRadius, FireColor);
			Effects->StarImpact(Target, -FallFrom, 2.8f, FireColor);
			for (int32 Puff = 0; Puff < 5; ++Puff)
			{
				Effects->FoxPoof(Target + FVector(RockHeight * 0.7f, 0.f, 30.f).RotateAngleAxis(Puff * 72.f + 15.f, FVector::UpVector), 0.8f);
			}
		}
		const TCHAR* const Words[] = { TEXT("BOOM!"), TEXT("KA-BOOM!"), TEXT("KABLAM!"), TEXT("CRASH!") };
		ARRRandomHUD::PopComicWord(this, Words[GetUniqueID() % UE_ARRAY_COUNT(Words)], Target + FVector(0.f, 0.f, RockHeight * 1.3f), FireColor, 1.2f);
	}
}

void ARRRandomOracleMeteor::Land()
{
	const FVector Target = GetActorLocation();
	// Hurt round it, whatever the team (no one threw it), more in the middle; walls and roofs in between shelter
	UGameplayStatics::ApplyRadialDamageWithFalloff(this, BlastFull, BlastEdge, Target + FVector(0.f, 0.f, 60.f), ARRRandomOracleStorm::BlastRadius * 0.25f, ARRRandomOracleStorm::BlastRadius, 1.f,
		UDamageType::StaticClass(), TArray<AActor*>(), this, nullptr, ECC_Visibility);
	// Everyone close knocked aside, away from the middle
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Friend = *It;
		const FVector Offset = Friend->GetActorLocation() - Target;
		if (!Friend->IsAlive() || Friend->IsDecoy() || Friend->GetVehicle() || Offset.Size2D() > ARRRandomOracleStorm::BlastRadius || FMath::Abs(Offset.Z) > 500.f)
		{
			continue;
		}
		const FVector Away = Offset.Size2D() > 10.f ? Offset.GetSafeNormal2D() : FVector(1.f, 0.f, 0.f).RotateAngleAxis(FMath::FRandRange(0.f, 360.f), FVector::UpVector);
		Friend->LaunchCharacter(Away * KnockSpeed * (1.f - 0.4f * Offset.Size2D() / ARRRandomOracleStorm::BlastRadius) + FVector(0.f, 0.f, KnockLift), true, true);
	}
	// The blocks it lands on smashed, for good
	ARRRandomWreckage::Wreck(this, Target, Target, WreckRadius, false, WreckDig, true);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: an oracle meteor lands at %s."), *Target.ToCompactString());
}
