#include "RRRandomBattleRoyale.h"
#include "RRRandomCannon.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceTown.h"
#include "RRRandomDefense.h"
#include "RRRandomGameState.h"
#include "RRRandomMapFloor.h"
#include "RRRandomOracle.h"
#include "RRRandomPlaneStation.h"
#include "RRRandomVehicle.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

namespace
{
	TAutoConsoleVariable<int32> CVarLogStreaming(TEXT("rr.BR.LogStreaming"), 1,
		TEXT("1: log the battle map's streaming each time the tiles shown up close change (and every 10 seconds while they don't)."));
	TAutoConsoleVariable<float> CVarDetailRadius(TEXT("rr.BR.DetailRadius"), RRBattle::DetailRadius,
		TEXT("How near this machine's view a battle map tile's square must be to show its detail (blocks, models, lamps); further, its far look."));
	TAutoConsoleVariable<int32> CVarStreamingOff(TEXT("rr.BR.StreamingOff"), 0,
		TEXT("1: every battle map tile shows its detail (to compare the frame time with streaming)."));

	// The streaming looks at the view this often, and changes at most this many tiles at once (each a hitch of its own)
	constexpr float StreamInterval = 0.15f;
	constexpr int32 MaxSwitchesPerUpdate = 3;

	// The island: a rounded square (as Dice Town's, superellipse power 5) this share of the map's half width out
	constexpr float IslandShare = 8450.f / 8800.f;
	constexpr float IslandPower = 5.f;

	// Where the teams start: a ring this share of the island's radius out, each start on open ground near its spot
	constexpr float StartRingShare = 0.62f;
	constexpr float StartClear = 260.f;
	// Round 97: the teams' places on the plaza during the get-ready time, this far round its middle
	constexpr float PlazaStartRadius = 1050.f;

	// The rides spread over the map: about this many of each per tile of ground; cannons shoot this far toward the middle
	constexpr float BikesPerTile = 0.55f;
	constexpr float KickboardsPerTile = 0.35f;
	constexpr float CarsPerTile = 0.3f;
	constexpr float CannonsPerTile = 0.4f;
	constexpr float StationsPerTile = 0.2f;
	constexpr float StarSetsPerTile = 0.07f;
	constexpr float CannonShot = 7000.f;

	/** The rounded square's radius in a direction (radians), on average (no wobble). */
	float IslandRadiusAt(float Half, float Theta)
	{
		const float C = FMath::Abs(FMath::Cos(Theta));
		const float S = FMath::Abs(FMath::Sin(Theta));
		return Half * IslandShare / FMath::Pow(FMath::Pow(C, IslandPower) + FMath::Pow(S, IslandPower), 1.f / IslandPower);
	}

	float TileSize()
	{
		return RRBattle::TileCells * 100.f;
	}

	// A landing must have a broad, level surface, clear space above it and no railing through the pad.
	bool FindRaisedLanding(const ARRRandomBattleMap& Map, const FVector& Anchor, FVector& Out)
	{
		UWorld* World = Map.GetWorld();
		const float Floor = Map.GetFloorZ();
		const ARRRandomDiceTown* Tile = Map.GetTileAt(FVector2D(Anchor));
		if (!Tile) { return false; }
		const float Top = Tile->GetTileFlightTop();
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(BRLanding), false);
		for (int32 Ring = 0; Ring <= 5; ++Ring)
		{
			const int32 Steps = Ring ? Ring * 8 : 1;
			for (int32 I = 0; I < Steps; ++I)
			{
				const float Angle = 2.f * PI * I / Steps;
				const FVector XY = Anchor + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * (Ring * 200.f);
				FHitResult Center;
				if (!World->LineTraceSingleByChannel(Center, FVector(XY.X, XY.Y, Top), FVector(XY.X, XY.Y, Floor + 500.f), ECC_Visibility, Params)
					|| Center.ImpactNormal.Z < 0.98f || Center.ImpactPoint.Z > Floor + 2500.f) { continue; }
				const FVector Spot = Center.ImpactPoint;
				bool bFlat = true;
				for (int32 X = -1; X <= 1 && bFlat; ++X)
				{
					for (int32 Y = -1; Y <= 1 && bFlat; ++Y)
					{
						const FVector Sample = Spot + FVector(X * 260.f, Y * 260.f, 0.f);
						FHitResult Hit;
						bFlat = World->LineTraceSingleByChannel(Hit, Sample + FVector(0.f, 0.f, 350.f), Sample - FVector(0.f, 0.f, 60.f), ECC_Visibility, Params)
							&& Hit.ImpactNormal.Z > 0.98f && FMath::Abs(Hit.ImpactPoint.Z - Spot.Z) < 15.f;
					}
				}
				if (!bFlat || World->OverlapBlockingTestByChannel(Spot + FVector(0.f, 0.f, 155.f), FQuat::Identity, ECC_Pawn,
					FCollisionShape::MakeBox(FVector(250.f, 250.f, 140.f)), Params)) { continue; }
				Out = Spot;
				return true;
			}
		}
		return false;
	}

	bool IsRampRouteClear(const ARRRandomBattleMap& Map, const FVector& Foot, const FVector& To, const FVector& Forward, float Length)
	{
		// A straight, wide 10 m run-up, including space to board the parked motorcycle.
		for (float Along = -1000.f; Along <= Length; Along += 250.f)
		{
			FVector Ground;
			const FVector Want = Foot + Forward * Along;
			if (!RRBattle::IsOnBattleIsland(Map.GetActorLocation(), Map.GetReach(), Want)
				|| !Map.FindOpenGround(FVector2D(Want), 220.f, 0.f, Ground)) { return false; }
		}
		constexpr float HalfHeight = 110.f;
		const FVector Start = Foot + Forward * (Length * 0.8f) + FVector(0.f, 0.f, ARRRandomJumpRamp::Rise * 0.8f + HalfHeight + 5.f);
		const FVector End = To + FVector(0.f, 0.f, HalfHeight + 5.f);
		const float Gravity = 1960.f; // Gravity changes time/speed, not this trajectory's shape.
		float Time;
		const FVector Velocity = ARRRandomJumpRamp::GetLandingVelocity(Start, End, Gravity, Time);
		const auto At = [&](float T) { return Start + Velocity * T - FVector(0.f, 0.f, 0.5f * Gravity * T * T); };
		const FCollisionShape Capsule = FCollisionShape::MakeCapsule(90.f, HalfHeight);
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(BRRampArc), false);
		for (int32 I = 0; I < 60; ++I)
		{
			if (Map.GetWorld()->SweepTestByChannel(At(Time * I / 60.f), At(Time * (I + 1) / 60.f), FQuat::Identity, ECC_Pawn, Capsule, Params)) { return false; }
		}
		return true;
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Districts

