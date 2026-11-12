#include "RRRandomDiceTown.h"
#include "RRRandomBattleRoyale.h"
#include "RRRandomCharacter.h"
#include "RRRandomDefense.h"
#include "RRRandomEffects.h"
#include "RRRandomHUD.h"
#include "RRRandomLava.h"
#include "RRRandomMapFloor.h"
#include "RRRandomUIStyle.h"
#include "RRRandomWreckage.h"
#include "EngineUtils.h"
#include "Misc/Paths.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UObjectGlobals.h"
#include "Algo/Reverse.h"

namespace
{
	// Blocks are 100 units (char-pipeline/scripts/blender_block_town.py; the engine cube too, for the hidden boxes)
	constexpr float BlockSize = 100.f;
	constexpr float FloorSearchUp = 500.f;
	constexpr float FloorSearchDown = 3000.f;

	// The grid: cells -HalfCells .. HalfCells - 1 each way from the middle (the town's 88; members, since a battle map tile
	// is smaller: round 92), levels LevelMin .. LevelMax. Level L is the block from (L - 1) x 100 to L x 100 above the
	// floor: level 0's top is the floor
	constexpr int32 TownHalfCells = 88;
	constexpr int32 LevelMin = -3;
	constexpr int32 LevelMax = 30;
	constexpr int32 GridLevels = LevelMax - LevelMin + 1;
	constexpr uint8 HiddenSolid = 255;

	// The island: a rounded square (a superellipse) about this far out, wobbling a little
	constexpr float RimBase = 8450.f;
	constexpr float RimPower = 5.f;
	constexpr int32 RimWallSegments = 260;
	constexpr float RimWallHeight = 6000.f;

	// Buildings: a story is 6 blocks (a floor block and 5 of room, 500: a giant is 384 tall); doors and windows 3 x 4
	constexpr int32 Story = 6;
	constexpr int32 OpeningWidth = 3;
	constexpr int32 OpeningHeight = 4;

	// Round 91, reflections and light here and there: the plaza's polished tiles, the dice's glossy plastic and the puddles'
	// mirror (M_BTBlock "Roughness"; the other blocks keep 0.6); about this many puddles on the roads (each 2-3 discs this
	// wide, on road cells with open sky this far apart); the star lamps' warm light (candelas, reach); the sun's shafts
	constexpr float PlazaRoughness = 0.12f;
	constexpr float DiceRoughness = 0.22f;
	constexpr float PuddleRoughness = 0.02f;
	constexpr int32 PuddleCount = 34;
	const FVector2D PuddleSize(110.f, 230.f);
	constexpr int32 PuddleSpacing = 6;
	// (dark, so what it mirrors is what shows)
	const FColor PuddleColor(34, 46, 70);
	constexpr float LampLightIntensity = 80.f;
	// Shop awnings' neon tubes (M_BTBlock "Glow")
	const FColor NeonColors[] = { FColor(255, 90, 190), FColor(70, 230, 255), FColor(120, 255, 170), FColor(255, 220, 90) };
	constexpr float NeonGlow = 3.f;
	constexpr float LampLightReach = 900.f;
	// Round 94: the big buildings' Tripo cover models (char-pipeline/input/props94, ue_import_props.py), by ARRRandomDiceTown::
	// ECoverModel (each keeps its own proportions: AddCoverModel)
	const TCHAR* const CoverModelNames[] = { TEXT(""), TEXT("SM_BCCrate"), TEXT("SM_BCShelf"), TEXT("SM_BCCounter"), TEXT("SM_BCSofa"), TEXT("SM_BCPlanter"),
		TEXT("SM_BCVending"), TEXT("SM_BCLockers"), TEXT("SM_BCDesk"), TEXT("SM_BCBookshelf"), TEXT("SM_BCCafeTable"), TEXT("SM_BCConveyor"), TEXT("SM_BCMachine"), TEXT("SM_BCTeddy"), TEXT("SM_BCCar"), TEXT("SM_BCKiosk"), TEXT("SM_BCAirCon"), TEXT("SM_BCWaterTank") };
	// The yaw of each model's front as it comes in: Tripo's models face -Y in Blender, which is +Y once in Unreal (checked
	// with RRGoCover: at -90 the vending machines and shelves showed their backs)
	constexpr float CoverModelFrontYaw = 90.f;
	// Round 94: the big buildings' ceiling lamps (a light at every other panel)
	const FColor IndoorLampColor(255, 244, 214);
	constexpr float IndoorLightIntensity = 55.f;
	constexpr float IndoorLightReach = 1500.f;
	const FLinearColor LampLightColor(1.f, 0.82f, 0.5f);
	constexpr float SunShaftBloomScale = 0.35f;

	// Districts, in cells from the middle (Chebyshev distance: the larger of |x| and |y|)
	constexpr float PlazaEdge = 13.f;
	constexpr float RoadHalf = 3.f;
	constexpr float RoadEnd = 56.f;
	constexpr float RingInner = 46.f;
	constexpr float RingOuter = 52.f;
	constexpr float DowntownEdge = 42.f;

	// Walking: room to stand is two empty blocks (a brawler is 192 tall); drops at most this far
	constexpr int32 Headroom = 2;
	constexpr float MaxDrop = 650.f;
	constexpr int32 MaxSearch = 80000;
	// Flying: the feet up to this far over the tallest structure
	constexpr float FlightHeadroom = 700.f;
	// Straightening a route: a capsule this wide and half as tall, its bottom this far over the feet (over a step)
	constexpr float RouteProbeRadius = 56.f;
	constexpr float RouteProbeHalfHeight = 70.f;
	constexpr float RouteProbeLift = 50.f;

	constexpr int32 GroundSeed = 3601;
	constexpr int32 LayoutSeed = 3602;
	constexpr int32 SkySeed = 3603;

	// Round 89. The ground is three layers (levels 0 to -2); a crater digging through all three breaks into the mantle
	// under the island: the bottom layer stays (the lava's bed) and the lava wells up. A dig reaches this far round where a
	// wreck meets the floor at most, so craters stay craters.
	constexpr int32 GroundBottom = -2;
	constexpr int32 BreachDepth = 3;
	constexpr float DigReachMax = 320.f;
	constexpr float DigReachMin = 50.f;
	// A part of a building coming down: falling this fast (a cartoon fall, a little quicker than real), tipping this far by
	// the time it lands, animating this many looks at most; a hit and a shove on who is under it
	constexpr float FallGravity = 2600.f;
	constexpr float FallTipDegrees = 9.f;
	constexpr int32 FallMaxPieces = 4000;
	constexpr int32 FallRubble = 90;
	constexpr float CrashDamage = 30.f;
	constexpr float CrashShove = 650.f;
	// What mends grows back from the bottom up: each level up waits this much longer, at most this long
	constexpr float RegrowStep = 0.07f;
	constexpr float RegrowMaxDelay = 1.2f;

	TAutoConsoleVariable<float> CVarSaturation(TEXT("rr.DiceTown.Saturation"), 1.f,
		TEXT("Dice Town's colors: 1 as made, higher deeper, lower paler. Live."));
	TAutoConsoleVariable<float> CVarBrightness(TEXT("rr.DiceTown.Brightness"), 1.f,
		TEXT("Dice Town's colors: 1 as made, higher lighter, lower darker. Live."));

	// Vivid candy colors (sRGB), deep enough to stay colorful under the bright sky
	const FColor GrassA(118, 214, 72);
	const FColor GrassB(98, 198, 64);
	const FColor GrassLime(170, 228, 80);
	const FColor Dirt(170, 108, 62);
	const FColor DirtDeep(140, 88, 54);
	const FColor Stone(196, 186, 176);
	const FColor Sand(245, 215, 150);
	const FColor PaveA(255, 232, 186);
	const FColor PaveB(250, 214, 160);
	const FColor PlazaPink(255, 160, 196);
	const FColor PlazaCream(255, 240, 222);
	const FColor Wood(205, 140, 82);
	const FColor WoodDark(160, 98, 56);
	const FColor Ceiling(255, 240, 220);
	const FColor White(252, 250, 245);
	const FColor Trunk(150, 96, 56);
	const FColor WaterBlue(70, 175, 255);
	const FColor Pink(255, 118, 170);
	const FColor Lemon(255, 214, 70);
	const FColor Mint(88, 214, 160);
	const FColor Sky(92, 182, 255);
	const FColor Lavender(176, 132, 255);
	const FColor Peach(255, 162, 92);
	const FColor Cream(255, 236, 206);
	const FColor Rainbow[] = { FColor(255, 70, 110), FColor(255, 150, 50), FColor(255, 222, 60), FColor(90, 214, 110), FColor(70, 170, 255), FColor(100, 110, 240), FColor(176, 110, 240) };
	const FColor Leaves[] = { FColor(86, 196, 82), FColor(64, 176, 96), FColor(150, 222, 84), FColor(255, 150, 196), FColor(255, 172, 64) };
	const FColor TeamTint[] = { FColor(120, 190, 255), FColor(255, 130, 140) };
	const FColor MapSky(190, 220, 255);
	const FColor MapWater(90, 170, 240);
	const FColor MapLava(240, 70, 40);

	/** A building's colors: walls downstairs and upstairs, trim (corners, bands, landings), roof, window frames. */
	struct FStyle
	{
		FColor Low;
		FColor High;
		FColor Trim;
		FColor Roof;
		FColor Frame;
	};
	const FStyle Styles[] =
	{
		{ Cream, Pink, White, FColor(235, 62, 120), White },
		{ Lemon, Lemon, Wood, FColor(255, 116, 40), WoodDark },
		{ Mint, Sky, White, FColor(40, 120, 230), White },
		{ Lavender, Lavender, White, FColor(130, 70, 220), White },
		{ Peach, Cream, Wood, FColor(220, 60, 60), WoodDark },
		{ Sky, Mint, White, FColor(30, 180, 170), White },
		{ Pink, Cream, White, FColor(150, 80, 230), White },
	};

	FColor Shade(const FColor& Color, float Share)
	{
		return FColor(uint8(FMath::Clamp(Color.R * Share, 0.f, 255.f)), uint8(FMath::Clamp(Color.G * Share, 0.f, 255.f)), uint8(FMath::Clamp(Color.B * Share, 0.f, 255.f)));
	}

	FColor Blend(const FColor& A, const FColor& B, float Alpha)
	{
		return FColor(uint8(FMath::Lerp(float(A.R), float(B.R), Alpha)), uint8(FMath::Lerp(float(A.G), float(B.G), Alpha)), uint8(FMath::Lerp(float(A.B), float(B.B), Alpha)));
	}

	/** Larger of |x| and |y| for a cell's middle, in cells. */
	float Cheb(int32 X, int32 Y)
	{
		return FMath::Max(FMath::Abs(X + 0.5f), FMath::Abs(Y + 0.5f));
	}

	uint32 CellHash(int32 X, int32 Y, int32 Z)
	{
		return HashCombine(HashCombine(GetTypeHash(X), GetTypeHash(Y)), GetTypeHash(Z));
	}

	UStaticMesh* LoadMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Environment/DiceTown/%s.%s"), Name, Name));
	}

	UMaterialInterface* LoadMaterial(const TCHAR* Name)
	{
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Environment/DiceTown/%s.%s"), Name, Name));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Frames and the grid

FIntPoint ARRRandomDiceTown::FFrame::Dir(int32 U, int32 V) const
{
	switch (Turn & 3)
	{
	case 1: return FIntPoint(-V, U);
	case 2: return FIntPoint(-U, -V);
	case 3: return FIntPoint(V, -U);
	default: return FIntPoint(U, V);
	}
}

FIntPoint ARRRandomDiceTown::FFrame::Cell(int32 U, int32 V) const
{
	return Origin + Dir(U, V);
}

ARRRandomDiceTown::FFrame ARRRandomDiceTown::FFrame::Mirror() const
{
	FFrame Other;
	Other.Origin = FIntPoint(-1 - Origin.X, -1 - Origin.Y);
	Other.Turn = (Turn + 2) & 3;
	Other.bRed = !bRed;
	return Other;
}

ARRRandomDiceTown::FFrame ARRRandomDiceTown::FFrame::Around(int32 InTurn)
{
	// Cell (U, V) turned round the corner point between the four middle cells
	static const FIntPoint Origins[] = { FIntPoint(0, 0), FIntPoint(-1, 0), FIntPoint(-1, -1), FIntPoint(0, -1) };
	FFrame Frame;
	Frame.Turn = InTurn & 3;
	Frame.Origin = Origins[Frame.Turn];
	return Frame;
}

ARRRandomDiceTown::ARRRandomDiceTown()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;

	// The host spawns it; clients build the same town from the same seeds (they need its walls to move)
	bReplicates = true;
	bAlwaysRelevant = true;
}

FBox2D ARRRandomDiceTown::GetMapBounds() const
{
	// (a battle map tile: the whole island's square round the map's middle)
	const FVector2D Center = FVector2D(GetActorLocation().X, GetActorLocation().Y) - TileOffset;
	const float Half = IsBattleTile() ? (BattleTile.Reach * 2 + 1) * BattleTile.Cells * BlockSize * 0.5f : MapHalf * RimScale;
	return FBox2D(Center - FVector2D(Half), Center + FVector2D(Half));
}

FBox2D ARRRandomDiceTown::GetGroundBounds() const
{
	if (!IsBattleTile())
	{
		return GetMapBounds();
	}
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	return FBox2D(Center - FVector2D(MapHalf), Center + FVector2D(MapHalf));
}

ARRRandomDiceTown* ARRRandomDiceTown::FindAt(const UObject* WorldContext, const FVector& WorldLocation)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	ARRRandomDiceTown* Best = nullptr;
	double BestDistance = UE_BIG_NUMBER;
	const FVector2D Spot(WorldLocation.X, WorldLocation.Y);
	for (TActorIterator<ARRRandomDiceTown> It(World); It; ++It)
	{
		if (!It->IsBuilt())
		{
			continue;
		}
		const FBox2D Ground = It->GetGroundBounds();
		if (Ground.IsInside(Spot))
		{
			return *It;
		}
		const double Distance = Ground.ComputeSquaredDistanceToPoint(Spot);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = *It;
		}
	}
	return Best;
}

void ARRRandomDiceTown::FindTouching(const UObject* WorldContext, const FBox& Box, float Margin, TArray<ARRRandomDiceTown*>& OutTowns)
{
	OutTowns.Reset();
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	const FBox2D Flat(FVector2D(Box.Min.X - Margin, Box.Min.Y - Margin), FVector2D(Box.Max.X + Margin, Box.Max.Y + Margin));
	for (TActorIterator<ARRRandomDiceTown> It(World); It; ++It)
	{
		if (It->IsBuilt() && It->GetGroundBounds().Intersect(Flat))
		{
			OutTowns.Add(*It);
		}
	}
}

FBox ARRRandomDiceTown::GetFlightArea() const
{
	// Over the whole island, the feet up to well over its tallest structure (the dice mountain, the keeps, the roofs), so a flyer
	// can land on any of them and look about over them; the rim's tall hidden wall keeps flyers over the island
	// (a battle map tile: the whole map's, as high as its highest tile's)
	if (IsBattleTile())
	{
		if (const ARRRandomBattleMap* Battle = ARRRandomBattleMap::Get(this))
		{
			return Battle->GetFlightArea();
		}
	}
	const FBox2D Map = GetMapBounds();
	return FBox(FVector(Map.Min, FloorZ - 1000.f), FVector(Map.Max, FloorZ + TopLevel * BlockSize + FlightHeadroom));
}

float ARRRandomDiceTown::GetTileFlightTop() const
{
	return FloorZ + TopLevel * BlockSize + FlightHeadroom;
}

bool ARRRandomDiceTown::IsInGrid(int32 X, int32 Y, int32 Level) const
{
	return X >= -HalfCells && X < HalfCells && Y >= -HalfCells && Y < HalfCells && Level >= LevelMin && Level <= LevelMax;
}

int32 ARRRandomDiceTown::GridIndex(int32 X, int32 Y, int32 Level) const
{
	return ((Level - LevelMin) * GridCells + (Y + HalfCells)) * GridCells + (X + HalfCells);
}

uint8 ARRRandomDiceTown::Get(int32 X, int32 Y, int32 Level) const
{
	Level += Lift;
	return IsInGrid(X, Y, Level) ? Grid[GridIndex(X, Y, Level)] : 0;
}

void ARRRandomDiceTown::Set(int32 X, int32 Y, int32 Level, uint8 Value)
{
	Level += Lift;
	if (IsInGrid(X, Y, Level))
	{
		Grid[GridIndex(X, Y, Level)] = Value;
	}
}

void ARRRandomDiceTown::Fill(const FFrame& Frame, int32 U0, int32 V0, int32 U1, int32 V1, int32 L0, int32 L1, uint8 Value)
{
	for (int32 U = FMath::Min(U0, U1); U <= FMath::Max(U0, U1); ++U)
	{
		for (int32 V = FMath::Min(V0, V1); V <= FMath::Max(V0, V1); ++V)
		{
			const FIntPoint Cell = Frame.Cell(U, V);
			for (int32 Level = FMath::Min(L0, L1); Level <= FMath::Max(L0, L1); ++Level)
			{
				Set(Cell.X, Cell.Y, Level, Value);
			}
		}
	}
}

uint8 ARRRandomDiceTown::MakeLook(const FColor& Body, const FColor& Accent, bool bDie, bool bUnderMesh, bool bGloss)
{
	for (int32 Index = 0; Index < Looks.Num(); ++Index)
	{
		if (Looks[Index].Body == Body && Looks[Index].Accent == Accent && Looks[Index].bDie == bDie && Looks[Index].bUnderMesh == bUnderMesh
			&& Looks[Index].bGloss == bGloss)
		{
			return uint8(Index + 1);
		}
	}
	if (Looks.Num() >= HiddenSolid - 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: dice town ran out of looks."));
		return 1;
	}
	Looks.Add({ Body, Accent, bDie, bUnderMesh, bGloss });
	return uint8(Looks.Num());
}

float ARRRandomDiceTown::RimRadius(float Theta) const
{
	const float C = FMath::Abs(FMath::Cos(Theta));
	const float S = FMath::Abs(FMath::Sin(Theta));
	const float Square = RimBase * RimScale / FMath::Pow(FMath::Pow(C, RimPower) + FMath::Pow(S, RimPower), 1.f / RimPower);
	return Square + 110.f * FMath::Sin(3.f * Theta + 1.f) + 70.f * FMath::Sin(7.f * Theta + 2.f);
}

bool ARRRandomDiceTown::IsOnIsland(int32 X, int32 Y) const
{
	const FVector2D Local = IslandSpot(X, Y);
	return Local.Size() < RimRadius(FMath::Atan2(Local.Y, Local.X)) - 50.f;
}

FVector2D ARRRandomDiceTown::CellCenter(const FIntPoint& Cell)
{
	return FVector2D((Cell.X + 0.5f) * BlockSize, (Cell.Y + 0.5f) * BlockSize);
}

FVector ARRRandomDiceTown::At(const FVector2D& Local, float Height) const
{
	return FVector(GetActorLocation().X + Local.X, GetActorLocation().Y + Local.Y, FloorZ + Height);
}

bool ARRRandomDiceTown::IsRoad(int32 X, int32 Y) const
{
	if (DefenseStage > 0)
	{
		return IsDefenseRoad(X, Y);
	}
	if (IsBattleTile())
	{
		return IsBattleRoad(X, Y);
	}
	const float CX = X + 0.5f;
	const float CY = Y + 0.5f;
	const float Distance = Cheb(X, Y);
	return (FMath::Abs(CY) < RoadHalf && FMath::Abs(CX) < RoadEnd) || (FMath::Abs(CX) < RoadHalf && FMath::Abs(CY) < RoadEnd)
		|| (Distance >= RingInner && Distance < RingOuter);
}

FIntRect ARRRandomDiceTown::Footprint(const FFrame& Frame, int32 U0, int32 V0, int32 U1, int32 V1, int32 Margin)
{
	const FIntPoint A = Frame.Cell(U0, V0);
	const FIntPoint B = Frame.Cell(U1, V1);
	return FIntRect(FMath::Min(A.X, B.X) - Margin, FMath::Min(A.Y, B.Y) - Margin, FMath::Max(A.X, B.X) + 1 + Margin, FMath::Max(A.Y, B.Y) + 1 + Margin);
}

bool ARRRandomDiceTown::IsFree(const FIntRect& Rect) const
{
	const FIntRect Mirrored(-Rect.Max.X, -Rect.Max.Y, -Rect.Min.X, -Rect.Min.Y);
	if (IsSymmetric() && Rect.Intersect(Mirrored))
	{
		return false;
	}
	// (a battle map tile: inside its own grid, a cell in from its edge, so nothing reaches into the next tile)
	if (IsBattleTile() && (Rect.Min.X < -HalfCells + 1 || Rect.Min.Y < -HalfCells + 1 || Rect.Max.X > HalfCells - 1 || Rect.Max.Y > HalfCells - 1))
	{
		return false;
	}
	for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X)
	{
		for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
		{
			// Two cells in from the rim, off the roads and the plaza
			if (!IsOnIsland(X, Y) || !IsOnIsland(X + 2, Y) || !IsOnIsland(X - 2, Y) || !IsOnIsland(X, Y + 2) || !IsOnIsland(X, Y - 2)
				|| IsRoad(X, Y) || (!IsBattleTile() && Cheb(X, Y) < PlazaEdge))
			{
				return false;
			}
		}
	}
	for (const FIntRect& Other : Reserved)
	{
		// (a defense stage isn't the same both ways round: only the rect itself)
		if (Rect.Intersect(Other) || (IsSymmetric() && Mirrored.Intersect(Other)))
		{
			return false;
		}
	}
	return true;
}

void ARRRandomDiceTown::Reserve(const FIntRect& Rect)
{
	Reserved.Add(Rect);
	if (IsSymmetric())
	{
		Reserved.Add(FIntRect(-Rect.Max.X, -Rect.Max.Y, -Rect.Min.X, -Rect.Min.Y));
	}
}

void ARRRandomDiceTown::Both(const FFrame& Frame, TFunctionRef<void(const FFrame&)> Place)
{
	Place(Frame);
	Place(Frame.Mirror());
}

UInstancedStaticMeshComponent* ARRRandomDiceTown::MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bSolid, int32 CustomFloats)
{
	// Never saved: the level's editor preview is built again on load, the game's at BeginPlay
	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	Instances->SetStaticMesh(Mesh);
	if (Material)
	{
		for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
		{
			Instances->SetMaterial(Slot, Material);
		}
	}
	Instances->NumCustomDataFloats = CustomFloats;
	Instances->SetCollisionProfileName(bSolid ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	Instances->SetGenerateOverlapEvents(false);
	// The navmesh (the bots' routes) is made from what stops things
	Instances->SetCanEverAffectNavigation(bSolid);
	Instances->SetupAttachment(RootComponent);
	// Instances are placed in world space
	Instances->SetUsingAbsoluteLocation(true);
	Instances->SetUsingAbsoluteRotation(true);
	Instances->SetUsingAbsoluteScale(true);
	Instances->RegisterComponent();
	Instances->SetWorldTransform(FTransform::Identity);
	return Instances;
}

void ARRRandomDiceTown::AddColored(UInstancedStaticMeshComponent* Instances, const FTransform& Transform, const FColor& Body, const FColor& Accent)
{
	if (!Instances)
	{
		return;
	}
	// The block material reads custom floats 0 to 2 as the body's linear color, 3 to 5 as the accent's
	const int32 Index = Instances->AddInstance(Transform, true);
	const FLinearColor A(Body);
	const FLinearColor B(Accent);
	const float Data[6] = { A.R, A.G, A.B, B.R, B.G, B.B };
	Instances->SetCustomData(Index, MakeArrayView(Data, FMath::Min(6, Instances->NumCustomDataFloats)));
}

// ---------------------------------------------------------------------------------------------------------------------
// Structures

void ARRRandomDiceTown::AddStairs(const FFrame& Frame, int32 U, int32 V, int32 DirU, int32 DirV, int32 AcrossU, int32 AcrossV, int32 Width, int32 Base, int32 Rise, const FColor& Color)
{
	// Stairs keep their own levels, Lift included
	Base += Lift;
	FStair Stair;
	Stair.Start = Frame.Cell(U, V);
	Stair.Dir = Frame.Dir(DirU, DirV);
	Stair.Across = Frame.Dir(AcrossU, AcrossV);
	Stair.Width = Width;
	Stair.Base = Base;
	Stair.Rise = Rise;
	Stair.StepFirst = Steps->GetInstanceCount();
	Stair.StepCount = Rise * 2;
	Stair.SolidFirst = Solid->GetInstanceCount();
	Stair.SolidCount = Rise + 2;
	Stairs.Add(Stair);
	for (int32 K = 0; K < Rise; ++K)
	{
		for (int32 W = 0; W < Width; ++W)
		{
			const FIntPoint Cell = Stair.Start + Stair.Dir * K + Stair.Across * W;
			StairCells.Add(Cell);
			// Nothing over the steps
			for (int32 Level = Base + 1; Level <= Base + Rise + Headroom; ++Level)
			{
				Set(Cell.X, Cell.Y, Level - Lift, 0);
			}
		}
	}

	// Looks: a column per step, half a block deep, up to its tread. Where the run starts (the edge of the first cell), the
	// way it runs and the middle across it, relative to the island's middle
	const FVector2D Dir(Stair.Dir.X, Stair.Dir.Y);
	const FVector2D Across(Stair.Across.X, Stair.Across.Y);
	const FVector2D Edge = CellCenter(Stair.Start) - Dir * (BlockSize * 0.5f) + Across * ((Width - 1) * BlockSize * 0.5f);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	const float BaseZ = Base * BlockSize;
	const FColor Tread = Color;
	const FColor Riser = Shade(Color, 0.9f);
	for (int32 Step = 1; Step <= Rise * 2; ++Step)
	{
		const float Height = Step * BlockSize * 0.5f;
		const FVector2D Middle = Edge + Dir * ((Step - 0.5f) * BlockSize * 0.5f);
		AddColored(Steps, FTransform(FRotator(0.f, Yaw, 0.f), At(Middle, BaseZ + Height * 0.5f), FVector(0.5f, Width, Height / BlockSize)), Step % 2 ? Riser : Shade(Riser, 0.96f), Tread);
	}

	// What stops: a slab along the treads' front edges (z = s + 50 from the run's start, 45 degrees), a box under each
	// cell below the slab, and the last half step flat
	const float Run = Rise * BlockSize - BlockSize * 0.5f;
	const float SlabThickness = 100.f;
	const float SlopeLength = (Run + BlockSize * 0.5f) * UE_SQRT_2;
	const FVector2D SlabMiddle2D = Edge + Dir * ((Run - BlockSize * 0.5f) * 0.5f + SlabThickness * 0.5f * UE_INV_SQRT_2);
	const float SlabMiddleZ = BaseZ + (Run + BlockSize * 0.5f) * 0.5f - SlabThickness * 0.5f * UE_INV_SQRT_2;
	Solid->AddInstance(FTransform(FRotator(45.f, Yaw, 0.f), At(SlabMiddle2D, SlabMiddleZ), FVector(SlopeLength / BlockSize, Width, SlabThickness / BlockSize)), true);
	for (int32 K = 0; K < Rise; ++K)
	{
		const float Top = K * BlockSize + 40.f;
		const FVector2D Middle = Edge + Dir * ((K + 0.5f) * BlockSize);
		Solid->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), At(Middle, BaseZ + Top * 0.5f), FVector(1.f, Width, Top / BlockSize)), true);
	}
	const FVector2D Last = Edge + Dir * (Rise * BlockSize - BlockSize * 0.25f);
	Solid->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), At(Last, BaseZ + Rise * BlockSize * 0.5f), FVector(0.5f, Width, Rise)), true);
}

void ARRRandomDiceTown::CutOpening(const FFrame& Frame, int32 W, int32 D, int32 Side, int32 Start, int32 Width, int32 L0, int32 L1, uint8 FrameLook)
{
	// The wall's cell for a spot along it
	auto WallCell = [&](int32 Along) -> FIntPoint
	{
		switch (Side)
		{
		case 0: return Frame.Cell(Along, 0);
		case 1: return Frame.Cell(W - 1, Along);
		case 2: return Frame.Cell(Along, D - 1);
		default: return Frame.Cell(0, Along);
		}
	};
	// The frame first (the wall round the hole, where there is wall), then the hole
	for (int32 Along = Start - 1; Along <= Start + Width; ++Along)
	{
		for (int32 Level = L0 - 1; Level <= L1 + 1; ++Level)
		{
			const FIntPoint Cell = WallCell(Along);
			if (Get(Cell.X, Cell.Y, Level) != 0)
			{
				Set(Cell.X, Cell.Y, Level, FrameLook);
			}
		}
	}
	for (int32 Along = Start; Along < Start + Width; ++Along)
	{
		const FIntPoint Cell = WallCell(Along);
		for (int32 Level = L0; Level <= L1; ++Level)
		{
			Set(Cell.X, Cell.Y, Level, 0);
		}
	}
}

void ARRRandomDiceTown::AddBuilding(const FFrame& Frame, int32 W, int32 D, int32 Stories, bool bFlatRoof, int32 Style, int32 Seed)
{
	FStyle Colors = Styles[Style % UE_ARRAY_COUNT(Styles)];
	FRandomStream Stream(Seed);
	// The dreamier pass: pastel trim and colored window frames instead of white everywhere, now and then a checkered or
	// striped upper story, and on a cottage now and then a giant die for a roof
	FRandomStream Dream(Seed + 4701);
	int32 WallPattern = 0;
	bool bDieRoof = false;
	if (bDream)
	{
		if (Colors.Trim == White)
		{
			Colors.Trim = Blend(Colors.High, White, 0.55f);
		}
		if (Colors.Frame == White)
		{
			Colors.Frame = Blend(Colors.Roof, White, 0.3f);
		}
		WallPattern = Dream.RandRange(0, 3);
		bDieRoof = Stories == 1 && !bFlatRoof && Dream.FRand() < 0.4f;
	}
	const uint8 TrimLook = Plain(Colors.Trim);
	const uint8 FrameLook = Plain(Colors.Frame);
	const uint8 StoneLook = Plain(Stone);
	const uint8 FloorLook = MakeLook(Dirt, Wood);
	const uint8 SlabLook = MakeLook(Ceiling, Wood);
	const int32 Top = Stories * Story;

	// Walls, story by story: stone at the foot, trim at the corners and round each floor
	for (int32 S = 0; S < Stories; ++S)
	{
		const uint8 WallLook = Plain(S == 0 ? Colors.Low : Colors.High);
		const uint8 PatternLook = Plain(Blend(S == 0 ? Colors.Low : Colors.High, White, 0.5f));
		const int32 Base = S * Story;
		for (int32 U = 0; U < W; ++U)
		{
			for (int32 V = 0; V < D; ++V)
			{
				const bool bEdgeU = U == 0 || U == W - 1;
				const bool bEdgeV = V == 0 || V == D - 1;
				const FIntPoint Cell = Frame.Cell(U, V);
				if (bEdgeU || bEdgeV)
				{
					for (int32 Level = Base + 1; Level <= Base + Story; ++Level)
					{
						const bool bTrim = (bEdgeU && bEdgeV) || Level == Base + Story;
						// Upper story patterns: 1 a checker, 2 upright stripes, 3 a band across the middle
						const int32 Along = bEdgeV ? U : V;
						const bool bPattern = S == Stories - 1 && ((WallPattern == 1 && (Along + Level) % 2 == 0)
							|| (WallPattern == 2 && Along % 2 == 0) || (WallPattern == 3 && Level == Base + 3));
						Set(Cell.X, Cell.Y, Level, S == 0 && Level == 1 ? StoneLook : bTrim ? TrimLook : bPattern ? PatternLook : WallLook);
					}
				}
				else
				{
					// Wooden floors: the ground floor's, then each floor above with a cream ceiling under it
					if (S == 0)
					{
						Set(Cell.X, Cell.Y, 0, FloorLook);
					}
					Set(Cell.X, Cell.Y, Base + Story, SlabLook);
				}
			}
		}
	}

	// Doors downstairs (front and left), the upstairs door onto the landing, windows everywhere else
	const int32 FrontDoor = W / 2 - 1;
	const int32 LeftDoor = D / 2 - 1;
	const int32 UpperDoor = 6;
	CutOpening(Frame, W, D, 0, FrontDoor, OpeningWidth, 1, OpeningHeight, FrameLook);
	CutOpening(Frame, W, D, 3, LeftDoor, OpeningWidth, 1, OpeningHeight, FrameLook);
	if (bDream)
	{
		AddAwning(Frame, FrontDoor - 1, OpeningWidth + 2, Colors.Roof);
	}
	for (int32 S = 0; S < Stories; ++S)
	{
		const int32 Base = S * Story;
		for (int32 Side = 0; Side < 4; ++Side)
		{
			const int32 Length = Side % 2 == 0 ? W : D;
			const int32 Count = FMath::Max(1, (Length - 2) / 5);
			const int32 Span = Count * OpeningWidth + (Count - 1) * 2;
			const int32 First = FMath::Max(1, (Length - Span) / 2);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				const int32 Start = First + Index * 5;
				if (Start + OpeningWidth > Length - 1)
				{
					continue;
				}
				auto Overlaps = [Start](int32 Other) { return Start < Other + OpeningWidth + 1 && Other < Start + OpeningWidth + 1; };
				// Not over a door; not behind the stairs' landing or the roof flight's wall
				if (S == 0 && ((Side == 0 && Overlaps(FrontDoor)) || (Side == 3 && Overlaps(LeftDoor))))
				{
					continue;
				}
				if (Stories > 1 && Side == 1 && ((S == 0 && Start + OpeningWidth > 5) || (S == 1 && Overlaps(UpperDoor))))
				{
					continue;
				}
				if (bFlatRoof && Stories > 1 && Side == 2 && Start + OpeningWidth > W - 8)
				{
					continue;
				}
				CutOpening(Frame, W, D, Side, Start, OpeningWidth, Base + 2, Base + 1 + OpeningHeight, FrameLook);
			}
		}
	}

	// A few crates and stacks inside each floor, clear of the doors
	const uint8 CrateLook = MakeLook(WoodDark, Wood);
	for (int32 S = 0; S < Stories; ++S)
	{
		const int32 Base = S * Story;
		for (int32 Try = 0, Placed = 0; Try < 20 && Placed < 3; ++Try)
		{
			const int32 U = Stream.RandRange(2, W - 3);
			const int32 V = Stream.RandRange(2, D - 3);
			const bool bDoorway = (S == 0 && FMath::Abs(U - (FrontDoor + 1)) <= 2 && V <= 3) || (S == 0 && U <= 3 && FMath::Abs(V - (LeftDoor + 1)) <= 2)
				|| (S == 1 && U >= W - 4 && V >= UpperDoor - 1 && V <= UpperDoor + OpeningWidth);
			if (bDoorway)
			{
				continue;
			}
			Fill(Frame, U, V, U, V, Base + 1, Base + Stream.RandRange(1, 2), CrateLook);
			++Placed;
		}
	}

	if (Stories > 1)
	{
		// Up the right side to a landing at the upstairs door, railed on its outer edge
		const FColor StairColor = Colors.Trim;
		AddStairs(Frame, W, 0, 0, 1, 1, 0, 2, 0, Story, StairColor);
		const int32 LandingEnd = bFlatRoof ? D + 1 : UpperDoor + OpeningWidth - 1;
		Fill(Frame, W, UpperDoor, W + 1, LandingEnd, 1, Story, TrimLook);
		Fill(Frame, W + 1, UpperDoor, W + 1, LandingEnd, Story + 1, Story + 1, FrameLook);
		CutOpening(Frame, W, D, 1, UpperDoor, OpeningWidth, Story + 1, Story + OpeningHeight, FrameLook);
		if (bFlatRoof)
		{
			// Along the back on a wall of blocks, to a landing at the roof's edge
			Fill(Frame, W - 6, D, W - 1, D + 1, 1, Story, StoneLook);
			AddStairs(Frame, W - 1, D, -1, 0, 0, 1, 2, Story, Story, StairColor);
			Fill(Frame, W - 8, D, W - 7, D + 1, 1, Story * 2, TrimLook);
		}
	}

	if (bFlatRoof)
	{
		// A parapet of every other block, open where the roof flight arrives
		for (int32 U = 0; U < W; ++U)
		{
			for (int32 V = 0; V < D; ++V)
			{
				const bool bEdge = U == 0 || U == W - 1 || V == 0 || V == D - 1;
				const bool bArrival = Stories > 1 && V == D - 1 && U >= W - 8 && U <= W - 7;
				if (bEdge && !bArrival && (U + V) % 2 == 0)
				{
					Fill(Frame, U, V, U, V, Top + 1, Top + 1, TrimLook);
				}
			}
		}
		if (bDream)
		{
			// A die and a little one on it in the front left corner, clear of where the roof flight arrives
			const int32 Kind = Dream.RandRange(0, 47);
			AddDieInFrame(Frame, 1, 1, Top + 1, 2, Kind);
			AddDieInFrame(Frame, 1, 1, Top + 3, 1, Kind + 7);
		}
	}
	else if (bDieRoof)
	{
		// A rim round the roof and a giant die sitting in the middle of it
		const uint8 RimLook = Plain(Colors.Roof);
		for (int32 U = -1; U <= W; ++U)
		{
			for (int32 V = -1; V <= D; ++V)
			{
				if (U == -1 || U == W || V == -1 || V == D)
				{
					Fill(Frame, U, V, U, V, Top, Top, RimLook);
				}
			}
		}
		const int32 Size = FMath::Clamp(FMath::Min(W, D) - 3, 3, 5);
		AddDieInFrame(Frame, (W - Size) / 2, (D - Size) / 2, Top + 1, Size, Dream.RandRange(0, 47));
	}
	else
	{
		// A hip roof of stepped blocks hanging one over the walls, striped light and dark
		for (int32 K = 0; ; ++K)
		{
			const int32 U0 = -1 + K;
			const int32 U1 = W - K;
			const int32 V0 = -1 + K;
			const int32 V1 = D - K;
			if (U0 > U1 || V0 > V1)
			{
				// The dreamier pass: the steps stay as what stops things, hidden under a cartoon roof stretched over them
				// (its band of shingles reaching the steps' outer edge, its point at their top), a die on the point
				if (bDream && Roofs)
				{
					AddColored(Roofs, FTransform(FRotator(0.f, FrameYaw(Frame), 0.f), FrameSpot(Frame, (W - 1) * 0.5f, (D - 1) * 0.5f, (Top + Lift) * BlockSize),
						FVector((W + 2) / 1.1f, (D + 2) / 1.1f, float(K))), Colors.Roof, Blend(Colors.Roof, White, 0.65f));
				}
				if (bDream && K > 0)
				{
					AddDieInFrame(Frame, (W - 2) / 2, (D - 2) / 2, Top + 1 + K, 2, Dream.RandRange(0, 47));
				}
				break;
			}
			const uint8 RoofLook = bDream && Roofs ? HiddenLook(Colors.Roof) : Plain(K % 2 == 0 ? Colors.Roof : Shade(Colors.Roof, 0.86f));
			const bool bSolidLayer = U1 - U0 <= 1 || V1 - V0 <= 1;
			for (int32 U = U0; U <= U1; ++U)
			{
				for (int32 V = V0; V <= V1; ++V)
				{
					if (bSolidLayer || U == U0 || U == U1 || V == V0 || V == V1)
					{
						Fill(Frame, U, V, U, V, Top + 1 + K, Top + 1 + K, RoofLook);
					}
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Round 94: big buildings to fight inside

FIntPoint ARRRandomDiceTown::GetBigBuildingSize(EBigBuilding Kind)
{
	switch (Kind)
	{
	case EBigBuilding::Mall: return FIntPoint(36, 28);
	case EBigBuilding::Factory: return FIntPoint(32, 22);
	case EBigBuilding::School: return FIntPoint(34, 14);
	default: return FIntPoint(30, 24);
	}
}

const TCHAR* ARRRandomDiceTown::GetBigBuildingName(EBigBuilding Kind)
{
	switch (Kind)
	{
	case EBigBuilding::Mall: return TEXT("mall");
	case EBigBuilding::Factory: return TEXT("factory");
	case EBigBuilding::School: return TEXT("school");
	default: return TEXT("garage");
	}
}

void ARRRandomDiceTown::AddRamp(const FFrame& Frame, int32 U, int32 V, int32 DirU, int32 DirV, int32 AcrossU, int32 AcrossV, int32 Width, int32 Base, int32 Rise, int32 Run, const FColor& Color)
{
	// Ramps keep their own levels, Lift included (as stairs do)
	Base += Lift;
	const FIntPoint Start = Frame.Cell(U, V);
	const FIntPoint DirCell = Frame.Dir(DirU, DirV);
	const FIntPoint AcrossCell = Frame.Dir(AcrossU, AcrossV);
	// Nothing over it (the floor above gets its hole)
	for (int32 K = 0; K < Run; ++K)
	{
		for (int32 W = 0; W < Width; ++W)
		{
			const FIntPoint Cell = Start + DirCell * K + AcrossCell * W;
			StairCells.Add(Cell);
			for (int32 Level = Base + 1; Level <= Base + Rise + Headroom; ++Level)
			{
				Set(Cell.X, Cell.Y, Level - Lift, 0);
			}
		}
	}
	// The deck: a slab whose top runs from the floor at the start to the floor Rise up at the far end (a little longer, to
	// meet both), drawn and solid
	const FVector2D Dir(DirCell.X, DirCell.Y);
	const FVector2D Across(AcrossCell.X, AcrossCell.Y);
	const FVector2D Edge = CellCenter(Start) - Dir * (BlockSize * 0.5f) + Across * ((Width - 1) * BlockSize * 0.5f);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	const float Length = Run * BlockSize;
	const float Height = Rise * BlockSize;
	const float Slope = FMath::Sqrt(Length * Length + Height * Height);
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Height, Length));
	const float Thick = BlockSize * 0.5f;
	// (the slab's middle: half way up its top, then half its thickness in under it)
	const FVector2D Middle = Edge + Dir * (Length * 0.5f + Height / Slope * Thick * 0.5f);
	const float MiddleZ = Base * BlockSize + Height * 0.5f - Length / Slope * Thick * 0.5f;
	const FTransform Deck(FRotator(Pitch, Yaw, 0.f), At(Middle, MiddleZ), FVector((Slope + 40.f) / BlockSize, Width, Thick / BlockSize));
	AddColored(Steps, Deck, Shade(Color, 0.9f), Color);
	Solid->AddInstance(Deck, true);
}

namespace
{
	// Round 96, the diagonal blocks: SM_BTSlope rises toward +X; SM_BTSlant keeps the half toward +X -Y and SM_BTSlopeCorner
	// rises toward +X -Y (Blender's +Y comes in as Unreal's -Y): their way is this yaw. A bank's slab is this thick under
	// its slope (its top edge juts this over the wall behind, by 1 / sqrt 2 of it: a lip of 14 at the top of a 100 wall); a
	// cut corner's box reaches this far in from the cut
	constexpr float SlantMeshYaw = -45.f;
	constexpr float BankSlabThickness = 20.f;
	constexpr float SlantBoxDepth = 100.f;
}

void ARRRandomDiceTown::AddSlopeRun(const FFrame& Frame, int32 U, int32 V, int32 AlongU, int32 AlongV, int32 UpU, int32 UpV, int32 Count, int32 Level, const FColor& Body, const FColor& Accent)
{
	if (!SlopeBlocks || Count <= 0)
	{
		return;
	}
	const int32 Absolute = Level + Lift;
	const FIntPoint First = Frame.Cell(U, V);
	const FIntPoint Along = Frame.Dir(AlongU, AlongV);
	const FVector2D Up(Frame.Dir(UpU, UpV).X, Frame.Dir(UpU, UpV).Y);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Up.Y, Up.X));
	FShapeRun Run;
	Run.Foot = FIntVector(First.X, First.Y, Absolute - 1);
	const FVector2D AlongDir(Along.X, Along.Y);
	for (int32 I = 0; I < Count; ++I)
	{
		const FIntPoint Cell = First + Along * I;
		Set(Cell.X, Cell.Y, Level, 0);
		const float Jitter = 0.95f + 0.1f * float(CellHash(Cell.X, Cell.Y, Absolute) % 1000) / 1000.f;
		// (the ends of a run 3 or longer: a corner rising inward and toward the run's middle, as a hip roof's end)
		const bool bEnd = Count >= 3 && SlopeCorners && (I == 0 || I == Count - 1);
		const FVector2D Corner = (Up + AlongDir * (I == 0 ? 1.f : -1.f)).GetSafeNormal();
		UInstancedStaticMeshComponent* Instances = bEnd ? SlopeCorners.Get() : SlopeBlocks.Get();
		const float PieceYaw = bEnd ? FMath::RadiansToDegrees(FMath::Atan2(Corner.Y, Corner.X)) - SlantMeshYaw : Yaw;
		Run.Pieces.Emplace(Instances, Instances->GetInstanceCount());
		AddColored(Instances, FTransform(FRotator(0.f, PieceYaw, 0.f), At(CellCenter(Cell), (Absolute - 0.5f) * BlockSize)), Shade(Body, Jitter), Shade(Accent, Jitter));
		FIntPoint& Taken = ShapedCells.FindOrAdd(Cell, FIntPoint(Absolute, Absolute));
		Taken = FIntPoint(FMath::Min(Taken.X, Absolute), FMath::Max(Taken.Y, Absolute));
	}
	// What stops: a thin slab under the slope (the run's length across, pitched 45 degrees), and a box filling the back of
	// the wedge (x 25..50, z -50..25 of the cell, x up the slope) so the wall at its high side stops too
	const FVector2D Middle = (CellCenter(First) + CellCenter(First + Along * (Count - 1))) * 0.5f;
	const float MiddleZ = (Absolute - 0.5f) * BlockSize;
	Run.SolidFirst = Solid->GetInstanceCount();
	Run.SolidCount = 2;
	const float Half = BankSlabThickness * 0.5f * UE_INV_SQRT_2;
	Solid->AddInstance(FTransform(FRotator(45.f, Yaw, 0.f), At(Middle + Up * Half, MiddleZ - Half), FVector(UE_SQRT_2, Count, BankSlabThickness / BlockSize)), true);
	Solid->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), At(Middle + Up * (BlockSize * 0.375f), MiddleZ - BlockSize * 0.125f), FVector(0.25f, Count, 0.75f)), true);
	ShapeRuns.Add(Run);
}

void ARRRandomDiceTown::AddSlantRun(const FFrame& Frame, int32 U, int32 V, int32 StepU, int32 StepV, int32 InU, int32 InV, int32 Count, int32 L0, int32 L1, TFunctionRef<uint8(int32)> LookAt)
{
	if (!SlantBlocks || Count <= 0 || L1 < L0)
	{
		return;
	}
	const FIntPoint First = Frame.Cell(U, V);
	const FIntPoint Step = Frame.Dir(StepU, StepV);
	const FVector2D In = FVector2D(Frame.Dir(InU, InV).X, Frame.Dir(InU, InV).Y).GetSafeNormal();
	const float InYaw = FMath::RadiansToDegrees(FMath::Atan2(In.Y, In.X));
	for (int32 I = 0; I < Count; ++I)
	{
		const FIntPoint Cell = First + Step * I;
		for (int32 Level = L0; Level <= L1; ++Level)
		{
			Set(Cell.X, Cell.Y, Level, 0);
			const uint8 Value = LookAt(Level);
			const FLook& Look = Looks[FMath::Max(1, int32(Value)) - 1];
			const int32 Absolute = Level + Lift;
			const float Jitter = 0.95f + 0.1f * float(CellHash(Cell.X, Cell.Y, Absolute) % 1000) / 1000.f;
			// Each its own run, so it breaks as a block does (a whole cut corner going with one block reached was odd): its
			// look, and a box from the cut through the cell's middle reaching in (the cut's cells' boxes meet end to end)
			FShapeRun Run;
			Run.Foot = FIntVector(Cell.X, Cell.Y, L0 + Lift - 1);
			Run.Pieces.Emplace(SlantBlocks.Get(), SlantBlocks->GetInstanceCount());
			AddColored(SlantBlocks, FTransform(FRotator(0.f, InYaw - SlantMeshYaw, 0.f), At(CellCenter(Cell), (Absolute - 0.5f) * BlockSize)), Shade(Look.Body, Jitter), Shade(Look.Accent, Jitter));
			Run.SolidFirst = Solid->GetInstanceCount();
			Run.SolidCount = 1;
			Solid->AddInstance(FTransform(FRotator(0.f, InYaw, 0.f), At(CellCenter(Cell) + In * (SlantBoxDepth * 0.5f), (Absolute - 0.5f) * BlockSize),
				FVector(SlantBoxDepth / BlockSize, UE_SQRT_2, 1.f)), true);
			ShapeRuns.Add(Run);
		}
		FIntPoint& Taken = ShapedCells.FindOrAdd(Cell, FIntPoint(L0 + Lift, L1 + Lift));
		Taken = FIntPoint(FMath::Min(Taken.X, L0 + Lift), FMath::Max(Taken.Y, L1 + Lift));
	}
}

void ARRRandomDiceTown::AddSlopeRamp(const FFrame& Frame, int32 U, int32 V, int32 DirU, int32 DirV, int32 AcrossU, int32 AcrossV, int32 Width, int32 Base, int32 Rise,
	const FColor& Body, const FColor& Accent, uint8 FillLook)
{
	if (!SlopeBlocks)
	{
		return;
	}
	// Ramps keep their own levels, Lift included (as stairs do); each cell K up it is earth to level K and a slope over that
	Base += Lift;
	FStair Stair;
	Stair.Start = Frame.Cell(U, V);
	Stair.Dir = Frame.Dir(DirU, DirV);
	Stair.Across = Frame.Dir(AcrossU, AcrossV);
	Stair.Width = Width;
	Stair.Base = Base;
	Stair.Rise = Rise;
	Stair.StepSet = SlopeBlocks;
	Stair.StepFirst = SlopeBlocks->GetInstanceCount();
	Stair.StepCount = Rise * Width;
	Stair.SolidFirst = Solid->GetInstanceCount();
	Stair.SolidCount = 1;
	Stair.NodeShift = -BlockSize * 0.5f;
	Stairs.Add(Stair);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(float(Stair.Dir.Y), float(Stair.Dir.X)));
	for (int32 K = 0; K < Rise; ++K)
	{
		for (int32 W = 0; W < Width; ++W)
		{
			const FIntPoint Cell = Stair.Start + Stair.Dir * K + Stair.Across * W;
			StairCells.Add(Cell);
			for (int32 Level = Base + 1; Level <= Base + Rise + Headroom; ++Level)
			{
				Set(Cell.X, Cell.Y, Level - Lift, Level <= Base + K ? FillLook : 0);
			}
			const float Jitter = 0.95f + 0.1f * float(CellHash(Cell.X, Cell.Y, Base + K) % 1000) / 1000.f;
			AddColored(SlopeBlocks, FTransform(FRotator(0.f, Yaw, 0.f), At(CellCenter(Cell), (Base + K + 0.5f) * BlockSize)), Shade(Body, Jitter), Shade(Accent, Jitter));
		}
	}
	// What stops: a slab whose top runs up the slopes (as AddRamp's deck, a little longer to meet the ground and the top)
	const FVector2D Dir(Stair.Dir.X, Stair.Dir.Y);
	const FVector2D Across(Stair.Across.X, Stair.Across.Y);
	const FVector2D Edge = CellCenter(Stair.Start) - Dir * (BlockSize * 0.5f) + Across * ((Width - 1) * BlockSize * 0.5f);
	const float Length = Rise * BlockSize;
	const float Slope = Length * UE_SQRT_2;
	const float Thick = BlockSize * 0.5f;
	const FVector2D Middle = Edge + Dir * (Length * 0.5f + UE_INV_SQRT_2 * Thick * 0.5f);
	const float MiddleZ = Base * BlockSize + Length * 0.5f - UE_INV_SQRT_2 * Thick * 0.5f;
	Solid->AddInstance(FTransform(FRotator(45.f, Yaw, 0.f), At(Middle, MiddleZ), FVector((Slope + 40.f) / BlockSize, Width, Thick / BlockSize)), true);
}

bool ARRRandomDiceTown::AddCoverModel(ECoverModel Model, const FFrame& Frame, int32 W, int32 D, int32 S, int32 U0, int32 V0, int32 U1, int32 V1, int32 Height,
	uint8 Hidden)
{
	UInstancedStaticMeshComponent* Instances = CoverModels.IsValidIndex(int32(Model) - 1) ? CoverModels[int32(Model) - 1].Get() : nullptr;
	if (!Instances || !Instances->GetStaticMesh())
	{
		return false;
	}
	// The model's own box (pivot at the middle of its foot): its X across its front, Y front to back
	const FVector Size = Instances->GetStaticMesh()->GetBoundingBox().GetSize().ComponentMax(FVector(1.f));
	// The block along its longer side, the models in a row along it (crates also stacked one a level), each turned to face
	// into the building, across the row. A model keeps its own proportions (the user: never stretched to the cells), scaled
	// evenly to the block's height (what it hides behind); as many fit along the row as its own width gives, and the hidden
	// cells are only those under a model (the middle of the cell inside its foot), so it may stand a little past them
	const int32 LenU = FMath::Abs(U1 - U0) + 1;
	const int32 LenV = FMath::Abs(V1 - V0) + 1;
	const bool bAlongU = LenU >= LenV;
	const int32 Long = bAlongU ? LenU : LenV;
	const int32 Stack = Model == ECoverModel::Crate ? FMath::Max(1, Height) : 1;
	const float Tall = Height * BlockSize / Stack;
	const float Even = Tall / Size.Z;
	const float Wide = Size.X * Even / BlockSize;
	const float Deep = Size.Y * Even / BlockSize;
	// (a landmark stands alone: the mall's kiosk, the teddy pile, a roof's water tank)
	const bool bSingle = Model == ECoverModel::Kiosk || Model == ECoverModel::Teddy || Model == ECoverModel::WaterTank;
	const int32 Tiles = bSingle ? 1 : FMath::Max(1, FMath::RoundToInt(Long / Wide));
	const float MidU = (U0 + U1) * 0.5f;
	const float MidV = (V0 + V1) * 0.5f;
	const FIntPoint Face = bAlongU ? Frame.Dir(0, MidV < D * 0.5f ? 1 : -1) : Frame.Dir(MidU < W * 0.5f ? 1 : -1, 0);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(float(Face.Y), float(Face.X))) - CoverModelFrontYaw;
	const float Each = float(Long) / Tiles;
	const int32 AlongLo = bAlongU ? FMath::Min(U0, U1) : FMath::Min(V0, V1);
	const int32 AcrossLo = bAlongU ? FMath::Min(V0, V1) : FMath::Min(U0, U1);
	const int32 AcrossHi = bAlongU ? FMath::Max(V0, V1) : FMath::Max(U0, U1);
	const float Across = bAlongU ? MidV : MidU;
	// The cells (of the block) whose middles are within Half of Middle, or the nearest one if none is
	auto CellsUnder = [](float Middle, float Half, int32 Lo, int32 Hi, int32& OutFrom, int32& OutTo)
	{
		OutFrom = FMath::Max(Lo, FMath::CeilToInt(Middle - Half));
		OutTo = FMath::Min(Hi, FMath::FloorToInt(Middle + Half));
		if (OutFrom > OutTo)
		{
			OutFrom = OutTo = FMath::Clamp(FMath::RoundToInt(Middle), Lo, Hi);
		}
	};
	// (a crate turned a quarter on every other level: its foot is the wider of the two)
	const float FootAlong = (Model == ECoverModel::Crate && Stack > 1 ? FMath::Max(Wide, Deep) : Wide) * 0.5f;
	const float FootAcross = (Model == ECoverModel::Crate && Stack > 1 ? FMath::Max(Wide, Deep) : Deep) * 0.5f;
	int32 AcrossFrom = 0, AcrossTo = 0;
	CellsUnder(Across, FootAcross, AcrossLo, AcrossHi, AcrossFrom, AcrossTo);
	for (int32 Tile = 0; Tile < Tiles; ++Tile)
	{
		const float Along = AlongLo - 0.5f + (Tile + 0.5f) * Each;
		for (int32 Level = 0; Level < Stack; ++Level)
		{
			const FVector Where = FrameSpot(Frame, bAlongU ? Along : MidU, bAlongU ? MidV : Along, S * Story * BlockSize + Level * Tall);
			Instances->AddInstance(FTransform(FRotator(0.f, Yaw + (Model == ECoverModel::Crate ? Level * 90.f : 0.f), 0.f), Where, FVector(Even)), true);
		}
		int32 AlongFrom = 0, AlongTo = 0;
		CellsUnder(Along, FootAlong, AlongLo, AlongLo + Long - 1, AlongFrom, AlongTo);
		const FIntPoint Low = bAlongU ? FIntPoint(AlongFrom, AcrossFrom) : FIntPoint(AcrossFrom, AlongFrom);
		const FIntPoint High = bAlongU ? FIntPoint(AlongTo, AcrossTo) : FIntPoint(AcrossTo, AlongTo);
		Fill(Frame, Low.X, Low.Y, High.X, High.Y, S * Story + 1, S * Story + Height, Hidden);
		// (for the bots: the cells it fills are the cover)
		const FIntPoint A = Frame.Cell(Low.X, Low.Y);
		const FIntPoint B = Frame.Cell(High.X, High.Y);
		CoverPieces.Add({ At((CellCenter(A) + CellCenter(B)) * 0.5f, S * Story * BlockSize),
			FVector2D((FMath::Abs(A.X - B.X) + 1) * BlockSize * 0.5f, (FMath::Abs(A.Y - B.Y) + 1) * BlockSize * 0.5f) });
	}
	return true;
}

bool ARRRandomDiceTown::GetCoverModelSpot(int32 Model, FVector& OutWhere, float& OutFrontYaw) const
{
	const UInstancedStaticMeshComponent* Instances = CoverModels.IsValidIndex(Model - 1) ? CoverModels[Model - 1].Get() : nullptr;
	FTransform Transform;
	if (!Instances || Instances->GetInstanceCount() == 0 || !Instances->GetInstanceTransform(0, Transform, true))
	{
		return false;
	}
	OutWhere = Transform.GetLocation();
	OutFrontYaw = Transform.Rotator().Yaw + CoverModelFrontYaw;
	return true;
}

void ARRRandomDiceTown::AddBigBuilding(const FFrame& Frame, EBigBuilding Kind, int32 Seed)
{
	FRandomStream Stream(Seed);
	const FIntPoint Size = GetBigBuildingSize(Kind);
	const int32 W = Size.X;
	const int32 D = Size.Y;
	FStyle Colors = Styles[Stream.RandRange(0, UE_ARRAY_COUNT(Styles) - 1)];
	if (Colors.Trim == White)
	{
		Colors.Trim = Blend(Colors.High, White, 0.55f);
	}
	if (Colors.Frame == White)
	{
		Colors.Frame = Blend(Colors.Roof, White, 0.3f);
	}
	const bool bGarage = Kind == EBigBuilding::Garage;
	const int32 Stories = Kind == EBigBuilding::Mall || Kind == EBigBuilding::School ? 3 : 2;
	const int32 Top = Stories * Story;
	const uint8 TrimLook = Plain(Colors.Trim);
	const uint8 FrameLook = Plain(Colors.Frame);
	const uint8 StoneLook = Plain(Stone);
	const uint8 SlabLook = MakeLook(Ceiling, Wood);
	const uint8 FloorLook = Kind == EBigBuilding::Mall ? MakeLook(PlazaCream, PlazaPink, false, false, true)
		: Kind == EBigBuilding::School ? MakeLook(Wood, WoodDark) : MakeLook(Stone, Blend(Stone, White, 0.4f));
	const uint8 InnerLook = Plain(Blend(Colors.Low, White, 0.45f));
	// The cover, blocks for now: crates and shelves (full), counters and sofas (half), desks and tables (low)
	const uint8 FullLook = MakeLook(WoodDark, Wood);
	const uint8 HalfLook = MakeLook(Pink, Cream);
	const uint8 LowLook = MakeLook(Lemon, Cream);
	const uint8 RailLook = MakeLook(Blend(Sky, White, 0.55f), White);
	const uint8 MachineLook = MakeLook(Lavender, Sky);
	const uint8 BeltLook = MakeLook(FColor(96, 92, 120), Lemon);
	const uint8 LockerLook = MakeLook(Sky, White);
	const uint8 ShelfLook = MakeLook(WoodDark, Lemon);
	const FColor StairColor = Colors.Trim;

	// The shell, story by story: walls round the edge (a garage: a low wall and a post every 6 cells), each floor's slab
	for (int32 S = 0; S < Stories; ++S)
	{
		const uint8 WallLook = Plain(S == 0 ? Colors.Low : Colors.High);
		const int32 Base = S * Story;
		for (int32 U = 0; U < W; ++U)
		{
			for (int32 V = 0; V < D; ++V)
			{
				const bool bEdgeU = U == 0 || U == W - 1;
				const bool bEdgeV = V == 0 || V == D - 1;
				const FIntPoint Cell = Frame.Cell(U, V);
				if (S == 0)
				{
					Set(Cell.X, Cell.Y, 0, FloorLook);
				}
				if (bEdgeU || bEdgeV)
				{
					const int32 Along = bEdgeV ? U : V;
					const bool bPost = (bEdgeU && bEdgeV) || Along % 6 == 0;
					const int32 WallTop = bGarage && !bPost ? Base + 1 : Base + Story;
					for (int32 Level = Base + 1; Level <= WallTop; ++Level)
					{
						const bool bTrim = (bEdgeU && bEdgeV) || Level == Base + Story || (bGarage && bPost);
						Set(Cell.X, Cell.Y, Level, S == 0 && Level == 1 ? StoneLook : bTrim ? TrimLook : WallLook);
					}
					if (bGarage)
					{
						Set(Cell.X, Cell.Y, Base + Story, SlabLook);
					}
				}
				else
				{
					Set(Cell.X, Cell.Y, Base + Story, SlabLook);
				}
			}
		}
	}
	// A parapet all round the roof
	for (int32 U = 0; U < W; ++U)
	{
		for (int32 V = 0; V < D; ++V)
		{
			if (U == 0 || U == W - 1 || V == 0 || V == D - 1)
			{
				Fill(Frame, U, V, U, V, Top + 1, Top + 1, TrimLook);
			}
		}
	}

	// Doors (4 wide; the factory's big ones 6; the garage's a gap in its low wall for cars) and windows all round
	struct FDoor
	{
		int32 Side;
		int32 Start;
		int32 Width;
		int32 Height;
	};
	TArray<FDoor> Doors;
	switch (Kind)
	{
	case EBigBuilding::Mall: Doors = { { 2, 16, 4, 4 }, { 3, 12, 4, 4 }, { 1, 12, 4, 4 }, { 0, 20, 4, 4 } }; break;
	case EBigBuilding::Factory: Doors = { { 2, 22, 6, 5 }, { 1, 13, 6, 5 }, { 3, 15, 3, 4 } }; break;
	case EBigBuilding::School: Doors = { { 3, 5, 4, 4 }, { 1, 5, 4, 4 }, { 2, 15, 4, 4 } }; break;
	default: Doors = { { 2, 19, 10, 5 }, { 3, 8, 4, 5 } }; break;
	}
	if (!bGarage)
	{
		for (int32 S = 0; S < Stories; ++S)
		{
			const int32 Base = S * Story;
			for (int32 Side = 0; Side < 4; ++Side)
			{
				const int32 Length = Side % 2 == 0 ? W : D;
				const int32 Count = FMath::Max(1, (Length - 2) / 5);
				const int32 Span = Count * OpeningWidth + (Count - 1) * 2;
				const int32 First = FMath::Max(1, (Length - Span) / 2);
				for (int32 Index = 0; Index < Count; ++Index)
				{
					const int32 Start = First + Index * 5;
					if (Start + OpeningWidth > Length - 1)
					{
						continue;
					}
					bool bOverDoor = false;
					for (const FDoor& Door : Doors)
					{
						bOverDoor |= S == 0 && Door.Side == Side && Start < Door.Start + Door.Width + 1 && Door.Start < Start + OpeningWidth + 1;
					}
					if (!bOverDoor)
					{
						CutOpening(Frame, W, D, Side, Start, OpeningWidth, Base + 2, Base + 1 + OpeningHeight, FrameLook);
					}
				}
			}
		}
	}
	for (const FDoor& Door : Doors)
	{
		CutOpening(Frame, W, D, Door.Side, Door.Start, Door.Width, 1, Door.Height, FrameLook);
	}

	// Building blocks, story S (floor F counts from the ground's 0: the slab under story F). Cover and pillars are listed
	// for the bots (CoverPieces: the middle on its floor, the half size)
	auto NoteCover = [&](int32 S, int32 U0, int32 V0, int32 U1, int32 V1)
	{
		const FIntPoint A = Frame.Cell(U0, V0);
		const FIntPoint B = Frame.Cell(U1, V1);
		const FVector2D Middle = (CellCenter(A) + CellCenter(B)) * 0.5f;
		const FVector2D Extent((FMath::Abs(A.X - B.X) + 1) * BlockSize * 0.5f, (FMath::Abs(A.Y - B.Y) + 1) * BlockSize * 0.5f);
		CoverPieces.Add({ At(Middle, S * Story * BlockSize), Extent });
	};
	// (with a Tripo model: the blocks hidden under it, still solid)
	auto Cover = [&](int32 S, int32 U0, int32 V0, int32 U1, int32 V1, int32 Height, uint8 Look, ECoverModel Model = ECoverModel::None)
	{
		const bool bModel = Model != ECoverModel::None && CoverModels.IsValidIndex(int32(Model) - 1) && CoverModels[int32(Model) - 1];
		// (the model fills only the cells it stands on; without one, the whole block)
		if (!bModel || !AddCoverModel(Model, Frame, W, D, S, U0, V0, U1, V1, Height, HiddenLook(Looks[Look - 1].Body)))
		{
			Fill(Frame, U0, V0, U1, V1, S * Story + 1, S * Story + Height, Look);
			NoteCover(S, U0, V0, U1, V1);
		}
	};
	auto Wall = [&](int32 S, int32 U0, int32 V0, int32 U1, int32 V1)
	{
		Fill(Frame, U0, V0, U1, V1, S * Story + 1, S * Story + Story - 1, InnerLook);
	};
	auto Gap = [&](int32 S, int32 U0, int32 V0, int32 U1, int32 V1)
	{
		Fill(Frame, U0, V0, U1, V1, S * Story + 1, S * Story + OpeningHeight, 0);
	};
	auto Hole = [&](int32 F, int32 U0, int32 V0, int32 U1, int32 V1)
	{
		Fill(Frame, U0, V0, U1, V1, F * Story, F * Story, 0);
	};
	auto Rail = [&](int32 F, int32 U0, int32 V0, int32 U1, int32 V1)
	{
		Fill(Frame, U0, V0, U1, V1, F * Story + 1, F * Story + 1, RailLook);
	};
	auto Pillar = [&](int32 S, int32 U, int32 V)
	{
		Fill(Frame, U, V, U + 1, V + 1, S * Story + 1, S * Story + Story - 1, TrimLook);
		NoteCover(S, U, V, U + 1, V + 1);
	};
	// Switchback stairs in a well 4 lanes wide and 6 steps long (8 with a landing at each end): the even flights up lanes
	// 0-1, the odd ones back up lanes 2-3; each railed round its hole on the floor it arrives at, but where it arrives (the
	// next flight clears the rail over its own steps)
	auto Stairwell = [&](int32 U0, int32 V0, int32 Flights, bool bAlongU)
	{
		auto Local = [&](int32 Lane, int32 K) { return bAlongU ? FIntPoint(U0 + K, V0 + Lane) : FIntPoint(U0 + Lane, V0 + K); };
		for (int32 F = 0; F < Flights; ++F)
		{
			const bool bEven = F % 2 == 0;
			const int32 Lane0 = bEven ? 0 : 2;
			const FIntPoint From = Local(Lane0, bEven ? 0 : 5);
			if (bAlongU)
			{
				AddStairs(Frame, From.X, From.Y, bEven ? 1 : -1, 0, 0, 1, 2, F * Story, Story, StairColor);
			}
			else
			{
				AddStairs(Frame, From.X, From.Y, 0, bEven ? 1 : -1, 1, 0, 2, F * Story, Story, StairColor);
			}
			const int32 StartEnd = bEven ? -1 : 6;
			const int32 Arrival = bEven ? 6 : -1;
			for (int32 K = -1; K <= 6; ++K)
			{
				for (int32 Lane = Lane0 - 1; Lane <= Lane0 + 2; ++Lane)
				{
					const bool bHole = K >= 0 && K <= 5 && Lane >= Lane0 && Lane <= Lane0 + 1;
					const bool bRing = K == StartEnd || Lane == Lane0 - 1 || Lane == Lane0 + 2;
					const FIntPoint Cell = Local(Lane, K);
					const bool bEdge = Cell.X <= 0 || Cell.X >= W - 1 || Cell.Y <= 0 || Cell.Y >= D - 1;
					if (!bHole && bRing && K != Arrival && !bEdge)
					{
						Rail(F + 1, Cell.X, Cell.Y, Cell.X, Cell.Y);
					}
				}
			}
		}
	};

	switch (Kind)
	{
	case EBigBuilding::Mall:
	{
		// The atrium: open from the 2nd floor to the roof, a bridge across it on the 2nd, railed all round; pillars at its
		// corners on every floor; switchback stairs in two corners and an escalator up from the court
		Hole(1, 12, 9, 15, 18);
		Hole(1, 20, 9, 23, 18);
		Hole(2, 12, 9, 23, 18);
		Rail(1, 12, 8, 23, 8);
		Rail(1, 12, 19, 20, 19);
		Rail(1, 23, 19, 23, 19);
		Rail(1, 11, 9, 11, 18);
		Rail(1, 24, 9, 24, 18);
		Rail(1, 16, 9, 16, 18);
		Rail(1, 19, 9, 19, 18);
		Rail(2, 12, 8, 23, 8);
		Rail(2, 12, 19, 23, 19);
		Rail(2, 11, 9, 11, 18);
		Rail(2, 24, 9, 24, 18);
		Stairwell(2, 3, 3, false);
		Stairwell(30, 19, 2, false);
		AddStairs(Frame, 21, 25, 0, -1, 1, 0, 2, 0, Story, Colors.Roof);
		Rail(1, 20, 20, 20, 25);
		Rail(1, 23, 20, 23, 25);
		Rail(1, 21, 26, 22, 26);
		for (int32 S = 0; S < 3; ++S)
		{
			Pillar(S, 10, 7);
			Pillar(S, 24, 7);
			Pillar(S, 10, 19);
			Pillar(S, 24, 19);
		}
		// 1st floor: a kiosk in the court (it cuts the 36 m from door to door), counters round it, shelves, two shops
		Cover(0, 16, 12, 19, 15, 3, FullLook, ECoverModel::Kiosk);
		Cover(0, 13, 10, 15, 10, 1, HalfLook, ECoverModel::Counter);
		Cover(0, 20, 10, 22, 10, 1, HalfLook, ECoverModel::Counter);
		Cover(0, 13, 17, 15, 17, 1, HalfLook, ECoverModel::Counter);
		Cover(0, 20, 17, 22, 17, 1, HalfLook, ECoverModel::Counter);
		Cover(0, 3, 11, 3, 17, 2, ShelfLook, ECoverModel::Shelf);
		Cover(0, 6, 11, 6, 17, 2, ShelfLook, ECoverModel::Shelf);
		Cover(0, 12, 24, 12, 24, 1, HalfLook, ECoverModel::Planter);
		Cover(0, 29, 13, 29, 13, 1, HalfLook, ECoverModel::Planter);
		Cover(0, 15, 23, 16, 23, 1, HalfLook, ECoverModel::Crate);
		Wall(0, 27, 1, 27, 9);
		Gap(0, 27, 4, 27, 5);
		Wall(0, 27, 10, 34, 10);
		Cover(0, 30, 2, 33, 2, 2, ShelfLook, ECoverModel::Shelf);
		Cover(0, 29, 7, 32, 7, 1, HalfLook, ECoverModel::Counter);
		Wall(0, 1, 20, 9, 20);
		Gap(0, 4, 20, 5, 20);
		Wall(0, 10, 21, 10, 26);
		Cover(0, 2, 24, 4, 24, 1, HalfLook, ECoverModel::Sofa);
		Cover(0, 6, 24, 7, 25, 2, FullLook, ECoverModel::Crate);
		// 2nd: shops along the back, sofas round the balcony, a vending machine
		Wall(1, 10, 1, 10, 5);
		Wall(1, 10, 6, 26, 6);
		Gap(1, 15, 6, 16, 6);
		Gap(1, 22, 6, 23, 6);
		Wall(1, 26, 1, 26, 5);
		Cover(1, 12, 2, 14, 2, 1, HalfLook, ECoverModel::Counter);
		Cover(1, 19, 2, 21, 2, 1, HalfLook, ECoverModel::Counter);
		Cover(1, 23, 3, 23, 4, 1, HalfLook, ECoverModel::Crate);
		Cover(1, 13, 22, 15, 22, 1, HalfLook, ECoverModel::Sofa);
		Cover(1, 16, 23, 18, 23, 1, HalfLook, ECoverModel::Sofa);
		Cover(1, 8, 13, 8, 15, 1, HalfLook, ECoverModel::Sofa);
		Cover(1, 27, 13, 27, 15, 1, HalfLook, ECoverModel::Sofa);
		Cover(1, 34, 12, 34, 13, 2, FullLook, ECoverModel::Vending);
		Cover(1, 8, 3, 9, 4, 2, FullLook, ECoverModel::Crate);
		Cover(1, 25, 22, 26, 23, 2, FullLook, ECoverModel::Crate);
		Cover(1, 30, 4, 30, 4, 1, HalfLook, ECoverModel::Planter);
		// 3rd: a food court, the kitchen, arcade machines, sofas
		for (const int32 U : { 27, 31 })
		{
			for (const int32 V : { 3, 7, 11 })
			{
				Cover(2, U, V, U + 1, V + 1, 1, LowLook, ECoverModel::CafeTable);
			}
		}
		Cover(2, 28, 14, 31, 15, 2, FullLook, ECoverModel::Shelf);
		Cover(2, 2, 12, 2, 12, 2, MachineLook, ECoverModel::Vending);
		Cover(2, 2, 15, 2, 15, 2, MachineLook, ECoverModel::Vending);
		Cover(2, 2, 18, 2, 18, 2, MachineLook, ECoverModel::Vending);
		Cover(2, 14, 22, 16, 22, 1, HalfLook, ECoverModel::Sofa);
		Cover(2, 19, 23, 21, 23, 1, HalfLook, ECoverModel::Sofa);
		Cover(2, 9, 22, 10, 23, 2, FullLook, ECoverModel::Crate);
		break;
	}
	case EBigBuilding::Factory:
	{
		// One tall hall (no 2nd floor) under an L of mezzanine along the back and the left, railed at its edge, a flight up
		// to each arm
		Hole(1, 6, 6, W - 2, D - 2);
		Rail(1, 5, 5, W - 2, 5);
		Rail(1, 5, 6, 5, D - 2);
		AddStairs(Frame, 28, 11, 0, -1, 1, 0, 3, 0, Story, StairColor);
		Fill(Frame, 28, 5, 30, 5, Story + 1, Story + 1, 0);
		AddStairs(Frame, 11, 17, -1, 0, 0, 1, 3, 0, Story, StairColor);
		Fill(Frame, 5, 17, 5, 19, Story + 1, Story + 1, 0);
		// The hall: two conveyor lines (half), machines under the mezzanine and by the wall, crate stacks, a heap of teddies
		Cover(0, 6, 8, 21, 9, 1, BeltLook, ECoverModel::Conveyor);
		Cover(0, 6, 14, 17, 15, 1, BeltLook, ECoverModel::Conveyor);
		Cover(0, 24, 2, 27, 4, 2, MachineLook, ECoverModel::Machine);
		Cover(0, 24, 9, 27, 11, 2, MachineLook, ECoverModel::Machine);
		Cover(0, 2, 6, 3, 7, 2, FullLook, ECoverModel::Crate);
		Cover(0, 20, 18, 21, 19, 2, FullLook, ECoverModel::Crate);
		Cover(0, 13, 18, 14, 19, 2, FullLook, ECoverModel::Crate);
		Cover(0, 20, 11, 21, 12, 2, FullLook, ECoverModel::Crate);
		Cover(0, 12, 4, 12, 4, 1, HalfLook, ECoverModel::Crate);
		Cover(0, 3, 11, 3, 12, 1, HalfLook, ECoverModel::Crate);
		Cover(0, 13, 11, 15, 12, 1, HalfLook, ECoverModel::Teddy);
		Cover(0, 26, 16, 27, 16, 1, HalfLook, ECoverModel::Crate);
		// The mezzanine: crates and the control room
		Cover(1, 10, 2, 11, 2, 1, HalfLook, ECoverModel::Crate);
		Cover(1, 20, 2, 21, 2, 1, HalfLook, ECoverModel::Crate);
		Cover(1, 2, 12, 2, 13, 1, HalfLook, ECoverModel::Crate);
		Cover(1, 2, 8, 3, 9, 2, FullLook, ECoverModel::Crate);
		Cover(1, 14, 1, 17, 3, 2, MachineLook, ECoverModel::Machine);
		break;
	}
	case EBigBuilding::School:
	{
		// A corridor 4 wide (rows 5-8) the whole length; classrooms along the back (one library on the 3rd), a lobby, a room
		// and a store along the front; switchback stairs at both ends
		Stairwell(2, 1, 3, true);
		Stairwell(26, 9, 2, true);
		for (int32 S = 0; S < 3; ++S)
		{
			Wall(S, 9, 4, 32, 4);
			Gap(S, 11, 4, 12, 4);
			Gap(S, 19, 4, 20, 4);
			Gap(S, 27, 4, 28, 4);
			Wall(S, 9, 1, 9, 3);
			if (S < 2)
			{
				Wall(S, 17, 1, 17, 3);
				Wall(S, 25, 1, 25, 3);
			}
			Wall(S, 1, 9, 24, 9);
			Gap(S, 6, 9, 7, 9);
			Gap(S, 22, 9, 23, 9);
			if (S == 0)
			{
				Gap(S, 13, 9, 20, 9);
			}
			else
			{
				Gap(S, 15, 9, 16, 9);
			}
			Wall(S, 12, 10, 12, 12);
			Wall(S, 21, 10, 21, 12);
			Wall(S, 24, 10, 24, 12);
		}
		// Desks (low) and teacher's desks (half) in the classrooms, lockers and vending machines breaking up the corridor
		for (int32 S = 0; S < 3; ++S)
		{
			for (const int32 Room : { 10, 18, 26 })
			{
				if (S == 2)
				{
					continue;
				}
				for (const int32 Step : { 1, 3, 5 })
				{
					Cover(S, Room + Step, 1, Room + Step, 1, 1, LowLook, ECoverModel::Desk);
				}
				Cover(S, Room + 4, 3, Room + 5, 3, 1, HalfLook, ECoverModel::Desk);
			}
			for (const int32 U : { 2, 4, 9 })
			{
				Cover(S, U, 11, U, 11, 1, LowLook, ECoverModel::Desk);
			}
			Cover(S, 2, 12, 3, 12, 1, HalfLook, ECoverModel::Desk);
		}
		Cover(0, 14, 12, 14, 12, 1, HalfLook, ECoverModel::Planter);
		Cover(0, 19, 12, 19, 12, 1, HalfLook, ECoverModel::Planter);
		Cover(0, 16, 12, 17, 12, 2, FullLook, ECoverModel::Bookshelf);
		Cover(0, 13, 5, 15, 5, 2, LockerLook, ECoverModel::Lockers);
		Cover(0, 19, 5, 19, 5, 2, MachineLook, ECoverModel::Vending);
		Cover(1, 6, 8, 8, 8, 2, LockerLook, ECoverModel::Lockers);
		Cover(1, 21, 5, 23, 5, 2, LockerLook, ECoverModel::Lockers);
		Cover(1, 29, 5, 29, 5, 2, MachineLook, ECoverModel::Vending);
		Cover(1, 14, 11, 14, 11, 1, LowLook, ECoverModel::Desk);
		Cover(1, 17, 11, 17, 11, 1, LowLook, ECoverModel::Desk);
		Cover(1, 18, 12, 19, 12, 1, HalfLook, ECoverModel::Desk);
		Cover(2, 12, 8, 14, 8, 2, LockerLook, ECoverModel::Lockers);
		Cover(2, 22, 5, 22, 5, 2, MachineLook, ECoverModel::Vending);
		for (const int32 U : { 13, 16, 22, 25, 30 })
		{
			Cover(2, U, 1, U, 3, 2, ShelfLook, ECoverModel::Bookshelf);
		}
		Cover(2, 18, 2, 19, 2, 1, LowLook, ECoverModel::CafeTable);
		Cover(2, 14, 11, 15, 11, 1, HalfLook, ECoverModel::Sofa);
		Cover(2, 17, 11, 17, 11, 1, LowLook, ECoverModel::Desk);
		break;
	}
	default:
	{
		// Open decks round pillars, a ramp up to the 2nd and another on to the roof, switchback stairs in a corner, cars
		Stairwell(1, 15, 2, false);
		Rail(1, 25, 22, 28, 22);
		Rail(2, 20, 10, 20, 21);
		Rail(2, 25, 10, 25, 21);
		Rail(2, 21, 9, 24, 9);
		AddRamp(Frame, 25, 21, 0, -1, 1, 0, 4, 0, Story, 12, Colors.Trim);
		AddRamp(Frame, 21, 10, 0, 1, 1, 0, 4, Story, Story, 12, Colors.Trim);
		for (int32 S = 0; S < 2; ++S)
		{
			for (const int32 U : { 5, 12, 18 })
			{
				for (const int32 V : { 5, 11, 17 })
				{
					Pillar(S, U, V);
				}
			}
		}
		static const FColor CarColors[] = { FColor(255, 118, 170), FColor(92, 182, 255), FColor(255, 214, 70), FColor(88, 214, 160), FColor(176, 132, 255) };
		auto Car = [&](int32 S, int32 U0, int32 V0, int32 U1, int32 V1)
		{
			const FColor Paint = CarColors[Stream.RandRange(0, UE_ARRAY_COUNT(CarColors) - 1)];
			Cover(S, U0, V0, U1, V1, 1, MakeLook(Paint, White), ECoverModel::Car);
		};
		Car(0, 2, 1, 3, 4);
		Car(0, 9, 1, 10, 4);
		Car(0, 15, 1, 16, 4);
		Car(0, 9, 14, 10, 17);
		Car(0, 15, 14, 16, 17);
		Car(1, 2, 8, 5, 9);
		Car(1, 9, 1, 10, 4);
		Car(1, 15, 14, 16, 17);
		Car(1, 8, 14, 11, 15);
		Cover(2, 3, 3, 5, 5, 3, FullLook, ECoverModel::WaterTank);
		Cover(2, 11, 10, 12, 11, 2, MachineLook, ECoverModel::AirCon);
		Cover(2, 17, 15, 18, 16, 2, MachineLook, ECoverModel::AirCon);
		Car(2, 9, 1, 10, 4);
		Car(2, 14, 4, 17, 5);
		Car(2, 7, 17, 10, 18);
		break;
	}
	}

	// Ceiling lamps: a glowing panel every 8 cells under each ceiling with room under it (the neon blocks: shown up close
	// only), and at every other one a soft light without shadows, warm white
	for (int32 S = 0; S < Stories && Neon; ++S)
	{
		const int32 LampTop = S * Story + Story;
		int32 Lamps = 0;
		for (int32 U = 4; U < W - 2; U += 8)
		{
			for (int32 V = 4; V < D - 2; V += 8)
			{
				const FIntPoint Cell = Frame.Cell(U, V);
				if (Get(Cell.X, Cell.Y, LampTop) == 0 || Get(Cell.X, Cell.Y, LampTop - 1) != 0)
				{
					continue;
				}
				AddColored(Neon, FTransform(FRotator::ZeroRotator, At(CellCenter(Cell), (LampTop - 1) * BlockSize - 6.f), FVector(1.4f, 1.4f, 0.12f)),
					IndoorLampColor, IndoorLampColor);
				if (Lamps++ % 2 == 0)
				{
					UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
					Light->SetupAttachment(RootComponent);
					Light->SetUsingAbsoluteLocation(true);
					Light->SetIntensityUnits(ELightUnits::Candelas);
					Light->SetIntensity(IndoorLightIntensity);
					Light->SetAttenuationRadius(IndoorLightReach);
					Light->SetLightColor(FLinearColor(IndoorLampColor));
					Light->SetCastShadows(false);
					Light->RegisterComponent();
					Light->SetWorldLocation(At(CellCenter(Cell), (LampTop - 1) * BlockSize - 60.f));
					Light->SetVisibility(bDetailShown);
					LampLights.Add(Light);
				}
			}
		}
	}
}

bool ARRRandomDiceTown::PlaceBigBuilding(EBigBuilding Kind, FRandomStream& Stream)
{
	const FIntPoint Size = GetBigBuildingSize(Kind);
	// Where to try its middle: round the middle of each quarter of the tile (between the avenue round it and the street
	// across it), nudged up to 12 cells each way (a cell at a time), any way round; in a shuffled order. A quick look at what is reserved and the tile's edge first: the full check looks at every cell
	const int32 Quarter = (HalfCells - int32(RRBattle::AvenueHalf) + int32(RRBattle::StreetHalf)) / 2;
	TArray<TPair<FIntPoint, int32>> Tries;
	for (const int32 SX : { -1, 1 })
	{
		for (const int32 SY : { -1, 1 })
		{
			for (int32 NX = -12; NX <= 12; ++NX)
			{
				for (int32 NY = -12; NY <= 12; ++NY)
				{
					for (int32 Turn = 0; Turn < 4; ++Turn)
					{
						Tries.Add(TPair<FIntPoint, int32>(FIntPoint(SX * Quarter + NX, SY * Quarter + NY), Turn));
					}
				}
			}
		}
	}
	for (int32 Index = Tries.Num() - 1; Index > 0; --Index)
	{
		Tries.Swap(Index, Stream.RandRange(0, Index));
	}
	for (const TPair<FIntPoint, int32>& Try : Tries)
	{
		FFrame Frame;
		Frame.Turn = Try.Value;
		Frame.Origin = FIntPoint::ZeroValue;
		const FIntRect Around = Footprint(Frame, 0, 0, Size.X - 1, Size.Y - 1, 0);
		Frame.Origin = Try.Key - (Around.Min + Around.Max) / 2;
		const FIntRect Rect = Footprint(Frame, 0, 0, Size.X - 1, Size.Y - 1, 1);
		if (Rect.Min.X < -HalfCells + 1 || Rect.Min.Y < -HalfCells + 1 || Rect.Max.X > HalfCells - 1 || Rect.Max.Y > HalfCells - 1)
		{
			continue;
		}
		bool bClear = true;
		for (const FIntRect& Other : Reserved)
		{
			bClear &= !Rect.Intersect(Other);
		}
		if (!bClear)
		{
			continue;
		}
		if (!IsFree(Rect))
		{
			continue;
		}
		Reserve(Rect);
		AddBigBuilding(Frame, Kind, Stream.RandRange(0, 1 << 30));
		// A spot inside on the ground floor, clear on every floor, for the cheat that goes there
		static const FIntPoint Inside[] = { FIntPoint(18, 23), FIntPoint(25, 15), FIntPoint(16, 6), FIntPoint(8, 9) };
		BigBuildingSpots.Add(At(CellCenter(Frame.Cell(Inside[int32(Kind)].X, Inside[int32(Kind)].Y)), 0.f));
		BigBuildingsBuilt += FString::Printf(TEXT("%s%s"), BigBuildingsBuilt.IsEmpty() ? TEXT("") : TEXT(", "), GetBigBuildingName(Kind));
		return true;
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: no room for a %s on battle tile (%d, %d)."), GetBigBuildingName(Kind), BattleTile.X, BattleTile.Y);
	return false;
}

void ARRRandomDiceTown::AddTower(const FFrame& Frame, int32 Style)
{
	// Four legs, a platform 6 blocks up with crenels, and a flight up to it from the front
	const FStyle& Colors = Styles[Style % UE_ARRAY_COUNT(Styles)];
	const uint8 Legs = Plain(Colors.Trim);
	const uint8 Deck = MakeLook(Colors.Low, Wood);
	const uint8 Crenel = Plain(Colors.Roof);
	for (const FIntPoint Corner : { FIntPoint(0, 0), FIntPoint(3, 0), FIntPoint(0, 3), FIntPoint(3, 3) })
	{
		Fill(Frame, Corner.X, Corner.Y, Corner.X, Corner.Y, 1, Story - 1, Legs);
	}
	Fill(Frame, 0, 0, 3, 3, Story, Story, Deck);
	for (int32 U = 0; U < 4; ++U)
	{
		for (int32 V = 0; V < 4; ++V)
		{
			const bool bEdge = U == 0 || U == 3 || V == 0 || V == 3;
			const bool bArrival = V == 0 && (U == 1 || U == 2);
			if (bEdge && !bArrival && (U + V) % 2 == 0)
			{
				Fill(Frame, U, V, U, V, Story + 1, Story + 1, Crenel);
			}
		}
	}
	AddStairs(Frame, 1, -Story, 0, 1, 1, 0, 2, 0, Story, Colors.Trim);
}

void ARRRandomDiceTown::AddTree(const FIntPoint& Cell, int32 Kind, int32 Seed)
{
	if (bDream && AddPropTree(Cell, Kind, Seed))
	{
		return;
	}
	if (Kind >= 2)
	{
		AddDreamTree(Cell, Kind, Seed);
		return;
	}
	// A trunk of blocks and a blocky crown: a small 3 x 3 one, or a big 5 x 5 one with its corners off
	FRandomStream Stream(Seed);
	const int32 Height = Kind == 0 ? Stream.RandRange(2, 3) : Stream.RandRange(3, 4);
	const uint8 Bark = MakeLook(Trunk, Shade(Trunk, 1.1f));
	const FColor Green = Leaves[Stream.RandRange(0, UE_ARRAY_COUNT(Leaves) - 1)];
	const uint8 Leaf = Plain(Green);
	const uint8 LeafLight = Plain(Shade(Green, 1.12f));
	for (int32 Level = 1; Level <= Height; ++Level)
	{
		Set(Cell.X, Cell.Y, Level, Bark);
	}
	const int32 Reach = Kind == 0 ? 1 : 2;
	for (int32 Layer = 0; Layer < (Kind == 0 ? 3 : 4); ++Layer)
	{
		const int32 R = Layer >= 2 ? Reach - (Layer - 1) : Reach;
		for (int32 DX = -R; DX <= R; ++DX)
		{
			for (int32 DY = -R; DY <= R; ++DY)
			{
				if (R == 2 && FMath::Abs(DX) == 2 && FMath::Abs(DY) == 2)
				{
					continue;
				}
				if (R < 0)
				{
					continue;
				}
				Set(Cell.X + DX, Cell.Y + DY, Height + 1 + Layer, (DX + DY + Layer) % 3 == 0 ? LeafLight : Leaf);
			}
		}
	}
}

void ARRRandomDiceTown::AddDie(const FIntPoint& Cell, int32 Level, int32 Size, int32 Kind)
{
	// A die Size blocks across with its lowest corner cell at Cell, bottom on Level; solid on the grid, drawn as one die
	static const FColor Bodies[] = { White, Pink, Sky, Lemon, Mint, Lavender };
	static const FColor Pips[] = { FColor(40, 40, 62), White, White, FColor(110, 70, 30), White, White };
	const int32 Pick = Kind % UE_ARRAY_COUNT(Bodies);
	for (int32 DX = 0; DX < Size; ++DX)
	{
		for (int32 DY = 0; DY < Size; ++DY)
		{
			for (int32 L = Level; L < Level + Size; ++L)
			{
				Set(Cell.X + DX, Cell.Y + DY, L, HiddenSolid);
			}
		}
	}
	const FVector2D Middle = CellCenter(Cell) + FVector2D((Size - 1) * BlockSize * 0.5f);
	const FRotator Turn(90.f * (Kind / 6 % 4 == 3 ? 1 : 0), 90.f * (Kind / 6 % 4), 90.f * (Kind / 24 % 2));
	AddColored(DieBlocks, FTransform(Turn, At(Middle, (Level + Lift - 1) * BlockSize + Size * BlockSize * 0.5f), FVector(Size * 0.98f)), Bodies[Pick], Pips[Pick]);
	// Remembered: a wreck takes it whole (round 89)
	GiantDice.Add({ FIntVector(Cell.X, Cell.Y, Level + Lift), Size, DieBlocks->GetInstanceCount() - 1, Bodies[Pick], Pips[Pick] });
}

void ARRRandomDiceTown::AddHill(const FFrame& Frame, int32 W, int32 D, int32 Levels)
{
	// Grass terraces, each 3 cells in from the one below; a ramp up each, at the front and back in turn
	const uint8 Grass = MakeLook(Dirt, GrassA);
	for (int32 K = 0; K < Levels; ++K)
	{
		Fill(Frame, 3 * K, 3 * K, W - 1 - 3 * K, D - 1 - 3 * K, 1, K + 1, Grass);
	}
	for (int32 K = 0; K < Levels; ++K)
	{
		const int32 U = W / 2 - 1;
		if (K % 2 == 0)
		{
			AddStairs(Frame, U, 3 * K - 1, 0, 1, 1, 0, 2, K, 1, Sand);
		}
		else
		{
			AddStairs(Frame, U, D - 3 * K, 0, -1, 1, 0, 2, K, 1, Sand);
		}
	}
}

void ARRRandomDiceTown::AddBigHill(const FFrame& Frame, int32 W, int32 D, int32 Seed)
{
	FRandomStream Stream(Seed);
	constexpr int32 Inset = 5;
	// (every cliff 3 blocks: a brawler climbs ledges up to 2 above its feet, so each terrace is reached by its ways up only)
	constexpr int32 Tops[] = { 3, 6, 9 };
	constexpr int32 Tiers = UE_ARRAY_COUNT(Tops);
	// Round 96, with the diagonal blocks: each terrace's corners cut at 45 degrees this many cells, so the hill reads as a
	// round mound, not a stack of boxes (the top's cut small: the pavilion stands one cell in from it)
	constexpr int32 Cuts[] = { 5, 4, 2 };
	constexpr int32 StairWidth = 3;
	constexpr int32 RampWidth = 5;
	const bool bShapes = HasDiagonalBlocks();
	const uint8 Grass = MakeLook(Dirt, GrassA);
	const uint8 WallLook = MakeLook(Shade(Stone, 0.9f), Blend(Stone, White, 0.3f));
	// Round 96: each terrace grass on top over layers of earth (a block's grass lip on every level made the cliffs stripy)
	const FColor EarthLayers[] = { Blend(Dirt, Sand, 0.3f), Dirt, DirtDeep };
	auto LookAt = [&](int32 K, int32 Level) -> uint8
	{
		return Level >= Tops[K] ? Grass : Plain(EarthLayers[FMath::Clamp(Tops[K] - Level - 1, 0, 2)]);
	};
	auto At = [&Frame](int32 U, int32 V, int32 Turn)
	{
		FFrame Child;
		Child.Origin = Frame.Cell(U, V);
		Child.Turn = (Frame.Turn + Turn) & 3;
		Child.bRed = Frame.bRed;
		return Child;
	};

	// The terraces, the lowest first, each only from the top of the one under it (that one fills under this one); round
	// 96: a cut corner's cells beyond the cut stay the terrace below, the cells on the cut are slanted
	for (int32 K = 0; K < Tiers; ++K)
	{
		const int32 E = Inset * K;
		const int32 Cut = bShapes ? Cuts[K] : 0;
		const int32 L0 = K == 0 ? 1 : Tops[K - 1] + 1;
		for (int32 U = E; U <= W - 1 - E; ++U)
		{
			for (int32 V = E; V <= D - 1 - E; ++V)
			{
				const int32 FromCorner = FMath::Min(U - E, W - 1 - E - U) + FMath::Min(V - E, D - 1 - E - V);
				if (FromCorner < Cut)
				{
					continue;
				}
				for (int32 Level = L0; Level <= Tops[K]; ++Level)
				{
					Fill(Frame, U, V, U, V, Level, Level, LookAt(K, Level));
				}
			}
		}
		if (Cut > 0)
		{
			for (const FIntPoint Sign : { FIntPoint(1, 1), FIntPoint(-1, 1), FIntPoint(1, -1), FIntPoint(-1, -1) })
			{
				const int32 CornerU = Sign.X > 0 ? E : W - 1 - E;
				const int32 CornerV = Sign.Y > 0 ? E : D - 1 - E;
				AddSlantRun(Frame, CornerU + Sign.X * (Cut - 1), CornerV, -Sign.X, Sign.Y, Sign.X, Sign.Y, Cut, L0, Tops[K], [&](int32 Level) { return LookAt(K, Level); });
			}
		}
	}
	// A flight up each, on the next side round (front, right, back, left) from a random start, 3 wide in the middle of it;
	// round 96: and a second way onto the first terrace, a ramp of slopes 5 wide on the side across from its flight (a hill
	// with one way up was a trap). The banks round each terrace's edge leave a gap where a way arrives
	const int32 FirstSide = Stream.RandRange(0, 3);
	TArray<FIntPoint> Arrivals[Tiers];
	const int32 MidU = W / 2 - 1;
	const int32 MidV = D / 2 - 1;
	for (int32 K = 0; K < Tiers; ++K)
	{
		const int32 Base = K == 0 ? 0 : Tops[K - 1];
		const int32 Rise = Tops[K] - Base;
		const int32 E = Inset * K;
		const FColor StepColor = K == Tiers - 1 ? Cream : Sand;
		switch ((FirstSide + K) % 4)
		{
		case 0:
			AddStairs(Frame, MidU, E - Rise, 0, 1, 1, 0, StairWidth, Base, Rise, StepColor);
			Arrivals[K].Add(FIntPoint(MidU + 1, E));
			break;
		case 1:
			AddStairs(Frame, W - 1 - E + Rise, MidV, -1, 0, 0, 1, StairWidth, Base, Rise, StepColor);
			Arrivals[K].Add(FIntPoint(W - 1 - E, MidV + 1));
			break;
		case 2:
			AddStairs(Frame, MidU, D - 1 - E + Rise, 0, -1, 1, 0, StairWidth, Base, Rise, StepColor);
			Arrivals[K].Add(FIntPoint(MidU + 1, D - 1 - E));
			break;
		default:
			AddStairs(Frame, E - Rise, MidV, 1, 0, 0, 1, StairWidth, Base, Rise, StepColor);
			Arrivals[K].Add(FIntPoint(E, MidV + 1));
			break;
		}
	}
	if (bShapes)
	{
		const int32 Rise = Tops[0];
		const uint8 Earth = Plain(EarthLayers[0]);
		const FColor Path = Blend(Sand, GrassLime, 0.25f);
		switch ((FirstSide + 2) % 4)
		{
		case 0:
			AddSlopeRamp(Frame, MidU - 1, -Rise, 0, 1, 1, 0, RampWidth, 0, Rise, EarthLayers[0], Path, Earth);
			Arrivals[0].Add(FIntPoint(MidU + 1, 0));
			break;
		case 1:
			AddSlopeRamp(Frame, W - 1 + Rise, MidV - 1, -1, 0, 0, 1, RampWidth, 0, Rise, EarthLayers[0], Path, Earth);
			Arrivals[0].Add(FIntPoint(W - 1, MidV + 1));
			break;
		case 2:
			AddSlopeRamp(Frame, MidU - 1, D - 1 + Rise, 0, -1, 1, 0, RampWidth, 0, Rise, EarthLayers[0], Path, Earth);
			Arrivals[0].Add(FIntPoint(MidU + 1, D - 1));
			break;
		default:
			AddSlopeRamp(Frame, -Rise, MidV - 1, 1, 0, 0, 1, RampWidth, 0, Rise, EarthLayers[0], Path, Earth);
			Arrivals[0].Add(FIntPoint(0, MidV + 1));
			break;
		}
		const FStair& Ramp = Stairs.Last();
		UE_LOG(LogTemp, Log, TEXT("RRRandom: big hill, its ramp's foot at %s, up toward (%d, %d)."),
			*this->At(CellCenter(Ramp.Start + Ramp.Across * (RampWidth / 2)) - FVector2D(Ramp.Dir.X, Ramp.Dir.Y) * BlockSize, 0.f).ToString(), Ramp.Dir.X, Ramp.Dir.Y);
	}
	// (and the top's: the foot of the pavilion's flight, a cell in from the top's front edge)
	Arrivals[Tiers - 1].Add(FIntPoint(Inset * (Tiers - 1) + 5, Inset * (Tiers - 1)));
	auto NearArrival = [&](int32 K, int32 U, int32 V)
	{
		for (const FIntPoint& Arrival : Arrivals[K])
		{
			if (FMath::Abs(U - Arrival.X) + FMath::Abs(V - Arrival.Y) <= (bShapes ? 4 : 3))
			{
				return true;
			}
		}
		return false;
	};
	// Round 96: grassy banks along each terrace's straight edges, 5 on and 2 off (the ends tapered), rising inward to a wall a block high:
	// something to crouch behind up there, not a fort (round 95's knee walls without the diagonal blocks)
	for (int32 K = 0; K < Tiers; ++K)
	{
		const int32 E = Inset * K;
		if (!bShapes)
		{
			for (int32 U = E; U <= W - 1 - E; ++U)
			{
				for (int32 V = E; V <= D - 1 - E; ++V)
				{
					const bool bEdge = U == E || U == W - 1 - E || V == E || V == D - 1 - E;
					if (bEdge && ((U + V) / 3) % 2 == 0 && !NearArrival(K, U, V))
					{
						Fill(Frame, U, V, U, V, Tops[K] + 1, Tops[K] + 1, WallLook);
					}
				}
			}
			continue;
		}
		// Each side: its first cell, the way along it, the way in, its length
		struct FSide { FIntPoint Start; FIntPoint Along; FIntPoint In; int32 Length; };
		const FSide Sides[] =
		{
			{ FIntPoint(E, E), FIntPoint(1, 0), FIntPoint(0, 1), W - 2 * E },
			{ FIntPoint(W - 1 - E, E), FIntPoint(0, 1), FIntPoint(-1, 0), D - 2 * E },
			{ FIntPoint(E, D - 1 - E), FIntPoint(1, 0), FIntPoint(0, -1), W - 2 * E },
			{ FIntPoint(E, E), FIntPoint(0, 1), FIntPoint(1, 0), D - 2 * E },
		};
		for (const FSide& Side : Sides)
		{
			int32 RunStart = INDEX_NONE;
			for (int32 T = 0; T <= Side.Length; ++T)
			{
				const FIntPoint Cell = Side.Start + Side.Along * T;
				const int32 FromEnd = FMath::Min(T, Side.Length - 1 - T);
				const bool bOn = T < Side.Length && FromEnd > Cuts[K] && (T - Cuts[K]) % 7 < 5 && !NearArrival(K, Cell.X, Cell.Y);
				if (bOn && RunStart == INDEX_NONE)
				{
					RunStart = T;
				}
				else if (!bOn && RunStart != INDEX_NONE)
				{
					const FIntPoint From = Side.Start + Side.Along * RunStart;
					AddSlopeRun(Frame, From.X, From.Y, Side.Along.X, Side.Along.Y, Side.In.X, Side.In.Y, T - RunStart, Tops[K] + 1, Dirt, GrassA);
					RunStart = INDEX_NONE;
				}
			}
		}
	}
	// (No dice or crates on the terraces: anything standing there is a step that brings the next cliff within a climb)

	// On top (the top terrace is W - 20 by D - 20, from cell 10 in): open structures, nothing to hide in (round 95: a house
	// up there was full cover with windows to shoot from; the hill's advantage is meant to cost being seen). An open
	// pavilion (pillars, a rail, a roof deck with crenels reached by an outside flight) and a watchtower
	const int32 T0 = Inset * (Tiers - 1);
	Lift = Tops[Tiers - 1];
	const int32 Style = Stream.RandRange(0, UE_ARRAY_COUNT(Styles) - 1);
	AddPavilion(At(T0 + 1, T0 + 6, 0), 9, Style);
	AddTower(At(T0 + 11, T0 + 11, 0), (Style + 2) % UE_ARRAY_COUNT(Styles));
	Lift = 0;
	// Round 96: where the bots go to take the hill: the floor under the pavilion's roof, and its roof deck (5 over the top)
	HillTops.Add(this->At(CellCenter(Frame.Cell(T0 + 7, T0 + 8)), Tops[Tiers - 1] * BlockSize));
	HillTops.Add(this->At(CellCenter(Frame.Cell(T0 + 5, T0 + 10)), (Tops[Tiers - 1] + 5) * BlockSize));
}

void ARRRandomDiceTown::AddPavilion(const FFrame& Frame, int32 N, int32 Style)
{
	// N x N: a pillar at each corner and the middle of each side, 4 blocks tall; a knee-high rail between them every other
	// cell, open in the middle of each side; a flat roof on top at 5 with crenels round it; a flight up the front from 5
	// cells out to the roof
	const FStyle& Colors = Styles[Style % UE_ARRAY_COUNT(Styles)];
	const uint8 PillarLook = Plain(Colors.Trim);
	const uint8 RailLook = MakeLook(Colors.Low, Wood);
	const uint8 RoofLook = MakeLook(Colors.Roof, Blend(Colors.Roof, White, 0.35f));
	const uint8 CrenelLook = Plain(Colors.High);
	constexpr int32 RoofLevel = 5;
	const int32 Mid = N / 2;
	for (int32 U = 0; U < N; ++U)
	{
		for (int32 V = 0; V < N; ++V)
		{
			const bool bEdgeU = U == 0 || U == N - 1;
			const bool bEdgeV = V == 0 || V == N - 1;
			if (!bEdgeU && !bEdgeV)
			{
				continue;
			}
			const bool bPillar = (bEdgeU || U == Mid) && (bEdgeV || V == Mid);
			if (bPillar)
			{
				Fill(Frame, U, V, U, V, 1, RoofLevel - 1, PillarLook);
			}
			else if ((U + V) % 2 == 0 && FMath::Abs(U - Mid) > 1 && FMath::Abs(V - Mid) > 1)
			{
				Fill(Frame, U, V, U, V, 1, 1, RailLook);
			}
			// The roof's crenels, open where the flight arrives (the front, V 0, in the middle)
			if ((U + V) % 2 == 0 && !(V == 0 && FMath::Abs(U - Mid) <= 1))
			{
				Fill(Frame, U, V, U, V, RoofLevel + 1, RoofLevel + 1, CrenelLook);
			}
		}
	}
	Fill(Frame, 0, 0, N - 1, N - 1, RoofLevel, RoofLevel, RoofLook);
	AddStairs(Frame, Mid - 1, -RoofLevel, 0, 1, 1, 0, 2, 0, RoofLevel, Colors.Trim);
}

void ARRRandomDiceTown::AddCanal(const FFrame& Frame, int32 Length, int32 Width)
{
	// Dug one block down to a sandy bed, water most of the way up; where it leaves the island, a waterfall
	const uint8 Bed = MakeLook(Dirt, Sand);
	for (int32 U = 0; U < Length; ++U)
	{
		for (int32 V = 0; V < Width; ++V)
		{
			const FIntPoint Cell = Frame.Cell(U, V);
			if (!IsOnIsland(Cell.X, Cell.Y))
			{
				continue;
			}
			Set(Cell.X, Cell.Y, 0, 0);
			Set(Cell.X, Cell.Y, -1, Bed);
			AddColored(Water, FTransform(FRotator::ZeroRotator, At(CellCenter(Cell), -65.f), FVector(1.f, 1.f, 0.7f)), WaterBlue, WaterBlue);
		}
	}
	for (int32 V = 0; V < Width; ++V)
	{
		int32 Edge = 0;
		while (Edge < Length && !IsOnIsland(Frame.Cell(Edge, V).X, Frame.Cell(Edge, V).Y))
		{
			++Edge;
		}
		if (Edge == 0 || Edge >= Length)
		{
			continue;
		}
		const FVector2D Fall = CellCenter(Frame.Cell(Edge - 1, V));
		for (int32 Drop = 0; Drop < 24; ++Drop)
		{
			const FColor Tint = Blend(WaterBlue, White, (Drop % 3) * 0.12f);
			AddColored(Water, FTransform(FRotator::ZeroRotator, At(Fall, -65.f - Drop * BlockSize), FVector(1.f, 1.f, 1.f)), Tint, Tint);
		}
	}
}

void ARRRandomDiceTown::AddRainbow(const FVector& Base, const FVector& Across, float Radius)
{
	// Seven bands of blocks on a half circle, snapped to a grid of blocks in its plane (a pixel rainbow)
	for (int32 Band = 0; Band < UE_ARRAY_COUNT(Rainbow); ++Band)
	{
		const float Bend = Radius - Band * BlockSize;
		TSet<FIntPoint> Done;
		for (float Angle = 0.f; Angle <= PI; Angle += 0.5f * BlockSize / Bend)
		{
			const FIntPoint Spot(FMath::RoundToInt(FMath::Cos(Angle) * Bend / BlockSize), FMath::RoundToInt(FMath::Sin(Angle) * Bend / BlockSize));
			if (Done.Contains(Spot))
			{
				continue;
			}
			Done.Add(Spot);
			const FVector Where = Base + Across * (Spot.X * BlockSize) + FVector(0.f, 0.f, Spot.Y * BlockSize);
			AddColored(Blocks, FTransform(FRotator(0.f, FMath::RadiansToDegrees(FMath::Atan2(Across.Y, Across.X)), 0.f), Where), Rainbow[Band], Rainbow[Band]);
		}
	}
}

void ARRRandomDiceTown::AddCloud(const FVector& Middle, float Size, FRandomStream& Stream)
{
	// A heap of flat white boxes, a hint of pink, lilac or blue
	static const FColor Puffs[] = { FColor(255, 255, 255), FColor(255, 232, 242), FColor(236, 228, 255), FColor(228, 240, 255) };
	const FColor Tint = Puffs[Stream.RandRange(0, UE_ARRAY_COUNT(Puffs) - 1)];
	const int32 Count = Stream.RandRange(3, 6);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Share = Index == 0 ? 1.f : Stream.FRandRange(0.45f, 0.8f);
		const FVector Offset = Index == 0 ? FVector::ZeroVector
			: FVector(Stream.FRandRange(-0.6f, 0.6f) * Size, Stream.FRandRange(-0.6f, 0.6f) * Size, Stream.FRandRange(-0.1f, 0.25f) * Size * 0.4f);
		const FVector Scale(Size * Share * Stream.FRandRange(0.9f, 1.4f), Size * Share * Stream.FRandRange(0.7f, 1.1f), Size * Share * 0.38f);
		AddColored(Clouds, FTransform(FRotator(0.f, 90.f * Stream.RandRange(0, 1), 0.f), Middle + Offset, Scale / BlockSize), Tint, Tint);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Building the town

void ARRRandomDiceTown::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Placed in a level (/Game/Maps/DiceTown): the editor shows the town, and its navmesh, as the game will build it
	if (!bBuilt && GetWorld() && GetWorld()->WorldType == EWorldType::Editor)
	{
		Build();
	}
}

void ARRRandomDiceTown::BeginPlay()
{
	Super::BeginPlay();
	// Nothing of an editor preview comes along into play; built afresh
	TInlineComponentArray<UActorComponent*> Preview(this);
	for (UActorComponent* Component : Preview)
	{
		if (Component != RootComponent)
		{
			Component->DestroyComponent();
		}
	}
	Build();
	// Its lava, each machine its own (round 89)
	if (GetWorld()->IsGameWorld())
	{
		SetUpLightEffects();
		FActorSpawnParameters Params;
		Params.Owner = this;
		Lava = GetWorld()->SpawnActor<ARRRandomLava>(ARRRandomLava::StaticClass(), FTransform::Identity, Params);
	}
}

void ARRRandomDiceTown::Build()
{
	UWorld* World = GetWorld();
	// A defense stage (round 90) is a smaller island: half the town's width, two thirds, then all of it
	static const float StageScales[] = { 1.f, 0.5f, 2.f / 3.f, 1.f };
	RimScale = StageScales[FMath::Clamp(DefenseStage, 0, 3)];
	HalfCells = TownHalfCells;
	TileOffset = FVector2D::ZeroVector;
	SkyScale = 1.f;
	// A battle map tile (round 92): a grid of its own size, standing at its place on the island; the island's rim is the
	// whole map's (as much bigger as the map is wider than the town), and so is the sky round it
	if (IsBattleTile())
	{
		HalfCells = FMath::Max(8, BattleTile.Cells / 2);
		TileOffset = FVector2D(BattleTile.X, BattleTile.Y) * float(BattleTile.Cells) * BlockSize;
		const float IslandHalf = (BattleTile.Reach * 2 + 1) * BattleTile.Cells * BlockSize * 0.5f;
		RimScale = IslandHalf / (TownHalfCells * BlockSize);
		SkyScale = RimScale;
	}
	GridCells = HalfCells * 2;
	MapHalf = HalfCells * BlockSize;
	Reserved.Reset();
	BigBuildingSpots.Reset();
	CoverPieces.Reset();
	BigBuildingsBuilt.Reset();
	LampLights.Reset();
	Stairs.Reset();
	StairCells.Reset();
	ShapeRuns.Reset();
	ShapedCells.Reset();
	HillTops.Reset();
	DefenseGates.Reset();
	// Loaded here rather than in the constructor, so the editor's tools still run before the assets are imported. Made by
	// char-pipeline/scripts/blender_block_town.py and ue_import_block_town.py
	BlockMesh = LoadMesh(TEXT("SM_BTBlock"));
	DieMesh = LoadMesh(TEXT("SM_BTDieBlock"));
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* CloudMaterial = LoadMaterial(TEXT("MI_BTCloud"));
	UMaterialInterface* WaterMaterial = LoadMaterial(TEXT("MI_BTWater"));
	if (!BlockMesh || !DieMesh || !Cube)
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: dice town meshes missing (/Game/Environment/DiceTown)."));
		return;
	}

	// The floor under the actor: the town stands on it and hides it (its own ground blocks are solid too). A battle map
	// tile stands at the map's floor height (the battle map hides the level's floor: most tiles have none under them)
	if (IsBattleTile())
	{
		FloorZ = BattleTile.FloorZ;
	}
	else
	{
		UPrimitiveComponent* Floor = RRMapFloor::Find(this, FloorSearchUp, FloorSearchDown, FloorZ);
		if (!Floor)
		{
			UE_LOG(LogTemp, Warning, TEXT("RRRandom: no floor under the dice town at %s."), *GetActorLocation().ToString());
			return;
		}
		Floor->SetVisibility(false);
		// Nor does anything stand on it in play: the town's own ground is solid, and its craters (round 89) go down past the
		// floor's height
		if (GetWorld()->IsGameWorld())
		{
			Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// The dreamier pass (round 47): on unless -RRTownClassic asks for the town as it was (and only with its shapes imported)
	PuffMesh = LoadMesh(TEXT("SM_BTPuff"));
	StarMesh = LoadMesh(TEXT("SM_BTStar"));
	MoonMesh = LoadMesh(TEXT("SM_BTMoon"));
	RoofMesh = LoadMesh(TEXT("SM_BTRoof"));
	bDream = !FParse::Param(FCommandLine::Get(), TEXT("RRTownClassic")) && PuffMesh && StarMesh && MoonMesh;

	Blocks = MakeInstances(BlockMesh, nullptr, false, 6);
	DieBlocks = MakeInstances(DieMesh, nullptr, false, 6);
	// Round 91 (reflections here and there): the plaza's tiles polished, the dice a glossy plastic, and puddles on the
	// roads mirror smooth, so the sky and the town show in them (Lumen reflections, DefaultEngine.ini)
	GlossBlocks = MakeInstances(BlockMesh, nullptr, false, 6);
	const auto Finish = [this](UInstancedStaticMeshComponent* Instances, float Roughness, float PixelStrength)
	{
		for (int32 Slot = 0; Instances && Slot < Instances->GetNumMaterials(); ++Slot)
		{
			UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Instances->GetMaterial(Slot), this);
			Dynamic->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			Dynamic->SetScalarParameterValue(TEXT("PixelStrength"), PixelStrength);
			Instances->SetMaterial(Slot, Dynamic);
		}
	};
	Finish(GlossBlocks, PlazaRoughness, 0.04f);
	Finish(DieBlocks, DiceRoughness, 0.05f);
	if (UStaticMesh* Disc = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		Puddles = MakeInstances(Disc, LoadMaterial(TEXT("M_BTBlock")), false, 6);
		Puddles->CastShadow = false;
		Finish(Puddles, PuddleRoughness, 0.f);
	}
	PuddleDiscs.Reset();
	Steps = MakeInstances(BlockMesh, nullptr, false, 6);
	// Round 96: the diagonal blocks (char-pipeline/scripts/blender_slope_blocks.py; without them the hills build as round 95's)
	SlopeMesh = LoadMesh(TEXT("SM_BTSlope"));
	SlantMesh = LoadMesh(TEXT("SM_BTSlant"));
	SlopeCornerMesh = LoadMesh(TEXT("SM_BTSlopeCorner"));
	const bool bDiagonal = SlopeMesh && SlantMesh && SlopeCornerMesh;
	SlopeBlocks = bDiagonal ? MakeInstances(SlopeMesh, nullptr, false, 6) : nullptr;
	SlantBlocks = bDiagonal ? MakeInstances(SlantMesh, nullptr, false, 6) : nullptr;
	SlopeCorners = bDiagonal ? MakeInstances(SlopeCornerMesh, nullptr, false, 6) : nullptr;
	Underside = MakeInstances(BlockMesh, nullptr, false, 6);
	Water = MakeInstances(BlockMesh, WaterMaterial, false, 6);
	Water->CastShadow = false;
	Clouds = MakeInstances(bDream ? PuffMesh.Get() : BlockMesh.Get(), CloudMaterial, false, 6);
	Clouds->CastShadow = false;
	if (bDream)
	{
		// Lit from within: the stars and moons
		UMaterialInstanceDynamic* StarGlow = UMaterialInstanceDynamic::Create(LoadMaterial(TEXT("M_BTBlock")), this);
		StarGlow->SetScalarParameterValue(TEXT("Glow"), 0.9f);
		UMaterialInstanceDynamic* MoonGlow = UMaterialInstanceDynamic::Create(LoadMaterial(TEXT("M_BTBlock")), this);
		MoonGlow->SetScalarParameterValue(TEXT("Glow"), 0.45f);
		Decor = MakeInstances(BlockMesh, nullptr, false, 6);
		UMaterialInstanceDynamic* NeonGlowing = UMaterialInstanceDynamic::Create(LoadMaterial(TEXT("M_BTBlock")), this);
		NeonGlowing->SetScalarParameterValue(TEXT("Glow"), NeonGlow);
		NeonGlowing->SetScalarParameterValue(TEXT("PixelStrength"), 0.f);
		Neon = MakeInstances(BlockMesh, NeonGlowing, false, 6);
		Neon->CastShadow = false;
		Stars = MakeInstances(StarMesh, StarGlow, false, 6);
		Stars->CastShadow = false;
		Moons = MakeInstances(MoonMesh, MoonGlow, false, 6);
		Moons->CastShadow = false;
		FloatDice = MakeInstances(DieMesh, nullptr, false, 6);
		if (RoofMesh)
		{
			Roofs = MakeInstances(RoofMesh, nullptr, false, 6);
		}
		Marshmallows = MakeInstances(PuffMesh, nullptr, false, 6);
		if (UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			Poles = MakeInstances(Cylinder, LoadMaterial(TEXT("M_BTBlock")), false, 6);
		}
		// Roofs, marshmallows and posts smooth and a little glossy: without the blocks' faint pixel grid
		for (UInstancedStaticMeshComponent* Smooth : { Roofs.Get(), Marshmallows.Get(), Poles.Get() })
		{
			for (int32 Slot = 0; Smooth && Slot < Smooth->GetNumMaterials(); ++Slot)
			{
				UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Smooth->GetMaterial(Slot), this);
				Dynamic->SetScalarParameterValue(TEXT("PixelStrength"), 0.02f);
				Dynamic->SetScalarParameterValue(TEXT("Roughness"), 0.42f);
				Smooth->SetMaterial(Slot, Dynamic);
			}
		}
		// The Tripo trees (all five, or none: the trees are blocks then)
		for (const TCHAR* Name : { TEXT("SM_DTRoundTree"), TEXT("SM_DTTierTree"), TEXT("SM_DTCandyTree"), TEXT("SM_DTMushroom"), TEXT("SM_DTLollipop") })
		{
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Environment/DiceTown/Props/%s.%s"), Name, Name)))
			{
				PropTrees.Add(MakeInstances(Mesh, nullptr, false, 0));
			}
		}
		if (PropTrees.Num() < 5)
		{
			PropTrees.Reset();
		}
	}
	// Round 94: the big buildings' Tripo cover models (any that are missing stay blocks)
	CoverModels.Reset();
	if (IsBattleTile())
	{
		for (int32 Model = 1; Model < int32(ECoverModel::Count); ++Model)
		{
			const TCHAR* Name = CoverModelNames[Model];
			UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("%s.%s"), Name[0] == TEXT('/') ? Name : *(FString(TEXT("/Game/Environment/DiceTown/Props/")) + Name), *FPaths::GetBaseFilename(Name)));
			UInstancedStaticMeshComponent* Instances = Mesh ? MakeInstances(Mesh, nullptr, false, 0) : nullptr;
			if (Instances)
			{
				Instances->CastShadow = true;
			}
			CoverModels.Add(Instances);
		}
	}
	// Everything solid: hidden boxes, merged from the grid, plus the stairs' slabs
	Solid = MakeInstances(Cube, nullptr, true, 0);
	Solid->SetVisibility(false);
	Solid->SetHiddenInGame(true);
	// The rim: a hidden wall that stops brawlers only; shots and the bots' sight (world static sweeps) pass
	RimWalls = MakeInstances(Cube, nullptr, false, 0);
	RimWalls->SetVisibility(false);
	RimWalls->SetHiddenInGame(true);
	RimWalls->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RimWalls->SetCollisionObjectType(ECC_WorldDynamic);
	RimWalls->SetCollisionResponseToAllChannels(ECR_Ignore);
	RimWalls->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	RimWalls->SetCanEverAffectNavigation(true);
	// (a battle map tile: round the whole island's middle, only the stretch of rim over its own square, in as many more
	// segments as the rim is longer)
	const FVector2D Center = FVector2D(GetActorLocation().X, GetActorLocation().Y) - TileOffset;
	const int32 Segments = FMath::RoundToInt(RimWallSegments * FMath::Max(1.f, RimScale));
	for (int32 Segment = 0; Segment < Segments; ++Segment)
	{
		const float Theta = UE_TWO_PI * Segment / Segments;
		const float Out = RimRadius(Theta) - 40.f;
		const float Length = UE_TWO_PI * (Out + 100.f) / Segments * 1.2f;
		const FVector2D Where(FMath::Cos(Theta) * (Out + 50.f), FMath::Sin(Theta) * (Out + 50.f));
		if (IsBattleTile() && (FMath::Abs(Where.X - TileOffset.X) > MapHalf || FMath::Abs(Where.Y - TileOffset.Y) > MapHalf))
		{
			continue;
		}
		RimWalls->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Theta), 0.f),
			FVector(Center.X + Where.X, Center.Y + Where.Y, FloorZ + RimWallHeight * 0.5f - 300.f),
			FVector(1.f, Length / BlockSize, RimWallHeight / BlockSize)), true);
	}

	Grid.SetNumZeroed(GridCells * GridCells * GridLevels);
	GiantDice.Reset();
	bFoundAnchored = false;
	DugDepth.Reset();
	LavaColumns.Reset();
	Falling.Reset();
	if (IsBattleTile())
	{
		LayOutBattle();
	}
	else if (DefenseStage > 0)
	{
		LayOutDefense();
	}
	else
	{
		LayOut();
	}
	MakeBlocks();
	MakeCollision();
	MakeWalkGraph();
	MakeUnderside();
	if (BuildsSky())
	{
		// A battle map's sky goes in blocks of its own, shown whatever the middle tile's streaming (its trees as blocks)
		TObjectPtr<UInstancedStaticMeshComponent> TownBlocks = Blocks;
		TObjectPtr<UInstancedStaticMeshComponent> TownDice = DieBlocks;
		TArray<TObjectPtr<UInstancedStaticMeshComponent>> TownTrees;
		if (IsBattleTile())
		{
			SkyBlocks = MakeInstances(BlockMesh, nullptr, false, 6);
			SkyDice = MakeInstances(DieMesh, nullptr, false, 6);
			Blocks = SkyBlocks;
			DieBlocks = SkyDice;
			TownTrees = MoveTemp(PropTrees);
			PropTrees.Reset();
		}
		MakeSky();
		if (bDream)
		{
			MakeDreamSky();
		}
		if (IsBattleTile())
		{
			Blocks = TownBlocks;
			DieBlocks = TownDice;
			PropTrees = MoveTemp(TownTrees);
		}
	}
	if (IsBattleTile())
	{
		MakeFarLook();
	}

	// Every material here as a dynamic instance, so the console can tune the colors live (Tick)
	TInlineComponentArray<UMeshComponent*> Meshes(this);
	for (UMeshComponent* Mesh : Meshes)
	{
		if (Mesh == Solid || Mesh == RimWalls)
		{
			continue;
		}
		for (int32 Slot = 0; Slot < Mesh->GetNumMaterials(); ++Slot)
		{
			UMaterialInterface* Material = Mesh->GetMaterial(Slot);
			UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
			if (!Dynamic && Material)
			{
				Dynamic = UMaterialInstanceDynamic::Create(Material, this);
				Mesh->SetMaterial(Slot, Dynamic);
			}
			if (Dynamic)
			{
				Tunable.Add(Dynamic);
			}
		}
	}

	bBuilt = true;
	// A battle map tile starts out of the scene (its far look shown): the battle map streams in the ones near the view
	if (IsBattleTile())
	{
		SetDetailShown(false);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town %d blocks, %d dice, %d steps, %d water, %d underside, %d cloud boxes, %d solid boxes, %d stairs, %d walk nodes (%d roamable), %d looks."),
		Blocks->GetInstanceCount(), DieBlocks->GetInstanceCount(), Steps->GetInstanceCount(), Water->GetInstanceCount(), Underside->GetInstanceCount(),
		Clouds->GetInstanceCount(), Solid->GetInstanceCount(), Stairs.Num(), Nodes.Num(), Roamable.Num(), Looks.Num());
	if (HasDiagonalBlocks() && ShapeRuns.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town diagonal blocks: %d slopes, %d corners, %d slants in %d runs; %d hill tops."),
			SlopeBlocks->GetInstanceCount(), SlopeCorners->GetInstanceCount(), SlantBlocks->GetInstanceCount(), ShapeRuns.Num(), HillTops.Num());
	}
	if (bDream)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town dream pass: %d decor, %d stars, %d moons, %d floating dice, %d floaters."),
			Decor->GetInstanceCount(), Stars->GetInstanceCount(), Moons->GetInstanceCount(), FloatDice->GetInstanceCount(), Floaters.Num());
		int32 PropCount = 0;
		for (const UInstancedStaticMeshComponent* Props : PropTrees)
		{
			PropCount += Props->GetInstanceCount();
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town meshes: %d roofs, %d Tripo trees, %d marshmallows."),
			Roofs ? Roofs->GetInstanceCount() : 0, PropCount, Marshmallows ? Marshmallows->GetInstanceCount() : 0);
	}
}

void ARRRandomDiceTown::LayOut()
{
	// The frames: the island's own (X, Y as they are) and a structure's inside another's
	const FFrame Island = FFrame();
	auto Sub = [](const FFrame& Parent, int32 U, int32 V, int32 Turn)
	{
		FFrame Frame;
		Frame.Origin = Parent.Cell(U, V);
		Frame.Turn = (Parent.Turn + Turn) & 3;
		Frame.bRed = Parent.bRed;
		return Frame;
	};
	// A die in a frame: its lowest corner cell, whichever way the frame turns
	auto DieAt = [this](const FFrame& Frame, int32 U, int32 V, int32 Level, int32 Size, int32 Kind)
	{
		const FIntPoint A = Frame.Cell(U, V);
		const FIntPoint B = Frame.Cell(U + Size - 1, V + Size - 1);
		AddDie(FIntPoint(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), Level, Size, Kind);
	};

	// Ground: three blocks deep, grass on top (now and then lime), paving on the roads, a pink and cream checker on the plaza
	const uint8 GrassLookA = MakeLook(Dirt, GrassA);
	const uint8 GrassLookB = MakeLook(Dirt, GrassB);
	const uint8 GrassLookLime = MakeLook(Dirt, GrassLime);
	const uint8 DirtLook = Plain(Dirt);
	const uint8 DeepLook = Plain(DirtDeep);
	const uint8 PaveLookA = MakeLook(Stone, PaveA);
	const uint8 PaveLookB = MakeLook(Stone, PaveB);
	// (round 91: polished, the sky and the town show in them)
	const uint8 PlazaLookA = MakeLook(Stone, PlazaPink, false, false, true);
	const uint8 PlazaLookB = MakeLook(Stone, PlazaCream, false, false, true);
	for (int32 X = -HalfCells; X < HalfCells; ++X)
	{
		for (int32 Y = -HalfCells; Y < HalfCells; ++Y)
		{
			if (!IsOnIsland(X, Y))
			{
				continue;
			}
			const bool bChecker = ((X + Y) & 1) == 0;
			const uint32 Roll = CellHash(X, Y, GroundSeed) % 100;
			uint8 Top = Roll < 8 ? GrassLookLime : Roll < 55 ? GrassLookA : GrassLookB;
			if (Cheb(X, Y) < PlazaEdge)
			{
				Top = bChecker ? PlazaLookA : PlazaLookB;
			}
			else if (IsRoad(X, Y))
			{
				Top = bChecker ? PaveLookA : PaveLookB;
			}
			Set(X, Y, -2, DeepLook);
			Set(X, Y, -1, DirtLook);
			Set(X, Y, 0, Top);
		}
	}

	// The plaza: a giant die on two tiers, ramps up all four sides, small dice round the edge
	Fill(Island, -8, -8, 7, 7, 1, 1, MakeLook(Shade(Lavender, 0.85f), Lavender));
	Fill(Island, -5, -5, 4, 4, 2, 2, MakeLook(Shade(Pink, 0.85f), Pink));
	AddDie(FIntPoint(-3, -3), 3, 6, 0);
	for (int32 Turn = 0; Turn < 4; ++Turn)
	{
		const FFrame Side = FFrame::Around(Turn);
		AddStairs(Side, -1, -9, 0, 1, 1, 0, 2, 0, 1, Cream);
		AddStairs(Side, -1, -6, 0, 1, 1, 0, 2, 1, 1, Cream);
		DieAt(Side, -11, -11, 1, 2, 1 + Turn);
		DieAt(Side, -11, -6, 1, 1, 7 + Turn * 6);
		DieAt(Side, -6, -11, 1, 1, 9 + Turn * 6);
	}

	// Each base: a courtyard paved in the team's color round its end of the main road, low walls and crates at its front,
	// a watchtower at either front corner; gates over the main roads where they enter downtown
	Reserve(FIntRect(-46, -16, -32, 16));
	Reserve(FIntRect(-33, -6, -30, 6));
	Reserve(FIntRect(-6, -33, 6, -30));
	Both(Island, [&](const FFrame& Half)
	{
		const FColor Tint = TeamTint[Half.bRed ? 1 : 0];
		const uint8 CourtA = MakeLook(Stone, Blend(PaveA, Tint, 0.45f));
		const uint8 CourtB = MakeLook(Stone, Blend(PaveB, Tint, 0.6f));
		for (int32 U = -45; U <= -33; ++U)
		{
			for (int32 V = -15; V <= 14; ++V)
			{
				const FIntPoint Cell = Half.Cell(U, V);
				if (!IsRoad(Cell.X, Cell.Y))
				{
					Set(Cell.X, Cell.Y, 0, ((U + V) & 1) ? CourtA : CourtB);
				}
			}
		}
		const uint8 Wall = MakeLook(Shade(Tint, 0.8f), Tint);
		Fill(Half, -34, -11, -34, -6, 1, 2, Wall);
		Fill(Half, -34, 5, -34, 10, 1, 2, Wall);
		const uint8 Crate = MakeLook(WoodDark, Wood);
		Fill(Half, -40, -7, -39, -6, 1, 1, Crate);
		Fill(Half, -40, 6, -39, 7, 1, 1, Crate);
		Fill(Half, -40, 6, -40, 6, 2, 2, Crate);
		AddTower(Sub(Half, -37, -11, 3), Half.bRed ? 0 : 2);
		AddTower(Sub(Half, -37, 13, 3), Half.bRed ? 0 : 2);

		// The gates: one over the main road, the same turned a quarter over the cross road
		const uint8 Pillar = Plain(White);
		const uint8 Beam = Plain(Half.bRed ? FColor(235, 70, 100) : FColor(70, 140, 240));
		for (const FFrame& Gate : { Half, Sub(FFrame::Around(Half.bRed ? 3 : 1), 0, 0, 0) })
		{
			Fill(Gate, -32, -5, -31, -4, 1, 8, Pillar);
			Fill(Gate, -32, 3, -31, 4, 1, 8, Pillar);
			Fill(Gate, -32, -5, -31, 4, 9, 9, Beam);
			for (int32 V = -5; V <= 4; V += 3)
			{
				Fill(Gate, -32, V, -31, V, 10, 10, Pillar);
			}
		}

		// Behind the base: a stepped hill, a watchtower on top
		AddHill(Sub(Half, -58, -12, 1), 24, 20, 3);
		Lift = 3;
		AddTower(Sub(Half, -69, 2, 0), 4);
		Lift = 0;
	});
	Reserve(FIntRect(-78, -13, -56, 13));

	// The canal quarter on each team's right: a canal from the rim (a waterfall) almost to the side road, three arched
	// footbridges over it
	const int32 BridgeAt[] = { -62, -42, -22 };
	Reserve(FIntRect(-90, 70, -5, 78));
	for (const int32 X : BridgeAt)
	{
		Reserve(FIntRect(X - 1, 68, X + 4, 80));
	}
	Both(Island, [&](const FFrame& Half)
	{
		AddCanal(Sub(Half, -90, 72, 0), 84, 4);
		const uint8 Deck = MakeLook(WoodDark, Wood);
		for (const int32 X : BridgeAt)
		{
			const FFrame Bridge = Sub(Half, X, 72, 0);
			AddStairs(Bridge, 0, -1, 0, 1, 1, 0, 3, 0, 1, Wood);
			Fill(Bridge, 0, 0, 2, 3, 1, 1, Deck);
			AddStairs(Bridge, 0, 4, 0, -1, 1, 0, 3, 0, 1, Wood);
		}
	});

	if (bDream)
	{
		// The dreamier pass: in the wood a mountain of giant dice with stairs to its top; over each main road a rainbow
		// of dice, where it crosses the ring outside the base and between downtown and the ring on the cross road
		const FFrame Mountain = Sub(Island, -39, -66, 2);
		Reserve(Footprint(Mountain, -1, -5, 12, 12, 1));
		Both(Mountain, [&](const FFrame& Where) { AddDiceMountain(Where); });
		FFrame ArchX;
		ArchX.Origin = FIntPoint(-54, 0);
		ArchX.Turn = 1;
		FFrame ArchY;
		ArchY.Origin = FIntPoint(0, -45);
		for (const FFrame& Arch : { ArchX, ArchY })
		{
			Reserve(Footprint(Arch, -12, 0, 11, 1, 1));
			Both(Arch, [&](const FFrame& Where) { AddDiceArch(Where); });
		}
	}
	else
	{
		// The wood on each team's left: a hill of giant dice (a flight up its first tier), then trees
		Reserve(FIntRect(-50, -78, -38, -63));
		Both(Island, [&](const FFrame& Half)
		{
			for (int32 I = 0; I < 3; ++I)
			{
				for (int32 J = 0; J < 3; ++J)
				{
					DieAt(Half, -48 + 3 * I, -76 + 3 * J, 1, 3, I * 3 + J + 6 * (I + J));
				}
			}
			for (int32 I = 0; I < 2; ++I)
			{
				for (int32 J = 0; J < 2; ++J)
				{
					DieAt(Half, -47 + 3 * I, -75 + 3 * J, 4, 3, 2 + I + J * 2 + 12);
				}
			}
			DieAt(Half, -46, -74, 7, 3, 5);
			AddStairs(Half, -45, -65, 0, -1, 1, 0, 2, 0, 3, Sand);
		});
	}

	FRandomStream Layout(LayoutSeed);
	// Cells of a rect all in a zone
	auto AllIn = [](const FIntRect& Rect, TFunctionRef<bool(int32, int32)> Zone)
	{
		for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X)
		{
			for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
			{
				if (!Zone(X, Y))
				{
					return false;
				}
			}
		}
		return true;
	};
	auto PlaceBuildings = [&](int32 Count, int32 Tries, const FIntRect& Area, TFunctionRef<bool(int32, int32)> Zone, int32 MinW, int32 MaxW, int32 MinD, int32 MaxD, float TwoStoryChance, float FlatChance)
	{
		int32 Placed = 0;
		for (int32 Try = 0; Try < Tries && Placed < Count; ++Try)
		{
			const int32 W = Layout.RandRange(MinW, MaxW);
			const int32 D = Layout.RandRange(MinD, MaxD);
			const int32 Stories = Layout.FRand() < TwoStoryChance ? 2 : 1;
			const bool bFlat = Stories > 1 && Layout.FRand() < FlatChance;
			const int32 Style = Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1);
			const int32 Seed = Layout.RandRange(0, 1 << 30);
			FFrame Frame;
			Frame.Turn = Layout.RandRange(0, 3);
			Frame.Origin = FIntPoint(Layout.RandRange(Area.Min.X, Area.Max.X), Layout.RandRange(Area.Min.Y, Area.Max.Y));
			const FIntRect Rect = Footprint(Frame, -1, -1, W + 1, D + 1, 1);
			if (!AllIn(Rect, Zone) || !IsFree(Rect))
			{
				continue;
			}
			Reserve(Rect);
			Both(Frame, [&](const FFrame& Where) { AddBuilding(Where, W, D, Stories, bFlat, Style, Seed); });
			++Placed;
		}
		return Placed;
	};
	// The dreamier pass: a house that is a giant die downtown and another outside the ring, on each half
	int32 DieHouses = 0;
	if (bDream)
	{
		auto PlaceDieHouse = [&](int32 Tries, const FIntRect& Area, TFunctionRef<bool(int32, int32)> Zone)
		{
			for (int32 Try = 0; Try < Tries; ++Try)
			{
				FFrame Frame;
				Frame.Turn = Layout.RandRange(0, 3);
				Frame.Origin = FIntPoint(Layout.RandRange(Area.Min.X, Area.Max.X), Layout.RandRange(Area.Min.Y, Area.Max.Y));
				const int32 Seed = Layout.RandRange(0, 1 << 30);
				const FIntRect Rect = Footprint(Frame, -1, -1, 14, 14, 1);
				if (AllIn(Rect, Zone) && IsFree(Rect))
				{
					Reserve(Rect);
					Both(Frame, [&](const FFrame& Where) { AddDieHouse(Where, Seed); });
					++DieHouses;
					return;
				}
			}
		};
		PlaceDieHouse(12000, FIntRect(-46, -46, 0, 46), [](int32 X, int32 Y) { return X < 0 && Cheb(X, Y) <= DowntownEdge; });
		PlaceDieHouse(12000, FIntRect(-88, -88, 0, 88), [](int32 X, int32 Y) { return X < -2 && Cheb(X, Y) >= RingOuter + 1; });
	}

	// Downtown round the plaza, mostly two stories; houses along the canal; cottages in the corners outside the ring
	const int32 Downtown = PlaceBuildings(16, 12000, FIntRect(-46, -46, 0, 46), [](int32 X, int32 Y) { return X < 0 && Cheb(X, Y) <= DowntownEdge; }, 9, 11, 10, 12, 0.85f, 0.45f);
	const int32 Canal = PlaceBuildings(6, 6000, FIntRect(-86, 50, 0, 72), [](int32 X, int32 Y) { return X < -2 && Y >= 53; }, 9, 11, 10, 12, 0.8f, 0.3f);
	const int32 Cottages = PlaceBuildings(8, 6000, FIntRect(-88, -88, 0, 88), [](int32 X, int32 Y) { return X < -2 && Cheb(X, Y) >= RingOuter + 1; }, 7, 9, 7, 9, 0.f, 0.f);

	// Watchtowers out in the open, for a look over the roofs
	int32 Towers = 0;
	for (int32 Try = 0; Try < 6000 && Towers < 5; ++Try)
	{
		FFrame Frame;
		Frame.Turn = Layout.RandRange(0, 3);
		Frame.Origin = FIntPoint(Layout.RandRange(-HalfCells, 0), Layout.RandRange(-HalfCells, HalfCells - 1));
		const int32 Style = Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1);
		const FIntRect Rect = Footprint(Frame, 0, -Story - 1, 3, 3, 1);
		if (!AllIn(Rect, [](int32 X, int32 Y) { return X < -2 && Cheb(X, Y) > DowntownEdge; }) || !IsFree(Rect))
		{
			continue;
		}
		Reserve(Rect);
		Both(Frame, [&](const FFrame& Where) { AddTower(Where, Style); });
		++Towers;
	}

	// Trees: thick in the wood, a few everywhere else outside downtown
	auto PlaceTrees = [&](int32 Count, int32 Tries, TFunctionRef<bool(int32, int32)> Zone)
	{
		for (int32 Try = 0, Placed = 0; Try < Tries && Placed < Count; ++Try)
		{
			const FIntPoint Cell(Layout.RandRange(-HalfCells, 0), Layout.RandRange(-HalfCells, HalfCells - 1));
			int32 Kind = Layout.RandRange(0, 1);
			const int32 Seed = Layout.RandRange(0, 1 << 30);
			if (bDream)
			{
				// A third green trees, the rest candy trees, spotted mushrooms and lollipops
				const int32 Roll = Layout.RandRange(0, 99);
				Kind = Roll < 34 ? Kind : Roll < 66 ? 2 : Roll < 84 ? 3 : 4;
			}
			const int32 Reach = Kind == 0 ? 1 : Kind == 3 ? 3 : 2;
			const FIntRect Rect(Cell.X - Reach - 1, Cell.Y - Reach - 1, Cell.X + Reach + 2, Cell.Y + Reach + 2);
			if (!AllIn(Rect, Zone) || !IsFree(Rect))
			{
				continue;
			}
			Reserve(Rect);
			AddTree(Cell, Kind, Seed);
			AddTree(FIntPoint(-1 - Cell.X, -1 - Cell.Y), Kind, Seed);
			++Placed;
		}
	};
	// (The dreamier pass tries four times as hard: its Tripo trees fill the wood out)
	const int32 TreeTries = bDream ? 4 : 1;
	PlaceTrees(55, 3000 * TreeTries, [](int32 X, int32 Y) { return X < -2 && Y <= -53; });
	PlaceTrees(25, 2000 * TreeTries, [](int32 X, int32 Y) { return X < -2 && Cheb(X, Y) > DowntownEdge; });

	// Cover: block walls, hedges, crates and dice, out in the open between downtown and the ring and beyond
	auto PlaceCover = [&](int32 Count, int32 Tries, TFunctionRef<bool(int32, int32)> Zone)
	{
		int32 Placed = 0;
		for (int32 Try = 0; Try < Tries && Placed < Count; ++Try)
		{
			FFrame Frame;
			Frame.Turn = Layout.RandRange(0, 3);
			Frame.Origin = FIntPoint(Layout.RandRange(-HalfCells, 0), Layout.RandRange(-HalfCells, HalfCells - 1));
			const int32 Piece = Layout.RandRange(0, 4);
			const int32 Kind = Layout.RandRange(0, 47);
			const int32 Tall = Layout.RandRange(1, 2);
			const FColor Color = Styles[Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1)].Low;
			const FIntRect Rect = Footprint(Frame, 0, 0, Piece == 0 ? 3 : 1, Piece == 0 ? 0 : 1, 1);
			if (!AllIn(Rect, Zone) || !IsFree(Rect))
			{
				continue;
			}
			Reserve(Rect);
			Both(Frame, [&](const FFrame& Where)
			{
				switch (Piece)
				{
				case 0:
					Fill(Where, 0, 0, 3, 0, 1, 2, MakeLook(Shade(Color, 0.85f), Color));
					break;
				case 1:
					if (Marshmallows)
					{
						// The dreamier pass: a round hedge, a big marshmallow and two smaller ones on it, over hidden cells
						const FColor Green = Leaves[Kind % 3];
						Fill(Where, 0, 0, 1, 1, 1, Tall, HiddenLook(Green));
						AddColored(Marshmallows, FTransform(FRotator(0.f, FrameYaw(Where), 0.f), FrameSpot(Where, 0.5f, 0.5f, Tall * 50.f), FVector(2.15f, 2.15f, Tall * 1.05f)), Shade(Green, 0.9f), Blend(Green, White, 0.15f));
						AddColored(Marshmallows, FTransform(FRotator(0.f, FrameYaw(Where) + 20.f, 0.f), FrameSpot(Where, 0.15f, 0.25f, Tall * BlockSize + 10.f), FVector(1.2f, 1.1f, 0.75f)), Green, Blend(Green, White, 0.25f));
						AddColored(Marshmallows, FTransform(FRotator(0.f, FrameYaw(Where) - 25.f, 0.f), FrameSpot(Where, 0.9f, 0.75f, Tall * BlockSize - 5.f), FVector(0.95f, 1.05f, 0.65f)), Green, Blend(Green, White, 0.25f));
						break;
					}
					Fill(Where, 0, 0, 1, 1, 1, Tall, MakeLook(Shade(Leaves[0], 0.8f), Leaves[Kind % 3]));
					break;
				case 2:
					Fill(Where, 0, 0, 1, 1, 1, 1, MakeLook(WoodDark, Wood));
					Fill(Where, 0, 0, 0, 0, 2, Tall + 1, MakeLook(WoodDark, Wood));
					break;
				case 3:
					DieAt(Where, 0, 0, 1, 2, Kind);
					break;
				default:
					DieAt(Where, 0, 0, 1, 1, Kind);
					DieAt(Where, 1, 1, 1, 1, Kind + 1);
					DieAt(Where, 1, 0, 1, 1, Kind + 2);
					DieAt(Where, 1, 0, 2, 1, Kind + 3);
					break;
				}
			});
			++Placed;
		}
		return Placed;
	};
	const int32 Middle = PlaceCover(26, 3000, [](int32 X, int32 Y) { return X < 0 && Cheb(X, Y) > DowntownEdge && Cheb(X, Y) < RingInner; });
	const int32 Outer = PlaceCover(30, 3000, [](int32 X, int32 Y) { return X < 0 && Cheb(X, Y) >= RingOuter; });
	const int32 Inner = PlaceCover(10, 2000, [](int32 X, int32 Y) { return X < 0 && Cheb(X, Y) <= DowntownEdge; });
	if (bDream)
	{
		AddStarLamps();
	}
	AddPuddles();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town placed %d downtown, %d canal and %d cottage buildings, %d towers, %d + %d + %d cover (each half)."),
		Downtown, Canal, Cottages, Towers, Inner, Middle, Outer);
	if (bDream)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town dream pass placed %d die houses (each half)."), DieHouses);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Defense stages (round 90)

namespace
{
	// The roads out to the gates in the order the gates open: across from each other first, then the cross, then the
	// diagonals (degrees from +X toward +Y)
	constexpr float LaneAngles[RRDefense::MaxGates] = { 0.f, 180.f, 90.f, 270.f, 45.f, 225.f, 135.f, 315.f };
	// Each road this many cells either side of its middle line; the plaza inside the castle wall, the wall's footprint and the
	// ring road round it outside (cells, Chebyshev)
	constexpr float LaneHalf = 3.f;
	constexpr float CastleIn = RRDefense::WallRingCells;
	constexpr float CastleOut = RRDefense::WallRingCells + RRDefense::WallThickCells;
	constexpr float RingRoadIn = CastleOut;
	constexpr float RingRoadOut = CastleOut + 5.f;
	// Houses and the rest stand this far out at least (cells)
	constexpr float DefenseBuildFrom = RingRoadOut + 2.f;
	// A gate stands this many cells in from the rim, its villains starting a few cells further in
	constexpr float GateFromRim = 6.f;
	constexpr float GateStartIn = 3.f;
	const FColor GatePillar(92, 60, 120);
	const FColor GateTop(255, 214, 70);
}

void ARRRandomDiceTown::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomDiceTown, DefenseStage);
	DOREPLIFETIME(ARRRandomDiceTown, BattleTile);
}

FVector2D ARRRandomDiceTown::GetLaneWay(int32 Lane)
{
	const float Radians = FMath::DegreesToRadians(LaneAngles[FMath::Clamp(Lane, 0, RRDefense::MaxGates - 1)]);
	return FVector2D(FMath::Cos(Radians), FMath::Sin(Radians));
}

bool ARRRandomDiceTown::IsDefenseRoad(int32 X, int32 Y) const
{
	const float Distance = Cheb(X, Y);
	if (Distance < RingRoadIn)
	{
		return false;
	}
	if (Distance < RingRoadOut)
	{
		return true;
	}
	const FVector2D Local(X + 0.5f, Y + 0.5f);
	for (int32 Lane = 0; Lane < RRDefense::MaxGates; ++Lane)
	{
		const FVector2D Way = GetLaneWay(Lane);
		if (FVector2D::DotProduct(Local, Way) > 0.f && FMath::Abs(FVector2D::CrossProduct(Local, Way)) < LaneHalf)
		{
			return true;
		}
	}
	return false;
}

void ARRRandomDiceTown::GetDefenseGates(TArray<FVector>& OutGates) const
{
	OutGates.Reset();
	for (const FVector2D& Gate : DefenseGates)
	{
		OutGates.Add(At(Gate, 0.f));
	}
}

bool ARRRandomDiceTown::IsCastle(int32 X, int32 Y) const
{
	return DefenseStage > 0 && Cheb(X, Y) < CastleOut;
}

float ARRRandomDiceTown::GetDefenseRingRadius() const
{
	return (RRDefense::WallRingCells + 0.5f) * BlockSize;
}

float ARRRandomDiceTown::GetDefenseRimRadius() const
{
	return RimBase * RimScale;
}

void ARRRandomDiceTown::LayOutDefense()
{
	const int32 Stage = FMath::Clamp(DefenseStage, 1, 3);
	FRandomStream Layout(LayoutSeed + 100 * Stage);
	// How much room there is round the ring, against the whole town's
	const float Area = RimScale * RimScale;

	// Ground: grass, the paved roads and ring road, the plaza's pink and cream checker inside the ring
	const uint8 GrassLookA = MakeLook(Dirt, GrassA);
	const uint8 GrassLookB = MakeLook(Dirt, GrassB);
	const uint8 GrassLookLime = MakeLook(Dirt, GrassLime);
	const uint8 DirtLook = Plain(Dirt);
	const uint8 DeepLook = Plain(DirtDeep);
	const uint8 PaveLookA = MakeLook(Stone, PaveA);
	const uint8 PaveLookB = MakeLook(Stone, PaveB);
	// (round 91: polished, the sky and the town show in them)
	const uint8 PlazaLookA = MakeLook(Stone, PlazaPink, false, false, true);
	const uint8 PlazaLookB = MakeLook(Stone, PlazaCream, false, false, true);
	for (int32 X = -HalfCells; X < HalfCells; ++X)
	{
		for (int32 Y = -HalfCells; Y < HalfCells; ++Y)
		{
			if (!IsOnIsland(X, Y))
			{
				continue;
			}
			const bool bChecker = ((X + Y) & 1) == 0;
			const uint32 Roll = CellHash(X, Y, GroundSeed) % 100;
			uint8 Top = Roll < 8 ? GrassLookLime : Roll < 55 ? GrassLookA : GrassLookB;
			if (Cheb(X, Y) < CastleIn)
			{
				Top = bChecker ? PlazaLookA : PlazaLookB;
			}
			else if (Cheb(X, Y) < CastleOut)
			{
				Top = PaveLookB;
			}
			else if (IsDefenseRoad(X, Y))
			{
				Top = bChecker ? PaveLookA : PaveLookB;
			}
			Set(X, Y, -2, DeepLook);
			Set(X, Y, -1, DirtLook);
			Set(X, Y, 0, Top);
		}
	}
	// The plaza and the ring are kept clear (the tower, the walls, the friends' start, the cannons and stations)
	Reserve(FIntRect(-int32(RingRoadOut) - 1, -int32(RingRoadOut) - 1, int32(RingRoadOut) + 1, int32(RingRoadOut) + 1));
	// The castle wall round the plaza: cream stone WallLevels high and WallThickCells thick (low, to shoot down from), a walkway
	// on top (its top blocks lilac), battlements on every other cell of its outer edge; the east and west gateways cut through
	// it open to the sky between pink posts a level taller (the gate doors fill them). Unbreakable (IsCastle). Inside, a flight
	// of stairs up to the walkway on either side of each wall's middle.
	const int32 WallTop = RRDefense::WallLevels;
	const uint8 WallStone = MakeLook(Shade(Cream, 0.86f), Cream);
	const uint8 WallWalk = MakeLook(Shade(Cream, 0.86f), Blend(Lavender, White, 0.45f));
	const uint8 WallMerlon = MakeLook(Shade(Lavender, 0.85f), Lavender);
	const uint8 GatePost = MakeLook(Shade(Pink, 0.85f), Pink);
	const uint8 GateCap = MakeLook(Shade(Pink, 0.85f), Lemon);
	auto IsGateway = [](int32 X, int32 Y, float Reach)
	{
		return FMath::Abs(Y + 0.5f) < Reach && FMath::Abs(X + 0.5f) >= CastleIn;
	};
	for (int32 X = -int32(CastleOut); X < int32(CastleOut); ++X)
	{
		for (int32 Y = -int32(CastleOut); Y < int32(CastleOut); ++Y)
		{
			const float Ring = Cheb(X, Y);
			if (Ring < CastleIn || Ring >= CastleOut || IsGateway(X, Y, RRDefense::GateHalfCells))
			{
				continue;
			}
			// The posts either side of a gateway: a level over the walkway, capped in lemon
			if (IsGateway(X, Y, RRDefense::GateHalfCells + 1))
			{
				Fill(FFrame(), X, Y, X, Y, 1, WallTop + 1, GatePost);
				Set(X, Y, WallTop + 2, GateCap);
				continue;
			}
			for (int32 Level = 1; Level <= WallTop; ++Level)
			{
				Set(X, Y, Level, Level == WallTop ? WallWalk : WallStone);
			}
			// The battlements: on the outer row, every other cell
			if (Ring >= CastleOut - 1.f && ((X + Y) & 1) == 0)
			{
				Set(X, Y, WallTop + 1, WallMerlon);
			}
		}
	}
	for (int32 Turn = 0; Turn < 4; ++Turn)
	{
		const FFrame Side = FFrame::Around(Turn);
		// Running out toward the wall from the plaza, arriving on the walkway
		AddStairs(Side, int32(CastleIn) - WallTop, 6, 1, 0, 0, 1, 2, 0, WallTop, Cream);
		AddStairs(Side, int32(CastleIn) - WallTop, -8, 1, 0, 0, 1, 2, 0, WallTop, Cream);
	}
	// Room kept for the outer jump ramp and the run-up to it (ARRRandomJumpRamp, north of the castle)
	const int32 RampCellX = FMath::FloorToInt(RRDefense::OuterRampX / BlockSize);
	Reserve(FIntRect(RampCellX - 3, int32(RingRoadOut) - 1, RampCellX + 3, FMath::CeilToInt(RRDefense::OuterRampFootY / BlockSize) + 6));
	// and its mirror on the south side, for the outer plane station
	Reserve(FIntRect(RampCellX - 4, -FMath::CeilToInt(RRDefense::OuterRampFootY / BlockSize) - 6, RampCellX + 4, -int32(RingRoadOut) + 1));

	// The gates: on each road a few cells in from the rim, two pillars and a beam over the road, villain purple with a
	// cheese-yellow top; the villains come in just inside
	const uint8 Pillar = MakeLook(GatePillar, Shade(GatePillar, 1.2f));
	const uint8 Beam = MakeLook(Shade(GatePillar, 0.85f), GateTop);
	auto CellAt = [](const FVector2D& Spot) { return FIntPoint(FMath::FloorToInt(Spot.X), FMath::FloorToInt(Spot.Y)); };
	for (int32 Lane = 0; Lane < RRDefense::MaxGates; ++Lane)
	{
		const FVector2D Way = GetLaneWay(Lane);
		const FVector2D Across(-Way.Y, Way.X);
		const float Theta = FMath::Atan2(Way.Y, Way.X);
		const float GateOut = RimRadius(Theta) / BlockSize - GateFromRim;
		DefenseGates.Add(Way * (GateOut - GateStartIn) * BlockSize);
		for (const float Side : { -1.f, 1.f })
		{
			const FIntPoint Foot = CellAt(Way * GateOut + Across * Side * (LaneHalf + 1.f));
			for (int32 DX = 0; DX < 2; ++DX)
			{
				for (int32 DY = 0; DY < 2; ++DY)
				{
					for (int32 Level = 1; Level <= 7; ++Level)
					{
						Set(Foot.X + DX, Foot.Y + DY, Level, Level == 7 ? Beam : Pillar);
					}
				}
			}
			Reserve(FIntRect(Foot.X - 1, Foot.Y - 1, Foot.X + 3, Foot.Y + 3));
		}
		for (float Step = -LaneHalf - 1.f; Step <= LaneHalf + 2.f; Step += 0.5f)
		{
			for (int32 Deep = 0; Deep < 2; ++Deep)
			{
				const FIntPoint Cell = CellAt(Way * (GateOut + Deep) + Across * Step);
				Set(Cell.X, Cell.Y, 8, Beam);
			}
			const FIntPoint Top = CellAt(Way * GateOut + Across * Step);
			Set(Top.X, Top.Y, 9, (FMath::FloorToInt(Step * 2.f) & 1) ? Beam : Pillar);
		}
	}

	auto AllIn = [](const FIntRect& Rect, TFunctionRef<bool(int32, int32)> Zone)
	{
		for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X)
		{
			for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
			{
				if (!Zone(X, Y))
				{
					return false;
				}
			}
		}
		return true;
	};
	auto Outside = [](int32 X, int32 Y) { return Cheb(X, Y) >= DefenseBuildFrom; };
	const int32 Reach = FMath::CeilToInt(RimBase * RimScale / BlockSize) + 2;
	auto RandomFrame = [&]()
	{
		FFrame Frame;
		Frame.Turn = Layout.RandRange(0, 3);
		Frame.Origin = FIntPoint(Layout.RandRange(-Reach, Reach), Layout.RandRange(-Reach, Reach));
		Frame.bRed = Layout.FRand() < 0.5f;
		return Frame;
	};

	// The dreamier pass: from stage 2 a rainbow of dice over each of the four straight roads, halfway out; on stage 3 a
	// mountain of giant dice in two of the quarters
	int32 Arches = 0;
	int32 Mountains = 0;
	if (bDream && Stage >= 2)
	{
		const int32 Out = FMath::RoundToInt((RingRoadOut + (RimBase * RimScale / BlockSize - GateFromRim)) * 0.5f);
		const FIntPoint Ways[] = { FIntPoint(1, 0), FIntPoint(0, 1), FIntPoint(-1, 0), FIntPoint(0, -1) };
		for (int32 Turn = 0; Turn < 4; ++Turn)
		{
			// Its U runs across the road
			FFrame Arch;
			Arch.Origin = Ways[Turn] * Out;
			Arch.Turn = Turn == 0 || Turn == 2 ? 1 : 0;
			Reserve(Footprint(Arch, -12, 0, 11, 1, 1));
			AddDiceArch(Arch);
			++Arches;
		}
	}
	if (bDream && Stage >= 3)
	{
		for (int32 Try = 0; Try < 6000 && Mountains < 2; ++Try)
		{
			const FFrame Frame = RandomFrame();
			const FIntRect Rect = Footprint(Frame, -1, -5, 12, 12, 1);
			if (AllIn(Rect, Outside) && IsFree(Rect))
			{
				Reserve(Rect);
				AddDiceMountain(Frame);
				++Mountains;
			}
		}
	}

	// Houses between the roads (the bigger the stage, the more and the taller), a giant die house or two from stage 2
	int32 DieHouses = 0;
	if (bDream && Stage >= 2)
	{
		for (int32 Try = 0; Try < 8000 && DieHouses < Stage - 1; ++Try)
		{
			const FFrame Frame = RandomFrame();
			const FIntRect Rect = Footprint(Frame, -1, -1, 14, 14, 1);
			if (AllIn(Rect, Outside) && IsFree(Rect))
			{
				Reserve(Rect);
				AddDieHouse(Frame, Layout.RandRange(0, 1 << 30));
				++DieHouses;
			}
		}
	}
	int32 Buildings = 0;
	const int32 WantBuildings = FMath::RoundToInt(30.f * Area);
	const float TwoStoryChance = Stage == 1 ? 0.3f : Stage == 2 ? 0.6f : 0.8f;
	for (int32 Try = 0; Try < 20000 && Buildings < WantBuildings; ++Try)
	{
		const int32 W = Layout.RandRange(7, 11);
		const int32 D = Layout.RandRange(7, 12);
		const int32 Stories = Layout.FRand() < TwoStoryChance ? 2 : 1;
		const bool bFlat = Stories > 1 && Layout.FRand() < 0.4f;
		const int32 Style = Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1);
		const int32 Seed = Layout.RandRange(0, 1 << 30);
		const FFrame Frame = RandomFrame();
		const FIntRect Rect = Footprint(Frame, -1, -1, W + 1, D + 1, 1);
		if (!AllIn(Rect, Outside) || !IsFree(Rect))
		{
			continue;
		}
		Reserve(Rect);
		AddBuilding(Frame, W, D, Stories, bFlat, Style, Seed);
		++Buildings;
	}
	// Watchtowers by the roads, for a look down them
	int32 Towers = 0;
	for (int32 Try = 0; Try < 8000 && Towers < Stage + 1; ++Try)
	{
		const FFrame Frame = RandomFrame();
		const FIntRect Rect = Footprint(Frame, 0, -Story - 1, 3, 3, 1);
		if (!AllIn(Rect, Outside) || !IsFree(Rect))
		{
			continue;
		}
		Reserve(Rect);
		AddTower(Frame, Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1));
		++Towers;
	}
	// Trees
	int32 Trees = 0;
	const int32 WantTrees = FMath::RoundToInt(90.f * Area);
	for (int32 Try = 0; Try < 20000 && Trees < WantTrees; ++Try)
	{
		const FIntPoint Cell(Layout.RandRange(-Reach, Reach), Layout.RandRange(-Reach, Reach));
		int32 Kind = Layout.RandRange(0, 1);
		const int32 Seed = Layout.RandRange(0, 1 << 30);
		if (bDream)
		{
			const int32 Roll = Layout.RandRange(0, 99);
			Kind = Roll < 34 ? Kind : Roll < 66 ? 2 : Roll < 84 ? 3 : 4;
		}
		const int32 Room = Kind == 0 ? 1 : Kind == 3 ? 3 : 2;
		const FIntRect Rect(Cell.X - Room - 1, Cell.Y - Room - 1, Cell.X + Room + 2, Cell.Y + Room + 2);
		if (!AllIn(Rect, Outside) || !IsFree(Rect))
		{
			continue;
		}
		Reserve(Rect);
		AddTree(Cell, Kind, Seed);
		++Trees;
	}
	// Cover beside the roads: block walls, hedges, crates and dice, to fight the villains from as they come
	auto DieAt = [this](const FFrame& Frame, int32 U, int32 V, int32 Level, int32 Size, int32 DieKind)
	{
		const FIntPoint A = Frame.Cell(U, V);
		const FIntPoint B = Frame.Cell(U + Size - 1, V + Size - 1);
		AddDie(FIntPoint(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), Level, Size, DieKind);
	};
	int32 Cover = 0;
	const int32 WantCover = FMath::RoundToInt(80.f * Area) + 10;
	for (int32 Try = 0; Try < 20000 && Cover < WantCover; ++Try)
	{
		const FFrame Where = RandomFrame();
		const int32 Piece = Layout.RandRange(0, 4);
		const int32 Kind = Layout.RandRange(0, 47);
		const int32 Tall = Layout.RandRange(1, 2);
		const FColor Color = Styles[Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1)].Low;
		const FIntRect Rect = Footprint(Where, 0, 0, Piece == 0 ? 3 : 1, Piece == 0 ? 0 : 1, 1);
		if (!AllIn(Rect, [](int32 X, int32 Y) { return Cheb(X, Y) >= RingRoadOut; }) || !IsFree(Rect))
		{
			continue;
		}
		Reserve(Rect);
		switch (Piece)
		{
		case 0:
			Fill(Where, 0, 0, 3, 0, 1, 2, MakeLook(Shade(Color, 0.85f), Color));
			break;
		case 1:
			if (Marshmallows)
			{
				const FColor Green = Leaves[Kind % 3];
				Fill(Where, 0, 0, 1, 1, 1, Tall, HiddenLook(Green));
				AddColored(Marshmallows, FTransform(FRotator(0.f, FrameYaw(Where), 0.f), FrameSpot(Where, 0.5f, 0.5f, Tall * 50.f), FVector(2.15f, 2.15f, Tall * 1.05f)), Shade(Green, 0.9f), Blend(Green, White, 0.15f));
				AddColored(Marshmallows, FTransform(FRotator(0.f, FrameYaw(Where) + 20.f, 0.f), FrameSpot(Where, 0.15f, 0.25f, Tall * BlockSize + 10.f), FVector(1.2f, 1.1f, 0.75f)), Green, Blend(Green, White, 0.25f));
				break;
			}
			Fill(Where, 0, 0, 1, 1, 1, Tall, MakeLook(Shade(Leaves[0], 0.8f), Leaves[Kind % 3]));
			break;
		case 2:
			Fill(Where, 0, 0, 1, 1, 1, 1, MakeLook(WoodDark, Wood));
			Fill(Where, 0, 0, 0, 0, 2, Tall + 1, MakeLook(WoodDark, Wood));
			break;
		case 3:
			DieAt(Where, 0, 0, 1, 2, Kind);
			break;
		default:
			DieAt(Where, 0, 0, 1, 1, Kind);
			DieAt(Where, 1, 1, 1, 1, Kind + 1);
			DieAt(Where, 1, 0, 1, 1, Kind + 2);
			DieAt(Where, 1, 0, 2, 1, Kind + 3);
			break;
		}
		++Cover;
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: defense stage %d (x%.2f): %d gates, %d buildings, %d die houses, %d towers, %d trees, %d cover, %d arches, %d dice mountains."),
		Stage, RimScale, DefenseGates.Num(), Buildings, DieHouses, Towers, Trees, Cover, Arches, Mountains);
	AddPuddles();
}

void ARRRandomDiceTown::MakeBlocks()
{
	// A block wherever one shows (a face against an empty cell), each a shade lighter or darker at random; the bottom of
	// the ground shows only past the edge of the cone of blocks under it
	constexpr float UnderShare = 0.92f;
	// (a battle map tile: past its edge is the next tile's ground, so the ground's sides there don't show; a block under a
	// model hides nothing: the floor under a Tripo cover model shows)
	auto Filled = [this](int32 X, int32 Y, int32 Level)
	{
		return HidesFace(X, Y, Level) || IsNeighborGround(X, Y, Level);
	};
	for (int32 Level = LevelMin; Level <= LevelMax; ++Level)
	{
		for (int32 Y = -HalfCells; Y < HalfCells; ++Y)
		{
			for (int32 X = -HalfCells; X < HalfCells; ++X)
			{
				const uint8 Value = Get(X, Y, Level);
				if (Value == 0 || Value == HiddenSolid || Looks[Value - 1].bUnderMesh)
				{
					continue;
				}
				bool bShows = !Filled(X + 1, Y, Level) || !Filled(X - 1, Y, Level) || !Filled(X, Y + 1, Level) || !Filled(X, Y - 1, Level) || !HidesFace(X, Y, Level + 1);
				if (!bShows)
				{
					if (Level > -2)
					{
						bShows = !HidesFace(X, Y, Level - 1);
					}
					else
					{
						const FVector2D Local = IslandSpot(X, Y);
						bShows = Local.Size() > RimRadius(FMath::Atan2(Local.Y, Local.X)) * UnderShare - 200.f;
					}
				}
				if (bShows)
				{
					AddCellBlock(X, Y, Level);
				}
			}
		}
	}

	// The minimap: each cell's top
	MapColors.Init(MapSky, GridCells * GridCells);
	MapLevels.Init(-1, GridCells * GridCells);
	for (int32 Y = -HalfCells; Y < HalfCells; ++Y)
	{
		for (int32 X = -HalfCells; X < HalfCells; ++X)
		{
			if (!IsOnIsland(X, Y))
			{
				continue;
			}
			const int32 Index = (Y + HalfCells) * GridCells + (X + HalfCells);
			for (int32 Level = LevelMax; Level >= LevelMin; --Level)
			{
				const uint8 Value = Get(X, Y, Level);
				if (Value)
				{
					MapColors[Index] = Value == HiddenSolid ? White : Level < 0 ? MapWater : Looks[Value - 1].Accent;
					MapLevels[Index] = int8(FMath::Clamp(Level, 0, 20));
					break;
				}
			}
			if (StairCells.Contains(FIntPoint(X, Y)))
			{
				MapColors[Index] = Cream;
				MapLevels[Index] = int8(FMath::Max(1, int32(MapLevels[Index])));
			}
		}
	}
}

void ARRRandomDiceTown::MakeCollision()
{
	// Greedy boxes: from each solid block not yet covered, as far as it goes along X, then Y, then up
	TBitArray<> Covered(false, Grid.Num());
	// (Remembered by box and by cell, for cutting round wrecks)
	FirstGridBox = Solid->GetInstanceCount();
	GridBoxes.Reset();
	BoxOfCell.Init(INDEX_NONE, Grid.Num());
	auto IsOpen = [&](int32 X, int32 Y, int32 Level)
	{
		return IsInGrid(X, Y, Level) && Grid[GridIndex(X, Y, Level)] != 0 && !Covered[GridIndex(X, Y, Level)];
	};
	for (int32 Level = LevelMin; Level <= LevelMax; ++Level)
	{
		for (int32 Y = -HalfCells; Y < HalfCells; ++Y)
		{
			for (int32 X = -HalfCells; X < HalfCells; ++X)
			{
				if (!IsOpen(X, Y, Level))
				{
					continue;
				}
				int32 X1 = X;
				while (IsOpen(X1 + 1, Y, Level))
				{
					++X1;
				}
				auto RowOpen = [&](int32 AtY, int32 AtLevel)
				{
					for (int32 I = X; I <= X1; ++I)
					{
						if (!IsOpen(I, AtY, AtLevel))
						{
							return false;
						}
					}
					return true;
				};
				int32 Y1 = Y;
				while (RowOpen(Y1 + 1, Level))
				{
					++Y1;
				}
				int32 L1 = Level;
				for (bool bGrow = true; bGrow; )
				{
					for (int32 J = Y; J <= Y1 && bGrow; ++J)
					{
						bGrow = RowOpen(J, L1 + 1);
					}
					L1 += bGrow ? 1 : 0;
				}
				for (int32 L = Level; L <= L1; ++L)
				{
					for (int32 J = Y; J <= Y1; ++J)
					{
						for (int32 I = X; I <= X1; ++I)
						{
							Covered[GridIndex(I, J, L)] = true;
							BoxOfCell[GridIndex(I, J, L)] = GridBoxes.Num();
						}
					}
				}
				GridBoxes.Add({ FIntVector(X, Y, Level), FIntVector(X1, Y1, L1) });
				TopLevel = FMath::Max(TopLevel, L1);
				const FVector2D Middle = (CellCenter(FIntPoint(X, Y)) + CellCenter(FIntPoint(X1, Y1))) * 0.5f;
				const float Bottom = (Level - 1) * BlockSize;
				const float Top = L1 * BlockSize;
				Solid->AddInstance(FTransform(FRotator::ZeroRotator, At(Middle, (Bottom + Top) * 0.5f),
					FVector(X1 - X + 1, Y1 - Y + 1, (Top - Bottom) / BlockSize)), true);
			}
		}
	}
}

void ARRRandomDiceTown::MakeUnderside()
{
	// Under the island an upside-down cone of big blocks (two cells across), each layer a little smaller: earth, then
	// peach, pink and lilac toward the point, a die here and there; only the blocks that show
	// (a battle map tile: its own square of the cone under the whole island)
	constexpr int32 LayerCount = 12;
	constexpr float Big = BlockSize * 2.f;
	const int32 HalfBig = HalfCells / 2;
	auto Inside = [this](int32 BX, int32 BY, int32 Layer)
	{
		const FVector2D Local = FVector2D((BX + 0.5f) * Big, (BY + 0.5f) * Big) + TileOffset;
		const float Share = 0.92f - 0.075f * (Layer - 1);
		return Share > 0.f && Local.Size() < RimRadius(FMath::Atan2(Local.Y, Local.X)) * Share - Big * 0.5f;
	};
	const FColor Bands[] = { Dirt, DirtDeep, FColor(230, 128, 80), FColor(255, 120, 150), FColor(220, 110, 220), FColor(150, 110, 250) };
	for (int32 Layer = 1; Layer <= LayerCount; ++Layer)
	{
		const float T = float(Layer - 1) / (LayerCount - 1) * (UE_ARRAY_COUNT(Bands) - 1);
		const int32 Band = FMath::Min(int32(T), int32(UE_ARRAY_COUNT(Bands)) - 2);
		const FColor Color = Blend(Bands[Band], Bands[Band + 1], T - Band);
		const float Z = -300.f - (Layer - 0.5f) * Big;
		for (int32 BX = -HalfBig; BX < HalfBig; ++BX)
		{
			for (int32 BY = -HalfBig; BY < HalfBig; ++BY)
			{
				if (!Inside(BX, BY, Layer))
				{
					continue;
				}
				const bool bShows = !Inside(BX + 1, BY, Layer) || !Inside(BX - 1, BY, Layer) || !Inside(BX, BY + 1, Layer) || !Inside(BX, BY - 1, Layer)
					|| !Inside(BX, BY, Layer + 1);
				if (!bShows)
				{
					continue;
				}
				const uint32 Hash = CellHash(BX, BY, Layer + 77);
				const FVector Spot = At(FVector2D((BX + 0.5f) * Big, (BY + 0.5f) * Big), Z);
				const FColor Tint = Shade(Color, 0.93f + 0.14f * (Hash % 100) / 100.f);
				if (Hash % 100 < 5)
				{
					AddColored(DieBlocks, FTransform(FRotator(0.f, 90.f * (Hash % 4), 0.f), Spot, FVector(1.9f)), Blend(Tint, White, 0.5f), Shade(Tint, 0.6f));
				}
				else
				{
					AddColored(Underside, FTransform(FRotator::ZeroRotator, Spot, FVector(2.f)), Tint, Tint);
				}
			}
		}
	}
}

void ARRRandomDiceTown::MakeSky()
{
	// A sea of block clouds under and round the island (bigger and higher further out, closing the horizon), puffs up high,
	// a pixel rainbow beyond each end, and little floating islands out of reach
	// (a battle map's sky, SkyScale: spread as much further out, the far clouds as much bigger, more puffs up high)
	FRandomStream Air(SkySeed);
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	const float Spread = SkyScale;
	for (int32 Index = 0; Index < 150; ++Index)
	{
		const float Theta = Air.FRandRange(0.f, UE_TWO_PI);
		const float Far = FMath::Sqrt(Air.FRand());
		const float Out = FMath::Lerp(9500.f, 40000.f, Far) * Spread;
		const float Size = FMath::Lerp(1400.f, 6000.f, Far) * Air.FRandRange(0.8f, 1.2f) * FMath::Lerp(1.f, Spread, Far);
		const float Z = FMath::Lerp(-2400.f, 400.f, FMath::Square(Far)) * FMath::Lerp(1.f, Spread * 0.6f, Far);
		AddCloud(FVector(Center.X + FMath::Cos(Theta) * Out, Center.Y + FMath::Sin(Theta) * Out, FloorZ + Z), Size, Air);
	}
	const int32 HighPuffs = FMath::RoundToInt(14 * Spread * Spread);
	for (int32 Index = 0; Index < HighPuffs; ++Index)
	{
		const float Theta = Air.FRandRange(0.f, UE_TWO_PI);
		const float Out = Air.FRandRange(4000.f, 16000.f) * Spread;
		AddCloud(FVector(Center.X + FMath::Cos(Theta) * Out, Center.Y + FMath::Sin(Theta) * Out, FloorZ + Air.FRandRange(2600.f, 5000.f)), Air.FRandRange(500.f, 1000.f), Air);
	}
	AddRainbow(FVector(Center.X + 15000.f * Spread, Center.Y - 3000.f, FloorZ - 2500.f), FVector(0.f, 1.f, 0.f), 7600.f);
	AddRainbow(FVector(Center.X - 15000.f * Spread, Center.Y + 3000.f, FloorZ - 2500.f), FVector(0.f, -1.f, 0.f), 7600.f);

	// Little floating islands, each with a tree
	struct FSkyIsland
	{
		FVector2D Where;
		float Height;
		int32 Radius;
	};
	const FSkyIsland Isles[] = { { FVector2D(6000.f, 11500.f), 900.f, 6 }, { FVector2D(-6000.f, -11500.f), 900.f, 6 },
		{ FVector2D(12500.f, 6500.f), 1600.f, 4 }, { FVector2D(-12500.f, -6500.f), 1600.f, 4 } };
	for (const FSkyIsland& Isle : Isles)
	{
		const FVector Middle(Center + Isle.Where * Spread, FloorZ + Isle.Height);
		for (int32 Layer = 0; Layer < Isle.Radius; ++Layer)
		{
			const int32 Reach = Isle.Radius - Layer;
			for (int32 DX = -Reach; DX <= Reach; ++DX)
			{
				for (int32 DY = -Reach; DY <= Reach; ++DY)
				{
					if (DX * DX + DY * DY > Reach * Reach)
					{
						continue;
					}
					const FVector Spot = Middle + FVector(DX * BlockSize, DY * BlockSize, -(Layer + 0.5f) * BlockSize);
					const FColor Side = Blend(Dirt, FColor(255, 130, 160), float(Layer) / Isle.Radius);
					AddColored(Blocks, FTransform(Spot), Side, Layer == 0 ? GrassA : Side);
				}
			}
		}
		if (PropTrees.Num() == 5)
		{
			// The dreamier pass: a Tripo tree instead
			PropTrees[Isle.Radius > 5 ? 1 : 0]->AddInstance(FTransform(FRotator::ZeroRotator, Middle, FVector(5.5f)), true);
			continue;
		}
		for (int32 Level = 0; Level < 3; ++Level)
		{
			AddColored(Blocks, FTransform(Middle + FVector(0.f, 0.f, (Level + 0.5f) * BlockSize)), Trunk, Trunk);
		}
		const FColor Crown = Leaves[FMath::Abs(FMath::RoundToInt(Isle.Where.X + Isle.Where.Y)) % UE_ARRAY_COUNT(Leaves)];
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 Level = 3; Level < 5; ++Level)
				{
					AddColored(Blocks, FTransform(Middle + FVector(DX * BlockSize, DY * BlockSize, (Level + 0.5f) * BlockSize)), Crown, Crown);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The dreamier pass (round 47; -RRTownClassic leaves it out): giant dice to climb and live in, rainbows of
// dice, candy trees, spotted mushrooms and lollipops, star lamps, and dice and stars drifting in the air

namespace
{
	const FColor Candies[] = { FColor(255, 176, 214), FColor(204, 172, 255), FColor(160, 212, 255), FColor(160, 238, 206), FColor(255, 232, 140) };
	const FColor CapColors[] = { FColor(240, 70, 96), FColor(255, 120, 180), FColor(170, 110, 240), FColor(80, 160, 255) };
	const FColor StarYellow(255, 206, 60);
	const FColor MoonYellow(255, 236, 160);
	const FColor Stem(255, 240, 222);
	// The dice's colors (as AddDie's): body and pips
	const FColor DieBodies[] = { FColor(252, 250, 245), FColor(255, 118, 170), FColor(92, 182, 255), FColor(255, 214, 70), FColor(88, 214, 160), FColor(176, 132, 255) };
	const FColor DiePips[] = { FColor(40, 40, 62), FColor(252, 250, 245), FColor(252, 250, 245), FColor(110, 70, 30), FColor(252, 250, 245), FColor(252, 250, 245) };

	/** Where the pips are on a die's face showing Value, in thirds across it (-1, 0, 1). */
	TArray<FIntPoint> PipSpots(int32 Value)
	{
		switch (Value)
		{
		case 1: return { FIntPoint(0, 0) };
		case 2: return { FIntPoint(-1, -1), FIntPoint(1, 1) };
		case 3: return { FIntPoint(-1, -1), FIntPoint(0, 0), FIntPoint(1, 1) };
		case 4: return { FIntPoint(-1, -1), FIntPoint(-1, 1), FIntPoint(1, -1), FIntPoint(1, 1) };
		case 5: return { FIntPoint(-1, -1), FIntPoint(-1, 1), FIntPoint(1, -1), FIntPoint(1, 1), FIntPoint(0, 0) };
		default: return { FIntPoint(-1, -1), FIntPoint(-1, 0), FIntPoint(-1, 1), FIntPoint(1, -1), FIntPoint(1, 0), FIntPoint(1, 1) };
		}
	}
}

FVector ARRRandomDiceTown::FrameSpot(const FFrame& Frame, float U, float V, float Height) const
{
	const FVector2D AxisU(Frame.Dir(1, 0));
	const FVector2D AxisV(Frame.Dir(0, 1));
	return At(CellCenter(Frame.Origin) + (AxisU * U + AxisV * V) * BlockSize, Height);
}

float ARRRandomDiceTown::FrameYaw(const FFrame& Frame)
{
	const FIntPoint AxisU = Frame.Dir(1, 0);
	return FMath::RadiansToDegrees(FMath::Atan2(float(AxisU.Y), float(AxisU.X)));
}

void ARRRandomDiceTown::AddDieInFrame(const FFrame& Frame, int32 U, int32 V, int32 Level, int32 Size, int32 Kind)
{
	// Its lowest corner cell, whichever way the frame turns
	const FIntPoint A = Frame.Cell(U, V);
	const FIntPoint B = Frame.Cell(U + Size - 1, V + Size - 1);
	AddDie(FIntPoint(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), Level, Size, Kind);
}

void ARRRandomDiceTown::AddDiceMountain(const FFrame& Frame)
{
	// Dice four blocks across: nine, four on them against the back, one on top in the back corner; each tier leaves a
	// strip at its front for a flight up to the next (front at V = -1)
	const int32 Shift = Frame.bRed ? 3 : 0;
	for (int32 I = 0; I < 3; ++I)
	{
		for (int32 J = 0; J < 3; ++J)
		{
			AddDieInFrame(Frame, 4 * I, 4 * J, 1, 4, (I * 3 + J + Shift) * 7);
		}
	}
	for (int32 I = 0; I < 2; ++I)
	{
		for (int32 J = 0; J < 2; ++J)
		{
			AddDieInFrame(Frame, 4 * I, 4 + 4 * J, 5, 4, (I * 2 + J + Shift) * 11 + 2);
		}
	}
	AddDieInFrame(Frame, 0, 8, 9, 4, 5 + Shift);
	// Up from the ground to the first tier; up its front strip to the second, from a step of little dice built out in
	// front (the strip is the flight's own length, so nothing to start from on it); up the second's strip to the top,
	// from a step of little dice on the first tier's strip
	AddStairs(Frame, 8, -4, 0, 1, 1, 0, 2, 0, 4, Sand);
	AddStairs(Frame, 4, 0, 0, 1, 1, 0, 2, 4, 4, Sand);
	AddStairs(Frame, 0, 4, 0, 1, 1, 0, 2, 8, 4, Sand);
	for (int32 U = 3; U <= 6; ++U)
	{
		for (int32 Level = 1; Level <= 4; ++Level)
		{
			AddDieInFrame(Frame, U, -1, Level, 1, U * 5 + Level * 3 + Shift);
		}
	}
	for (int32 U = 0; U <= 3; ++U)
	{
		for (int32 Level = 5; Level <= 8; ++Level)
		{
			AddDieInFrame(Frame, U, 3, Level, 1, U * 7 + Level * 2 + Shift);
		}
	}
	// Little dice at its foot, and a star turning over the top
	AddDieInFrame(Frame, 10, -3, 1, 2, 9 + Shift);
	AddDieInFrame(Frame, 11, -1, 1, 1, 13 + Shift);
	AddDieInFrame(Frame, 5, -2, 1, 1, 20 + Shift);
	AddFloater(Stars, FrameSpot(Frame, 1.5f, 9.5f, 12 * BlockSize + 260.f), FRotator(0.f, FrameYaw(Frame), 0.f), 2.6f, StarYellow, StarYellow,
		30.f, 45.f, 0.f, 0.f, 0.f, Frame.bRed ? 1.f : 0.f);
}

void ARRRandomDiceTown::AddDiceArch(const FFrame& Frame)
{
	// Seven bands of dice on a half circle round the road's middle line (between U = -1 and 0), the outer band 11.5
	// blocks out and the inner 4.5, so four blocks of room over the road (a giant passes); its feet in clouds
	uint8 BandLooks[UE_ARRAY_COUNT(Rainbow)];
	for (int32 Band = 0; Band < UE_ARRAY_COUNT(Rainbow); ++Band)
	{
		BandLooks[Band] = MakeLook(Rainbow[Band], White, true);
	}
	for (int32 U = -12; U <= 11; ++U)
	{
		for (int32 Level = 1; Level <= 12; ++Level)
		{
			const float R = FMath::Sqrt(FMath::Square(U + 0.5f) + FMath::Square(Level - 0.5f));
			if (R < 4.5f || R >= 11.5f)
			{
				continue;
			}
			const uint8 Look = BandLooks[FMath::Clamp(int32(11.5f - R), 0, int32(UE_ARRAY_COUNT(Rainbow)) - 1)];
			Fill(Frame, U, 0, U, 1, Level, Level, Look);
		}
	}
	for (const float Foot : { -8.5f, 7.5f })
	{
		AddColored(Clouds, FTransform(FRotator(0.f, FrameYaw(Frame), 0.f), FrameSpot(Frame, Foot, 0.5f, 70.f), FVector(4.2f, 3.4f, 1.5f)), White, White);
		AddColored(Clouds, FTransform(FRotator(0.f, FrameYaw(Frame) + 90.f, 0.f), FrameSpot(Frame, Foot + 0.8f, 0.2f, 150.f), FVector(2.4f, 2.2f, 1.4f)), FColor(255, 236, 246), FColor(255, 236, 246));
	}
}

void ARRRandomDiceTown::AddDieHouse(const FFrame& Frame, int32 Seed)
{
	// A hollow die N cells a side: the edges a little lighter (a die's rounded edges), pips painted on the walls the way a
	// die's faces go (opposite faces add to 7) and raised a block on the roof (cover up there); doors front and left, an
	// outside flight up the right side to a landing and a second flight along the back to the roof
	constexpr int32 N = 12;
	FRandomStream Stream(Seed);
	// A colored die with white pips (dark pips on a white house read as a face)
	const int32 Pick = Stream.RandRange(1, UE_ARRAY_COUNT(DieBodies) - 1);
	const FColor Body = Blend(DieBodies[Pick], White, 0.15f);
	const FColor Edge = Blend(Body, White, 0.4f);
	const uint8 BodyLook = Plain(Body);
	const uint8 EdgeLook = Plain(Edge);
	const uint8 PipLook = Plain(White);
	const uint8 FloorLook = MakeLook(Dirt, Wood);
	for (int32 U = 0; U < N; ++U)
	{
		for (int32 V = 0; V < N; ++V)
		{
			const bool bEdgeU = U == 0 || U == N - 1;
			const bool bEdgeV = V == 0 || V == N - 1;
			if (!bEdgeU && !bEdgeV)
			{
				Fill(Frame, U, V, U, V, 0, 0, FloorLook);
			}
			for (int32 Level = 1; Level <= N; ++Level)
			{
				const int32 Edges = int32(bEdgeU) + int32(bEdgeV) + int32(Level == N || Level == 1);
				if (bEdgeU || bEdgeV || Level == N)
				{
					Fill(Frame, U, V, U, V, Level, Level, Edges >= 2 ? EdgeLook : BodyLook);
				}
			}
		}
	}
	// Faces: 0 front, 1 right, 2 back, 3 left, 4 the roof; A across a face, B up it (or along the roof), each 0 .. N - 1
	auto Paint = [&](int32 Face, int32 Value)
	{
		for (const FIntPoint& Pip : PipSpots(Value))
		{
			for (int32 DA = 0; DA < 2; ++DA)
			{
				for (int32 DB = 0; DB < 2; ++DB)
				{
					const int32 A = N / 2 - 1 + 3 * Pip.X + DA;
					const int32 B = N / 2 - 1 + 3 * Pip.Y + DB;
					switch (Face)
					{
					case 0: Fill(Frame, A, 0, A, 0, B + 1, B + 1, PipLook); break;
					case 1: Fill(Frame, N - 1, A, N - 1, A, B + 1, B + 1, PipLook); break;
					case 2: Fill(Frame, N - 1 - A, N - 1, N - 1 - A, N - 1, B + 1, B + 1, PipLook); break;
					case 3: Fill(Frame, 0, N - 1 - A, 0, N - 1 - A, B + 1, B + 1, PipLook); break;
					default: Fill(Frame, A, B, A, B, N + 1, N + 1, PipLook); break;
					}
				}
			}
		}
	};
	Paint(0, 3);
	Paint(1, 2);
	Paint(2, 4);
	Paint(3, 5);
	Paint(4, 6);
	// Doors, front and left, clear of the pips
	Fill(Frame, 4, 0, 6, 0, 1, OpeningHeight, 0);
	Fill(Frame, 0, 5, 0, 7, 1, OpeningHeight, 0);
	// Up the right side to a landing halfway, along the back to the roof's edge (as a flat roofed building's)
	const FColor StairColor = Blend(Body, White, 0.35f);
	AddStairs(Frame, N, 0, 0, 1, 1, 0, 2, 0, Story, StairColor);
	Fill(Frame, N, Story, N + 1, N + 1, 1, Story, EdgeLook);
	Fill(Frame, N + 1, Story, N + 1, N - 1, Story + 1, Story + 1, PipLook);
	Fill(Frame, N - 6, N, N - 1, N + 1, 1, Story, EdgeLook);
	AddStairs(Frame, N - 1, N, -1, 0, 0, 1, 2, Story, Story, StairColor);
	Fill(Frame, N - 8, N, N - 7, N + 1, 1, N, EdgeLook);
	// Inside: a few crates, a star hanging under the roof
	const uint8 CrateLook = MakeLook(WoodDark, Wood);
	Fill(Frame, 2, 9, 3, 10, 1, 1, CrateLook);
	Fill(Frame, 2, 10, 2, 10, 2, 2, CrateLook);
	Fill(Frame, 9, 2, 9, 3, 1, 2, CrateLook);
	AddFloater(Stars, FrameSpot(Frame, (N - 1) * 0.5f, (N - 1) * 0.5f, (8.f + Lift) * BlockSize), FRotator(0.f, FrameYaw(Frame), 0.f), 2.2f, StarYellow, StarYellow,
		25.f, 35.f, 0.f, 0.f, 0.f, float(Seed % 100));
}

void ARRRandomDiceTown::AddDreamTree(const FIntPoint& Cell, int32 Kind, int32 Seed)
{
	FRandomStream Stream(Seed);
	if (Kind == 2)
	{
		// A candy tree: a striped trunk, a round crown of one pastel, lighter on top, a few sprinkles
		const int32 Height = Stream.RandRange(3, 4);
		const int32 Pick = Stream.RandRange(0, UE_ARRAY_COUNT(Candies) - 1);
		const FColor Color = Candies[Pick];
		const uint8 StripeA = Plain(White);
		const uint8 StripeB = Plain(Shade(Color, 0.85f));
		for (int32 Level = 1; Level <= Height; ++Level)
		{
			Set(Cell.X, Cell.Y, Level, Level % 2 ? StripeA : StripeB);
		}
		const uint8 Crown = Plain(Color);
		const uint8 CrownLight = Plain(Blend(Color, White, 0.35f));
		const uint8 Sprinkle = Plain(Stream.FRand() < 0.5f ? White : Candies[(Pick + 2) % UE_ARRAY_COUNT(Candies)]);
		const int32 Middle = Height + 3;
		for (int32 DX = -2; DX <= 2; ++DX)
		{
			for (int32 DY = -2; DY <= 2; ++DY)
			{
				for (int32 Level = Height + 1; Level <= Height + 5; ++Level)
				{
					if (DX * DX + DY * DY + FMath::Square(Level - Middle) > 6)
					{
						continue;
					}
					const uint32 Hash = CellHash(Cell.X + DX, Cell.Y + DY, Level + Seed % 97);
					Set(Cell.X + DX, Cell.Y + DY, Level, Hash % 9 == 0 ? Sprinkle : Level > Middle ? CrownLight : Crown);
				}
			}
		}
	}
	else if (Kind == 3)
	{
		// A spotted mushroom: a thick pale stem, a domed cap with white spots on top
		const FColor Color = CapColors[Stream.RandRange(0, UE_ARRAY_COUNT(CapColors) - 1)];
		const uint8 StemLook = MakeLook(Stem, Stem);
		const uint8 CapLook = MakeLook(Shade(Color, 0.9f), Color);
		const uint8 SpotLook = Plain(White);
		const int32 Tall = Stream.RandRange(3, 4);
		for (const FIntPoint Offset : { FIntPoint(0, 0), FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
		{
			for (int32 Level = 1; Level <= Tall; ++Level)
			{
				Set(Cell.X + Offset.X, Cell.Y + Offset.Y, Level, StemLook);
			}
		}
		const int32 Limits[] = { 10, 6, 2 };
		for (int32 Layer = 0; Layer < 3; ++Layer)
		{
			for (int32 DX = -3; DX <= 3; ++DX)
			{
				for (int32 DY = -3; DY <= 3; ++DY)
				{
					const int32 D2 = DX * DX + DY * DY;
					if (D2 > Limits[Layer])
					{
						continue;
					}
					// A spot where this layer is the top
					const bool bTop = Layer == 2 || D2 > Limits[Layer + 1];
					const bool bSpot = bTop && CellHash(Cell.X + DX, Cell.Y + DY, Seed % 89) % 4 == 0;
					Set(Cell.X + DX, Cell.Y + DY, Tall + 1 + Layer, bSpot ? SpotLook : CapLook);
				}
			}
		}
	}
	else
	{
		// A lollipop: a white stick, a round swirl standing on it across X or Y
		const bool bAlongX = Seed % 2 == 0;
		const FColor A = Candies[Stream.RandRange(0, UE_ARRAY_COUNT(Candies) - 1)];
		const FColor B = Stream.FRand() < 0.5f ? White : FColor(255, 110, 150);
		const uint8 StickLook = Plain(White);
		const uint8 LookA = Plain(A);
		const uint8 LookB = Plain(B);
		for (int32 Level = 1; Level <= 4; ++Level)
		{
			Set(Cell.X, Cell.Y, Level, StickLook);
		}
		constexpr int32 Middle = 7;
		for (int32 Across = -2; Across <= 2; ++Across)
		{
			for (int32 Level = Middle - 2; Level <= Middle + 2; ++Level)
			{
				const float R = FMath::Sqrt(float(Across * Across + FMath::Square(Level - Middle)));
				if (R > 2.45f)
				{
					continue;
				}
				// A swirl: stripes that turn with the distance from the middle
				const float Angle = FMath::Atan2(float(Level - Middle), float(Across)) / UE_TWO_PI;
				const int32 Stripe = FMath::FloorToInt(Angle * 4.f + R * 0.6f + 8.f) % 2;
				const FIntPoint Where = bAlongX ? FIntPoint(Cell.X + Across, Cell.Y) : FIntPoint(Cell.X, Cell.Y + Across);
				Set(Where.X, Where.Y, Level, Stripe ? LookA : LookB);
			}
		}
	}
}

bool ARRRandomDiceTown::AddPropTree(const FIntPoint& Cell, int32 Kind, int32 Seed)
{
	if (PropTrees.Num() < 5)
	{
		return false;
	}
	// Which model, how tall (the models are 100 high, pivot at the bottom middle), how much of it is trunk, the minimap's
	// color: round green trees small and big, tiered ones among the big, candy trees, mushrooms, lollipops
	FRandomStream Stream(Seed + 911);
	int32 Model = 0;
	float Height = 0.f;
	float TrunkShare = 0.f;
	FColor MapColor = Leaves[0];
	switch (Kind)
	{
	case 0: Model = 0; Height = Stream.FRandRange(430.f, 520.f); TrunkShare = 0.3f; break;
	case 1: Model = Stream.FRand() < 0.5f ? 0 : 1; Height = Stream.FRandRange(620.f, 740.f); TrunkShare = Model == 0 ? 0.3f : 0.15f; MapColor = Leaves[Model == 0 ? 0 : 2]; break;
	case 2: Model = 2; Height = Stream.FRandRange(580.f, 680.f); TrunkShare = 0.3f; MapColor = Candies[0]; break;
	case 3: Model = 3; Height = Stream.FRandRange(460.f, 540.f); TrunkShare = 0.4f; MapColor = CapColors[0]; break;
	default: Model = 4; Height = Stream.FRandRange(640.f, 720.f); TrunkShare = 0.45f; MapColor = Candies[1]; break;
	}
	// What stops: the trunk (a mushroom's thick stem and its cap too, the lollipop's stick)
	const uint8 TrunkLook = HiddenLook(MapColor);
	const int32 TrunkTop = FMath::Max(1, FMath::FloorToInt(Height * TrunkShare / BlockSize));
	for (int32 Level = 1; Level <= TrunkTop; ++Level)
	{
		Set(Cell.X, Cell.Y, Level, TrunkLook);
	}
	if (Kind == 3)
	{
		for (const FIntPoint Offset : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
		{
			for (int32 Level = 1; Level <= TrunkTop; ++Level)
			{
				Set(Cell.X + Offset.X, Cell.Y + Offset.Y, Level, TrunkLook);
			}
		}
		for (int32 DX = -2; DX <= 2; ++DX)
		{
			for (int32 DY = -2; DY <= 2; ++DY)
			{
				if (DX * DX + DY * DY <= 5)
				{
					Set(Cell.X + DX, Cell.Y + DY, TrunkTop + 1, TrunkLook);
				}
			}
		}
	}
	// Lollipops face along a road's axis (their disc stands across X or Y), the rest turn any way
	const float Yaw = Kind == 4 ? (Seed % 2 == 0 ? 0.f : 90.f) + Stream.FRandRange(-12.f, 12.f) : Stream.FRandRange(0.f, 360.f);
	PropTrees[Model]->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), At(CellCenter(Cell), 0.f), FVector(Height / BlockSize)), true);
	return true;
}

void ARRRandomDiceTown::AddAwning(const FFrame& Frame, int32 Start, int32 Width, const FColor& Color)
{
	// Stripes a cell wide, sloping out and down from just over the door (the door is 4 blocks high; a giant passes)
	const FIntPoint Out = Frame.Dir(0, -1);
	const float OutYaw = FMath::RadiansToDegrees(FMath::Atan2(float(Out.Y), float(Out.X)));
	for (int32 U = Start; U < Start + Width; ++U)
	{
		const FColor Stripe = (U - Start) % 2 == 0 ? Color : White;
		AddColored(Decor, FTransform(FRotator(-20.f, OutYaw, 0.f), FrameSpot(Frame, float(U), -1.2f, 455.f), FVector(1.5f, 1.f, 0.16f)), Stripe, Stripe);
	}
	// A scalloped edge: a little block under every other stripe's end
	for (int32 U = Start; U < Start + Width; U += 2)
	{
		AddColored(Decor, FTransform(FRotator(0.f, OutYaw, 0.f), FrameSpot(Frame, float(U), -1.92f, 405.f), FVector(0.18f, 0.5f, 0.22f)), Color, Color);
	}
	// Round 91: a neon tube along the awning's edge, one candy neon color per shop (it glows even at noon, and lights the
	// shop front a little through Lumen)
	if (Neon)
	{
		const FColor Tube = NeonColors[FMath::Abs(Frame.Origin.X * 7 + Frame.Origin.Y * 13 + Start) % UE_ARRAY_COUNT(NeonColors)];
		AddColored(Neon, FTransform(FRotator(0.f, OutYaw, 0.f), FrameSpot(Frame, Start + (Width - 1) * 0.5f, -2.02f, 392.f), FVector(0.07f, float(Width), 0.07f)), Tube, Tube);
	}
}

void ARRRandomDiceTown::AddStarLamps()
{
	// Along both main roads, either side, every 6 cells from the plaza out to the road's end, where the ground beside it
	// is clear: a candy striped post 3 blocks high with a star turning on top
	const uint8 PoleA = Plain(White);
	const uint8 PoleB = Plain(FColor(255, 150, 196));
	const uint8 PoleHidden = HiddenLook(FColor(255, 150, 196));
	int32 Placed = 0;
	auto IsClear = [this](const FIntPoint& Cell)
	{
		if (!IsOnIsland(Cell.X, Cell.Y) || IsRoad(Cell.X, Cell.Y) || !Get(Cell.X, Cell.Y, 0) || StairCells.Contains(Cell))
		{
			return false;
		}
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				for (int32 Level = 1; Level <= 6; ++Level)
				{
					if (Get(Cell.X + DX, Cell.Y + DY, Level))
					{
						return false;
					}
				}
			}
		}
		return true;
	};
	for (int32 Road = 0; Road < 2; ++Road)
	{
		for (const int32 Side : { -4, 3 })
		{
			for (int32 T = 16; T <= 54; T += 6)
			{
				const FIntPoint Cell = Road == 0 ? FIntPoint(-T, Side) : FIntPoint(Side, -T);
				const FIntPoint Other(-1 - Cell.X, -1 - Cell.Y);
				if (!IsClear(Cell) || !IsClear(Other))
				{
					continue;
				}
				for (const FIntPoint& Where : { Cell, Other })
				{
					for (int32 Level = 1; Level <= 3; ++Level)
					{
						Set(Where.X, Where.Y, Level, Poles ? PoleHidden : Level % 2 ? PoleA : PoleB);
					}
					if (Poles)
					{
						// A smooth candy striped post: bands of a round post, a disc at its foot
						for (int32 Band = 0; Band < 6; ++Band)
						{
							const FColor Stripe = Band % 2 ? FColor(255, 150, 196) : White;
							AddColored(Poles, FTransform(FRotator::ZeroRotator, At(CellCenter(Where), (Band + 0.5f) * 50.f), FVector(0.38f, 0.38f, 0.5f)), Stripe, Stripe);
						}
						AddColored(Poles, FTransform(FRotator::ZeroRotator, At(CellCenter(Where), 10.f), FVector(0.8f, 0.8f, 0.2f)), White, White);
					}
					AddFloater(Stars, At(CellCenter(Where), 3 * BlockSize + 80.f), FRotator(0.f, Road == 0 ? 0.f : 90.f, 0.f), 1.3f, StarYellow, StarYellow,
						8.f, 60.f, 0.f, 0.f, 0.f, float(T + Side));
					// Round 91: a real lamp, lighting the road round it warm
					UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
					Light->SetupAttachment(RootComponent);
					Light->SetUsingAbsoluteLocation(true);
					Light->SetIntensityUnits(ELightUnits::Candelas);
					Light->SetIntensity(LampLightIntensity);
					Light->SetAttenuationRadius(LampLightReach);
					Light->SetLightColor(LampLightColor);
					Light->SetCastShadows(false);
					Light->RegisterComponent();
					Light->SetWorldLocation(At(CellCenter(Where), 3 * BlockSize + 60.f));
					LampLights.Add(Light);
				}
				++Placed;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town dream pass placed %d star lamps (each half)."), Placed);
}

void ARRRandomDiceTown::AddPuddles()
{
	if (!Puddles)
	{
		return;
	}
	// Road cells with open sky, off the plaza and the stairs, a few cells apart, picked the same way on every machine
	// (a battle map tile: fewer, its own seed)
	FRandomStream Stream(GroundSeed + 91 + (IsBattleTile() ? BattleTile.Seed : 0));
	const int32 Want = IsBattleTile() ? 8 : PuddleCount;
	TArray<FIntPoint> Placed;
	for (int32 Try = 0; Try < Want * 40 && Placed.Num() < Want; ++Try)
	{
		const FIntPoint Cell(Stream.RandRange(-HalfCells, HalfCells - 1), Stream.RandRange(-HalfCells, HalfCells - 1));
		if (!IsOnIsland(Cell.X, Cell.Y) || !IsRoad(Cell.X, Cell.Y) || (!IsBattleTile() && Cheb(Cell.X, Cell.Y) < PlazaEdge + 2) || !Get(Cell.X, Cell.Y, 0)
			|| !IsClearAbove(Cell.X, Cell.Y, 1, 4) || StairCells.Contains(Cell))
		{
			continue;
		}
		if (Placed.ContainsByPredicate([&Cell](const FIntPoint& Other) { return FMath::Max(FMath::Abs(Other.X - Cell.X), FMath::Abs(Other.Y - Cell.Y)) < PuddleSpacing; }))
		{
			continue;
		}
		Placed.Add(Cell);
		// Two or three flat discs overlapping into a blob, just over the paving (the engine cylinder: 100 across, 100 high)
		TArray<TPair<int32, FTransform>>& Discs = PuddleDiscs.FindOrAdd(Cell);
		const int32 Count = Stream.RandRange(2, 3);
		for (int32 Disc = 0; Disc < Count; ++Disc)
		{
			const float Size = Stream.FRandRange(PuddleSize.X, PuddleSize.Y);
			const FVector2D Offset(Stream.FRandRange(-45.f, 45.f), Stream.FRandRange(-45.f, 45.f));
			const FTransform Where(FRotator(0.f, Stream.FRandRange(0.f, 180.f), 0.f), At(CellCenter(Cell) + Offset, 0.6f + Disc * 0.05f),
				FVector(Size / 100.f, Size * Stream.FRandRange(0.6f, 0.9f) / 100.f, 0.004f));
			AddColored(Puddles, Where, PuddleColor, PuddleColor);
			Discs.Add({ Puddles->GetInstanceCount() - 1, Where });
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town round 91 lights: %d puddles, %d star lamp lights."), Placed.Num(), LampLights.Num());
}

void ARRRandomDiceTown::SetUpLightEffects()
{
	// The sun's light shafts (bloom streaming from it when it's in view past the rooftops), and a sky light that keeps up
	// with the sky, so the polished tiles, puddles and glossy vehicles have the sky to show
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		if (UDirectionalLightComponent* Sun = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
		{
			Sun->bEnableLightShaftBloom = true;
			Sun->BloomScale = SunShaftBloomScale;
			Sun->MarkRenderStateDirty();
		}
	}
	for (TActorIterator<ASkyLight> It(GetWorld()); It; ++It)
	{
		if (USkyLightComponent* SkyLighting = It->GetLightComponent())
		{
			SkyLighting->SetRealTimeCaptureEnabled(true);
		}
	}
}

void ARRRandomDiceTown::AddFloater(UInstancedStaticMeshComponent* Instances, const FVector& Base, const FRotator& Turn, float Scale, const FColor& Body, const FColor& Accent,
	float Bob, float Spin, float Tumble, float Orbit, float OrbitRate, float Phase)
{
	if (!Instances)
	{
		return;
	}
	FFloater Floater;
	Floater.Instances = Instances;
	Floater.Index = Instances->GetInstanceCount();
	Floater.Base = Base;
	Floater.Turn = Turn;
	Floater.Scale = Scale;
	Floater.Bob = Bob;
	Floater.Phase = Phase;
	Floater.Spin = Spin;
	Floater.Tumble = Tumble;
	Floater.Orbit = Orbit;
	Floater.OrbitRate = OrbitRate;
	const FVector Where = Base + (Orbit > 0.f ? FVector(FMath::Cos(Phase), FMath::Sin(Phase), 0.f) * Orbit : FVector::ZeroVector);
	AddColored(Instances, FTransform(Turn, Where, FVector(Scale)), Body, Accent);
	Floaters.Add(Floater);
}

void ARRRandomDiceTown::AnimateFloaters(float Time)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (const FFloater& Floater : Floaters)
	{
		FVector Where = Floater.Base;
		if (Floater.Orbit > 0.f)
		{
			const float Angle = Floater.Phase + Time * Floater.OrbitRate;
			Where += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Floater.Orbit;
		}
		Where.Z += Floater.Bob * FMath::Sin(Time * 1.1f + Floater.Phase * 2.3f);
		const FQuat Spin(FRotator(0.f, Time * Floater.Spin, 0.f));
		const FQuat Tumble(FRotator(Time * Floater.Tumble * 0.7f, 0.f, Time * Floater.Tumble));
		Floater.Instances->UpdateInstanceTransform(Floater.Index, FTransform(Spin * FQuat(Floater.Turn) * Tumble, Where, FVector(Floater.bWrecked ? 0.f : Floater.Scale)), true, false, true);
	}
	for (UInstancedStaticMeshComponent* Instances : { Stars.Get(), FloatDice.Get() })
	{
		if (Instances)
		{
			Instances->MarkRenderStateDirty();
		}
	}
}

void ARRRandomDiceTown::MakeDreamSky()
{
	FRandomStream Air(SkySeed + 4700);
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);

	// A floor of pale cloud far below, instead of the dark ground showing under the cloud sea
	// (a battle map's sky reaches as much further: SkyScale)
	const float Spread = SkyScale;
	AddColored(Clouds, FTransform(FRotator::ZeroRotator, FVector(Center, FloorZ - 6500.f * FMath::Sqrt(Spread)), FVector(20000.f * Spread, 20000.f * Spread, 4.f)), FColor(168, 210, 255), FColor(168, 210, 255));

	// A rainbow of dice in a vertical plane: seven bands on a half circle across Across from Base
	auto DiceArc = [this](const FVector& Base, const FVector& Across, float Radius)
	{
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Across.Y, Across.X));
		for (int32 Band = 0; Band < UE_ARRAY_COUNT(Rainbow); ++Band)
		{
			const float Bend = Radius - Band * BlockSize;
			TSet<FIntPoint> Done;
			for (float Angle = 0.f; Angle <= PI; Angle += 0.5f * BlockSize / Bend)
			{
				const FIntPoint Spot(FMath::RoundToInt(FMath::Cos(Angle) * Bend / BlockSize), FMath::RoundToInt(FMath::Sin(Angle) * Bend / BlockSize));
				if (!Done.Contains(Spot))
				{
					Done.Add(Spot);
					AddColored(DieBlocks, FTransform(FRotator(0.f, Yaw, 0.f), Base + Across * (Spot.X * BlockSize) + FVector(0.f, 0.f, Spot.Y * BlockSize)), Rainbow[Band], White);
				}
			}
		}
	};
	// A floating island: grass on top, earth fading to pink and lilac under it, a candy tree and a few dice on it
	auto Isle = [&](const FVector& Middle, int32 Radius, int32 Seed)
	{
		FRandomStream Stream(Seed);
		for (int32 Layer = 0; Layer < Radius + 2; ++Layer)
		{
			const float Reach = Radius - Layer * 0.75f;
			for (int32 DX = -Radius; DX <= Radius; ++DX)
			{
				for (int32 DY = -Radius; DY <= Radius; ++DY)
				{
					if (DX * DX + DY * DY > Reach * Reach)
					{
						continue;
					}
					const FColor Side = Blend(Blend(Dirt, FColor(255, 140, 180), FMath::Min(1.f, Layer * 0.35f)), FColor(190, 140, 255), FMath::Max(0.f, Layer - 3.f) * 0.25f);
					AddColored(Blocks, FTransform(Middle + FVector(DX * BlockSize, DY * BlockSize, -(Layer + 0.5f) * BlockSize)), Side, Layer == 0 ? GrassA : Side);
				}
			}
		}
		const FColor Crown = Candies[Stream.RandRange(0, UE_ARRAY_COUNT(Candies) - 1)];
		const FVector TreeAt = Middle + FVector(-BlockSize, -BlockSize, 0.f);
		if (PropTrees.Num() == 5)
		{
			// A candy tree and a round green one (the Tripo models)
			PropTrees[2]->AddInstance(FTransform(FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f), TreeAt, FVector(6.f)), true);
			PropTrees[0]->AddInstance(FTransform(FRotator(0.f, Stream.FRandRange(0.f, 360.f), 0.f), Middle + FVector(BlockSize, 2.f * BlockSize, 0.f), FVector(4.f)), true);
		}
		else
		{
			for (int32 Level = 0; Level < 3; ++Level)
			{
				const FColor Stripe = Level % 2 ? Shade(Crown, 0.85f) : White;
				AddColored(Blocks, FTransform(TreeAt + FVector(0.f, 0.f, (Level + 0.5f) * BlockSize)), Stripe, Stripe);
			}
			for (int32 DX = -2; DX <= 2; ++DX)
			{
				for (int32 DY = -2; DY <= 2; ++DY)
				{
					for (int32 DZ = -2; DZ <= 2; ++DZ)
					{
						if (DX * DX + DY * DY + DZ * DZ <= 5)
						{
							const FColor Tint = DZ > 0 ? Blend(Crown, White, 0.35f) : Crown;
							AddColored(Blocks, FTransform(TreeAt + FVector(DX * BlockSize, DY * BlockSize, (5.5f + DZ) * BlockSize)), Tint, Tint);
						}
					}
				}
			}
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const int32 Pick = Stream.RandRange(0, UE_ARRAY_COUNT(DieBodies) - 1);
			const float Size = Index == 0 ? 2.f : 1.f;
			const FVector Spot = Middle + FVector(Stream.RandRange(1, Radius - 2) * BlockSize, Stream.RandRange(-Radius + 2, Radius - 2) * BlockSize, Size * BlockSize * 0.5f);
			AddColored(DieBlocks, FTransform(FRotator(0.f, 90.f * Stream.RandRange(0, 3), 0.f), Spot, FVector(Size * 0.98f)), DieBodies[Pick], DiePips[Pick]);
		}
	};
	for (const float Sign : { -1.f, 1.f })
	{
		// Two islands off each team's end, joined by a rainbow of dice
		// (a battle map: out past its rim, the pair as far apart as in the town)
		const float IsleOut = 10600.f * Spread + (Spread > 1.f ? 3000.f : 0.f);
		const FVector A(Center.X + Sign * IsleOut, Center.Y + Sign * 6600.f, FloorZ + 600.f);
		const FVector B(Center.X + Sign * IsleOut, Center.Y + Sign * 1400.f, FloorZ + 600.f);
		Isle(A, 6, Sign > 0.f ? 11 : 12);
		Isle(B, 5, Sign > 0.f ? 13 : 14);
		DiceArc((A + B) * 0.5f + FVector(0.f, 0.f, BlockSize * 0.5f), (B - A).GetSafeNormal(), (B - A).Size() * 0.5f - BlockSize * 3.f);
		// A crescent moon high beyond them, stars round it
		const FVector Moon(Center.X + Sign * 21000.f * Spread, Center.Y - Sign * 7000.f * Spread, FloorZ + 7600.f * FMath::Sqrt(Spread));
		const float MoonYaw = FMath::RadiansToDegrees(FMath::Atan2(Center.Y - Moon.Y, Center.X - Moon.X));
		AddColored(Moons, FTransform(FRotator(0.f, MoonYaw, -18.f), Moon, FVector(42.f)), MoonYellow, MoonYellow);
		for (int32 Index = 0; Index < 7; ++Index)
		{
			const FVector Around = Moon + FVector(Air.FRandRange(-1.f, 1.f) * 2500.f, Air.FRandRange(-1.f, 1.f) * 6000.f, Air.FRandRange(-2400.f, 3200.f));
			AddFloater(Stars, Around, FRotator(0.f, MoonYaw, 0.f), Air.FRandRange(6.f, 12.f), StarYellow, StarYellow, 120.f, Air.FRandRange(10.f, 25.f), 0.f, 0.f, 0.f, Air.FRandRange(0.f, 6.f));
		}
	}

	// Dice circling over the giant die in the plaza
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const int32 Pick = Index % UE_ARRAY_COUNT(DieBodies);
		AddFloater(FloatDice, At(FVector2D::ZeroVector, 1250.f), FRotator(0.f, 60.f * Index, 90.f * (Index % 2)), 1.6f, DieBodies[Pick], DiePips[Pick],
			40.f, 30.f, 20.f, 950.f, 0.22f, Index * UE_TWO_PI / 6.f);
	}
	// Dice and stars drifting high over the town and out round the rim, the same on both halves
	auto Pair = [&](TFunctionRef<void(const FVector2D&, float)> Place, const FVector2D& Local, float Phase)
	{
		Place(Local, Phase);
		Place(-Local, Phase + 1.7f);
	};
	for (int32 Index = 0; Index < 26; ++Index)
	{
		const bool bFar = Index >= 10;
		const float Theta = Air.FRandRange(0.f, UE_TWO_PI);
		const float Out = (bFar ? Air.FRandRange(9300.f, 13000.f) : Air.FRandRange(1600.f, 7600.f)) * Spread;
		const float Height = bFar ? Air.FRandRange(-600.f, 2600.f) : Air.FRandRange(2100.f, 3600.f);
		const float Size = bFar ? Air.FRandRange(2.f, 4.5f) : Air.FRandRange(1.2f, 2.4f);
		const int32 Pick = Air.RandRange(0, UE_ARRAY_COUNT(DieBodies) - 1);
		const FRotator Turn(Air.FRandRange(-40.f, 40.f), Air.FRandRange(0.f, 360.f), Air.FRandRange(-40.f, 40.f));
		const float Spin = Air.FRandRange(8.f, 22.f) * (Air.FRand() < 0.5f ? -1.f : 1.f);
		Pair([&](const FVector2D& Local, float Phase)
		{
			AddFloater(FloatDice, At(Local, Height), Turn, Size, DieBodies[Pick], DiePips[Pick], Air.FRandRange(40.f, 90.f), Spin, Spin * 0.5f, 0.f, 0.f, Phase);
		}, FVector2D(FMath::Cos(Theta), FMath::Sin(Theta)) * Out, Air.FRandRange(0.f, 6.f));
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const float Theta = Air.FRandRange(0.f, UE_TWO_PI);
		const float Out = Air.FRandRange(2000.f, 11000.f) * Spread;
		const float Height = Air.FRandRange(2600.f, 4600.f);
		const float Size = Air.FRandRange(1.8f, 3.2f);
		Pair([&](const FVector2D& Local, float Phase)
		{
			AddFloater(Stars, At(Local, Height), FRotator(0.f, Air.FRandRange(0.f, 360.f), 0.f), Size, StarYellow, StarYellow, 60.f, Air.FRandRange(20.f, 40.f), 0.f, 0.f, 0.f, Phase);
		}, FVector2D(FMath::Cos(Theta), FMath::Sin(Theta)) * Out, Air.FRandRange(0.f, 6.f));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Walking routes

bool ARRRandomDiceTown::IsClearAbove(int32 X, int32 Y, int32 Level, int32 Count) const
{
	for (int32 Above = 1; Above <= Count; ++Above)
	{
		if (Get(X, Y, Level + Above))
		{
			return false;
		}
	}
	return true;
}

void ARRRandomDiceTown::MakeWalkGraph()
{
	// Where each stair cell is along its flight
	TMap<FIntPoint, TPair<int32, int32>> OnStairs;
	for (int32 Index = 0; Index < Stairs.Num(); ++Index)
	{
		const FStair& Stair = Stairs[Index];
		for (int32 K = 0; K < Stair.Rise; ++K)
		{
			for (int32 W = 0; W < Stair.Width; ++W)
			{
				OnStairs.Add(Stair.Start + Stair.Dir * K + Stair.Across * W, TPair<int32, int32>(Index, K));
			}
		}
	}

	NodeStart.SetNumUninitialized(GridCells * GridCells + 1);
	for (int32 Y = -HalfCells; Y < HalfCells; ++Y)
	{
		for (int32 X = -HalfCells; X < HalfCells; ++X)
		{
			NodeStart[(Y + HalfCells) * GridCells + (X + HalfCells)] = Nodes.Num();
			if (const TPair<int32, int32>* Step = OnStairs.Find(FIntPoint(X, Y)))
			{
				// Mid-cell on the 45 degree treads
				const FStair& Stair = Stairs[Step->Key];
				Nodes.Add({ FIntPoint(X, Y), (Stair.Base + Step->Value + 1) * BlockSize + Stair.NodeShift, Stair.Dir });
				continue;
			}
			// (round 96: not in a diagonal block, nor on one)
			const FIntPoint* Shaped = ShapedCells.Find(FIntPoint(X, Y));
			for (int32 Level = LevelMin; Level < LevelMax; ++Level)
			{
				if (Shaped && Level >= Shaped->X - 1 && Level <= Shaped->Y)
				{
					continue;
				}
				if (Get(X, Y, Level) && IsClearAbove(X, Y, Level, Headroom))
				{
					Nodes.Add({ FIntPoint(X, Y), Level * BlockSize, FIntPoint::ZeroValue });
				}
			}
		}
	}
	NodeStart[GridCells * GridCells] = Nodes.Num();

	// The ground anyone can reach from the plaza's edge (a battle map tile: from any of its roads)
	TBitArray<> Seen(false, Nodes.Num());
	TArray<int32> Queue;
	if (IsBattleTile())
	{
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			const FWalkNode& Node = Nodes[Index];
			if (Node.Z == 0.f && Node.StairDir == FIntPoint::ZeroValue && IsRoad(Node.Cell.X, Node.Cell.Y))
			{
				Seen[Index] = true;
				Queue.Add(Index);
			}
		}
	}
	else
	{
		const int32 Seed = FindNode(At(CellCenter(FIntPoint(0, -12)), 0.f), 2);
		if (Seed == INDEX_NONE)
		{
			return;
		}
		Queue.Add(Seed);
		Seen[Seed] = true;
	}
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		ForEachLink(Queue[Head], [&](int32 Next, float)
		{
			if (!Seen[Next])
			{
				Seen[Next] = true;
				Queue.Add(Next);
			}
		});
	}
	Roamable = MoveTemp(Queue);
}

void ARRRandomDiceTown::ForEachLink(int32 Node, TFunctionRef<void(int32, float)> Visit) const
{
	const FWalkNode& From = Nodes[Node];
	const int32 FromLevel = FMath::FloorToInt(From.Z / BlockSize + 0.01f);
	auto NodesAt = [this](const FIntPoint& Cell, int32& OutFirst, int32& OutLast)
	{
		if (Cell.X < -HalfCells || Cell.X >= HalfCells || Cell.Y < -HalfCells || Cell.Y >= HalfCells)
		{
			OutFirst = OutLast = 0;
			return;
		}
		const int32 Index = (Cell.Y + HalfCells) * GridCells + (Cell.X + HalfCells);
		OutFirst = NodeStart[Index];
		OutLast = NodeStart[Index + 1];
	};
	auto HasFlat = [&](const FIntPoint& Cell, float Z)
	{
		int32 First;
		int32 Last;
		NodesAt(Cell, First, Last);
		for (int32 Other = First; Other < Last; ++Other)
		{
			if (FMath::Abs(Nodes[Other].Z - Z) < 5.f && Nodes[Other].StairDir == FIntPoint::ZeroValue)
			{
				return true;
			}
		}
		return false;
	};
	for (int32 DX = -1; DX <= 1; ++DX)
	{
		for (int32 DY = -1; DY <= 1; ++DY)
		{
			if (DX == 0 && DY == 0)
			{
				continue;
			}
			const FIntPoint Offset(DX, DY);
			const FIntPoint Cell = From.Cell + Offset;
			const bool bDiagonal = DX != 0 && DY != 0;
			int32 First;
			int32 Last;
			NodesAt(Cell, First, Last);
			for (int32 Other = First; Other < Last; ++Other)
			{
				const FWalkNode& To = Nodes[Other];
				const float Rise = To.Z - From.Z;
				const bool bStairs = From.StairDir != FIntPoint::ZeroValue || To.StairDir != FIntPoint::ZeroValue;
				if (bDiagonal)
				{
					// Flat ground only, and not cutting a corner
					if (!bStairs && FMath::Abs(Rise) < 5.f && HasFlat(From.Cell + FIntPoint(DX, 0), From.Z) && HasFlat(From.Cell + FIntPoint(0, DY), From.Z))
					{
						Visit(Other, BlockSize * UE_SQRT_2);
					}
					continue;
				}
				if (FMath::Abs(Rise) < 5.f)
				{
					Visit(Other, BlockSize);
					continue;
				}
				// Up or down a flight, along its run
				const FIntPoint Back(-DX, -DY);
				const bool bAlong = (From.StairDir != FIntPoint::ZeroValue && (From.StairDir == Offset || From.StairDir == Back))
					|| (To.StairDir != FIntPoint::ZeroValue && (To.StairDir == Offset || To.StairDir == Back));
				if (bStairs && bAlong && FMath::Abs(Rise) <= BlockSize + 5.f)
				{
					Visit(Other, BlockSize * UE_SQRT_2);
					continue;
				}
				const int32 ToLevel = FMath::FloorToInt(To.Z / BlockSize + 0.01f);
				// A block up: a jump climbs it, with room overhead
				if (!bStairs && Rise > 5.f && Rise <= BlockSize + 5.f && IsClearAbove(From.Cell.X, From.Cell.Y, FromLevel, Headroom + 1))
				{
					Visit(Other, BlockSize * 3.f);
					continue;
				}
				// Off a ledge, with room to step out at the top
				if (Rise < -5.f && -Rise <= MaxDrop && IsClearAbove(Cell.X, Cell.Y, ToLevel, FromLevel - ToLevel + Headroom))
				{
					Visit(Other, BlockSize - Rise * 0.5f);
				}
			}
		}
	}
}

int32 ARRRandomDiceTown::FindNode(const FVector& Location, int32 SearchCells) const
{
	if (Nodes.Num() == 0)
	{
		return INDEX_NONE;
	}
	const FVector Local = Location - FVector(GetActorLocation().X, GetActorLocation().Y, FloorZ);
	const FIntPoint Here(FMath::FloorToInt(Local.X / BlockSize), FMath::FloorToInt(Local.Y / BlockSize));
	int32 Best = INDEX_NONE;
	float BestScore = UE_BIG_NUMBER;
	for (int32 Ring = 0; Ring <= SearchCells; ++Ring)
	{
		for (int32 DX = -Ring; DX <= Ring; ++DX)
		{
			for (int32 DY = -Ring; DY <= Ring; ++DY)
			{
				if (FMath::Max(FMath::Abs(DX), FMath::Abs(DY)) != Ring)
				{
					continue;
				}
				const FIntPoint Cell = Here + FIntPoint(DX, DY);
				if (Cell.X < -HalfCells || Cell.X >= HalfCells || Cell.Y < -HalfCells || Cell.Y >= HalfCells)
				{
					continue;
				}
				const int32 Index = (Cell.Y + HalfCells) * GridCells + (Cell.X + HalfCells);
				for (int32 Node = NodeStart[Index]; Node < NodeStart[Index + 1]; ++Node)
				{
					// Near, at the feet's height, and not over the head
					const float Up = Nodes[Node].Z - Local.Z;
					const float Score = FVector2D::Distance(CellCenter(Cell), FVector2D(Local.X, Local.Y)) + FMath::Abs(Up) * 1.5f + (Up > 60.f ? 500.f : 0.f);
					if (Score < BestScore)
					{
						BestScore = Score;
						Best = Node;
					}
				}
			}
		}
		if (Best != INDEX_NONE && Ring >= 1)
		{
			break;
		}
	}
	return Best;
}

bool ARRRandomDiceTown::FindPath(const FVector& From, const FVector& To, TArray<FVector>& OutPoints) const
{
	OutPoints.Reset();
	const int32 Start = FindNode(From, 3);
	const int32 Goal = FindNode(To, 6);
	if (Start == INDEX_NONE || Goal == INDEX_NONE)
	{
		return false;
	}
	auto Where = [this](int32 Node) { return At(CellCenter(Nodes[Node].Cell), Nodes[Node].Z); };
	const FVector GoalSpot = Where(Goal);

	// A* over the walking spots; if the goal can't be reached, the reachable spot nearest it
	TArray<float> Cost;
	Cost.Init(UE_BIG_NUMBER, Nodes.Num());
	TArray<int32> Parent;
	Parent.Init(INDEX_NONE, Nodes.Num());
	TBitArray<> Closed(false, Nodes.Num());
	struct FOpen
	{
		float Estimate;
		int32 Node;
	};
	TArray<FOpen> Open;
	auto ByEstimate = [](const FOpen& A, const FOpen& B) { return A.Estimate < B.Estimate; };
	Cost[Start] = 0.f;
	Open.HeapPush({ float(FVector::Dist(Where(Start), GoalSpot)), Start }, ByEstimate);
	int32 Best = Start;
	float BestLeft = FVector::Dist(Where(Start), GoalSpot);
	for (int32 Expanded = 0; Open.Num() > 0 && Expanded < MaxSearch; ++Expanded)
	{
		FOpen Top;
		Open.HeapPop(Top, ByEstimate, EAllowShrinking::No);
		if (Closed[Top.Node])
		{
			continue;
		}
		Closed[Top.Node] = true;
		const float Left = FVector::Dist(Where(Top.Node), GoalSpot);
		if (Left < BestLeft)
		{
			BestLeft = Left;
			Best = Top.Node;
		}
		if (Top.Node == Goal)
		{
			break;
		}
		ForEachLink(Top.Node, [&](int32 Next, float Step)
		{
			const float Through = Cost[Top.Node] + Step;
			if (!Closed[Next] && Through < Cost[Next])
			{
				Cost[Next] = Through;
				Parent[Next] = Top.Node;
				Open.HeapPush({ float(Through + FVector::Dist(Where(Next), GoalSpot)), Next }, ByEstimate);
			}
		});
	}
	const int32 End = Closed[Goal] ? Goal : Best;
	TArray<int32> Route;
	for (int32 Node = End; Node != INDEX_NONE && Node != Start; Node = Parent[Node])
	{
		Route.Add(Node);
	}
	Algo::Reverse(Route);
	if (Route.Num() == 0)
	{
		return End == Goal;
	}

	// Straightened: on level ground, the furthest spot ahead a brawler fits straight to. The probe is its body from just
	// over a step up to its head, wider than an animal friend (radius 42) by more than the 35 a walker may be off a corner it counts as reached, so a line past a lamp post or a wall's
	// corner doesn't scrape it (a narrower one did: bots stuck on the road's lamp posts); and the first stretch starts where
	// the walker really stands, or, if that way is blocked, from the middle of its cell
	const UWorld* World = GetWorld();
	const FCollisionShape Probe = FCollisionShape::MakeCapsule(RouteProbeRadius, RouteProbeHalfHeight);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomTownRoute), false);
	const FVector Raise(0.f, 0.f, RouteProbeLift + RouteProbeHalfHeight);
	FVector Anchor = Where(Start);
	const FVector Walker(From.X, From.Y, Anchor.Z);
	if (World->SweepTestByObjectType(Walker + Raise, Where(Route[0]) + Raise, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Probe, Params))
	{
		OutPoints.Add(Anchor);
	}
	else
	{
		Anchor = Walker;
	}
	for (int32 I = 0; I < Route.Num(); )
	{
		int32 Reach = I;
		const FWalkNode& First = Nodes[Route[I]];
		const bool bLevel = FMath::Abs(Anchor.Z - FloorZ - First.Z) < 5.f && First.StairDir == FIntPoint::ZeroValue;
		for (int32 J = I + 1; bLevel && J < FMath::Min(Route.Num(), I + 14); ++J)
		{
			const FWalkNode& Ahead = Nodes[Route[J]];
			if (FMath::Abs(Ahead.Z - First.Z) > 5.f || Ahead.StairDir != FIntPoint::ZeroValue
				|| World->SweepTestByObjectType(Anchor + Raise, Where(Route[J]) + Raise, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldStatic), Probe, Params))
			{
				break;
			}
			Reach = J;
		}
		Anchor = Where(Route[Reach]);
		OutPoints.Add(Anchor);
		I = Reach + 1;
	}
	return true;
}

FVector ARRRandomDiceTown::GetRandomWalkSpot(FRandomStream& Stream, float Radius) const
{
	for (int32 Try = 0; Try < 60 && Roamable.Num() > 0; ++Try)
	{
		const FWalkNode& Node = Nodes[Roamable[Stream.RandRange(0, Roamable.Num() - 1)]];
		if (CellCenter(Node.Cell).Size() > Radius)
		{
			continue;
		}
		// Not where the ground it stood on is broken, nor in a crater's lava (round 89: meteors, falling stars)
		const int32 Level = FMath::RoundToInt(Node.Z / BlockSize);
		if (Node.StairDir == FIntPoint::ZeroValue && !IsFilled(Node.Cell.X, Node.Cell.Y, Level))
		{
			continue;
		}
		if (LavaColumns.Contains((Node.Cell.Y + HalfCells) * GridCells + (Node.Cell.X + HalfCells)) || DugDepth.Contains((Node.Cell.Y + HalfCells) * GridCells + (Node.Cell.X + HalfCells)))
		{
			continue;
		}
		return At(CellCenter(Node.Cell), Node.Z);
	}
	return At(CellCenter(FIntPoint(0, -12)), 0.f);
}

// ---------------------------------------------------------------------------------------------------------------------

void ARRRandomDiceTown::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}
	// This map's rules for brawlers (now and then, for brawlers who joined since): its stairs are 45 degree slopes, so
	// they walk slopes a little steeper than usual (how high they fly is the map's GetFlightArea)
	RulesTimer -= DeltaSeconds;
	if (RulesTimer <= 0.f)
	{
		RulesTimer = 0.5f;
		for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
		{
			if (UCharacterMovementComponent* Movement = It->GetCharacterMovement())
			{
				Movement->SetWalkableFloorAngle(46.f);
			}
		}
	}
	// (a battle map tile streamed out keeps its lamp stars still: they are out of the scene)
	if (Floaters.Num() > 0 && (bDetailShown || BuildsSky()))
	{
		AnimateFloaters(GetWorld()->GetTimeSeconds());
	}
	if (Falling.Num() > 0)
	{
		TickFalling(DeltaSeconds);
	}
	// The town's colors (every material's "Saturation" and "Brightness"), when the console values change
	const float Saturation = CVarSaturation.GetValueOnGameThread();
	const float Brightness = CVarBrightness.GetValueOnGameThread();
	if (Saturation != AppliedSaturation || Brightness != AppliedBrightness)
	{
		AppliedSaturation = Saturation;
		AppliedBrightness = Brightness;
		for (UMaterialInstanceDynamic* Material : Tunable)
		{
			Material->SetScalarParameterValue(TEXT("Saturation"), Saturation);
			Material->SetScalarParameterValue(TEXT("Brightness"), Brightness);
		}
	}
}

FColor ARRRandomDiceTown::GetMapColor(const FVector2D& WorldLocation, int32& OutLevels) const
{
	const FVector2D Local = WorldLocation - FVector2D(GetActorLocation().X, GetActorLocation().Y);
	const int32 X = FMath::FloorToInt(Local.X / BlockSize);
	const int32 Y = FMath::FloorToInt(Local.Y / BlockSize);
	if (X < -HalfCells || X >= HalfCells || Y < -HalfCells || Y >= HalfCells || MapColors.Num() == 0)
	{
		OutLevels = -1;
		return MapSky;
	}
	const int32 Index = (Y + HalfCells) * GridCells + (X + HalfCells);
	OutLevels = MapLevels[Index];
	return MapColors[Index];
}

// ---------------------------------------------------------------------------------------------------------------------
// Wrecks (round 60)

namespace
{
	/** Looks gone with a wreck: drawn at no size where they stood. */
	FTransform Shrunk(const FTransform& Was)
	{
		return FTransform(Was.GetRotation(), Was.GetLocation(), FVector::ZeroVector);
	}

	/** What stopped things there: put far under the island. */
	FTransform Stowed(const FTransform& Was)
	{
		return FTransform(Was.GetRotation(), Was.GetLocation() - FVector(0.f, 0.f, 200000.f), Was.GetScale3D());
	}

	/** Floaters this high over the floor at most go with a wreck (the star lamps' stars, not the dice drifting in the sky). */
	constexpr float WreckFloaterHeight = 1200.f;
}

FTransform ARRRandomDiceTown::GetCellTransform(int32 X, int32 Y, int32 Level) const
{
	return FTransform(FRotator::ZeroRotator, At(CellCenter(FIntPoint(X, Y)), (Level - 0.5f) * BlockSize));
}

UInstancedStaticMeshComponent* ARRRandomDiceTown::GetBlockSet(bool bDie, bool bGloss) const
{
	return bDie ? DieBlocks.Get() : bGloss && GlossBlocks ? GlossBlocks.Get() : Blocks.Get();
}

void ARRRandomDiceTown::AddCellBlock(int32 X, int32 Y, int32 Level)
{
	const FLook& Look = Looks[Grid[GridIndex(X, Y, Level)] - 1];
	const float Jitter = 0.95f + 0.1f * float(CellHash(X, Y, Level) % 1000) / 1000.f;
	UInstancedStaticMeshComponent* Instances = GetBlockSet(Look.bDie, Look.bGloss);
	AddColored(Instances, GetCellTransform(X, Y, Level), Shade(Look.Body, Jitter), Shade(Look.Accent, Jitter));
	CellBlocks.Add(GridIndex(X, Y, Level), { Look.bDie, Instances->GetInstanceCount() - 1, true, Look.bGloss && !Look.bDie });
}

bool ARRRandomDiceTown::IsFilled(int32 X, int32 Y, int32 Level) const
{
	if (!IsInGrid(X, Y, Level))
	{
		return false;
	}
	const int32 Index = GridIndex(X, Y, Level);
	return Grid[Index] != 0 && !(Broken.IsValidIndex(Index) && Broken[Index]);
}

bool ARRRandomDiceTown::HidesFace(int32 X, int32 Y, int32 Level) const
{
	if (!IsFilled(X, Y, Level))
	{
		return false;
	}
	const uint8 Value = Grid[GridIndex(X, Y, Level)];
	return Value == HiddenSolid || !Looks[Value - 1].bUnderMesh;
}

bool ARRRandomDiceTown::Touches(const FRRWreck& Wreck, const FBox& Box) const
{
	if (Wreck.bColumn && (Box.Max.Z < FloorZ + 5.f || Box.Min.Z > FloorZ + LevelMax * BlockSize))
	{
		return false;
	}
	// The box's point nearest the line's point nearest the box's middle
	const FVector Middle = Box.GetCenter();
	const FVector Line = Wreck.To - Wreck.From;
	const float Alpha = Line.SizeSquared() > 1.f ? FMath::Clamp(FVector::DotProduct(Middle - Wreck.From, Line) / Line.SizeSquared(), 0.f, 1.f) : 0.f;
	FVector Near = Wreck.From + Line * Alpha;
	if (Wreck.bColumn)
	{
		Near.Z = Middle.Z;
	}
	return Wreck.Contains(Box.GetClosestPointTo(Near));
}

void ARRRandomDiceTown::RefreshCellBlock(int32 X, int32 Y, int32 Level, bool bGrow, TArray<FRRRegrow>& OutRegrow)
{
	// Down to the ground's bottom layer (a crater's floor); the island's underside keeps its own rule in MakeBlocks, so the
	// bottom layer's underneath doesn't count
	if (!IsInGrid(X, Y, Level) || Level < GroundBottom)
	{
		return;
	}
	const int32 Index = GridIndex(X, Y, Level);
	const uint8 Value = Grid[Index];
	if (Value == 0 || Value == HiddenSolid || Looks[Value - 1].bUnderMesh)
	{
		return;
	}
	const bool bBelowOpen = Level > GroundBottom && !HidesFace(X, Y, Level - 1);
	const bool bWant = !Broken[Index] && (!HidesFace(X + 1, Y, Level) || !HidesFace(X - 1, Y, Level) || !HidesFace(X, Y + 1, Level) || !HidesFace(X, Y - 1, Level)
		|| !HidesFace(X, Y, Level + 1) || bBelowOpen);
	FCellBlock* Block = CellBlocks.Find(Index);
	if (!Block)
	{
		// Inside a wall until now: drawn from now on, shown or hidden as it goes
		if (bWant)
		{
			AddCellBlock(X, Y, Level);
		}
		return;
	}
	if (Block->bShown == bWant)
	{
		return;
	}
	Block->bShown = bWant;
	UInstancedStaticMeshComponent* Instances = GetBlockSet(Block->bDie, Block->bGloss);
	const FTransform Where = GetCellTransform(X, Y, Level);
	Instances->UpdateInstanceTransform(Block->Index, bWant ? Where : Shrunk(Where), true, false, false);
	if (bWant && bGrow)
	{
		OutRegrow.Add({ Instances, Block->Index, Where, FMath::Min(RegrowMaxDelay, (Level - GroundBottom) * RegrowStep) });
	}
}

void ARRRandomDiceTown::ApplyWrecks(TConstArrayView<FRRWreck> InWrecks, TArray<FRRBrokenBit>& OutBroken, TArray<FRRRegrow>& OutRegrow)
{
	if (!bBuilt)
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();
	if (Broken.Num() != Grid.Num())
	{
		Broken.Init(false, Grid.Num());
	}
	const FVector Middle = GetActorLocation();
	const float Top = FloorZ + LevelMax * BlockSize;
	auto CellAt = [&](const FVector& World)
	{
		return FIntVector(FMath::FloorToInt((World.X - Middle.X) / BlockSize), FMath::FloorToInt((World.Y - Middle.Y) / BlockSize),
			FMath::FloorToInt((World.Z - FloorZ) / BlockSize) + 1);
	};
	auto CellOf = [this](int32 Index)
	{
		return FIntVector(Index % GridCells - HalfCells, Index / GridCells % GridCells - HalfCells, Index / (GridCells * GridCells) + LevelMin);
	};
	auto IsUnderMesh = [this](int32 Index)
	{
		const uint8 Value = Grid[Index];
		return Value != 0 && Value != HiddenSolid && Looks[Value - 1].bUnderMesh;
	};

	TBitArray<> Now(false, Grid.Num());
	TArray<int32> NowCells;
	auto Break = [&](int32 Index)
	{
		// (a defense stage's castle stands whatever hits it)
		if (IsCastle(Index % GridCells - HalfCells, Index / GridCells % GridCells - HalfCells))
		{
			return;
		}
		if (!Now[Index])
		{
			Now[Index] = true;
			NowCells.Add(Index);
		}
	};
	// The cells (over the floor) inside a box in the world, a little in from its sides
	auto ForCellsIn = [&](const FBox& Box, TFunctionRef<void(int32)> Visit)
	{
		const FIntVector Low = CellAt(Box.Min + FVector(10.f));
		const FIntVector High = CellAt(Box.Max - FVector(10.f));
		for (int32 Level = FMath::Max(1, Low.Z); Level <= FMath::Min(LevelMax, High.Z); ++Level)
		{
			for (int32 Y = FMath::Max(-HalfCells, Low.Y); Y <= FMath::Min(HalfCells - 1, High.Y); ++Y)
			{
				for (int32 X = FMath::Max(-HalfCells, Low.X); X <= FMath::Min(HalfCells - 1, High.X); ++X)
				{
					Visit(GridIndex(X, Y, Level));
				}
			}
		}
	};
	// Where a model stands (as it stood, if it's gone), and the box round it
	auto Standing = [this](UInstancedStaticMeshComponent* Instances, int32 Index)
	{
		if (const TMap<int32, FTransform>* Mine = Gone.Find(Instances))
		{
			if (const FTransform* Was = Mine->Find(Index))
			{
				return *Was;
			}
		}
		FTransform Transform;
		Instances->GetInstanceTransform(Index, Transform, true);
		return Transform;
	};
	auto BoundsOf = [&](UInstancedStaticMeshComponent* Instances, int32 Index)
	{
		return Instances->GetStaticMesh()->GetBoundingBox().TransformBy(Standing(Instances, Index));
	};
	auto IsHit = [&](const FBox& Box)
	{
		for (const FRRWreck& Wreck : InWrecks)
		{
			if (Touches(Wreck, Box))
			{
				return true;
			}
		}
		return false;
	};

	// Models drawn over hidden solid cells (roofs, Tripo trees, hedges, lamp posts): one a wreck reaches goes whole, its
	// cells with it; then any other drawn over a cell that went goes too (a hedge's smaller puffs, a post's other bands)
	TMap<UInstancedStaticMeshComponent*, TSet<int32>> WantGone;
	TArray<UInstancedStaticMeshComponent*> Owners = { Roofs.Get(), Marshmallows.Get(), Poles.Get() };
	for (UInstancedStaticMeshComponent* Trees : PropTrees)
	{
		Owners.Add(Trees);
	}
	for (UInstancedStaticMeshComponent* Models : CoverModels)
	{
		Owners.Add(Models);
	}
	Owners.RemoveAll([](const UInstancedStaticMeshComponent* Instances) { return !Instances || !Instances->GetStaticMesh(); });
	for (UInstancedStaticMeshComponent* Instances : Owners)
	{
		for (int32 Index = 0; Index < Instances->GetInstanceCount(); ++Index)
		{
			const FBox Box = BoundsOf(Instances, Index);
			if (IsHit(Box))
			{
				WantGone.FindOrAdd(Instances).Add(Index);
				ForCellsIn(Box, [&](int32 Cell) { if (IsUnderMesh(Cell)) { Break(Cell); } });
			}
		}
	}

	// The blocks: every cell over the floor whose middle is in a wreck (not the giant dice, nor what models draw)
	for (const FRRWreck& Wreck : InWrecks)
	{
		const FBox Box = Wreck.GetBounds(FloorZ, Top);
		const FIntVector Low = CellAt(Box.Min);
		const FIntVector High = CellAt(Box.Max);
		for (int32 Level = FMath::Max(1, Low.Z); Level <= FMath::Min(LevelMax, High.Z); ++Level)
		{
			for (int32 Y = FMath::Max(-HalfCells, Low.Y); Y <= FMath::Min(HalfCells - 1, High.Y); ++Y)
			{
				for (int32 X = FMath::Max(-HalfCells, Low.X); X <= FMath::Min(HalfCells - 1, High.X); ++X)
				{
					const int32 Index = GridIndex(X, Y, Level);
					const uint8 Value = Grid[Index];
					if (Value != 0 && Value != HiddenSolid && !Looks[Value - 1].bUnderMesh && Wreck.Contains(GetCellTransform(X, Y, Level).GetLocation()))
					{
						Break(Index);
					}
				}
			}
		}
	}
	// Round 89. The giant dice: one a wreck reaches goes whole, its cells with it
	TSet<int32> DiceHit;
	for (int32 Die = 0; Die < GiantDice.Num(); ++Die)
	{
		const FGiantDie& Giant = GiantDice[Die];
		const FVector Low = At(CellCenter(FIntPoint(Giant.Low.X, Giant.Low.Y)) - FVector2D(BlockSize * 0.5f), (Giant.Low.Z - 1) * BlockSize);
		if (!IsHit(FBox(Low, Low + FVector(Giant.Size * BlockSize))))
		{
			continue;
		}
		DiceHit.Add(Die);
		WantGone.FindOrAdd(DieBlocks).Add(Giant.Instance);
		for (int32 DX = 0; DX < Giant.Size; ++DX)
		{
			for (int32 DY = 0; DY < Giant.Size; ++DY)
			{
				for (int32 Level = Giant.Low.Z; Level < Giant.Low.Z + Giant.Size; ++Level)
				{
					if (IsInGrid(Giant.Low.X + DX, Giant.Low.Y + DY, Level))
					{
						Break(GridIndex(Giant.Low.X + DX, Giant.Low.Y + DY, Level));
					}
				}
			}
		}
	}

	// The ground: what each wreck that reaches the floor digs adds up column by column (blow after blow digs deeper), then
	// no column more than a layer deeper than any beside it (a crater is a bowl of steps a jump climbs out of). Three
	// layers is all the ground there is: a column dug that deep has broken through to the mantle (its bottom layer stays
	// as the lava's bed). The same on every machine: whole cells and the same list of wrecks.
	auto ColumnOf = [this](int32 X, int32 Y) { return (Y + HalfCells) * GridCells + (X + HalfCells); };
	auto ColumnCell = [this](int32 Column) { return FIntPoint(Column % GridCells - HalfCells, Column / GridCells - HalfCells); };
	TMap<int32, int32> Depth;
	for (const FRRWreck& Wreck : InWrecks)
	{
		if (Wreck.Dig == 0)
		{
			continue;
		}
		// How far round it reaches across the floor: a column all of its width, a ball as wide as it is where it cuts the floor
		const float Over = Wreck.bColumn ? 0.f : FMath::Min(Wreck.From.Z, Wreck.To.Z) - FloorZ;
		if (Over > Wreck.Radius)
		{
			continue;
		}
		const float Reach = FMath::Clamp(Over > 0.f ? FMath::Sqrt(FMath::Square(Wreck.Radius) - FMath::Square(Over)) : Wreck.Radius, DigReachMin, DigReachMax);
		const FVector A(Wreck.From.X, Wreck.From.Y, 0.f);
		const FVector B(Wreck.To.X, Wreck.To.Y, 0.f);
		const FIntVector LowCell = CellAt(A.ComponentMin(B) - FVector(Reach));
		const FIntVector HighCell = CellAt(A.ComponentMax(B) + FVector(Reach));
		for (int32 Y = FMath::Max(-HalfCells, LowCell.Y); Y <= FMath::Min(HalfCells - 1, HighCell.Y); ++Y)
		{
			for (int32 X = FMath::Max(-HalfCells, LowCell.X); X <= FMath::Min(HalfCells - 1, HighCell.X); ++X)
			{
				const FVector2D Spot = FVector2D(Middle) + CellCenter(FIntPoint(X, Y));
				if (IsOnIsland(X, Y) && !IsCastle(X, Y) && FMath::PointDistToSegment(FVector(Spot, 0.f), A, B) <= Reach)
				{
					int32& Dug = Depth.FindOrAdd(ColumnOf(X, Y));
					Dug = FMath::Min(BreachDepth, Dug + Wreck.Dig);
				}
			}
		}
	}
	for (bool bLowered = true; bLowered; )
	{
		bLowered = false;
		for (TPair<int32, int32>& Pair : Depth)
		{
			const FIntPoint Cell = ColumnCell(Pair.Key);
			int32 Shallowest = Pair.Value;
			for (const FIntPoint& Step : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
			{
				const int32* Next = Depth.Find(ColumnOf(Cell.X + Step.X, Cell.Y + Step.Y));
				Shallowest = FMath::Min(Shallowest, Next ? *Next : 0);
			}
			if (Pair.Value > Shallowest + 1)
			{
				Pair.Value = Shallowest + 1;
				bLowered = true;
			}
		}
	}
	for (const TPair<int32, int32>& Pair : Depth)
	{
		const FIntPoint Cell = ColumnCell(Pair.Key);
		for (int32 Level = 0; Level > FMath::Max(GroundBottom, -Pair.Value); --Level)
		{
			if (Grid[GridIndex(Cell.X, Cell.Y, Level)] != 0)
			{
				Break(GridIndex(Cell.X, Cell.Y, Level));
			}
		}
	}
	// Lava: every dug column joined (side by side) to one broken through
	TSet<int32> LavaNow;
	TArray<int32> LavaQueue;
	for (const TPair<int32, int32>& Pair : Depth)
	{
		if (Pair.Value >= BreachDepth)
		{
			LavaNow.Add(Pair.Key);
			LavaQueue.Add(Pair.Key);
		}
	}
	for (int32 Head = 0; Head < LavaQueue.Num(); ++Head)
	{
		const FIntPoint Cell = ColumnCell(LavaQueue[Head]);
		for (const FIntPoint& Step : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
		{
			const int32 Next = ColumnOf(Cell.X + Step.X, Cell.Y + Step.Y);
			if (Depth.Contains(Next) && !LavaNow.Contains(Next))
			{
				LavaNow.Add(Next);
				LavaQueue.Add(Next);
			}
		}
	}

	// What is no longer held up from the ground falls: everything solid that hangs together with the ground, through
	// cells side by side or one on another, stands (and whatever stood on nothing as built, the dream sky's pieces);
	// the rest comes down
	TBitArray<> Held;
	if (!bFoundAnchored)
	{
		bFoundAnchored = true;
		const TBitArray<> Nothing(false, Grid.Num());
		FindHeldUp(Nothing, Held);
		Anchored.Init(false, Grid.Num());
		int32 Count = 0;
		for (int32 Index = 0; Index < Grid.Num(); ++Index)
		{
			if (Grid[Index] != 0 && !Held[Index])
			{
				Anchored[Index] = true;
				++Count;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town: %d blocks stand on nothing as built (they never fall)."), Count);
	}
	FindHeldUp(Now, Held);
	TBitArray<> FallenNow(false, Grid.Num());
	TArray<int32> NewlyFallen;
	for (int32 Index = (1 - LevelMin) * GridCells * GridCells; Index < Grid.Num(); ++Index)
	{
		if (Grid[Index] != 0 && !Now[Index] && !Held[Index])
		{
			FallenNow[Index] = true;
			Break(Index);
			if (!(Fallen.IsValidIndex(Index) && Fallen[Index]))
			{
				NewlyFallen.Add(Index);
			}
		}
	}
	// The giant dice that fall go whole with their cells
	TSet<int32> DiceFalling;
	for (int32 Die = 0; Die < GiantDice.Num(); ++Die)
	{
		const FGiantDie& Giant = GiantDice[Die];
		if (IsInGrid(Giant.Low.X, Giant.Low.Y, Giant.Low.Z) && FallenNow[GridIndex(Giant.Low.X, Giant.Low.Y, Giant.Low.Z)])
		{
			WantGone.FindOrAdd(DieBlocks).Add(Giant.Instance);
			DiceFalling.Add(Giant.Instance);
		}
	}
	// Flights of stairs whose foot stood on something that went
	for (const FStair& Stair : Stairs)
	{
		const int32 Foot = IsInGrid(Stair.Start.X, Stair.Start.Y, Stair.Base) ? GridIndex(Stair.Start.X, Stair.Start.Y, Stair.Base) : INDEX_NONE;
		if (Foot == INDEX_NONE || Grid[Foot] == 0 || !Now[Foot])
		{
			continue;
		}
		for (int32 Step = Stair.StepFirst; Step < Stair.StepFirst + Stair.StepCount; ++Step)
		{
			WantGone.FindOrAdd(Stair.StepSet ? Stair.StepSet : Steps.Get()).Add(Step);
		}
		for (int32 Slab = Stair.SolidFirst; Slab < Stair.SolidFirst + Stair.SolidCount; ++Slab)
		{
			WantGone.FindOrAdd(Solid).Add(Slab);
		}
	}

	for (UInstancedStaticMeshComponent* Instances : Owners)
	{
		const TSet<int32>* Already = WantGone.Find(Instances);
		TArray<int32> More;
		for (int32 Index = 0; Index < Instances->GetInstanceCount(); ++Index)
		{
			if (Already && Already->Contains(Index))
			{
				continue;
			}
			bool bOver = false;
			const FBox Box = BoundsOf(Instances, Index);
			ForCellsIn(Box, [&](int32 Cell) { bOver |= Now[Cell] && IsUnderMesh(Cell); });
			// (Round 89) Or what it rests on is mostly gone: a roof whose walls came down, a lamp on dug ground
			if (!bOver)
			{
				int32 Under = 0;
				int32 UnderGone = 0;
				const FIntVector Low = CellAt(Box.Min + FVector(10.f));
				const FIntVector High = CellAt(Box.Max - FVector(10.f));
				const int32 Level = CellAt(Box.Min + FVector(0.f, 0.f, 20.f)).Z - 1;
				for (int32 Y = FMath::Max(-HalfCells, Low.Y); Y <= FMath::Min(HalfCells - 1, High.Y) && Level >= LevelMin; ++Y)
				{
					for (int32 X = FMath::Max(-HalfCells, Low.X); X <= FMath::Min(HalfCells - 1, High.X); ++X)
					{
						const int32 Cell = GridIndex(X, Y, Level);
						if (Grid[Cell] != 0)
						{
							++Under;
							UnderGone += Now[Cell] ? 1 : 0;
						}
					}
				}
				bOver = Under > 0 && UnderGone * 2 > Under;
			}
			if (bOver)
			{
				More.Add(Index);
			}
		}
		if (More.Num() > 0)
		{
			WantGone.FindOrAdd(Instances).Append(More);
		}
	}

	// Looks that stop nothing (awnings), and whole flights of stairs (looks and slabs) a wreck reaches a step of
	if (Decor)
	{
		for (int32 Index = 0; Index < Decor->GetInstanceCount(); ++Index)
		{
			if (IsHit(BoundsOf(Decor, Index)))
			{
				WantGone.FindOrAdd(Decor).Add(Index);
			}
		}
	}
	for (const FStair& Stair : Stairs)
	{
		// (a castle's stairs stand)
		if (IsCastle(Stair.Start.X, Stair.Start.Y))
		{
			continue;
		}
		bool bHit = false;
		for (int32 Step = Stair.StepFirst; Step < Stair.StepFirst + Stair.StepCount && !bHit; ++Step)
		{
			bHit = IsHit(BoundsOf(Stair.StepSet ? Stair.StepSet : Steps.Get(), Step));
		}
		if (bHit)
		{
			for (int32 Step = Stair.StepFirst; Step < Stair.StepFirst + Stair.StepCount; ++Step)
			{
				WantGone.FindOrAdd(Stair.StepSet ? Stair.StepSet : Steps.Get()).Add(Step);
			}
			for (int32 Slab = Stair.SolidFirst; Slab < Stair.SolidFirst + Stair.SolidCount; ++Slab)
			{
				WantGone.FindOrAdd(Solid).Add(Slab);
			}
		}
	}
	// Round 96: diagonal blocks, a run at a time: one whose middle a wreck reaches (as the blocks: a wreck merely touching
	// a cut corner took it and left the blocks behind it standing), or the cell under the run's first gone
	for (const FShapeRun& Run : ShapeRuns)
	{
		const int32 Foot = IsInGrid(Run.Foot.X, Run.Foot.Y, Run.Foot.Z) ? GridIndex(Run.Foot.X, Run.Foot.Y, Run.Foot.Z) : INDEX_NONE;
		bool bGone = Foot != INDEX_NONE && Grid[Foot] != 0 && Now[Foot];
		for (int32 Piece = 0; Piece < Run.Pieces.Num() && !bGone; ++Piece)
		{
			const FVector PieceMiddle = BoundsOf(Run.Pieces[Piece].Key, Run.Pieces[Piece].Value).GetCenter();
			bGone = IsHit(FBox(PieceMiddle - FVector(1.f), PieceMiddle + FVector(1.f)));
		}
		if (bGone)
		{
			for (const TPair<UInstancedStaticMeshComponent*, int32>& Piece : Run.Pieces)
			{
				WantGone.FindOrAdd(Piece.Key).Add(Piece.Value);
			}
			for (int32 Slab = Run.SolidFirst; Slab < Run.SolidFirst + Run.SolidCount; ++Slab)
			{
				WantGone.FindOrAdd(Solid).Add(Slab);
			}
		}
	}
	// The lamps' stars (floaters low over the town)
	for (FFloater& Floater : Floaters)
	{
		Floater.bWrecked = Floater.Orbit <= 0.f && Floater.Base.Z < FloorZ + WreckFloaterHeight && IsHit(FBox(Floater.Base - FVector(40.f), Floater.Base + FVector(40.f)));
	}

	// What changed: cells broken just now (rubble), cells mended
	TArray<int32> Changed;
	TSet<int32> Mended;
	for (const int32 Index : BrokenCells)
	{
		if (!Now[Index])
		{
			Changed.Add(Index);
			Mended.Add(Index);
		}
	}
	for (const int32 Index : NowCells)
	{
		if (!Broken[Index])
		{
			Changed.Add(Index);
			// What falls breaks up where it lands, a giant die in its own colors
			if (FallenNow[Index] || Grid[Index] == HiddenSolid)
			{
				continue;
			}
			const FIntVector Cell = CellOf(Index);
			const FLook& Look = Looks[Grid[Index] - 1];
			OutBroken.Add({ GetCellTransform(Cell.X, Cell.Y, Cell.Z).GetLocation(), FLinearColor(Look.Body), FLinearColor(Look.Accent), 1.f });
		}
	}
	// The blocks of what falls just now, as they stand (before they are hidden): they come down as one piece (StartFalling)
	TArray<FFallingPiece> FallingBlocks;
	for (const int32 Index : NewlyFallen)
	{
		const FCellBlock* Block = CellBlocks.Find(Index);
		if (Block && Block->bShown)
		{
			const FIntVector Cell = CellOf(Index);
			FallingBlocks.Add({ GetBlockSet(Block->bDie, Block->bGloss), Block->Index, GetCellTransform(Cell.X, Cell.Y, Cell.Z), Index });
		}
	}
	Broken = MoveTemp(Now);
	BrokenCells = MoveTemp(NowCells);
	Fallen = MoveTemp(FallenNow);

	// The blocks drawn: each changed cell and its neighbors (bared or buried again)
	TSet<int32> Refresh;
	for (const int32 Index : Changed)
	{
		const FIntVector Cell = CellOf(Index);
		for (const FIntVector& Step : { FIntVector(0, 0, 0), FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0), FIntVector(0, 0, 1), FIntVector(0, 0, -1) })
		{
			const FIntVector Next = Cell + Step;
			if (IsInGrid(Next.X, Next.Y, Next.Z))
			{
				Refresh.Add(GridIndex(Next.X, Next.Y, Next.Z));
			}
		}
	}
	for (const int32 Index : Refresh)
	{
		const FIntVector Cell = CellOf(Index);
		RefreshCellBlock(Cell.X, Cell.Y, Cell.Z, Mended.Contains(Index), OutRegrow);
	}
	if (Refresh.Num() > 0)
	{
		Blocks->MarkRenderStateDirty();
		DieBlocks->MarkRenderStateDirty();
		if (GlossBlocks)
		{
			GlossBlocks->MarkRenderStateDirty();
		}
	}

	// What stops: each merged box with a broken cell in it goes under the island, and boxes merged again from what's left
	// of it stand in for it (the way MakeCollision merges); once none of its cells is broken it comes back
	auto BoxTransform = [this](const FIntVector& Min, const FIntVector& Max)
	{
		const FVector2D BoxMiddle = (CellCenter(FIntPoint(Min.X, Min.Y)) + CellCenter(FIntPoint(Max.X, Max.Y))) * 0.5f;
		const float Bottom = (Min.Z - 1) * BlockSize;
		const float BoxTop = Max.Z * BlockSize;
		return FTransform(FRotator::ZeroRotator, At(BoxMiddle, (Bottom + BoxTop) * 0.5f), FVector(Max.X - Min.X + 1, Max.Y - Min.Y + 1, Max.Z - Min.Z + 1));
	};
	TSet<int32> DirtyBoxes;
	for (const int32 Index : Changed)
	{
		if (BoxOfCell.IsValidIndex(Index) && BoxOfCell[Index] != INDEX_NONE)
		{
			DirtyBoxes.Add(BoxOfCell[Index]);
		}
	}
	for (const int32 Box : DirtyBoxes)
	{
		const FGridBox& Cells = GridBoxes[Box];
		const FIntVector Size = Cells.Max - Cells.Min + FIntVector(1);
		TBitArray<> Covered(false, Size.X * Size.Y * Size.Z);
		auto Local = [&](int32 X, int32 Y, int32 Level) { return ((Level - Cells.Min.Z) * Size.Y + (Y - Cells.Min.Y)) * Size.X + (X - Cells.Min.X); };
		auto IsOpen = [&](int32 X, int32 Y, int32 Level)
		{
			return X >= Cells.Min.X && X <= Cells.Max.X && Y >= Cells.Min.Y && Y <= Cells.Max.Y && Level >= Cells.Min.Z && Level <= Cells.Max.Z
				&& !Broken[GridIndex(X, Y, Level)] && !Covered[Local(X, Y, Level)];
		};
		int32 BrokenCount = 0;
		for (int32 Level = Cells.Min.Z; Level <= Cells.Max.Z; ++Level)
		{
			for (int32 Y = Cells.Min.Y; Y <= Cells.Max.Y; ++Y)
			{
				for (int32 X = Cells.Min.X; X <= Cells.Max.X; ++X)
				{
					BrokenCount += Broken[GridIndex(X, Y, Level)] ? 1 : 0;
				}
			}
		}
		const int32 Instance = FirstGridBox + Box;
		if (BrokenCount == 0)
		{
			if (CutBoxes.Remove(Box) > 0)
			{
				Solid->UpdateInstanceTransform(Instance, BoxTransform(Cells.Min, Cells.Max), true, false, true);
			}
			continue;
		}
		TArray<FTransform> Left;
		for (int32 Level = Cells.Min.Z; Level <= Cells.Max.Z; ++Level)
		{
			for (int32 Y = Cells.Min.Y; Y <= Cells.Max.Y; ++Y)
			{
				for (int32 X = Cells.Min.X; X <= Cells.Max.X; ++X)
				{
					if (!IsOpen(X, Y, Level))
					{
						continue;
					}
					int32 X1 = X;
					while (IsOpen(X1 + 1, Y, Level))
					{
						++X1;
					}
					auto RowOpen = [&](int32 AtY, int32 AtLevel)
					{
						for (int32 I = X; I <= X1; ++I)
						{
							if (!IsOpen(I, AtY, AtLevel))
							{
								return false;
							}
						}
						return true;
					};
					int32 Y1 = Y;
					while (RowOpen(Y1 + 1, Level))
					{
						++Y1;
					}
					int32 L1 = Level;
					for (bool bGrow = true; bGrow; )
					{
						for (int32 J = Y; J <= Y1 && bGrow; ++J)
						{
							bGrow = RowOpen(J, L1 + 1);
						}
						L1 += bGrow ? 1 : 0;
					}
					for (int32 L = Level; L <= L1; ++L)
					{
						for (int32 J = Y; J <= Y1; ++J)
						{
							for (int32 I = X; I <= X1; ++I)
							{
								Covered[Local(I, J, L)] = true;
							}
						}
					}
					Left.Add(BoxTransform(FIntVector(X, Y, Level), FIntVector(X1, Y1, L1)));
				}
			}
		}
		if (!CutBoxes.Contains(Box))
		{
			Solid->UpdateInstanceTransform(Instance, Stowed(BoxTransform(Cells.Min, Cells.Max)), true, false, true);
		}
		CutBoxes.Add(Box, MoveTemp(Left));
	}

	// Models gone now, and back once no wreck reaches them (from the bottom up, a little at a time)
	TMap<int32, int32> DieOfInstance;
	for (int32 Die = 0; Die < GiantDice.Num(); ++Die)
	{
		DieOfInstance.Add(GiantDice[Die].Instance, Die);
	}
	TSet<UInstancedStaticMeshComponent*> Touched;
	for (TPair<UInstancedStaticMeshComponent*, TMap<int32, FTransform>>& Pair : Gone)
	{
		UInstancedStaticMeshComponent* Instances = Pair.Key;
		const TSet<int32>* Want = WantGone.Find(Instances);
		for (auto It = Pair.Value.CreateIterator(); It; ++It)
		{
			if (!Want || !Want->Contains(It.Key()))
			{
				Instances->UpdateInstanceTransform(It.Key(), It.Value(), true, false, true);
				if (Instances != Solid)
				{
					const float Up = (It.Value().GetLocation().Z - FloorZ) / BlockSize - GroundBottom;
					OutRegrow.Add({ Instances, It.Key(), It.Value(), FMath::Clamp(Up * RegrowStep, 0.f, RegrowMaxDelay) });
				}
				Touched.Add(Instances);
				It.RemoveCurrent();
			}
		}
	}
	TMap<UInstancedStaticMeshComponent*, TMap<int32, FTransform>> NewlyGone;
	for (const TPair<UInstancedStaticMeshComponent*, TSet<int32>>& Pair : WantGone)
	{
		TMap<int32, FTransform>& Mine = Gone.FindOrAdd(Pair.Key);
		for (const int32 Index : Pair.Value)
		{
			if (!Mine.Contains(Index))
			{
				FTransform Was;
				Pair.Key->GetInstanceTransform(Index, Was, true);
				Mine.Add(Index, Was);
				Pair.Key->UpdateInstanceTransform(Index, Pair.Key == Solid ? Stowed(Was) : Shrunk(Was), true, false, true);
				Touched.Add(Pair.Key);
				if (Pair.Key != Solid)
				{
					NewlyGone.FindOrAdd(Pair.Key).Add(Index, Was);
				}
				// A giant die smashed (not one coming down): rubble in its colors from all over it
				const int32* Die = Pair.Key == DieBlocks ? DieOfInstance.Find(Index) : nullptr;
				if (Die && !DiceFalling.Contains(Index))
				{
					const FGiantDie& Giant = GiantDice[*Die];
					const float Half = Giant.Size * BlockSize * 0.5f;
					for (int32 Bit = 0; Bit < 6 + Giant.Size * 3; ++Bit)
					{
						OutBroken.Add({ Was.GetLocation() + FVector(FMath::FRandRange(-Half, Half), FMath::FRandRange(-Half, Half), FMath::FRandRange(-Half, Half)),
							FLinearColor(Giant.Body), FLinearColor(Giant.Pips), 1.3f });
					}
				}
			}
		}
	}

	if (DirtyBoxes.Num() > 0)
	{
		if (!WreckSolid)
		{
			WreckSolid = MakeInstances(Cube, nullptr, true, 0);
			WreckSolid->SetVisibility(false);
			WreckSolid->SetHiddenInGame(true);
		}
		TArray<FTransform> All;
		for (const TPair<int32, TArray<FTransform>>& Pair : CutBoxes)
		{
			All.Append(Pair.Value);
		}
		WreckSolid->ClearInstances();
		WreckSolid->AddInstances(All, false, true, false);
	}
	// The navmesh is made from these boxes: told afresh, or the tiles under a cut ground box (one runs across several
	// tiles) come back with nothing in them (round 89: the ground digs)
	if (GetWorld()->IsGameWorld() && (DirtyBoxes.Num() > 0 || Touched.Contains(Solid.Get())))
	{
		UNavigationSystemV1::UpdateComponentInNavOctree(*Solid);
		if (WreckSolid)
		{
			UNavigationSystemV1::UpdateComponentInNavOctree(*WreckSolid);
		}
	}
	for (UInstancedStaticMeshComponent* Instances : Touched)
	{
		Instances->MarkRenderStateDirty();
	}
	// What falls just now comes down (on every machine; the server's landings hurt)
	if (NewlyFallen.Num() > 0)
	{
		StartFalling(NewlyFallen, FallingBlocks, NewlyGone);
	}

	// The lava: the columns flooded now, what broke through just now (a fountain for each patch of it); the server
	// tells the wreckage the pool is big
	const bool bLavaChanged = LavaNow.Num() != LavaColumns.Num() || LavaNow.Difference(LavaColumns).Num() > 0;
	bool bDepthChanged = Depth.Num() != DugDepth.Num();
	for (const TPair<int32, int32>& Pair : Depth)
	{
		const int32* Was = DugDepth.Find(Pair.Key);
		bDepthChanged |= !Was || *Was != Pair.Value;
	}
	if (bLavaChanged || (bDepthChanged && LavaNow.Num() > 0))
	{
		TArray<FRRLavaColumn> Flooded;
		FBox Pool(ForceInit);
		for (const int32 Column : LavaNow)
		{
			const FIntPoint Cell = ColumnCell(Column);
			const FVector Spot = At(CellCenter(Cell), -FMath::Min(Depth[Column], -GroundBottom) * BlockSize);
			Flooded.Add({ Cell, FVector2D(Spot), float(Spot.Z) });
			Pool += Spot;
		}
		// The breakthroughs new just now: a fountain in the middle of each patch of them
		TArray<FVector> Breaches;
		TSet<int32> Seen;
		for (const int32 Column : LavaNow)
		{
			if (Seen.Contains(Column) || Depth[Column] < BreachDepth || LavaColumns.Contains(Column))
			{
				continue;
			}
			TArray<int32> Patch = { Column };
			Seen.Add(Column);
			FVector2D Sum(0.f);
			for (int32 Head = 0; Head < Patch.Num(); ++Head)
			{
				const FIntPoint Cell = ColumnCell(Patch[Head]);
				Sum += CellCenter(Cell);
				for (const FIntPoint& Step : { FIntPoint(1, 0), FIntPoint(-1, 0), FIntPoint(0, 1), FIntPoint(0, -1) })
				{
					const int32 Next = ColumnOf(Cell.X + Step.X, Cell.Y + Step.Y);
					const int32* NextDepth = Depth.Find(Next);
					if (NextDepth && *NextDepth >= BreachDepth && !Seen.Contains(Next) && !LavaColumns.Contains(Next))
					{
						Seen.Add(Next);
						Patch.Add(Next);
					}
				}
			}
			Breaches.Add(At(Sum / Patch.Num(), -LavaBelowFloor));
		}
		if (Lava)
		{
			Lava->SetColumns(Flooded, FloorZ - LavaBelowFloor, Breaches);
		}
		if (HasAuthority() && LavaNow.Difference(LavaColumns).Num() > 0)
		{
			Pool.Min.Z = FloorZ + GroundBottom * BlockSize - BlockSize;
			Pool.Max.Z = FloorZ + BlockSize;
			NewBigAreas.Add(Pool.ExpandBy(FVector(BlockSize * 0.5f, BlockSize * 0.5f, 0.f)));
		}
	}

	// The minimap: the columns where anything changed or the lava came or went
	TSet<int32> MapColumns;
	for (const int32 Index : Changed)
	{
		const FIntVector Cell = CellOf(Index);
		MapColumns.Add(ColumnOf(Cell.X, Cell.Y));
	}
	MapColumns.Append(LavaNow.Difference(LavaColumns));
	MapColumns.Append(LavaColumns.Difference(LavaNow));
	DugDepth = MoveTemp(Depth);
	LavaColumns = MoveTemp(LavaNow);
	for (const int32 Column : MapColumns)
	{
		const FIntPoint Cell = ColumnCell(Column);
		RefreshMapColumn(Cell.X, Cell.Y);
	}
	if (MapColumns.Num() > 0)
	{
		++MapVersion;
	}

	int32 GoneCount = 0;
	for (const TPair<UInstancedStaticMeshComponent*, TMap<int32, FTransform>>& Pair : Gone)
	{
		GoneCount += Pair.Value.Num();
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town wrecks: %d in force, %d blocks broken (%d just now, %d mended), %d boxes cut into %d, %d models gone, %d giant dice hit, %d dug columns, %d lava, %d falling now (%d fallen), %.1f ms."),
		InWrecks.Num(), BrokenCells.Num(), OutBroken.Num(), Mended.Num(), CutBoxes.Num(), WreckSolid ? WreckSolid->GetInstanceCount() : 0, GoneCount,
		DiceHit.Num(), DugDepth.Num(), LavaColumns.Num(), NewlyFallen.Num(), Fallen.CountSetBits(), (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

// ---------------------------------------------------------------------------------------------------------------------
// Falling, lava and the minimap (round 89)

FIntPoint ARRRandomDiceTown::GetCellAt(const FVector& WorldLocation) const
{
	const FVector Middle = GetActorLocation();
	return FIntPoint(FMath::FloorToInt((WorldLocation.X - Middle.X) / BlockSize), FMath::FloorToInt((WorldLocation.Y - Middle.Y) / BlockSize));
}

bool ARRRandomDiceTown::GetLavaAt(const FVector& WorldLocation, float& OutTop) const
{
	const FIntPoint Cell = GetCellAt(WorldLocation);
	if (Cell.X < -HalfCells || Cell.X >= HalfCells || Cell.Y < -HalfCells || Cell.Y >= HalfCells
		|| !LavaColumns.Contains((Cell.Y + HalfCells) * GridCells + (Cell.X + HalfCells)))
	{
		return false;
	}
	OutTop = FloorZ - LavaBelowFloor;
	return !Lava || Lava->IsRisenAt(Cell);
}

bool ARRRandomDiceTown::ConsumeBigAreas(TArray<FBox>& OutAreas)
{
	OutAreas = MoveTemp(NewBigAreas);
	NewBigAreas.Reset();
	return OutAreas.Num() > 0;
}

void ARRRandomDiceTown::RefreshMapColumn(int32 X, int32 Y)
{
	const int32 Index = (Y + HalfCells) * GridCells + (X + HalfCells);
	if (!MapColors.IsValidIndex(Index) || !IsOnIsland(X, Y))
	{
		return;
	}
	// Round 91: a puddle goes with the paving under it (dug out, or lava) and comes back with it
	if (const TArray<TPair<int32, FTransform>>* Discs = Puddles ? PuddleDiscs.Find(FIntPoint(X, Y)) : nullptr)
	{
		const bool bShow = IsFilled(X, Y, 0) && !DugDepth.Contains(Index) && !LavaColumns.Contains(Index);
		for (const TPair<int32, FTransform>& Disc : *Discs)
		{
			Puddles->UpdateInstanceTransform(Disc.Key, bShow ? Disc.Value : Shrunk(Disc.Value), true, true, false);
		}
	}
	if (LavaColumns.Contains(Index))
	{
		MapColors[Index] = MapLava;
		MapLevels[Index] = 0;
		return;
	}
	MapColors[Index] = MapSky;
	MapLevels[Index] = -1;
	for (int32 Level = LevelMax; Level >= LevelMin; --Level)
	{
		if (!IsFilled(X, Y, Level))
		{
			continue;
		}
		const uint8 Value = Grid[GridIndex(X, Y, Level)];
		// (A dug crater shows its earth; a canal its water, as built)
		MapColors[Index] = Value == HiddenSolid ? White : DugDepth.Contains(Index) ? Looks[Value - 1].Body : Level < 0 ? MapWater : Looks[Value - 1].Accent;
		MapLevels[Index] = int8(FMath::Clamp(Level, 0, 20));
		break;
	}
	if (StairCells.Contains(FIntPoint(X, Y)) && !DugDepth.Contains(Index))
	{
		MapColors[Index] = Cream;
		MapLevels[Index] = int8(FMath::Max(1, int32(MapLevels[Index])));
	}
}

void ARRRandomDiceTown::FindHeldUp(const TBitArray<>& BrokenNow, TBitArray<>& OutHeld) const
{
	// A flood out from the ground (and what stood on nothing as built) through solid cells side by side and one on another
	OutHeld.Init(false, Grid.Num());
	TArray<int32> Queue;
	Queue.Reserve(400000);
	auto IsSolid = [&](int32 Index) { return Grid[Index] != 0 && !BrokenNow[Index]; };
	const int32 GroundEnd = (1 - LevelMin) * GridCells * GridCells;
	for (int32 Index = 0; Index < GroundEnd; ++Index)
	{
		if (IsSolid(Index))
		{
			OutHeld[Index] = true;
			Queue.Add(Index);
		}
	}
	if (bFoundAnchored)
	{
		for (TConstSetBitIterator<> It(Anchored); It; ++It)
		{
			if (IsSolid(It.GetIndex()) && !OutHeld[It.GetIndex()])
			{
				OutHeld[It.GetIndex()] = true;
				Queue.Add(It.GetIndex());
			}
		}
	}
	const int32 Plane = GridCells * GridCells;
	for (int32 Head = 0; Head < Queue.Num(); ++Head)
	{
		const int32 Index = Queue[Head];
		const int32 X = Index % GridCells;
		const int32 Y = Index / GridCells % GridCells;
		const int32 Layer = Index / Plane;
		auto Visit = [&](int32 Next)
		{
			if (!OutHeld[Next] && IsSolid(Next))
			{
				OutHeld[Next] = true;
				Queue.Add(Next);
			}
		};
		if (X > 0) { Visit(Index - 1); }
		if (X < GridCells - 1) { Visit(Index + 1); }
		if (Y > 0) { Visit(Index - GridCells); }
		if (Y < GridCells - 1) { Visit(Index + GridCells); }
		if (Layer > 0) { Visit(Index - Plane); }
		if (Layer < GridLevels - 1) { Visit(Index + Plane); }
	}
}

void ARRRandomDiceTown::StartFalling(const TArray<int32>& Cells, const TArray<FFallingPiece>& BlockPieces, const TMap<UInstancedStaticMeshComponent*, TMap<int32, FTransform>>& NewlyGone)
{
	auto CellOf = [this](int32 Index)
	{
		return FIntVector(Index % GridCells - HalfCells, Index / GridCells % GridCells - HalfCells, Index / (GridCells * GridCells) + LevelMin);
	};
	// The parts that hang together, each a piece of its own
	TSet<int32> Left(Cells);
	TMap<int32, int32> PartOf;
	TArray<TArray<int32>> Parts;
	for (const int32 Start : Cells)
	{
		if (!Left.Contains(Start))
		{
			continue;
		}
		TArray<int32>& Part = Parts.AddDefaulted_GetRef();
		Part.Add(Start);
		Left.Remove(Start);
		for (int32 Head = 0; Head < Part.Num(); ++Head)
		{
			const FIntVector Cell = CellOf(Part[Head]);
			for (const FIntVector& Step : { FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0), FIntVector(0, 0, 1), FIntVector(0, 0, -1) })
			{
				const FIntVector Next = Cell + Step;
				if (IsInGrid(Next.X, Next.Y, Next.Z) && Left.Remove(GridIndex(Next.X, Next.Y, Next.Z)) > 0)
				{
					Part.Add(GridIndex(Next.X, Next.Y, Next.Z));
				}
			}
		}
		for (const int32 Index : Part)
		{
			PartOf.Add(Index, Parts.Num() - 1);
		}
	}
	const int32 First = Falling.Num();
	for (const TArray<int32>& Part : Parts)
	{
		FFalling& Piece = Falling.AddDefaulted_GetRef();
		FIntVector Low(TNumericLimits<int32>::Max());
		FIntVector High(TNumericLimits<int32>::Min());
		TSet<FIntPoint> Footprint;
		for (const int32 Index : Part)
		{
			const FIntVector Cell = CellOf(Index);
			Low = Low.ComponentMin(Cell);
			High = High.ComponentMax(Cell);
			Footprint.Add(FIntPoint(Cell.X, Cell.Y));
		}
		Piece.Footprint = Footprint.Array();
		// It lands on the highest thing still standing under it
		int32 Land = GroundBottom - 1;
		for (const FIntPoint& Spot : Piece.Footprint)
		{
			for (int32 Level = Low.Z - 1; Level > Land; --Level)
			{
				if (IsFilled(Spot.X, Spot.Y, Level))
				{
					Land = Level;
					break;
				}
			}
		}
		const FVector2D Mid2D = (CellCenter(FIntPoint(Low.X, Low.Y)) + CellCenter(FIntPoint(High.X, High.Y))) * 0.5f;
		Piece.Pivot = At(Mid2D, (Low.Z - 1) * BlockSize);
		Piece.LandZ = FloorZ + Land * BlockSize;
		Piece.Drop = FMath::Max(30.f, Piece.Pivot.Z - Piece.LandZ);
		Piece.Duration = FMath::Clamp(FMath::Sqrt(2.f * Piece.Drop / FallGravity), 0.3f, 1.f);
		// It tips one way or another as it comes down (the same way on every machine)
		const float Angle = float(CellHash(Low.X, Low.Y, Low.Z) % 360);
		Piece.TipAxis = FVector(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)), 0.f);
		if (HasAuthority())
		{
			const FVector Corner = At(CellCenter(FIntPoint(Low.X, Low.Y)) - FVector2D(BlockSize * 0.5f), Land * BlockSize);
			NewBigAreas.Add(FBox(Corner, At(CellCenter(FIntPoint(High.X, High.Y)) + FVector2D(BlockSize * 0.5f), High.Z * BlockSize)));
		}
	}
	// Its looks: its blocks, the giant dice and the models standing in it (roofs, trees, lamps)
	auto Add = [&](int32 Part, const FFallingPiece& Look)
	{
		if (Falling[First + Part].Pieces.Num() < FallMaxPieces)
		{
			Falling[First + Part].Pieces.Add(Look);
		}
	};
	for (const FFallingPiece& Look : BlockPieces)
	{
		if (const int32* Part = PartOf.Find(Look.Cell))
		{
			Add(*Part, Look);
		}
	}
	for (const TPair<UInstancedStaticMeshComponent*, TMap<int32, FTransform>>& Pair : NewlyGone)
	{
		for (const TPair<int32, FTransform>& Model : Pair.Value)
		{
			// The cell its bottom stands in (a die's lowest, a roof's eaves)
			const FBox Box = Pair.Key->GetStaticMesh() ? Pair.Key->GetStaticMesh()->GetBoundingBox().TransformBy(Model.Value) : FBox(Model.Value.GetLocation(), Model.Value.GetLocation());
			const FIntPoint Spot = GetCellAt(Box.GetCenter());
			const int32 Level = FMath::FloorToInt((Box.Min.Z + 20.f - FloorZ) / BlockSize) + 1;
			for (int32 Try = Level; Try >= Level - 1; --Try)
			{
				if (!IsInGrid(Spot.X, Spot.Y, Try))
				{
					continue;
				}
				if (const int32* Part = PartOf.Find(GridIndex(Spot.X, Spot.Y, Try)))
				{
					Add(*Part, { Pair.Key, Model.Key, Model.Value, INDEX_NONE });
					break;
				}
			}
		}
	}
	for (int32 Index = First; Index < Falling.Num(); ++Index)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town: %d blocks lose their hold and come down %.0f (%d looks)."), Parts[Index - First].Num(), Falling[Index].Drop, Falling[Index].Pieces.Num());
	}
}

void ARRRandomDiceTown::TickFalling(float DeltaSeconds)
{
	TSet<UInstancedStaticMeshComponent*> Dirty;
	for (int32 Index = Falling.Num() - 1; Index >= 0; --Index)
	{
		FFalling& Piece = Falling[Index];
		Piece.Age += DeltaSeconds;
		if (Piece.Age >= Piece.Duration)
		{
			LandFalling(Piece);
			for (const FFallingPiece& Look : Piece.Pieces)
			{
				if (UInstancedStaticMeshComponent* Instances = Look.Instances.Get())
				{
					Dirty.Add(Instances);
				}
			}
			Falling.RemoveAtSwap(Index);
			continue;
		}
		// Down like a dropped thing, tipping a little more as it goes
		const float Share = Piece.Age / Piece.Duration;
		const FQuat Tip(Piece.TipAxis, FMath::DegreesToRadians(FallTipDegrees) * Share);
		const FVector Down(0.f, 0.f, -Piece.Drop * Share * Share);
		for (const FFallingPiece& Look : Piece.Pieces)
		{
			UInstancedStaticMeshComponent* Instances = Look.Instances.Get();
			if (!Instances)
			{
				continue;
			}
			const FTransform Now(Tip * Look.Was.GetRotation(), Piece.Pivot + Tip.RotateVector(Look.Was.GetLocation() - Piece.Pivot) + Down, Look.Was.GetScale3D());
			Instances->UpdateInstanceTransform(Look.Index, Now, true, false, false);
			Dirty.Add(Instances);
		}
	}
	for (UInstancedStaticMeshComponent* Instances : Dirty)
	{
		Instances->MarkRenderStateDirty();
	}
}

void ARRRandomDiceTown::LandFalling(FFalling& Piece)
{
	// Gone for good (unless it mended while it fell), the rubble from where its looks hit the floor
	const FQuat Tip(Piece.TipAxis, FMath::DegreesToRadians(FallTipDegrees));
	TArray<FRRBrokenBit> Bits;
	const int32 Every = FMath::Max(1, Piece.Pieces.Num() / FallRubble);
	for (int32 Look = 0; Look < Piece.Pieces.Num(); ++Look)
	{
		const FFallingPiece& Fell = Piece.Pieces[Look];
		UInstancedStaticMeshComponent* Instances = Fell.Instances.Get();
		if (!Instances)
		{
			continue;
		}
		const TMap<int32, FTransform>* Mine = Gone.Find(Instances);
		const bool bStillGone = Fell.Cell != INDEX_NONE ? Broken.IsValidIndex(Fell.Cell) && Broken[Fell.Cell] : Mine && Mine->Contains(Fell.Index);
		Instances->UpdateInstanceTransform(Fell.Index, bStillGone ? Shrunk(Fell.Was) : Fell.Was, true, false, false);
		if (!bStillGone || Look % Every != 0)
		{
			continue;
		}
		FLinearColor Body(1.f, 0.6f, 0.75f);
		FLinearColor Accent = FLinearColor::White;
		const int32 Floats = Instances->NumCustomDataFloats;
		if (Floats >= 6 && Instances->PerInstanceSMCustomData.IsValidIndex(Fell.Index * Floats + 5))
		{
			const float* Data = &Instances->PerInstanceSMCustomData[Fell.Index * Floats];
			Body = FLinearColor(Data[0], Data[1], Data[2]);
			Accent = FLinearColor(Data[3], Data[4], Data[5]);
		}
		const FVector Landed = Piece.Pivot + Tip.RotateVector(Fell.Was.GetLocation() - Piece.Pivot) - FVector(0.f, 0.f, Piece.Drop);
		Bits.Add({ Landed, Body, Accent, float(FMath::Clamp(Fell.Was.GetScale3D().GetMax(), 0.8, 2.0)) });
	}
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (ARRRandomWreckage* Wreckage = ARRRandomWreckage::Find(this))
		{
			Wreckage->ThrowRubble(Bits);
		}
		// Dust bursting out along the floor round it, and the word
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			const int32 Puffs = FMath::Min(6, Piece.Footprint.Num());
			for (int32 Puff = 0; Puff < Puffs; ++Puff)
			{
				const FIntPoint& Spot = Piece.Footprint[(Puff * Piece.Footprint.Num()) / Puffs];
				Effects->Footstep(At(CellCenter(Spot), Piece.LandZ - FloorZ), 1.6f);
			}
			Effects->DiceLandBurst(FVector(Piece.Pivot.X, Piece.Pivot.Y, Piece.LandZ + 60.f), 60.f);
		}
		ARRRandomHUD::PopComicWord(this, TEXT("CRASH!"), FVector(Piece.Pivot.X, Piece.Pivot.Y, Piece.LandZ + 380.f), RRUI::Gold, 1.6f);
	}
	// Server: whoever is under it is hit and shoved out from under it
	if (!HasAuthority())
	{
		return;
	}
	const TSet<FIntPoint> Under(Piece.Footprint);
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Who = *It;
		const FVector Where = Who->GetActorLocation();
		const float Feet = Where.Z - Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		if (!Who->IsAlive() || !Under.Contains(GetCellAt(Where)) || Feet < Piece.LandZ - 60.f || Feet > Piece.LandZ + 450.f)
		{
			continue;
		}
		UGameplayStatics::ApplyDamage(Who, CrashDamage, nullptr, this, UDamageType::StaticClass());
		const FVector Out = ((Where - Piece.Pivot) * FVector(1.f, 1.f, 0.f)).GetSafeNormal();
		Who->LaunchCharacter((Out.IsNearlyZero() ? FVector::ForwardVector : Out) * CrashShove + FVector(0.f, 0.f, 350.f), true, true);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s is under a falling building."), *Who->GetName());
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Battle map tiles (round 92): the battle royale's map is a square of these (ARRRandomBattleMap)

namespace
{
	constexpr int32 BattleLayoutSeed = 9200;
	// Lamps along the avenues: every this many cells, on this tile's +X and +Y edges only (each avenue gets one row)
	constexpr int32 BattleLampSpacing = 18;
	// The far look's blocks: the tile's rooftops in columns this many cells a side, merged where alike
	constexpr int32 FarCells = 4;
}

bool ARRRandomDiceTown::IsBattleRoad(int32 X, int32 Y) const
{
	const float CX = X + 0.5f;
	const float CY = Y + 0.5f;
	// The avenues along the tile's edges (the next tile paves the rest of each)
	const float Edge = HalfCells - RRBattle::AvenueHalf;
	if (FMath::Abs(CX) > Edge || FMath::Abs(CY) > Edge)
	{
		return true;
	}
	if (FMath::Abs(CX) >= RRBattle::StreetHalf && FMath::Abs(CY) >= RRBattle::StreetHalf)
	{
		return false;
	}
	// The streets across the middle, except inside a castle's wall or on the plaza (they have their own paving)
	const ERRDistrict District = ERRDistrict(BattleTile.District);
	if ((District == ERRDistrict::Castle && Cheb(X, Y) < RRBattle::CastleOut) || (District == ERRDistrict::Plaza && Cheb(X, Y) < RRBattle::PlazaEdge))
	{
		return false;
	}
	return true;
}

bool ARRRandomDiceTown::IsNeighborGround(int32 X, int32 Y, int32 Level) const
{
	return IsBattleTile() && Level <= 0 && Level >= GroundBottom && (X < -HalfCells || X >= HalfCells || Y < -HalfCells || Y >= HalfCells) && IsOnIsland(X, Y);
}

TArray<UPrimitiveComponent*> ARRRandomDiceTown::GetDetailComponents() const
{
	TArray<UPrimitiveComponent*> Out;
	const UInstancedStaticMeshComponent* Sets[] = { Blocks.Get(), DieBlocks.Get(), GlossBlocks.Get(), Puddles.Get(), Steps.Get(), Water.Get(), Decor.Get(),
		Neon.Get(), Roofs.Get(), Marshmallows.Get(), Poles.Get(), SlopeBlocks.Get(), SlantBlocks.Get(), SlopeCorners.Get() };
	for (const UInstancedStaticMeshComponent* Set : Sets)
	{
		if (Set)
		{
			Out.Add(const_cast<UInstancedStaticMeshComponent*>(Set));
		}
	}
	for (UInstancedStaticMeshComponent* Trees : PropTrees)
	{
		if (Trees)
		{
			Out.Add(Trees);
		}
	}
	for (UInstancedStaticMeshComponent* Models : CoverModels)
	{
		if (Models)
		{
			Out.Add(Models);
		}
	}
	// (the middle tile's stars include the sky's: always shown)
	if (Stars && !BuildsSky())
	{
		Out.Add(Stars);
	}
	return Out;
}

void ARRRandomDiceTown::SetDetailShown(bool bShown)
{
	if (bDetailShown == bShown || !IsBattleTile())
	{
		return;
	}
	bDetailShown = bShown;
	// Out of the scene (a hidden primitive isn't added to it: nothing drawn, no shadows, nothing for Lumen), and back
	for (UPrimitiveComponent* Component : GetDetailComponents())
	{
		Component->SetVisibility(bShown);
	}
	for (UPointLightComponent* Light : LampLights)
	{
		if (Light)
		{
			Light->SetVisibility(bShown);
		}
	}
	if (FarLook)
	{
		FarLook->SetVisibility(!bShown);
	}
}

int32 ARRRandomDiceTown::GetDetailInstanceCount() const
{
	int32 Count = 0;
	for (UPrimitiveComponent* Component : GetDetailComponents())
	{
		if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Component))
		{
			Count += Instances->GetInstanceCount();
		}
	}
	return Count;
}

int32 ARRRandomDiceTown::GetFarInstanceCount() const
{
	return FarLook ? FarLook->GetInstanceCount() : 0;
}

void ARRRandomDiceTown::MakeFarLook()
{
	// Each 4 x 4 cells as one column up to the highest thing standing in them, in its top's color (the minimap's), the
	// sides a shade darker; then columns side by side alike (as high, much the same color) merged into one stretched block.
	// No shadows, no collision: seen from afar only, while the tile's detail is out of the scene.
	FarLook = MakeInstances(BlockMesh, nullptr, false, 6);
	FarLook->CastShadow = false;
	FarLook->SetVisibility(!bDetailShown);
	const int32 Groups = GridCells / FarCells;
	TArray<int32> GroupLevel;
	TArray<FColor> GroupColor;
	GroupLevel.Init(-100, Groups * Groups);
	GroupColor.Init(FColor::Black, Groups * Groups);
	for (int32 GY = 0; GY < Groups; ++GY)
	{
		for (int32 GX = 0; GX < Groups; ++GX)
		{
			int32 Best = -100;
			FColor Color = FColor::Black;
			for (int32 DY = 0; DY < FarCells; ++DY)
			{
				for (int32 DX = 0; DX < FarCells; ++DX)
				{
					const int32 Index = (GY * FarCells + DY) * GridCells + GX * FarCells + DX;
					// (off the island: -1, nothing there)
					if (MapLevels.IsValidIndex(Index) && MapLevels[Index] >= 0 && MapLevels[Index] > Best)
					{
						Best = MapLevels[Index];
						Color = MapColors[Index];
					}
				}
			}
			GroupLevel[GY * Groups + GX] = Best;
			GroupColor[GY * Groups + GX] = Color;
		}
	}
	// Alike: as high, and the same color to 32 steps a channel (so grass of either shade merges)
	auto Key = [&](int32 Index)
	{
		const FColor& C = GroupColor[Index];
		return (GroupLevel[Index] + 8) * 32768 + (C.R / 32) * 1024 + (C.G / 32) * 32 + C.B / 32;
	};
	auto IsGround = [&](int32 Index) { return GroupLevel[Index] >= 0; };
	TBitArray<> Done(false, Groups * Groups);
	for (int32 GY = 0; GY < Groups; ++GY)
	{
		for (int32 GX = 0; GX < Groups; ++GX)
		{
			const int32 Start = GY * Groups + GX;
			if (Done[Start] || !IsGround(Start))
			{
				continue;
			}
			const int32 Wanted = Key(Start);
			int32 GX1 = GX;
			while (GX1 + 1 < Groups && !Done[GY * Groups + GX1 + 1] && IsGround(GY * Groups + GX1 + 1) && Key(GY * Groups + GX1 + 1) == Wanted)
			{
				++GX1;
			}
			int32 GY1 = GY;
			for (bool bGrow = true; bGrow && GY1 + 1 < Groups; )
			{
				for (int32 I = GX; I <= GX1 && bGrow; ++I)
				{
					const int32 Next = (GY1 + 1) * Groups + I;
					bGrow = !Done[Next] && IsGround(Next) && Key(Next) == Wanted;
				}
				GY1 += bGrow ? 1 : 0;
			}
			for (int32 J = GY; J <= GY1; ++J)
			{
				for (int32 I = GX; I <= GX1; ++I)
				{
					Done[J * Groups + I] = true;
				}
			}
			// From the ground's bottom (level -2) up to the top of the highest level
			const float Bottom = (GroundBottom - 1) * BlockSize;
			const float Top = GroupLevel[Start] * BlockSize;
			const FVector2D Low = CellCenter(FIntPoint(GX * FarCells - HalfCells, GY * FarCells - HalfCells)) - FVector2D(BlockSize * 0.5f);
			const FVector2D High = CellCenter(FIntPoint(GX1 * FarCells + FarCells - 1 - HalfCells, GY1 * FarCells + FarCells - 1 - HalfCells)) + FVector2D(BlockSize * 0.5f);
			const FColor& Color = GroupColor[Start];
			AddColored(FarLook, FTransform(FRotator::ZeroRotator, At((Low + High) * 0.5f, (Bottom + Top) * 0.5f),
				FVector((High.X - Low.X) / BlockSize, (High.Y - Low.Y) / BlockSize, (Top - Bottom) / BlockSize)), Shade(Color, 0.82f), Color);
		}
	}
}

void ARRRandomDiceTown::LayOutBattle()
{
	const ERRDistrict District = ERRDistrict(FMath::Min<uint8>(BattleTile.District, uint8(ERRDistrict::Count) - 1));
	FRandomStream Layout(BattleLayoutSeed + BattleTile.Seed);
	const int32 Half = HalfCells;
	const float Edge = HalfCells - RRBattle::AvenueHalf;

	// Ground: grass (now and then lime), paving on the roads, the plaza's pink and cream checker, a castle's courtyard
	const uint8 GrassLookA = MakeLook(Dirt, GrassA);
	const uint8 GrassLookB = MakeLook(Dirt, GrassB);
	const uint8 GrassLookLime = MakeLook(Dirt, GrassLime);
	const uint8 DirtLook = Plain(Dirt);
	const uint8 DeepLook = Plain(DirtDeep);
	const uint8 PaveLookA = MakeLook(Stone, PaveA);
	const uint8 PaveLookB = MakeLook(Stone, PaveB);
	// (the avenues' middle line, along the tile's edge: a lemon dash every other pair of cells)
	const uint8 LaneLook = MakeLook(Stone, Blend(Lemon, White, 0.35f));
	const uint8 PlazaLookA = MakeLook(Stone, PlazaPink, false, false, true);
	const uint8 PlazaLookB = MakeLook(Stone, PlazaCream, false, false, true);
	const uint8 CourtLookA = MakeLook(Stone, Blend(PaveA, Lavender, 0.25f));
	const uint8 CourtLookB = MakeLook(Stone, Blend(PaveB, Cream, 0.4f));
	int32 OnIsland = 0;
	for (int32 X = -Half; X < Half; ++X)
	{
		for (int32 Y = -Half; Y < Half; ++Y)
		{
			if (!IsOnIsland(X, Y))
			{
				continue;
			}
			++OnIsland;
			const FIntPoint Global = GlobalCell(X, Y);
			const bool bChecker = ((X + Y) & 1) == 0;
			const uint32 Roll = CellHash(Global.X, Global.Y, GroundSeed) % 100;
			uint8 Top = Roll < 8 ? GrassLookLime : Roll < 55 ? GrassLookA : GrassLookB;
			if (District == ERRDistrict::Plaza && Cheb(X, Y) < RRBattle::PlazaEdge)
			{
				Top = bChecker ? PlazaLookA : PlazaLookB;
			}
			else if (District == ERRDistrict::Castle && Cheb(X, Y) < RRBattle::CastleOut)
			{
				Top = bChecker ? CourtLookA : CourtLookB;
			}
			else if (IsRoad(X, Y))
			{
				Top = bChecker ? PaveLookA : PaveLookB;
				// The cells either side of the tile's edge are an avenue's middle: a dash along it, every other pair
				const bool bEdgeX = X == Half - 1 || X == -Half;
				const bool bEdgeY = Y == Half - 1 || Y == -Half;
				if ((bEdgeX && !bEdgeY && ((Global.Y / 2) & 1) == 0) || (bEdgeY && !bEdgeX && ((Global.X / 2) & 1) == 0))
				{
					Top = LaneLook;
				}
			}
			Set(X, Y, -2, DeepLook);
			Set(X, Y, -1, DirtLook);
			Set(X, Y, 0, Top);
		}
	}
	// How much of the tile is ground (a tile at the rim is partly sky): the counts below shrink with it
	const float Share = float(OnIsland) / float(GridCells * GridCells);
	auto Want = [Share](int32 Count) { return FMath::Max(Count > 0 ? 1 : 0, FMath::RoundToInt(Count * Share)); };
	if (OnIsland == 0)
	{
		return;
	}

	auto AllIn = [](const FIntRect& Rect, TFunctionRef<bool(int32, int32)> Zone)
	{
		for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X)
		{
			for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
			{
				if (!Zone(X, Y))
				{
					return false;
				}
			}
		}
		return true;
	};
	auto Anywhere = [](int32, int32) { return true; };
	auto RandomFrame = [&]()
	{
		FFrame Frame;
		Frame.Turn = Layout.RandRange(0, 3);
		Frame.Origin = FIntPoint(Layout.RandRange(-Half + 2, Half - 3), Layout.RandRange(-Half + 2, Half - 3));
		Frame.bRed = Layout.FRand() < 0.5f;
		return Frame;
	};
	auto Sub = [](const FFrame& Parent, int32 U, int32 V, int32 Turn)
	{
		FFrame Frame;
		Frame.Origin = Parent.Cell(U, V);
		Frame.Turn = (Parent.Turn + Turn) & 3;
		Frame.bRed = Parent.bRed;
		return Frame;
	};
	auto DieAt = [this](const FFrame& Frame, int32 U, int32 V, int32 Level, int32 Size, int32 Kind)
	{
		const FIntPoint A = Frame.Cell(U, V);
		const FIntPoint B = Frame.Cell(U + Size - 1, V + Size - 1);
		AddDie(FIntPoint(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), Level, Size, Kind);
	};
	auto PlaceBuildings = [&](int32 Count, int32 Tries, TFunctionRef<bool(int32, int32)> Zone, int32 MinW, int32 MaxW, int32 MinD, int32 MaxD, float TwoStoryChance, float FlatChance)
	{
		int32 Placed = 0;
		for (int32 Try = 0; Try < Tries && Placed < Count; ++Try)
		{
			const int32 W = Layout.RandRange(MinW, MaxW);
			const int32 D = Layout.RandRange(MinD, MaxD);
			const int32 Stories = Layout.FRand() < TwoStoryChance ? 2 : 1;
			const bool bFlat = Stories > 1 && Layout.FRand() < FlatChance;
			const int32 Style = Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1);
			const int32 Seed = Layout.RandRange(0, 1 << 30);
			const FFrame Frame = RandomFrame();
			const FIntRect Rect = Footprint(Frame, -1, -1, W + 1, D + 1, 1);
			if (!AllIn(Rect, Zone) || !IsFree(Rect))
			{
				continue;
			}
			Reserve(Rect);
			AddBuilding(Frame, W, D, Stories, bFlat, Style, Seed);
			++Placed;
		}
		return Placed;
	};
	auto PlaceTowers = [&](int32 Count, TFunctionRef<bool(int32, int32)> Zone)
	{
		int32 Placed = 0;
		for (int32 Try = 0; Try < 4000 && Placed < Count; ++Try)
		{
			const FFrame Frame = RandomFrame();
			const FIntRect Rect = Footprint(Frame, 0, -Story - 1, 3, 3, 1);
			if (!AllIn(Rect, Zone) || !IsFree(Rect))
			{
				continue;
			}
			Reserve(Rect);
			AddTower(Frame, Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1));
			++Placed;
		}
		return Placed;
	};
	auto PlaceTrees = [&](int32 Count, int32 Tries, TFunctionRef<bool(int32, int32)> Zone)
	{
		int32 Placed = 0;
		for (int32 Try = 0; Try < Tries && Placed < Count; ++Try)
		{
			const FIntPoint Cell(Layout.RandRange(-Half + 3, Half - 4), Layout.RandRange(-Half + 3, Half - 4));
			int32 Kind = Layout.RandRange(0, 1);
			const int32 Seed = Layout.RandRange(0, 1 << 30);
			if (bDream)
			{
				const int32 Roll = Layout.RandRange(0, 99);
				Kind = Roll < 34 ? Kind : Roll < 66 ? 2 : Roll < 84 ? 3 : 4;
			}
			const int32 Room = Kind == 0 ? 1 : Kind == 3 ? 3 : 2;
			const FIntRect Rect(Cell.X - Room - 1, Cell.Y - Room - 1, Cell.X + Room + 2, Cell.Y + Room + 2);
			if (!AllIn(Rect, Zone) || !IsFree(Rect))
			{
				continue;
			}
			Reserve(Rect);
			AddTree(Cell, Kind, Seed);
			++Placed;
		}
		return Placed;
	};
	auto PlaceCover = [&](int32 Count, int32 Tries, TFunctionRef<bool(int32, int32)> Zone)
	{
		int32 Placed = 0;
		for (int32 Try = 0; Try < Tries && Placed < Count; ++Try)
		{
			const FFrame Where = RandomFrame();
			const int32 Piece = Layout.RandRange(0, 4);
			const int32 Kind = Layout.RandRange(0, 47);
			const int32 Tall = Layout.RandRange(1, 2);
			const FColor Color = Styles[Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1)].Low;
			const FIntRect Rect = Footprint(Where, 0, 0, Piece == 0 ? 3 : 1, Piece == 0 ? 0 : 1, 1);
			if (!AllIn(Rect, Zone) || !IsFree(Rect))
			{
				continue;
			}
			Reserve(Rect);
			switch (Piece)
			{
			case 0:
				Fill(Where, 0, 0, 3, 0, 1, 2, MakeLook(Shade(Color, 0.85f), Color));
				break;
			case 1:
				if (Marshmallows)
				{
					const FColor Green = Leaves[Kind % 3];
					Fill(Where, 0, 0, 1, 1, 1, Tall, HiddenLook(Green));
					AddColored(Marshmallows, FTransform(FRotator(0.f, FrameYaw(Where), 0.f), FrameSpot(Where, 0.5f, 0.5f, Tall * 50.f), FVector(2.15f, 2.15f, Tall * 1.05f)), Shade(Green, 0.9f), Blend(Green, White, 0.15f));
					AddColored(Marshmallows, FTransform(FRotator(0.f, FrameYaw(Where) + 20.f, 0.f), FrameSpot(Where, 0.15f, 0.25f, Tall * BlockSize + 10.f), FVector(1.2f, 1.1f, 0.75f)), Green, Blend(Green, White, 0.25f));
					break;
				}
				Fill(Where, 0, 0, 1, 1, 1, Tall, MakeLook(Shade(Leaves[0], 0.8f), Leaves[Kind % 3]));
				break;
			case 2:
				Fill(Where, 0, 0, 1, 1, 1, 1, MakeLook(WoodDark, Wood));
				Fill(Where, 0, 0, 0, 0, 2, Tall + 1, MakeLook(WoodDark, Wood));
				break;
			case 3:
				DieAt(Where, 0, 0, 1, 2, Kind);
				break;
			default:
				DieAt(Where, 0, 0, 1, 1, Kind);
				DieAt(Where, 1, 1, 1, 1, Kind + 1);
				DieAt(Where, 1, 0, 1, 1, Kind + 2);
				DieAt(Where, 1, 0, 2, 1, Kind + 3);
				break;
			}
			++Placed;
		}
		return Placed;
	};
	auto PlaceDieHouse = [&](int32 Tries, TFunctionRef<bool(int32, int32)> Zone)
	{
		for (int32 Try = 0; Try < Tries; ++Try)
		{
			const FFrame Frame = RandomFrame();
			const int32 Seed = Layout.RandRange(0, 1 << 30);
			const FIntRect Rect = Footprint(Frame, -1, -1, 14, 14, 1);
			if (AllIn(Rect, Zone) && IsFree(Rect))
			{
				Reserve(Rect);
				AddDieHouse(Frame, Seed);
				return 1;
			}
		}
		return 0;
	};
	// A rainbow of dice over the street across the middle (its U across the street), if there is room for its feet
	auto PlaceArch = [&](int32 V)
	{
		if (!bDream)
		{
			return 0;
		}
		FFrame Arch;
		Arch.Origin = FIntPoint(0, V);
		const FIntRect Rect = Footprint(Arch, -12, 0, 11, 1, 1);
		bool bRoom = Rect.Min.X > -Half + 1 && Rect.Max.X < Half - 1 && Rect.Min.Y > -Half + 1 && Rect.Max.Y < Half - 1;
		for (const FIntRect& Other : Reserved)
		{
			bRoom = bRoom && !Rect.Intersect(Other);
		}
		for (int32 X = Rect.Min.X; X < Rect.Max.X && bRoom; ++X)
		{
			bRoom = IsOnIsland(X, Rect.Min.Y) && IsOnIsland(X, Rect.Max.Y);
		}
		if (!bRoom)
		{
			return 0;
		}
		Reserve(Rect);
		AddDiceArch(Arch);
		return 1;
	};

	int32 Buildings = 0;
	int32 Towers = 0;
	int32 Trees = 0;
	int32 Cover = 0;
	int32 Extras = 0;
	switch (District)
	{
	case ERRDistrict::Plaza:
	{
		// The middle: a giant die on two tiers, ramps up all four sides, small dice round the edge (as in the town)
		const FFrame Middle = FFrame();
		Fill(Middle, -8, -8, 7, 7, 1, 1, MakeLook(Shade(Lavender, 0.85f), Lavender));
		Fill(Middle, -5, -5, 4, 4, 2, 2, MakeLook(Shade(Pink, 0.85f), Pink));
		AddDie(FIntPoint(-3, -3), 3, 6, 0);
		for (int32 Turn = 0; Turn < 4; ++Turn)
		{
			const FFrame Side = FFrame::Around(Turn);
			AddStairs(Side, -1, -9, 0, 1, 1, 0, 2, 0, 1, Cream);
			AddStairs(Side, -1, -6, 0, 1, 1, 0, 2, 1, 1, Cream);
			DieAt(Side, -11, -11, 1, 2, 1 + Turn);
			DieAt(Side, -11, -6, 1, 1, 7 + Turn * 6);
			DieAt(Side, -6, -11, 1, 1, 9 + Turn * 6);
		}
		Reserve(FIntRect(-int32(RRBattle::PlazaEdge) - 1, -int32(RRBattle::PlazaEdge) - 1, int32(RRBattle::PlazaEdge) + 1, int32(RRBattle::PlazaEdge) + 1));
		Extras += PlaceArch(30) + PlaceArch(-32);
		Extras += bDream ? PlaceDieHouse(6000, Anywhere) : 0;
		Buildings = PlaceBuildings(Want(16), 12000, Anywhere, 9, 12, 9, 12, 0.85f, 0.45f);
		Towers = PlaceTowers(Want(2), Anywhere);
		Cover = PlaceCover(Want(20), 4000, Anywhere);
		Trees = PlaceTrees(Want(10), 4000, Anywhere);
		break;
	}
	case ERRDistrict::Downtown:
	{
		// Streets of houses and shops close together: mostly two stories, flat roofs to fight from, crates and walls between
		// Round 94: first a big building to fight inside (the test map: a mall and a school)
		if (BattleTile.Reach <= 1)
		{
			PlaceBigBuilding(EBigBuilding::Mall, Layout);
			PlaceBigBuilding(EBigBuilding::School, Layout);
		}
		else
		{
			const float Roll = Layout.FRand();
			PlaceBigBuilding(Roll < 0.3f ? EBigBuilding::Mall : Roll < 0.7f ? EBigBuilding::School : EBigBuilding::Garage, Layout);
		}
		Extras += PlaceArch(Layout.FRand() < 0.5f ? 28 : -30);
		Extras += bDream && Layout.FRand() < 0.6f ? PlaceDieHouse(6000, Anywhere) : 0;
		Buildings = PlaceBuildings(Want(22), 16000, Anywhere, 8, 12, 8, 12, 0.8f, 0.5f);
		Towers = PlaceTowers(Want(3), Anywhere);
		Cover = PlaceCover(Want(26), 5000, Anywhere);
		Trees = PlaceTrees(Want(8), 3000, Anywhere);
		break;
	}
	case ERRDistrict::Castle:
	{
		// The wall: cream stone CastleWallLevels high, three thick, a lilac walkway on top, battlements on every other cell of
		// its outer edge; a gateway where each street runs through it, open to the sky between pink posts a level taller.
		// Inside, a flight up to the walkway either side of each wall's middle, and the keep (a two story house, a flat roof
		// with a parapet) in a paved courtyard.
		const int32 WallTop = RRBattle::CastleWallLevels;
		const float In = RRBattle::CastleIn;
		const float Out = RRBattle::CastleOut;
		const uint8 WallStone = MakeLook(Shade(Cream, 0.86f), Cream);
		const uint8 WallWalk = MakeLook(Shade(Cream, 0.86f), Blend(Lavender, White, 0.45f));
		const uint8 WallMerlon = MakeLook(Shade(Lavender, 0.85f), Lavender);
		const uint8 GatePost = MakeLook(Shade(Pink, 0.85f), Pink);
		const uint8 GateCap = MakeLook(Shade(Pink, 0.85f), Lemon);
		auto IsGateway = [](int32 X, int32 Y, float Reach)
		{
			return FMath::Abs(Y + 0.5f) < Reach || FMath::Abs(X + 0.5f) < Reach;
		};
		for (int32 X = -int32(Out); X < int32(Out); ++X)
		{
			for (int32 Y = -int32(Out); Y < int32(Out); ++Y)
			{
				const float Ring = Cheb(X, Y);
				if (Ring < In || Ring >= Out || IsGateway(X, Y, RRBattle::StreetHalf) || !IsOnIsland(X, Y))
				{
					continue;
				}
				if (IsGateway(X, Y, RRBattle::StreetHalf + 1.f))
				{
					Fill(FFrame(), X, Y, X, Y, 1, WallTop + 1, GatePost);
					Set(X, Y, WallTop + 2, GateCap);
					continue;
				}
				for (int32 Level = 1; Level <= WallTop; ++Level)
				{
					Set(X, Y, Level, Level == WallTop ? WallWalk : WallStone);
				}
				if (Ring >= Out - 1.f && ((X + Y) & 1) == 0)
				{
					Set(X, Y, WallTop + 1, WallMerlon);
				}
			}
		}
		for (int32 Turn = 0; Turn < 4; ++Turn)
		{
			const FFrame Side = FFrame::Around(Turn);
			AddStairs(Side, int32(In) - WallTop, 6, 1, 0, 0, 1, 2, 0, WallTop, Cream);
			AddStairs(Side, int32(In) - WallTop, -8, 1, 0, 0, 1, 2, 0, WallTop, Cream);
		}
		// The wall's band and the stairs' run kept clear; the keep in the middle
		const int32 Band = int32(In) - WallTop - 2;
		const int32 Rim = int32(Out) + 1;
		Reserve(FIntRect(-Rim, -Rim, Rim, -Band));
		Reserve(FIntRect(-Rim, Band, Rim, Rim));
		Reserve(FIntRect(-Rim, -Band, -Band, Band));
		Reserve(FIntRect(Band, -Band, Rim, Band));
		FFrame Keep;
		Keep.Origin = FIntPoint(-7, -7);
		Keep.Turn = 0;
		Reserve(Footprint(Keep, -1, -1, 15, 15, 0));
		AddBuilding(Keep, 14, 14, 2, true, Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1), Layout.RandRange(0, 1 << 30));
		Buildings = 1;
		// Outside the wall: cottages, trees and cover
		auto OutsideWall = [](int32 X, int32 Y) { return Cheb(X, Y) >= RRBattle::CastleOut + 2.f; };
		Buildings += PlaceBuildings(Want(9), 8000, OutsideWall, 7, 10, 7, 10, 0.35f, 0.3f);
		Towers = PlaceTowers(Want(2), OutsideWall);
		Trees = PlaceTrees(Want(24), 6000, OutsideWall);
		Cover = PlaceCover(Want(22), 5000, OutsideWall);
		break;
	}
	case ERRDistrict::Canal:
	{
		// A canal across each half (either side of the street across the middle, which stays a bridge), footbridges over it
		const uint8 Deck = MakeLook(WoodDark, Wood);
		for (const int32 V0 : { 16, -20 })
		{
			for (const int32 Sign : { -1, 1 })
			{
				const int32 From = Sign < 0 ? -int32(Edge) + 1 : int32(RRBattle::StreetHalf) + 1;
				const int32 Length = int32(Edge) - 1 - int32(RRBattle::StreetHalf) - 1;
				FFrame Canal;
				Canal.Origin = FIntPoint(From, V0);
				Reserve(FIntRect(From - 1, V0 - 1, From + Length + 1, V0 + 5));
				AddCanal(Canal, Length, 4);
				for (int32 Across = 6; Across + 3 < Length; Across += 14)
				{
					const FFrame Bridge = Sub(Canal, Across, 0, 0);
					Reserve(Footprint(Bridge, 0, -2, 2, 5, 1));
					AddStairs(Bridge, 0, -1, 0, 1, 1, 0, 3, 0, 1, Wood);
					Fill(Bridge, 0, 0, 2, 3, 1, 1, Deck);
					AddStairs(Bridge, 0, 4, 0, -1, 1, 0, 3, 0, 1, Wood);
					++Extras;
				}
			}
		}
		// Round 94: now and then a big building beside the water (the test map: the garage)
		{
			const float Roll = BattleTile.Reach <= 1 ? 0.f : Layout.FRand();
			if (Roll < 0.5f)
			{
				PlaceBigBuilding(Roll < 0.3f ? EBigBuilding::Garage : EBigBuilding::Factory, Layout);
			}
		}
		Buildings = PlaceBuildings(Want(16), 12000, Anywhere, 8, 11, 8, 11, 0.7f, 0.3f);
		Towers = PlaceTowers(Want(2), Anywhere);
		Trees = PlaceTrees(Want(18), 5000, Anywhere);
		Cover = PlaceCover(Want(20), 4000, Anywhere);
		break;
	}
	case ERRDistrict::Park:
	{
		// A wood: a mountain of giant dice, a hill, trees thick everywhere, a few cottages, hedges
		if (bDream)
		{
			for (int32 Try = 0; Try < 4000; ++Try)
			{
				const FFrame Frame = RandomFrame();
				const FIntRect Rect = Footprint(Frame, -1, -5, 12, 12, 1);
				if (IsFree(Rect))
				{
					Reserve(Rect);
					AddDiceMountain(Frame);
					++Extras;
					break;
				}
			}
		}
		for (int32 Try = 0; Try < 3000; ++Try)
		{
			const FFrame Frame = RandomFrame();
			const FIntRect Rect = Footprint(Frame, -1, -2, 18, 16, 1);
			if (IsFree(Rect))
			{
				Reserve(Rect);
				AddHill(Frame, 18, 16, 2);
				++Extras;
				break;
			}
		}
		Buildings = PlaceBuildings(Want(5), 6000, Anywhere, 7, 9, 7, 9, 0.f, 0.f);
		Trees = PlaceTrees(Want(70), 20000, Anywhere);
		Cover = PlaceCover(Want(20), 4000, Anywhere);
		break;
	}
	case ERRDistrict::Hills:
	{
		// Round 95: big stepped hills (2, 5 and 8 blocks up) with a building on top, to see the town from and fight for
		// (round 92's were 24 a side and 3 high: hardly above the cover). Two where they fit, else one, else the old small one
		int32 BigHills = 0;
		for (int32 Try = 0; Try < 800 && Extras < 2; ++Try)
		{
			const FFrame Frame = RandomFrame();
			const int32 W = 36;
			const int32 D = 36;
			// (a quarter of the tile, between its middle streets and its avenues, is 46 cells: the hill and its first flight fit)
			const FIntRect Rect = Footprint(Frame, -3, -3, W + 2, D + 2, 1);
			if (!IsFree(Rect) || !AllIn(Rect, [this](int32 X, int32 Y) { return IsOnIsland(X, Y); }))
			{
				continue;
			}
			Reserve(Rect);
			AddBigHill(Frame, W, D, Layout.RandRange(0, 1 << 30));
			UE_LOG(LogTemp, Log, TEXT("RRRandom: big hill, its top at %s."), *FrameSpot(Frame, (W - 1) * 0.5f, (D - 1) * 0.5f, 900.f).ToString());
			++Extras;
			++BigHills;
		}
		for (int32 Try = 0; Try < 6000 && Extras < 1; ++Try)
		{
			const FFrame Frame = RandomFrame();
			const int32 W = 24;
			const int32 D = 24;
			const FIntRect Rect = Footprint(Frame, -1, -2, W, D + 1, 1);
			if (!IsFree(Rect))
			{
				continue;
			}
			Reserve(Rect);
			AddHill(Frame, W, D, 3);
			Lift = 3;
			AddTower(Sub(Frame, 10, 13, 0), Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1));
			Lift = 0;
			++Extras;
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: hills tile (%d, %d): %d big hill(s), %d small."), BattleTile.X, BattleTile.Y, BigHills, Extras - BigHills);
		Buildings = PlaceBuildings(Want(8), 8000, Anywhere, 7, 9, 7, 9, 0.2f, 0.f);
		Towers = PlaceTowers(Want(1), Anywhere);
		Trees = PlaceTrees(Want(30), 8000, Anywhere);
		Cover = PlaceCover(Want(18), 4000, Anywhere);
		break;
	}
	case ERRDistrict::Fort:
	default:
	{
		// Round 94: a fort district first gets a toy factory, mostly (always on the test map)
		if (District == ERRDistrict::Fort && (BattleTile.Reach <= 1 || Layout.FRand() < 0.7f))
		{
			PlaceBigBuilding(EBigBuilding::Factory, Layout);
		}
		// Walled compounds: a paved courtyard, a low wall round it (merlons every other cell, to climb and shoot over),
		// gateways front and back, crates inside, a watchtower in a back corner
		for (int32 Try = 0; Try < 8000 && Extras < 4; ++Try)
		{
			const FFrame Frame = RandomFrame();
			const int32 W = Layout.RandRange(16, 22);
			const int32 D = Layout.RandRange(14, 18);
			const FIntRect Rect = Footprint(Frame, -1, -1, W, D, 1);
			if (!IsFree(Rect))
			{
				continue;
			}
			Reserve(Rect);
			const FColor Tint = Styles[Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1)].High;
			const uint8 CourtA = MakeLook(Stone, Blend(PaveA, Tint, 0.45f));
			const uint8 CourtB = MakeLook(Stone, Blend(PaveB, Tint, 0.6f));
			const uint8 Wall = MakeLook(Shade(Tint, 0.8f), Tint);
			const uint8 Crate = MakeLook(WoodDark, Wood);
			const int32 GateFrom = W / 2 - 2;
			const int32 GateTo = W / 2 + 1;
			for (int32 U = 0; U < W; ++U)
			{
				for (int32 V = 0; V < D; ++V)
				{
					const FIntPoint Cell = Frame.Cell(U, V);
					Set(Cell.X, Cell.Y, 0, ((U + V) & 1) ? CourtA : CourtB);
					const bool bWallEdge = U == 0 || U == W - 1 || V == 0 || V == D - 1;
					const bool bGate = (V == 0 || V == D - 1) && U >= GateFrom && U <= GateTo;
					if (bWallEdge && !bGate)
					{
						Set(Cell.X, Cell.Y, 1, Wall);
						if (((U + V) & 1) == 0)
						{
							Set(Cell.X, Cell.Y, 2, Wall);
						}
					}
				}
			}
			for (int32 Pile = 0; Pile < 4; ++Pile)
			{
				const int32 U = Layout.RandRange(3, W - 5);
				const int32 V = Layout.RandRange(3, D - 8);
				Fill(Frame, U, V, U + 1, V + 1, 1, 1, Crate);
				if (Layout.FRand() < 0.5f)
				{
					Fill(Frame, U, V, U, V, 2, 2, Crate);
				}
			}
			AddTower(Sub(Frame, 1, D - 5, 0), Layout.RandRange(0, UE_ARRAY_COUNT(Styles) - 1));
			++Extras;
		}
		Buildings = PlaceBuildings(Want(6), 6000, Anywhere, 7, 10, 7, 10, 0.5f, 0.4f);
		Towers = PlaceTowers(Want(2), Anywhere);
		Trees = PlaceTrees(Want(16), 5000, Anywhere);
		Cover = PlaceCover(Want(26), 5000, Anywhere);
		break;
	}
	}

	// Star lamps along the avenues on this tile's +X and +Y edges (the next tiles light the others), where there is room
	int32 Lamps = 0;
	if (bDream && Poles && Stars)
	{
		const uint8 PoleHidden = HiddenLook(FColor(255, 150, 196));
		auto IsClear = [this](const FIntPoint& Cell)
		{
			if (!IsOnIsland(Cell.X, Cell.Y) || IsRoad(Cell.X, Cell.Y) || !Get(Cell.X, Cell.Y, 0) || StairCells.Contains(Cell))
			{
				return false;
			}
			for (int32 DX = -1; DX <= 1; ++DX)
			{
				for (int32 DY = -1; DY <= 1; ++DY)
				{
					for (int32 Level = 1; Level <= 6; ++Level)
					{
						if (Get(Cell.X + DX, Cell.Y + DY, Level))
						{
							return false;
						}
					}
				}
			}
			return true;
		};
		const int32 Inner = int32(Edge) - 1;
		for (int32 Along = -Inner + 4; Along <= Inner - 4; Along += BattleLampSpacing)
		{
			for (const FIntPoint& Cell : { FIntPoint(Inner, Along), FIntPoint(Along, Inner) })
			{
				if (!IsClear(Cell))
				{
					continue;
				}
				for (int32 Level = 1; Level <= 3; ++Level)
				{
					Set(Cell.X, Cell.Y, Level, PoleHidden);
				}
				for (int32 Band = 0; Band < 6; ++Band)
				{
					const FColor Stripe = Band % 2 ? FColor(255, 150, 196) : White;
					AddColored(Poles, FTransform(FRotator::ZeroRotator, At(CellCenter(Cell), (Band + 0.5f) * 50.f), FVector(0.38f, 0.38f, 0.5f)), Stripe, Stripe);
				}
				AddColored(Poles, FTransform(FRotator::ZeroRotator, At(CellCenter(Cell), 10.f), FVector(0.8f, 0.8f, 0.2f)), White, White);
				AddFloater(Stars, At(CellCenter(Cell), 3 * BlockSize + 80.f), FRotator::ZeroRotator, 1.3f, StarYellow, StarYellow, 8.f, 60.f, 0.f, 0.f, 0.f, float(Along));
				UPointLightComponent* Light = NewObject<UPointLightComponent>(this, NAME_None, RF_Transient);
				Light->SetupAttachment(RootComponent);
				Light->SetUsingAbsoluteLocation(true);
				Light->SetIntensityUnits(ELightUnits::Candelas);
				Light->SetIntensity(LampLightIntensity);
				Light->SetAttenuationRadius(LampLightReach);
				Light->SetLightColor(LampLightColor);
				Light->SetCastShadows(false);
				Light->RegisterComponent();
				Light->SetWorldLocation(At(CellCenter(Cell), 3 * BlockSize + 60.f));
				LampLights.Add(Light);
				++Lamps;
			}
		}
	}
	AddPuddles();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: battle tile (%d, %d) %s: %.0f%% ground, %d buildings, %d towers, %d trees, %d cover, %d extras, %d lamps, big: %s."),
		BattleTile.X, BattleTile.Y, RRBattle::GetDistrictName(District), Share * 100.f, Buildings, Towers, Trees, Cover, Extras, Lamps,
		BigBuildingsBuilt.IsEmpty() ? TEXT("none") : *BigBuildingsBuilt);
}
