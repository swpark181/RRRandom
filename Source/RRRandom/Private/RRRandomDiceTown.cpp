#include "RRRandomDiceTown.h"
#include "RRRandomCharacter.h"
#include "RRRandomWreckage.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "UObject/UObjectGlobals.h"
#include "Algo/Reverse.h"

namespace
{
	// Blocks are 100 units (char-pipeline/scripts/blender_block_town.py; the engine cube too, for the hidden boxes)
	constexpr float BlockSize = 100.f;
	constexpr float FloorSearchUp = 500.f;
	constexpr float FloorSearchDown = 3000.f;

	// The grid: cells -HalfCells .. HalfCells - 1 each way from the middle, levels LevelMin .. LevelMax. Level L is the
	// block from (L - 1) x 100 to L x 100 above the floor: level 0's top is the floor
	constexpr int32 HalfCells = 88;
	constexpr int32 GridCells = HalfCells * 2;
	constexpr int32 LevelMin = -3;
	constexpr int32 LevelMax = 30;
	constexpr int32 GridLevels = LevelMax - LevelMin + 1;
	constexpr uint8 HiddenSolid = 255;
	constexpr float MapHalf = HalfCells * BlockSize;

	// The island: a rounded square (a superellipse) about this far out, wobbling a little
	constexpr float RimBase = 8450.f;
	constexpr float RimPower = 5.f;
	constexpr int32 RimWallSegments = 260;
	constexpr float RimWallHeight = 6000.f;

	// Buildings: a story is 6 blocks (a floor block and 5 of room, 500: a giant is 384 tall); doors and windows 3 x 4
	constexpr int32 Story = 6;
	constexpr int32 OpeningWidth = 3;
	constexpr int32 OpeningHeight = 4;

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
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	return FBox2D(Center - FVector2D(MapHalf), Center + FVector2D(MapHalf));
}

