#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NavigationSystem.h"
#include "RRRandomBattleRoyale.generated.h"

class ARRRandomDiceTown;
class ARRRandomCharacter;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Round 92: the battle royale (ERRMatchRule::BattleRoyale). Up to ten teams of three animal friends on one big island; every
 * spot nobody plays is a bot. Nobody gets back up once knocked out; a ring of safe ground shrinks round a random spot, and
 * standing outside it hurts. The last team standing wins.
 *
 * The map (ARRRandomBattleMap) is Dice Town's style twenty times its ground: a square of town tiles (ARRRandomDiceTown in
 * battle tile mode, FRRBattleTile), each with a district of its own (ERRDistrict), avenues along their edges, streets
 * across their middles. Streaming: each machine shows a tile's blocks and models only while its view is near it; further
 * off a tile shows its far look (its rooftops in big blocks: the LOD). The navmesh is made only round the animal friends
 * (navigation invokers), as they go (URRRandomNavigationSystem on the battle levels).
 */
namespace RRBattle
{
	/** Cells along a tile's side (100 each): 10,800 units. */
	constexpr int32 TileCells = 108;
	/** Tiles each way from the middle one: 7 x 7 tiles, 75,600 units across (the island in it about 20 times Dice Town's ground). */
	constexpr int32 TileReach = 3;
	/** The test level's map (/Game/Maps/BRTest, -RRBRTiles=1): 3 x 3 tiles, the same rules. */
	constexpr int32 TestTileReach = 1;
	/** Teams, and animal friends on each. */
	constexpr int32 TeamCount = 10;
	constexpr int32 TeamSize = 3;
	/** The test level's teams (fewer: a quicker game on the smaller map). */
	constexpr int32 TestTeamCount = 4;
	/** A star car any team may combine (ARRRandomVehicle::SetStarTeam): the battle map's sets. */
	constexpr int32 AnyTeam = -1;
	/** Roads, in cells: avenues reach this far in from each tile edge (8 wide across two tiles), streets this far either side of a tile's middle. */
	constexpr int32 AvenueHalf = 4;
	constexpr float StreetHalf = 3.f;
	/** The castle district's wall (Chebyshev cells from the tile's middle) and the plaza's paving. */
	constexpr float CastleIn = 22.f;
	constexpr float CastleOut = 25.f;
	constexpr int32 CastleWallLevels = 5;
	constexpr float PlazaEdge = 14.f;
	/**
	 * Streaming: a tile shows its detail while this machine's view is within this of its square (in 2D), and drops it past
	 * DetailRadius + DetailSlack (no flicker at the edge).
	 */
	constexpr float DetailRadius = 9000.f;
	constexpr float DetailSlack = 1500.f;
	/** Navigation invokers on every animal friend: the navmesh is made this far round each, and dropped past the second. */
	constexpr float NavGenerateRadius = 6500.f;
	constexpr float NavRemoveRadius = 9000.f;
}

/** What stands on a battle map tile. */
enum class ERRDistrict : uint8
{
	/** The middle: a plaza round a giant die, downtown round it. */
	Plaza,
	/** Two- and one-story houses and shops close together, watchtowers, crates and walls between: street fighting. */
	Downtown,
	/** A walled castle (battlements, a walkway to shoot from, gateways on its four streets) round a keep, cottages outside. */
	Castle,
	/** Canals with footbridges, houses along them. */
	Canal,
	/** A wood of trees (Tripo candy trees, mushrooms and lollipops), a mountain of giant dice, cottages. */
	Park,
	/** Stepped hills with watchtowers on top, cottages and trees. */
	Hills,
	/** Walled compounds: courtyards with low walls, gateways, crates and a watchtower at the corners. */
	Fort,
	Count
};

namespace RRBattle
{
	/** The district of a tile on a map Reach tiles each way, the same on every machine for a seed. */
	ERRDistrict PickDistrict(int32 X, int32 Y, int32 Reach, int32 Seed);
	const TCHAR* GetDistrictName(ERRDistrict District);
	/** Whether a world spot is on the battle map's island (round, about as big as its square). */
	bool IsOnBattleIsland(const FVector& Center, int32 Reach, const FVector& WorldLocation);
}