ERRDistrict RRBattle::PickDistrict(int32 X, int32 Y, int32 Reach, int32 Seed)
{
	const int32 Ring = FMath::Max(FMath::Abs(X), FMath::Abs(Y));
	if (Ring == 0)
	{
		return ERRDistrict::Plaza;
	}
	using D = ERRDistrict;
	// Each ring's mix (repeated as far as the ring goes, then shuffled for the seed): downtown and castles round the middle,
	// woods, hills and forts out toward the rim. A test map (one ring) gets one of every kind.
	static const D TestRing[] = { D::Downtown, D::Castle, D::Canal, D::Park, D::Hills, D::Fort, D::Downtown, D::Castle };
	static const D InnerRing[] = { D::Downtown, D::Castle, D::Downtown, D::Canal, D::Downtown, D::Fort, D::Downtown, D::Castle };
	static const D MiddleRing[] = { D::Downtown, D::Park, D::Castle, D::Canal, D::Downtown, D::Hills, D::Fort, D::Downtown,
		D::Park, D::Canal, D::Downtown, D::Castle, D::Hills, D::Park, D::Fort, D::Downtown };
	static const D OuterRing[] = { D::Park, D::Hills, D::Fort, D::Canal, D::Park, D::Hills, D::Downtown, D::Park, D::Fort, D::Hills,
		D::Canal, D::Castle, D::Park, D::Hills, D::Fort, D::Canal, D::Park, D::Hills, D::Downtown, D::Fort, D::Park, D::Canal, D::Hills, D::Castle };
	const D* Mix = Reach <= 1 ? TestRing : Ring == 1 ? InnerRing : Ring == 2 ? MiddleRing : OuterRing;
	const int32 MixCount = Reach <= 1 ? UE_ARRAY_COUNT(TestRing) : Ring == 1 ? UE_ARRAY_COUNT(InnerRing) : Ring == 2 ? UE_ARRAY_COUNT(MiddleRing) : UE_ARRAY_COUNT(OuterRing);

	// The ring's tiles in a fixed order, then the mix shuffled the same way on every machine
	TArray<FIntPoint> Cells;
	for (int32 TY = -Ring; TY <= Ring; ++TY)
	{
		for (int32 TX = -Ring; TX <= Ring; ++TX)
		{
			if (FMath::Max(FMath::Abs(TX), FMath::Abs(TY)) == Ring)
			{
				Cells.Add(FIntPoint(TX, TY));
			}
		}
	}
	TArray<D> Pool;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		Pool.Add(Mix[Index % MixCount]);
	}
	FRandomStream Shuffle(Seed * 31 + Ring * 977);
	for (int32 Index = Pool.Num() - 1; Index > 0; --Index)
	{
		Pool.Swap(Index, Shuffle.RandRange(0, Index));
	}
	const int32 Place = Cells.IndexOfByKey(FIntPoint(X, Y));
	return Pool.IsValidIndex(Place) ? Pool[Place] : D::Park;
}

const TCHAR* RRBattle::GetDistrictName(ERRDistrict District)
{
	switch (District)
	{
	case ERRDistrict::Plaza: return TEXT("plaza");
	case ERRDistrict::Downtown: return TEXT("downtown");
	case ERRDistrict::Castle: return TEXT("castle");
	case ERRDistrict::Canal: return TEXT("canal");
	case ERRDistrict::Park: return TEXT("park");
	case ERRDistrict::Hills: return TEXT("hills");
	case ERRDistrict::Fort: return TEXT("fort");
	default: return TEXT("?");
	}
}

bool RRBattle::IsOnBattleIsland(const FVector& Center, int32 Reach, const FVector& WorldLocation)
{
	const FVector2D Local(WorldLocation.X - Center.X, WorldLocation.Y - Center.Y);
	const float Half = (Reach * 2 + 1) * TileSize() * 0.5f;
	return Local.Size() < IslandRadiusAt(Half, FMath::Atan2(Local.Y, Local.X)) - 200.f;
}

// ---------------------------------------------------------------------------------------------------------------------
// The battle map

ARRRandomBattleMap::ARRRandomBattleMap()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ARRRandomBattleMap::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomBattleMap, Reach);
	DOREPLIFETIME(ARRRandomBattleMap, Seed);
}

ARRRandomBattleMap* ARRRandomBattleMap::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ARRRandomBattleMap> It(World);
	return It ? *It : nullptr;
}

void ARRRandomBattleMap::BeginPlay()
{
	Super::BeginPlay();
	GatherTiles();
	// (the server's: a client has no navigation, its bots are the server's)
	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	UE_CLOG(HasAuthority(), LogTemp, Log, TEXT("RRRandom: battle map navigation %s, made round invokers only: %s."), Nav ? *Nav->GetClass()->GetName() : TEXT("none"),
		Nav && Nav->IsActiveTilesGenerationEnabled() ? TEXT("yes") : TEXT("no (the whole map at once)"));
}