FBox ARRRandomDiceTown::GetFlightArea() const
{
	// Over the whole island, the feet up to well over its tallest structure (the dice mountain, the keeps, the roofs), so a flyer
	// can land on any of them and look about over them; the rim's tall hidden wall keeps flyers over the island
	const FBox2D Map = GetMapBounds();
	return FBox(FVector(Map.Min, FloorZ - 1000.f), FVector(Map.Max, FloorZ + TopLevel * BlockSize + FlightHeadroom));
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

uint8 ARRRandomDiceTown::MakeLook(const FColor& Body, const FColor& Accent, bool bDie, bool bUnderMesh)
{
	for (int32 Index = 0; Index < Looks.Num(); ++Index)
	{
		if (Looks[Index].Body == Body && Looks[Index].Accent == Accent && Looks[Index].bDie == bDie && Looks[Index].bUnderMesh == bUnderMesh)
		{
			return uint8(Index + 1);
		}
	}
	if (Looks.Num() >= HiddenSolid - 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: dice town ran out of looks."));
		return 1;
	}
	Looks.Add({ Body, Accent, bDie, bUnderMesh });
	return uint8(Looks.Num());
}

float ARRRandomDiceTown::RimRadius(float Theta) const
{
	const float C = FMath::Abs(FMath::Cos(Theta));
	const float S = FMath::Abs(FMath::Sin(Theta));
	const float Square = RimBase / FMath::Pow(FMath::Pow(C, RimPower) + FMath::Pow(S, RimPower), 1.f / RimPower);
	return Square + 110.f * FMath::Sin(3.f * Theta + 1.f) + 70.f * FMath::Sin(7.f * Theta + 2.f);
}

bool ARRRandomDiceTown::IsOnIsland(int32 X, int32 Y) const
{
	const FVector2D Local = CellCenter(FIntPoint(X, Y));
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
	if (Rect.Intersect(Mirrored))
	{
		return false;
	}
	for (int32 X = Rect.Min.X; X < Rect.Max.X; ++X)
	{
		for (int32 Y = Rect.Min.Y; Y < Rect.Max.Y; ++Y)
		{
			// Two cells in from the rim, off the roads and the plaza
			if (!IsOnIsland(X, Y) || !IsOnIsland(X + 2, Y) || !IsOnIsland(X - 2, Y) || !IsOnIsland(X, Y + 2) || !IsOnIsland(X, Y - 2)
				|| IsRoad(X, Y) || Cheb(X, Y) < PlazaEdge)
			{
				return false;
			}
		}
	}
	for (const FIntRect& Other : Reserved)
	{
		if (Rect.Intersect(Other) || Mirrored.Intersect(Other))
		{
			return false;
		}
	}
	return true;
}

void ARRRandomDiceTown::Reserve(const FIntRect& Rect)
{
	Reserved.Add(Rect);
	Reserved.Add(FIntRect(-Rect.Max.X, -Rect.Max.Y, -Rect.Min.X, -Rect.Min.Y));
}

void ARRRandomDiceTown::Both(const FFrame& Frame, TFunctionRef<void(const FFrame&)> Place)
{
	Place(Frame);
	Place(Frame.Mirror());
}

UInstancedStaticMeshComponent* ARRRandomDiceTown::MakeInstances(UStaticMesh* Mesh, UMaterialInterface* Material, bool bSolid, int32 CustomFloats)
{
	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
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
	Instances->SetCanEverAffectNavigation(false);
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
					AddColored(Roofs, FTransform(FRotator(0.f, FrameYaw(Frame), 0.f), FrameSpot(Frame, (W - 1) * 0.5f, (D - 1) * 0.5f, Top * BlockSize),
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

void ARRRandomDiceTown::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ARRRandomDiceTown::Build()
{
	UWorld* World = GetWorld();
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

	// The floor under the actor: the town stands on it and hides it (its own ground blocks are solid too)
	FHitResult Hit;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, FloorSearchUp);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomDiceTown), false, this);
	if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, FloorSearchUp + FloorSearchDown), ECC_Visibility, Params) || !Hit.GetComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no floor under the dice town at %s."), *GetActorLocation().ToString());
		return;
	}
	FloorZ = Hit.ImpactPoint.Z;
	Hit.GetComponent()->SetVisibility(false);

	// The dreamier pass (round 47): on unless -RRTownClassic asks for the town as it was (and only with its shapes imported)
	PuffMesh = LoadMesh(TEXT("SM_BTPuff"));
	StarMesh = LoadMesh(TEXT("SM_BTStar"));
	MoonMesh = LoadMesh(TEXT("SM_BTMoon"));
	RoofMesh = LoadMesh(TEXT("SM_BTRoof"));
	bDream = !FParse::Param(FCommandLine::Get(), TEXT("RRTownClassic")) && PuffMesh && StarMesh && MoonMesh;

	Blocks = MakeInstances(BlockMesh, nullptr, false, 6);
	DieBlocks = MakeInstances(DieMesh, nullptr, false, 6);
	Steps = MakeInstances(BlockMesh, nullptr, false, 6);
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
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	for (int32 Segment = 0; Segment < RimWallSegments; ++Segment)
	{
		const float Theta = UE_TWO_PI * Segment / RimWallSegments;
		const float Out = RimRadius(Theta) - 40.f;
		const float Length = UE_TWO_PI * (Out + 100.f) / RimWallSegments * 1.2f;
		RimWalls->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Theta), 0.f),
			FVector(Center.X + FMath::Cos(Theta) * (Out + 50.f), Center.Y + FMath::Sin(Theta) * (Out + 50.f), FloorZ + RimWallHeight * 0.5f - 300.f),
			FVector(1.f, Length / BlockSize, RimWallHeight / BlockSize)), true);
	}

	Grid.SetNumZeroed(GridCells * GridCells * GridLevels);
	LayOut();
	MakeBlocks();
	MakeCollision();
	MakeWalkGraph();
	MakeUnderside();
	MakeSky();
	if (bDream)
	{
		MakeDreamSky();
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
	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town %d blocks, %d dice, %d steps, %d water, %d underside, %d cloud boxes, %d solid boxes, %d stairs, %d walk nodes (%d roamable), %d looks."),
		Blocks->GetInstanceCount(), DieBlocks->GetInstanceCount(), Steps->GetInstanceCount(), Water->GetInstanceCount(), Underside->GetInstanceCount(),
		Clouds->GetInstanceCount(), Solid->GetInstanceCount(), Stairs.Num(), Nodes.Num(), Roamable.Num(), Looks.Num());
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
	const uint8 PlazaLookA = MakeLook(Stone, PlazaPink);
	const uint8 PlazaLookB = MakeLook(Stone, PlazaCream);
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
	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town placed %d downtown, %d canal and %d cottage buildings, %d towers, %d + %d + %d cover (each half)."),
		Downtown, Canal, Cottages, Towers, Inner, Middle, Outer);
	if (bDream)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town dream pass placed %d die houses (each half)."), DieHouses);
	}
}