/**
 * The battle royale's map: lays out the square of town tiles round itself (the server spawns them, they replicate, every
 * machine builds the same), streams their detail by distance from this machine's view (ARRRandomDiceTown::SetDetailShown),
 * and answers for the whole map (its bounds, where flying is allowed). Placed in /Game/Maps/BattleRoyale and BRTest
 * (char-pipeline/scripts/ue_make_tps_levels.py); spawned by the game mode on a bare level otherwise.
 */
UCLASS()
class RRRANDOM_API ARRRandomBattleMap : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomBattleMap();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** The world's battle map, or null. */
	static ARRRandomBattleMap* Get(const UObject* WorldContext);

	/** Tiles each way from the middle one (set before it finishes spawning, or in the level: 3 for the full map, 1 for the test). */
	void SetReach(int32 InReach) { Reach = FMath::Clamp(InReach, 1, 5); }
	int32 GetReach() const { return Reach; }
	int32 GetSeed() const { return Seed; }
	/** Server: spawns the tiles (before play begins: the game mode's StartPlay), each built as it begins play. */
	void SpawnTiles();
	/** Every tile has built its ground. */
	bool IsBuilt() const;
	/** The tiles (this machine's, once they have arrived). */
	const TArray<TWeakObjectPtr<ARRRandomDiceTown>>& GetTiles() const { return Tiles; }
	ARRRandomDiceTown* GetTile(int32 X, int32 Y) const;
	/** The tile a world spot is over (the nearest at the edge). */
	ARRRandomDiceTown* GetTileAt(const FVector2D& WorldLocation) const;

	/** The whole map's square (the island in it is round), in the world. */
	FBox2D GetMapBounds() const;
	/** The island's radius from the middle (on average: its rim wobbles a little). */
	float GetIslandRadius() const;
	/** Over the whole island, up to well over its tallest structure (ARRRandomCharacter keeps flyers in it). */
	FBox GetFlightArea() const;
	/** The floor's height (level 0's top), or the actor's own before the tiles are built. */
	float GetFloorZ() const;

	/**
	 * Where each team starts (round 92): spread round the island on its roads, ten spots as far apart as there is room, a
	 * random turn of them each game (the host's). Feet height, on the ground.
	 */
	void GetTeamStarts(int32 TeamCount, TArray<FVector>& OutStarts) const;
	/** Open ground (feet height) near a spot, on a tile: off the buildings, flat, with room round it. False if there is none near. */
	bool FindOpenGround(const FVector2D& Want, float Clear, float MaxReach, FVector& OutSpot) const;

	/** Rides, human cannons, plane stations and star cars spread over the map (server, once the tiles are built). */
	void SpawnRides();

	/** The streaming log: tiles shown, instances drawn (rr.BR.LogStreaming 1). */
	void LogStreaming() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, Replicated, Category = "Battle Map")
	int32 Reach = RRBattle::TileReach;

	/** The map's seed (districts, layouts); 0 picks one at play (the host's, replicated). */
	UPROPERTY(EditAnywhere, Replicated, Category = "Battle Map")
	int32 Seed = 0;

private:
	/** Shows a tile's detail when this machine's view is near it, its far look otherwise. */
	void UpdateStreaming();
	/** This machine's view: its local player's camera (or pawn), false without one (a dedicated server). */
	bool GetViewLocation(FVector& OutLocation) const;
	void GatherTiles();

	TArray<TWeakObjectPtr<ARRRandomDiceTown>> Tiles;
	float StreamTimer = 0.f;
	float LogTimer = 0.f;
	/** Tiles shown up close last time (for the log when it changes). */
	int32 LastShown = -1;
};

/**
 * The navigation system on the battle levels (their World Settings name it): the navmesh is made only round navigation
 * invokers (every animal friend carries one there, RRBattle::NavGenerateRadius), so a map twenty times Dice Town's isn't
 * made all at once; it follows the animal friends about as they go.
 */