void ARRRandomBattleMap::SpawnTiles()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || Tiles.Num() > 0)
	{
		return;
	}
	// The map's seed: -RRBRSeed=N for a test, else a new one each game (the host's; replicated with the tiles' own)
	if (Seed == 0 && !FParse::Value(FCommandLine::Get(), TEXT("RRBRSeed="), Seed))
	{
		Seed = FMath::RandRange(1, 1 << 30);
	}
	// The level's floor under the map's middle: its height is the whole map's floor; hidden, no collision (the tiles'
	// ground is their own)
	float FloorZ = GetActorLocation().Z;
	if (UPrimitiveComponent* Floor = RRMapFloor::Find(this, 500.f, 3000.f, FloorZ))
	{
		Floor->SetVisibility(false);
		Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	const double StartTime = FPlatformTime::Seconds();
	const int32 Side = Reach * 2 + 1;
	Tiles.SetNum(Side * Side);
	for (int32 Y = -Reach; Y <= Reach; ++Y)
	{
		for (int32 X = -Reach; X <= Reach; ++X)
		{
			FRRBattleTile Tile;
			Tile.X = X;
			Tile.Y = Y;
			Tile.Reach = Reach;
			Tile.Cells = RRBattle::TileCells;
			Tile.District = uint8(RRBattle::PickDistrict(X, Y, Reach, Seed));
			Tile.Seed = int32(HashCombine(GetTypeHash(Seed), GetTypeHash(X * 131 + Y)) & 0x3fffffff);
			Tile.FloorZ = FloorZ;
			const FTransform Where(FVector(GetActorLocation().X + X * TileSize(), GetActorLocation().Y + Y * TileSize(), FloorZ));
			if (ARRRandomDiceTown* Town = World->SpawnActorDeferred<ARRRandomDiceTown>(ARRRandomDiceTown::StaticClass(), Where, this))
			{
				Town->SetBattleTile(Tile);
				Town->FinishSpawning(Where);
				Tiles[(Y + Reach) * Side + (X + Reach)] = Town;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: battle map seed %d, %d x %d tiles of %d cells (%.0f across), spawned in %.2f s."), Seed, Side, Side,
		RRBattle::TileCells, Side * TileSize(), FPlatformTime::Seconds() - StartTime);
}

void ARRRandomBattleMap::GatherTiles()
{
	const int32 Side = Reach * 2 + 1;
	if (Tiles.Num() == Side * Side && !Tiles.ContainsByPredicate([](const TWeakObjectPtr<ARRRandomDiceTown>& Tile) { return !Tile.IsValid(); }))
	{
		return;
	}
	Tiles.SetNum(Side * Side);
	for (TActorIterator<ARRRandomDiceTown> It(GetWorld()); It; ++It)
	{
		const FRRBattleTile& Tile = It->GetBattleTile();
		if (It->IsBattleTile() && FMath::Abs(Tile.X) <= Reach && FMath::Abs(Tile.Y) <= Reach)
		{
			Tiles[(Tile.Y + Reach) * Side + (Tile.X + Reach)] = *It;
		}
	}
}

bool ARRRandomBattleMap::IsBuilt() const
{
	const int32 Side = Reach * 2 + 1;
	if (Tiles.Num() != Side * Side)
	{
		return false;
	}
	for (const TWeakObjectPtr<ARRRandomDiceTown>& Tile : Tiles)
	{
		if (!Tile.IsValid() || !Tile->IsBuilt())
		{
			return false;
		}
	}
	return true;
}

ARRRandomDiceTown* ARRRandomBattleMap::GetTile(int32 X, int32 Y) const
{
	const int32 Side = Reach * 2 + 1;
	if (FMath::Abs(X) > Reach || FMath::Abs(Y) > Reach || Tiles.Num() != Side * Side)
	{
		return nullptr;
	}
	return Tiles[(Y + Reach) * Side + (X + Reach)].Get();
}

ARRRandomDiceTown* ARRRandomBattleMap::GetTileAt(const FVector2D& WorldLocation) const
{
	const int32 X = FMath::Clamp(FMath::RoundToInt((WorldLocation.X - GetActorLocation().X) / TileSize()), -Reach, Reach);
	const int32 Y = FMath::Clamp(FMath::RoundToInt((WorldLocation.Y - GetActorLocation().Y) / TileSize()), -Reach, Reach);
	return GetTile(X, Y);
}

FBox2D ARRRandomBattleMap::GetMapBounds() const
{
	const float Half = (Reach * 2 + 1) * TileSize() * 0.5f;
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	return FBox2D(Center - FVector2D(Half), Center + FVector2D(Half));
}

float ARRRandomBattleMap::GetIslandRadius() const
{
	return (Reach * 2 + 1) * TileSize() * 0.5f * IslandShare;
}

float ARRRandomBattleMap::GetFloorZ() const
{
	for (const TWeakObjectPtr<ARRRandomDiceTown>& Tile : Tiles)
	{
		if (Tile.IsValid() && Tile->IsBuilt())
		{
			return Tile->GetFloorZ();
		}
	}
	return GetActorLocation().Z;
}

FBox ARRRandomBattleMap::GetFlightArea() const
{
	// As high as the highest tile's own (its tallest structure, plus room over it)
	const FBox2D Map = GetMapBounds();
	const float FloorZ = GetFloorZ();
	float Top = FloorZ + 3000.f;
	for (const TWeakObjectPtr<ARRRandomDiceTown>& Tile : Tiles)
	{
		if (Tile.IsValid() && Tile->IsBuilt())
		{
			Top = FMath::Max(Top, float(Tile->GetTileFlightTop()));
		}
	}
	return FBox(FVector(Map.Min, FloorZ - 1000.f), FVector(Map.Max, Top));
}

bool ARRRandomBattleMap::FindOpenGround(const FVector2D& Want, float Clear, float MaxReach, FVector& OutSpot) const
{
	const ARRRandomDiceTown* Tile = GetTileAt(Want);
	return Tile && ARRRandomCannon::FindOpenGround(GetWorld(), Tile, Want, Clear, OutSpot, [](const FVector&) { return true; }, MaxReach);
}

void ARRRandomBattleMap::GetTeamStarts(int32 TeamCount, TArray<FVector>& OutStarts) const
{
	OutStarts.Reset();
	// Evenly round a ring, turned at random each game; each start the nearest open ground to its spot (roads count)
	FRandomStream Turn(Seed + 77);
	const float Offset = Turn.FRandRange(0.f, UE_TWO_PI);
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	const float Half = (Reach * 2 + 1) * TileSize() * 0.5f;
	for (int32 Team = 0; Team < TeamCount; ++Team)
	{
		const float Theta = Offset + UE_TWO_PI * Team / FMath::Max(1, TeamCount);
		const FVector2D Want = Center + FVector2D(FMath::Cos(Theta), FMath::Sin(Theta)) * IslandRadiusAt(Half, Theta) * StartRingShare;
		FVector Spot;
		if (!FindOpenGround(Want, StartClear, 3500.f, Spot))
		{
			Spot = FVector(Want, GetFloorZ());
			UE_LOG(LogTemp, Warning, TEXT("RRRandom: no open ground for team %d's start near %s."), Team, *Want.ToString());
		}
		OutStarts.Add(Spot);
	}
	FString List;
	for (int32 Team = 0; Team < OutStarts.Num(); ++Team)
	{
		List += FString::Printf(TEXT(" %d:(%.0f, %.0f)"), Team, OutStarts[Team].X - Center.X, OutStarts[Team].Y - Center.Y);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: battle map team starts:%s."), *List);
}

void ARRRandomBattleMap::GetStartSpots(TArray<FVector>& OutSpots) const
{
	OutSpots.Reset();
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	for (int32 Y = -Reach; Y <= Reach; ++Y)
	{
		for (int32 X = -Reach; X <= Reach; ++X)
		{
			if (X == 0 && Y == 0)
			{
				continue;
			}
			const FVector2D Want = Center + FVector2D(X, Y) * TileSize();
			FVector Spot;
			if (RRBattle::IsOnBattleIsland(GetActorLocation(), Reach, FVector(Want, 0.f)) && FindOpenGround(Want, StartClear, 3000.f, Spot)
				&& RRBattle::IsOnBattleIsland(GetActorLocation(), Reach, Spot))
			{
				OutSpots.Add(Spot);
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: battle map start spots: %d."), OutSpots.Num());
}

void ARRRandomBattleMap::GetPlazaStarts(int32 TeamCount, TArray<FVector>& OutStarts) const
{
	OutStarts.Reset();
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	for (int32 Team = 0; Team < TeamCount; ++Team)
	{
		const float Theta = UE_TWO_PI * Team / FMath::Max(1, TeamCount);
		const FVector2D Want = Center + FVector2D(FMath::Cos(Theta), FMath::Sin(Theta)) * PlazaStartRadius;
		FVector Spot;
		if (!FindOpenGround(Want, 220.f, 700.f, Spot))
		{
			Spot = FVector(Want, GetFloorZ());
		}
		OutStarts.Add(Spot);
	}
}

void ARRRandomBattleMap::SpawnHighRoutes(int32 DesiredEach, int32& OutCannons, int32& OutRamps, int32& OutBikes)
{
	struct FLanding { FVector Spot; const ARRRandomDiceTown* Tile; bool bHill; };
	TArray<FLanding> Landings;
	for (const TWeakObjectPtr<ARRRandomDiceTown>& WeakTile : Tiles)
	{
		const ARRRandomDiceTown* Tile = WeakTile.Get();
		if (!Tile) { continue; }
		for (int32 Kind = 0; Kind < 2; ++Kind)
		{
			for (const FVector& Anchor : Kind == 0 ? Tile->GetHillTops() : Tile->GetBigBuildingSpots())
			{
				FVector Spot;
				if (FindRaisedLanding(*this, Anchor, Spot)
					&& !Landings.ContainsByPredicate([&](const FLanding& Other) { return FVector::Dist2D(Spot, Other.Spot) < 1000.f; }))
				{
					Landings.Add({Spot, Tile, Kind == 0});
				}
			}
		}
	}
	FRandomStream Order(Seed + 177013);
	for (int32 I = Landings.Num() - 1; I > 0; --I) { Landings.Swap(I, Order.RandRange(0, I)); }
	TArray<FVector> UsedStarts, UsedLandings;
	for (int32 Type = 0; Type < 2; ++Type)
	{
		TSet<const ARRRandomDiceTown*> UsedTiles;
		int32& Count = Type == 0 ? OutCannons : OutRamps;
		for (int32 Kind = 0; Kind < 2; ++Kind)
		{
			const int32 Want = Kind == 0 ? FMath::DivideAndRoundUp(DesiredEach, 2) : DesiredEach;
			for (const FLanding& Landing : Landings)
			{
				if (Count >= Want) { break; }
				if (Landing.bHill != (Kind == 0) || UsedTiles.Contains(Landing.Tile)
					|| UsedLandings.ContainsByPredicate([&](const FVector& Used) { return FVector::Dist2D(Used, Landing.Spot) < 1000.f; })) { continue; }
				bool bPlaced = false;
				const int32 Turn = Order.RandRange(0, 3);
				for (float Distance : {2600.f, 3400.f, 4200.f})
				{
					for (int32 Direction = 0; Direction < 8 && !bPlaced; ++Direction)
					{
						const float Yaw = 90.f * (Direction % 4 + Turn) + (Direction >= 4 ? 45.f : 0.f);
						const FVector WantFrom = Landing.Spot - FRotator(0.f, Yaw, 0.f).Vector() * Distance;
						FVector From;
						if (!FindOpenGround(FVector2D(WantFrom), 300.f, 400.f, From)
							|| !RRBattle::IsOnBattleIsland(GetActorLocation(), Reach, From)
							|| UsedStarts.ContainsByPredicate([&](const FVector& Used) { return FVector::Dist2D(Used, From) < 2200.f; })) { continue; }
						const FVector Forward = (Landing.Spot - From).GetSafeNormal2D();
						const FTransform Transform(Forward.Rotation(), From);
						AActor* Route = nullptr;
						if (Type == 0)
						{
							if (!ARRRandomCannon::IsLaunchClear(GetWorld(), From, Landing.Spot)) { continue; }
							Route = ARRRandomCannon::SpawnSet(GetWorld(), From, Landing.Spot, OutCannons);
						}
						else
						{
							constexpr float RampLength = 500.f;
							if (!IsRampRouteClear(*this, From, Landing.Spot, Forward, RampLength)) { continue; }
							ARRRandomJumpRamp* Ramp = GetWorld()->SpawnActorDeferred<ARRRandomJumpRamp>(ARRRandomJumpRamp::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
							if (!Ramp) { continue; }
							Ramp->SetupLanding(RampLength, Landing.Spot);
							Ramp->FinishSpawning(Transform);
							Route = Ramp;
							const FTransform BikeAt(Forward.Rotation(), From - Forward * 750.f);
							if (ARRRandomVehicle* Bike = GetWorld()->SpawnActorDeferred<ARRRandomVehicle>(ARRRandomVehicle::StaticClass(), BikeAt, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
							{
								Bike->SetKind(ERRVehicleKind::Motorbike);
								Bike->Tags.Add(TEXT("BR_RampBike"));
								Bike->FinishSpawning(BikeAt);
								++OutBikes;
							}
						}
						if (!Route) { continue; }
						Route->Tags.Add(Type == 0 ? FName(TEXT("BR_HighCannon")) : FName(TEXT("BR_HighRamp")));
						Route->Tags.Add(Landing.bHill ? FName(TEXT("BR_Hill")) : FName(TEXT("BR_Roof")));
						UsedStarts.Add(From);
						UsedLandings.Add(Landing.Spot);
						UsedTiles.Add(Landing.Tile);
						UE_LOG(LogTemp, Log, TEXT("RRRandom: BR high route %s %d to %s tile (%d,%d): %s -> %s, gain %.0f."),
							Type == 0 ? TEXT("cannon") : TEXT("ramp"), Count, Landing.bHill ? TEXT("hill") : TEXT("roof"),
							Landing.Tile->GetBattleTile().X, Landing.Tile->GetBattleTile().Y, *From.ToCompactString(), *Landing.Spot.ToCompactString(), Landing.Spot.Z - From.Z);
						++Count;
						bPlaced = true;
					}
					if (bPlaced) { break; }
				}
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: BR elevated access: %d clear destinations, %d cannons, %d ramps, %d approach bikes."), Landings.Num(), OutCannons, OutRamps, OutBikes);
}

void ARRRandomBattleMap::SpawnRides()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || !IsBuilt())
	{
		return;
	}
	// How much ground there is, in tiles (the rim cuts the outer ones), and a stream for where things go
	const float Half = (Reach * 2 + 1) * TileSize() * 0.5f;
	const float GroundTiles = PI * FMath::Square(Half * IslandShare) * 0.93f / FMath::Square(TileSize());
	FRandomStream Spots(Seed + 4242);
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	auto RandomSpot = [&]()
	{
		for (int32 Try = 0; Try < 50; ++Try)
		{
			const FVector2D Spot = Center + FVector2D(Spots.FRandRange(-Half, Half), Spots.FRandRange(-Half, Half));
			if (RRBattle::IsOnBattleIsland(GetActorLocation(), Reach, FVector(Spot, 0.f)))
			{
				return Spot;
			}
		}
		return Center;
	};

	// Give high ground deliberate approaches before scattering the remaining rides.
	int32 Cannons = 0, Ramps = 0, Rides = 0;
	SpawnHighRoutes(FMath::Max(2, FMath::RoundToInt(GroundTiles * 0.24f)), Cannons, Ramps, Rides);
	// Kickboards, motorbikes and cars along the avenues and streets
	struct FKindCount
	{
		ERRVehicleKind Kind;
		float PerTile;
	};
	for (const FKindCount& Count : { FKindCount{ ERRVehicleKind::Motorbike, BikesPerTile }, FKindCount{ ERRVehicleKind::Kickboard, KickboardsPerTile }, FKindCount{ ERRVehicleKind::Car, CarsPerTile } })
	{
		const int32 Want = FMath::Max(1, FMath::RoundToInt(Count.PerTile * GroundTiles));
		for (int32 Index = 0; Index < Want; ++Index)
		{
			FVector Spot;
			if (!FindOpenGround(RandomSpot(), 260.f, 2500.f, Spot))
			{
				continue;
			}
			const FTransform Where(FRotator(0.f, 90.f * Spots.RandRange(0, 3), 0.f), Spot);
			if (ARRRandomVehicle* Vehicle = World->SpawnActorDeferred<ARRRandomVehicle>(ARRRandomVehicle::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Vehicle->SetKind(Count.Kind);
				Vehicle->FinishSpawning(Where);
				++Rides;
			}
		}
	}

	// Human cannons: one-way, toward the middle (where the ring of safe ground ends up, sooner or later)
	const int32 WantCannons = FMath::Max(2, FMath::RoundToInt(CannonsPerTile * GroundTiles));
	for (int32 Try = 0; Try < WantCannons * 4 && Cannons < WantCannons; ++Try)
	{
		const FVector2D From = RandomSpot();
		const FVector2D Toward = (Center - From).GetSafeNormal();
		if (Toward.IsNearlyZero() || FVector2D::Distance(From, Center) < CannonShot * 0.6f)
		{
			continue;
		}
		if (ARRRandomCannon::SpawnBattleSet(World, this, From, From + Toward * FMath::Min(CannonShot, FVector2D::Distance(From, Center)), Cannons))
		{
			++Cannons;
		}
	}

	// Plane stations
	int32 Stations = 0;
	const int32 WantStations = FMath::Max(1, FMath::RoundToInt(StationsPerTile * GroundTiles));
	for (int32 Try = 0; Try < WantStations * 4 && Stations < WantStations; ++Try)
	{
		FVector Spot;
		if (!FindOpenGround(RandomSpot(), 520.f, 2500.f, Spot))
		{
			continue;
		}
		const FTransform Where(FRotator(0.f, FVector(Center - FVector2D(Spot), 0.f).Rotation().Yaw, 0.f), Spot);
		World->SpawnActor<ARRRandomPlaneStation>(ARRRandomPlaneStation::StaticClass(), Where);
		++Stations;
	}

	// Star robot sets, any team's: three cars parked together, ready to combine (ARRRandomStarRobot)
	int32 StarSets = 0;
	const int32 WantStarSets = FMath::Max(1, FMath::RoundToInt(StarSetsPerTile * GroundTiles));
	for (int32 Try = 0; Try < WantStarSets * 6 && StarSets < WantStarSets; ++Try)
	{
		FVector Spot;
		if (!FindOpenGround(RandomSpot(), 900.f, 3000.f, Spot))
		{
			continue;
		}
		const float Yaw = 90.f * Spots.RandRange(0, 3);
		const FVector Ahead = FRotator(0.f, Yaw, 0.f).Vector();
		const FVector Side = FRotator(0.f, Yaw + 90.f, 0.f).Vector();
		const TPair<ERRVehicleKind, FVector> Parts[] = { { ERRVehicleKind::StarChest, Side * -500.f }, { ERRVehicleKind::StarArms, Side * 500.f }, { ERRVehicleKind::StarLegs, Ahead * 420.f } };
		for (const TPair<ERRVehicleKind, FVector>& Part : Parts)
		{
			const FTransform Where(FRotator(0.f, Yaw, 0.f), Spot + Part.Value);
			if (ARRRandomVehicle* Vehicle = World->SpawnActorDeferred<ARRRandomVehicle>(ARRRandomVehicle::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Vehicle->SetKind(Part.Key);
				Vehicle->SetStarTeam(RRBattle::AnyTeam);
				Vehicle->FinishSpawning(Where);
			}
		}
		++StarSets;
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: battle map rides (%.1f tiles of ground): %d kickboards/motorbikes/cars, %d cannons, %d elevated jump ramps, %d plane stations, %d star robot sets."),
		GroundTiles, Rides, Cannons, Ramps, Stations, StarSets);
}

// ---------------------------------------------------------------------------------------------------------------------
// Streaming

void ARRRandomBattleMap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	GatherTiles();
	StreamTimer -= DeltaSeconds;
	if (StreamTimer <= 0.f)
	{
		StreamTimer = StreamInterval;
		UpdateStreaming();
	}
	LogTimer -= DeltaSeconds;
	if (LogTimer <= 0.f && CVarLogStreaming.GetValueOnGameThread() != 0)
	{
		LogTimer = 10.f;
		LogStreaming();
	}
}

bool ARRRandomBattleMap::GetViewLocation(FVector& OutLocation) const
{
	const APlayerController* Player = GetWorld()->GetFirstPlayerController();
	if (!Player || !Player->IsLocalController())
	{
		return false;
	}
	if (Player->PlayerCameraManager)
	{
		OutLocation = Player->PlayerCameraManager->GetCameraLocation();
		return true;
	}
	if (const APawn* Pawn = Player->GetPawn())
	{
		OutLocation = Pawn->GetActorLocation();
		return true;
	}
	return false;
}

void ARRRandomBattleMap::UpdateStreaming()
{
	FVector View;
	// (a dedicated server draws nothing: every tile stays on its far look, out of the scene)
	if (GetNetMode() == NM_DedicatedServer || !GetViewLocation(View))
	{
		return;
	}
	const bool bAll = CVarStreamingOff.GetValueOnGameThread() != 0;
	const float Radius = CVarDetailRadius.GetValueOnGameThread();
	// Nearest first, so a jump across the map shows the ground underfoot before the rest
	TArray<TPair<float, ARRRandomDiceTown*>> Wanted;
	for (const TWeakObjectPtr<ARRRandomDiceTown>& Weak : Tiles)
	{
		ARRRandomDiceTown* Tile = Weak.Get();
		if (!Tile || !Tile->IsBuilt())
		{
			continue;
		}
		const float Distance = FMath::Sqrt(Tile->GetGroundBounds().ComputeSquaredDistanceToPoint(FVector2D(View.X, View.Y)));
		const bool bShow = bAll || Distance < (Tile->IsDetailShown() ? Radius + RRBattle::DetailSlack : Radius);
		if (bShow != Tile->IsDetailShown())
		{
			Wanted.Add({ bShow ? Distance : -Distance, Tile });
		}
	}
	Wanted.Sort([](const TPair<float, ARRRandomDiceTown*>& A, const TPair<float, ARRRandomDiceTown*>& B) { return FMath::Abs(A.Key) < FMath::Abs(B.Key); });
	for (int32 Index = 0; Index < Wanted.Num() && Index < MaxSwitchesPerUpdate; ++Index)
	{
		Wanted[Index].Value->SetDetailShown(!Wanted[Index].Value->IsDetailShown());
	}
	int32 Shown = 0;
	for (const TWeakObjectPtr<ARRRandomDiceTown>& Weak : Tiles)
	{
		Shown += Weak.IsValid() && Weak->IsDetailShown() ? 1 : 0;
	}
	if (Shown != LastShown && Wanted.Num() <= MaxSwitchesPerUpdate)
	{
		LastShown = Shown;
		if (CVarLogStreaming.GetValueOnGameThread() != 0)
		{
			LogStreaming();
		}
	}
}

void ARRRandomBattleMap::LogStreaming() const
{
	int32 Shown = 0;
	int32 Detail = 0;
	int32 Far = 0;
	int32 AllDetail = 0;
	TArray<FString> Names;
	for (const TWeakObjectPtr<ARRRandomDiceTown>& Weak : Tiles)
	{
		const ARRRandomDiceTown* Tile = Weak.Get();
		if (!Tile || !Tile->IsBuilt())
		{
			continue;
		}
		AllDetail += Tile->GetDetailInstanceCount();
		if (Tile->IsDetailShown())
		{
			++Shown;
			Detail += Tile->GetDetailInstanceCount();
			Names.Add(FString::Printf(TEXT("(%d,%d)"), Tile->GetBattleTile().X, Tile->GetBattleTile().Y));
		}
		else
		{
			Far += Tile->GetFarInstanceCount();
		}
	}
	FVector View = FVector::ZeroVector;
	GetViewLocation(View);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: battle map streaming at %s: %d of %d tiles up close %s, %d detail instances drawn (of %d), %d far look blocks."),
		*FVector2D(View.X - GetActorLocation().X, View.Y - GetActorLocation().Y).ToString(), Shown, Tiles.Num(), *FString::Join(Names, TEXT(" ")), Detail, AllDetail, Far);
}

// ---------------------------------------------------------------------------------------------------------------------
// The navigation system of the battle levels

URRRandomNavigationSystem::URRRandomNavigationSystem(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Made only round the invokers (every animal friend, ARRRandomCharacter::BeginPlay), as they go
	bGenerateNavigationOnlyAroundNavigationInvokers = true;
	ActiveTilesUpdateInterval = 0.5f;
}

// ---------------------------------------------------------------------------------------------------------------------
// The ring of safe ground

namespace
{
	/** A phase: the wait before the ring closes, how long it takes to close, how big the new ring is (a share of the first), the hurt outside. */
	struct FZonePhase
	{
		float Wait;
		float Close;
		float RadiusShare;
		float Damage;
	};
	const FZonePhase ZonePhases[] =
	{
		{ 90.f, 60.f, 0.60f, 2.f },
		{ 60.f, 50.f, 0.38f, 3.f },
		{ 50.f, 40.f, 0.22f, 5.f },
		{ 40.f, 35.f, 0.12f, 8.f },
		{ 30.f, 30.f, 0.05f, 12.f },
		{ 20.f, 30.f, 0.f, 20.f },
	};
	// The test map's phases go this much quicker (-RRZoneSpeed=N overrides it)
	constexpr float TestZoneSpeed = 2.f;
	// Hurt outside once a second
	constexpr float HurtInterval = 1.f;
	// The cotton candy cloud bank round the ring: rows of puffs, each row this high up and its puffs this big across
	// (spaced at most this many to a row)
	struct FPuffRow
	{
		float Height;
		float Size;
	};
	const FPuffRow PuffRows[] = { { 150.f, 1700.f }, { 1100.f, 1600.f }, { 2100.f, 1500.f }, { 3100.f, 1350.f }, { 4000.f, 1100.f } };
	constexpr int32 MaxPuffsPerRow = 240;
	/** 0 .. 1, the same for the same numbers (the puffs' wobble, the same on every machine). */
	float Jitter(int32 A, int32 B)
	{
		const uint32 Hash = (uint32(A) * 73856093u) ^ (uint32(B) * 19349663u) ^ 0x5bd1e995u;
		return float((Hash * 2654435761u) >> 8 & 0xFFFF) / 65535.f;
	}
}

ARRRandomBattleZone::ARRRandomBattleZone()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
}

void ARRRandomBattleZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomBattleZone, FromCenter);
	DOREPLIFETIME(ARRRandomBattleZone, FromRadius);
	DOREPLIFETIME(ARRRandomBattleZone, NextCenter);
	DOREPLIFETIME(ARRRandomBattleZone, NextRadius);
	DOREPLIFETIME(ARRRandomBattleZone, CloseStart);
	DOREPLIFETIME(ARRRandomBattleZone, CloseEnd);
	DOREPLIFETIME(ARRRandomBattleZone, Phase);
	DOREPLIFETIME(ARRRandomBattleZone, FloorZ);
	DOREPLIFETIME(ARRRandomBattleZone, TimeScale);
}

ARRRandomBattleZone* ARRRandomBattleZone::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	// Asked for by every animal friend's walk speed each frame: kept once found
	static TWeakObjectPtr<ARRRandomBattleZone> Found;
	if (Found.IsValid() && Found->GetWorld() == World)
	{
		return Found.Get();
	}
	TActorIterator<ARRRandomBattleZone> It(World);
	Found = It ? *It : nullptr;
	return Found.Get();
}

int32 ARRRandomBattleZone::GetPhaseCount()
{
	return UE_ARRAY_COUNT(ZonePhases);
}

void ARRRandomBattleZone::Begin(const FVector2D& InCenter, float InRadius, float StartTime, float InFloorZ, bool bTestMap)
{
	Stream.GenerateNewSeed();
	FirstRadius = InRadius;
	FromCenter = InCenter;
	FromRadius = InRadius;
	NextCenter = InCenter;
	NextRadius = InRadius;
	FloorZ = InFloorZ;
	TimeScale = bTestMap ? TestZoneSpeed : 1.f;
	FParse::Value(FCommandLine::Get(), TEXT("RRZoneSpeed="), TimeScale);
	TimeScale = FMath::Max(0.1f, TimeScale);
	StartPhase(1, StartTime);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: battle zone %.0f round %s, %d phases, x%.1f speed."), InRadius, *InCenter.ToString(), GetPhaseCount(), TimeScale);
}

void ARRRandomBattleZone::StartPhase(int32 InPhase, float Now)
{
	// From where the ring is now, to a smaller one wholly inside it (its middle anywhere that keeps it inside)
	const FVector2D CurrentCenter = GetCenter();
	const float CurrentRadius = GetRadius();
	// The ring has just closed in (phase InPhase - 1): the oracle dice roll and meteors fall on the lowest team (round 93)
	if (HasAuthority() && InPhase >= 2)
	{
		ARRRandomOracleStorm::RollFor(this, InPhase - 1, CurrentCenter, CurrentRadius);
	}
	Phase = InPhase;
	FromCenter = CurrentCenter;
	FromRadius = CurrentRadius;
	if (Phase > GetPhaseCount())
	{
		NextCenter = CurrentCenter;
		NextRadius = CurrentRadius;
		CloseStart = CloseEnd = Now;
		return;
	}
	const FZonePhase& Next = ZonePhases[Phase - 1];
	NextRadius = FirstRadius * Next.RadiusShare;
	const float Room = FMath::Max(0.f, CurrentRadius - NextRadius);
	const float Angle = Stream.FRandRange(0.f, UE_TWO_PI);
	NextCenter = CurrentCenter + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Room * FMath::Sqrt(Stream.FRand()) * 0.85f;
	CloseStart = Now + Next.Wait / TimeScale;
	CloseEnd = CloseStart + Next.Close / TimeScale;
	UE_LOG(LogTemp, Log, TEXT("RRRandom: battle zone phase %d: closes in %.0f s to %.0f round %s (over %.0f s)."), Phase, Next.Wait / TimeScale, NextRadius,
		*NextCenter.ToString(), Next.Close / TimeScale);
}

float ARRRandomBattleZone::GetNow() const
{
	const AGameStateBase* State = GetWorld()->GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
}

FVector2D ARRRandomBattleZone::GetCenter() const
{
	const float Now = GetNow();
	const float Alpha = CloseEnd > CloseStart ? FMath::Clamp((Now - CloseStart) / (CloseEnd - CloseStart), 0.f, 1.f) : (Now >= CloseEnd ? 1.f : 0.f);
	return FMath::Lerp(FromCenter, NextCenter, Alpha);
}

float ARRRandomBattleZone::GetRadius() const
{
	const float Now = GetNow();
	const float Alpha = CloseEnd > CloseStart ? FMath::Clamp((Now - CloseStart) / (CloseEnd - CloseStart), 0.f, 1.f) : (Now >= CloseEnd ? 1.f : 0.f);
	return FMath::Lerp(FromRadius, NextRadius, Alpha);
}

bool ARRRandomBattleZone::IsInside(const FVector& WorldLocation) const
{
	return Phase > 0 && FVector2D::Distance(FVector2D(WorldLocation.X, WorldLocation.Y), GetCenter()) <= GetRadius();
}

bool ARRRandomBattleZone::IsClosing() const
{
	const float Now = GetNow();
	return Now >= CloseStart && Now < CloseEnd;
}

float ARRRandomBattleZone::GetSecondsToChange() const
{
	const float Now = GetNow();
	return Now < CloseStart ? CloseStart - Now : FMath::Max(0.f, CloseEnd - Now);
}

float ARRRandomBattleZone::GetDamagePerSecond() const
{
	return ZonePhases[FMath::Clamp(Phase, 1, GetPhaseCount()) - 1].Damage;
}

float ARRRandomBattleZone::GetCloudShare(const FVector& WorldLocation) const
{
	if (Phase <= 0)
	{
		return 0.f;
	}
	const float Beyond = FVector2D::Distance(FVector2D(WorldLocation.X, WorldLocation.Y), GetCenter()) - GetRadius();
	return FMath::Clamp(Beyond / CloudDepth, 0.f, 1.f);
}

float ARRRandomBattleZone::GetDamagePerSecondAt(const FVector& WorldLocation) const
{
	return GetDamagePerSecond() * (1.f + CloudHurtBoost * GetCloudShare(WorldLocation));
}

float ARRRandomBattleZone::GetSpeedShareAt(const FVector& WorldLocation) const
{
	return IsInside(WorldLocation) ? 1.f : 1.f - CloudSlow * GetCloudShare(WorldLocation);
}

void ARRRandomBattleZone::SkipAhead()
{
	if (!HasAuthority())
	{
		return;
	}
	// The next closing starts now (or, closing already, the ring is done at once and the next phase waits)
	const float Now = GetNow();
	if (Now < CloseStart)
	{
		const float Length = CloseEnd - CloseStart;
		CloseStart = Now;
		CloseEnd = Now + Length;
	}
	else
	{
		CloseEnd = Now;
		StartPhase(Phase + 1, Now);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, the battle zone skips ahead (phase %d)."), Phase);
}

void ARRRandomBattleZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (Phase <= 0)
	{
		return;
	}
	if (HasAuthority())
	{
		// Closed: on to the next phase
		if (Phase <= GetPhaseCount() && GetNow() >= CloseEnd)
		{
			StartPhase(Phase + 1, GetNow());
		}
		HurtTimer -= DeltaSeconds;
		if (HurtTimer <= 0.f)
		{
			HurtTimer = HurtInterval;
			HurtOutside();
		}
	}
	UpdateWall();
}

void ARRRandomBattleZone::HurtOutside()
{
	if (ARRRandomGameState::IsAttackLocked(this))
	{
		return;
	}
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Who = *It;
		if (!Who->IsAlive() || Who->IsDecoy() || IsInside(Who->GetActorLocation()))
		{
			continue;
		}
		// Deeper in the cotton candy cloud, the more it hurts
		const float Damage = GetDamagePerSecondAt(Who->GetActorLocation()) * HurtInterval;
		UGameplayStatics::ApplyDamage(Who, Damage, nullptr, this, UDamageType::StaticClass());
	}
}

void ARRRandomBattleZone::UpdateWall()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (!Wall)
	{
		// Round puffs (the engine sphere: not Nanite, which can't draw see-through materials, unlike Dice Town's puff)
		// heaped in rows round the ring, in the cloak's glowing see-through material in cotton candy pink (brighter at
		// the rims): a soft bank of cloud you can still see through
		UStaticMesh* Puff = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/DiceFX/M_FXCloak.M_FXCloak"));
		if (!Puff)
		{
			return;
		}
		Wall = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Wall->SetStaticMesh(Puff);
		PuffSize = FMath::Max(1.f, Puff->GetBounds().BoxExtent.GetMax() * 2.f);
		Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Wall->SetCastShadow(false);
		Wall->SetupAttachment(RootComponent);
		Wall->SetUsingAbsoluteLocation(true);
		Wall->SetUsingAbsoluteRotation(true);
		Wall->SetUsingAbsoluteScale(true);
		Wall->RegisterComponent();
		Wall->SetWorldTransform(FTransform::Identity);
		if (Glass)
		{
			WallMaterial = UMaterialInstanceDynamic::Create(Glass, this);
			WallMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(FColor(255, 150, 200)));
			WallMaterial->SetScalarParameterValue(TEXT("Glow"), 1.8f);
			WallMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.2f);
			WallMaterial->SetScalarParameterValue(TEXT("Fill"), 1.1f);
			for (int32 Slot = 0; Slot < Wall->GetNumMaterials(); ++Slot)
			{
				Wall->SetMaterial(Slot, WallMaterial);
			}
		}
	}
	// Moved only when the ring has (closing, or the first time)
	const FVector2D Center = GetCenter();
	const float Radius = FMath::Max(GetRadius(), 50.f);
	if (FVector2D::Distance(Center, WallCenter) < 5.f && FMath::Abs(Radius - WallRadius) < 5.f)
	{
		return;
	}
	WallCenter = Center;
	WallRadius = Radius;
	// Each row: puffs overlapping round the ring (at least 12), each a little bigger or smaller, higher or lower, in or
	// out than the next, the rows offset from each other; squashed a little, like heaped cotton candy
	TArray<FTransform> Transforms;
	for (int32 Row = 0; Row < UE_ARRAY_COUNT(PuffRows); ++Row)
	{
		const FPuffRow& Line = PuffRows[Row];
		const int32 Count = FMath::Clamp(FMath::RoundToInt(UE_TWO_PI * Radius / (Line.Size * 0.45f)), 12, MaxPuffsPerRow);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Angle = UE_TWO_PI * (Index + 0.5f * (Row % 2)) / Count;
			const float Size = Line.Size * (0.8f + 0.4f * Jitter(Row, Index));
			const float Out = Radius + (Jitter(Index, Row + 7) - 0.5f) * 300.f;
			const FVector Where(Center.X + FMath::Cos(Angle) * Out, Center.Y + FMath::Sin(Angle) * Out, FloorZ + Line.Height + (Jitter(Row + 3, Index) - 0.5f) * 400.f);
			const float Scale = Size / PuffSize;
			Transforms.Add(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle) + Jitter(Index, 11) * 60.f, 0.f), Where, FVector(Scale, Scale, Scale * 0.8f)));
		}
	}
	if (Wall->GetInstanceCount() == Transforms.Num())
	{
		Wall->BatchUpdateInstancesTransforms(0, Transforms, true, true);
	}
	else
	{
		Wall->ClearInstances();
		Wall->AddInstances(Transforms, false, true);
	}
}