void ARRRandomDiceTown::MakeBlocks()
{
	// A block wherever one shows (a face against an empty cell), each a shade lighter or darker at random; the bottom of
	// the ground shows only past the edge of the cone of blocks under it
	constexpr float UnderShare = 0.92f;
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
				bool bShows = !Get(X + 1, Y, Level) || !Get(X - 1, Y, Level) || !Get(X, Y + 1, Level) || !Get(X, Y - 1, Level) || !Get(X, Y, Level + 1);
				if (!bShows)
				{
					if (Level > -2)
					{
						bShows = !Get(X, Y, Level - 1);
					}
					else
					{
						const FVector2D Local = CellCenter(FIntPoint(X, Y));
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
	constexpr int32 LayerCount = 12;
	constexpr float Big = BlockSize * 2.f;
	constexpr int32 HalfBig = HalfCells / 2;
	auto Inside = [this](int32 BX, int32 BY, int32 Layer)
	{
		const FVector2D Local((BX + 0.5f) * Big, (BY + 0.5f) * Big);
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
	FRandomStream Air(SkySeed);
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	for (int32 Index = 0; Index < 150; ++Index)
	{
		const float Theta = Air.FRandRange(0.f, UE_TWO_PI);
		const float Far = FMath::Sqrt(Air.FRand());
		const float Out = FMath::Lerp(9500.f, 40000.f, Far);
		const float Size = FMath::Lerp(1400.f, 6000.f, Far) * Air.FRandRange(0.8f, 1.2f);
		const float Z = FMath::Lerp(-2400.f, 400.f, FMath::Square(Far));
		AddCloud(FVector(Center.X + FMath::Cos(Theta) * Out, Center.Y + FMath::Sin(Theta) * Out, FloorZ + Z), Size, Air);
	}
	for (int32 Index = 0; Index < 14; ++Index)
	{
		const float Theta = Air.FRandRange(0.f, UE_TWO_PI);
		const float Out = Air.FRandRange(4000.f, 16000.f);
		AddCloud(FVector(Center.X + FMath::Cos(Theta) * Out, Center.Y + FMath::Sin(Theta) * Out, FloorZ + Air.FRandRange(2600.f, 5000.f)), Air.FRandRange(500.f, 1000.f), Air);
	}
	AddRainbow(FVector(Center.X + 15000.f, Center.Y - 3000.f, FloorZ - 2500.f), FVector(0.f, 1.f, 0.f), 7600.f);
	AddRainbow(FVector(Center.X - 15000.f, Center.Y + 3000.f, FloorZ - 2500.f), FVector(0.f, -1.f, 0.f), 7600.f);

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
		const FVector Middle(Center + Isle.Where, FloorZ + Isle.Height);
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
	AddFloater(Stars, FrameSpot(Frame, (N - 1) * 0.5f, (N - 1) * 0.5f, 8.f * BlockSize), FRotator(0.f, FrameYaw(Frame), 0.f), 2.2f, StarYellow, StarYellow,
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
				}
				++Placed;
			}
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town dream pass placed %d star lamps (each half)."), Placed);
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
	AddColored(Clouds, FTransform(FRotator::ZeroRotator, FVector(Center, FloorZ - 6500.f), FVector(20000.f, 20000.f, 4.f)), FColor(168, 210, 255), FColor(168, 210, 255));

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
		const FVector A(Center.X + Sign * 10600.f, Center.Y + Sign * 6600.f, FloorZ + 600.f);
		const FVector B(Center.X + Sign * 10600.f, Center.Y + Sign * 1400.f, FloorZ + 600.f);
		Isle(A, 6, Sign > 0.f ? 11 : 12);
		Isle(B, 5, Sign > 0.f ? 13 : 14);
		DiceArc((A + B) * 0.5f + FVector(0.f, 0.f, BlockSize * 0.5f), (B - A).GetSafeNormal(), (B - A).Size() * 0.5f - BlockSize * 3.f);
		// A crescent moon high beyond them, stars round it
		const FVector Moon(Center.X + Sign * 21000.f, Center.Y - Sign * 7000.f, FloorZ + 7600.f);
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
		const float Out = bFar ? Air.FRandRange(9300.f, 13000.f) : Air.FRandRange(1600.f, 7600.f);
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
		const float Out = Air.FRandRange(2000.f, 11000.f);
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
				Nodes.Add({ FIntPoint(X, Y), (Stair.Base + Step->Value + 1) * BlockSize, Stair.Dir });
				continue;
			}
			for (int32 Level = LevelMin; Level < LevelMax; ++Level)
			{
				if (Get(X, Y, Level) && IsClearAbove(X, Y, Level, Headroom))
				{
					Nodes.Add({ FIntPoint(X, Y), Level * BlockSize, FIntPoint::ZeroValue });
				}
			}
		}
	}
	NodeStart[GridCells * GridCells] = Nodes.Num();

	// The ground anyone can reach from the plaza's edge
	const int32 Seed = FindNode(At(CellCenter(FIntPoint(0, -12)), 0.f), 2);
	if (Seed == INDEX_NONE)
	{
		return;
	}
	TBitArray<> Seen(false, Nodes.Num());
	TArray<int32> Queue = { Seed };
	Seen[Seed] = true;
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
	for (int32 Try = 0; Try < 40 && Roamable.Num() > 0; ++Try)
	{
		const FWalkNode& Node = Nodes[Roamable[Stream.RandRange(0, Roamable.Num() - 1)]];
		if (CellCenter(Node.Cell).Size() <= Radius)
		{
			return At(CellCenter(Node.Cell), Node.Z);
		}
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
	if (Floaters.Num() > 0)
	{
		AnimateFloaters(GetWorld()->GetTimeSeconds());
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

void ARRRandomDiceTown::AddCellBlock(int32 X, int32 Y, int32 Level)
{
	const FLook& Look = Looks[Grid[GridIndex(X, Y, Level)] - 1];
	const float Jitter = 0.95f + 0.1f * float(CellHash(X, Y, Level) % 1000) / 1000.f;
	UInstancedStaticMeshComponent* Instances = Look.bDie ? DieBlocks.Get() : Blocks.Get();
	AddColored(Instances, GetCellTransform(X, Y, Level), Shade(Look.Body, Jitter), Shade(Look.Accent, Jitter));
	CellBlocks.Add(GridIndex(X, Y, Level), { Look.bDie, Instances->GetInstanceCount() - 1, true });
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
	// (The island's underside keeps its own rule in MakeBlocks; wrecks never come near it)
	if (!IsInGrid(X, Y, Level) || Level < -1)
	{
		return;
	}
	const int32 Index = GridIndex(X, Y, Level);
	const uint8 Value = Grid[Index];
	if (Value == 0 || Value == HiddenSolid || Looks[Value - 1].bUnderMesh)
	{
		return;
	}
	const bool bWant = !Broken[Index] && (!IsFilled(X + 1, Y, Level) || !IsFilled(X - 1, Y, Level) || !IsFilled(X, Y + 1, Level) || !IsFilled(X, Y - 1, Level)
		|| !IsFilled(X, Y, Level + 1) || !IsFilled(X, Y, Level - 1));
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
	UInstancedStaticMeshComponent* Instances = Block->bDie ? DieBlocks.Get() : Blocks.Get();
	const FTransform Where = GetCellTransform(X, Y, Level);
	Instances->UpdateInstanceTransform(Block->Index, bWant ? Where : Shrunk(Where), true, false, false);
	if (bWant && bGrow)
	{
		OutRegrow.Add({ Instances, Block->Index, Where });
	}
}

void ARRRandomDiceTown::ApplyWrecks(TConstArrayView<FRRWreck> InWrecks, TArray<FRRBrokenBit>& OutBroken, TArray<FRRRegrow>& OutRegrow)
{
	if (!bBuilt)
	{
		return;
	}
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
	auto CellOf = [](int32 Index)
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
			ForCellsIn(BoundsOf(Instances, Index), [&](int32 Cell) { bOver |= Now[Cell] && IsUnderMesh(Cell); });
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
		bool bHit = false;
		for (int32 Step = Stair.StepFirst; Step < Stair.StepFirst + Stair.StepCount && !bHit; ++Step)
		{
			bHit = IsHit(BoundsOf(Steps, Step));
		}
		if (bHit)
		{
			for (int32 Step = Stair.StepFirst; Step < Stair.StepFirst + Stair.StepCount; ++Step)
			{
				WantGone.FindOrAdd(Steps).Add(Step);
			}
			for (int32 Slab = Stair.SolidFirst; Slab < Stair.SolidFirst + Stair.SolidCount; ++Slab)
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
			const FIntVector Cell = CellOf(Index);
			const FLook& Look = Looks[Grid[Index] - 1];
			OutBroken.Add({ GetCellTransform(Cell.X, Cell.Y, Cell.Z).GetLocation(), FLinearColor(Look.Body), FLinearColor(Look.Accent), 1.f });
		}
	}
	Broken = MoveTemp(Now);
	BrokenCells = MoveTemp(NowCells);

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

	// Models gone now, and back once no wreck reaches them
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
					OutRegrow.Add({ Instances, It.Key(), It.Value() });
				}
				Touched.Add(Instances);
				It.RemoveCurrent();
			}
		}
	}
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
	for (UInstancedStaticMeshComponent* Instances : Touched)
	{
		Instances->MarkRenderStateDirty();
	}
	int32 GoneCount = 0;
	for (const TPair<UInstancedStaticMeshComponent*, TMap<int32, FTransform>>& Pair : Gone)
	{
		GoneCount += Pair.Value.Num();
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice town wrecks: %d in force, %d blocks broken (%d just now, %d mended), %d boxes cut into %d, %d models gone."),
		InWrecks.Num(), BrokenCells.Num(), OutBroken.Num(), Mended.Num(), CutBoxes.Num(), WreckSolid ? WreckSolid->GetInstanceCount() : 0, GoneCount);
}