UCLASS()
class RRRANDOM_API URRRandomNavigationSystem : public UNavigationSystemV1
{
	GENERATED_BODY()

public:
	URRRandomNavigationSystem(const FObjectInitializer& ObjectInitializer);
};

/**
 * The shrinking ring of safe ground (round 92). Phases: a wait, then the ring closes to the next one (smaller, somewhere
 * inside the last) over a while; outside it an animal friend loses health every second, more each phase. Run by the
 * server, replicated (the HUD shows it on the minimap, its time and a warning; a wall of light stands round it).
 */
UCLASS()
class RRRANDOM_API ARRRandomBattleZone : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomBattleZone();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	static ARRRandomBattleZone* Get(const UObject* WorldContext);

	/** Server: starts the phases (round the map's middle, as big as the island), at play start; the test map's go quicker. */
	void Begin(const FVector2D& InCenter, float InRadius, float StartTime, float InFloorZ, bool bTestMap);
	/** Cheat (RRZone): straight on to the next phase's closing. */
	void SkipAhead();

	/** The safe ring now (center and radius, in the world), and the one it is closing to (the same once closed). */
	FVector2D GetCenter() const;
	float GetRadius() const;
	FVector2D GetNextCenter() const { return NextCenter; }
	float GetNextRadius() const { return NextRadius; }
	bool IsInside(const FVector& WorldLocation) const;
	/** Phase (1 for the first), whether the ring is closing now, seconds until it starts or finishes closing. */
	int32 GetPhase() const { return Phase; }
	bool IsClosing() const;
	float GetSecondsToChange() const;
	/** Health lost a second outside the ring now, at its edge. */
	float GetDamagePerSecond() const;
	/**
	 * Round 93, the cotton candy cloud outside the ring: how deep in it a spot is, 0 inside the ring or at its edge to 1
	 * CloudDepth beyond (every machine works it out the same from the replicated ring).
	 */
	float GetCloudShare(const FVector& WorldLocation) const;
	/** Health lost a second there: the phase's, up to (1 + CloudHurtBoost) times as much deep in the cloud. */
	float GetDamagePerSecondAt(const FVector& WorldLocation) const;
	/** How fast an animal friend there moves, as a share of its speed (1 inside the ring). */
	float GetSpeedShareAt(const FVector& WorldLocation) const;
	float GetFloorZ() const { return FloorZ; }

	/** How far into the cloud it is at its thickest; the hurt added and the speed taken away there. */
	static constexpr float CloudDepth = 8000.f;
	static constexpr float CloudHurtBoost = 2.f;
	static constexpr float CloudSlow = 0.4f;
	/** Phases in all. */
	static int32 GetPhaseCount();

private:
	void StartPhase(int32 InPhase, float Now);
	void HurtOutside();
	void UpdateWall();
	/** The server's clock (the game state's). */
	float GetNow() const;

	UPROPERTY(Replicated) FVector2D FromCenter = FVector2D::ZeroVector;
	UPROPERTY(Replicated) float FromRadius = 0.f;
	UPROPERTY(Replicated) FVector2D NextCenter = FVector2D::ZeroVector;
	UPROPERTY(Replicated) float NextRadius = 0.f;
	/** Server times: closing starts, closing ends. */
	UPROPERTY(Replicated) float CloseStart = 0.f;
	UPROPERTY(Replicated) float CloseEnd = 0.f;
	UPROPERTY(Replicated) int32 Phase = 0;
	UPROPERTY(Replicated) float FloorZ = 0.f;
	/** How much quicker than the table its phases go (the test map's). */
	UPROPERTY(Replicated) float TimeScale = 1.f;
	/** The first ring's radius (the phases' rings are shares of it). */
	float FirstRadius = 0.f;

	float HurtTimer = 0.f;
	FRandomStream Stream;

	/** The cotton candy cloud bank round the ring (round 93): rows of pink see-through puffs, moved as it closes. */
	UPROPERTY(Transient) TObjectPtr<class UInstancedStaticMeshComponent> Wall;
	/** How big the puff model is across. */
	float PuffSize = 100.f;
	FVector2D WallCenter = FVector2D(UE_BIG_NUMBER);
	float WallRadius = -1.f;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> WallMaterial;
};
