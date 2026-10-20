#include "RRRandomDiceLand.h"
#include "RRRandomCharacter.h"
#include "EngineUtils.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Math/RandomStream.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	// The engine's cube is 100 units across, centered on its pivot; so are the die and the ball, the trunk is 100 across
	// and 100 high from its foot, the ring 100 across (char-pipeline/scripts/blender_dice_land.py)
	constexpr float BasicSize = 100.f;
	constexpr float FloorSearchUp = 500.f;
	constexpr float FloorSearchDown = 3000.f;

	// The island: a wobbly circle about this far out, cushion tiles this wide (their tops level with the floor), and over
	// the floor a square of this half size for the minimap
	constexpr float BaseRadius = 3050.f;
	constexpr float TileSize = 200.f;
	constexpr float MapHalf = 3400.f;
	// The underlay under the tiles' seams: this far under the floor, this thick
	constexpr float UnderlayTop = 9.f;
	constexpr float UnderlayThickness = 10.f;

	// Roads (half widths), matching the game mode: the teams start TeamSpawnDistance (3060) apart on rows across the main
	// road; the side lanes are where the game mode parks the cars (Y -1890 and 1710)
	constexpr float LaneHalf = 180.f;
	constexpr float TeamRowX = 1530.f;
	constexpr float SideLaneY = 1800.f;
	constexpr float PlazaRadius = 560.f;
	constexpr float PlazaRing = 720.f;
	// Things keep this far inside the rim, and this far apart
	constexpr float RimMargin = 170.f;
	constexpr float Spacing = 70.f;

	// The rim's hidden wall: high enough that flying brawlers can't get over it
	constexpr float RimWallHeight = 6000.f;
	// Flying: the feet up to this far over the floor
	constexpr float FlightCeiling = 1800.f;
	constexpr int32 RimWallSegments = 160;

	// Cover sizes: a wall of dice (two rows), a hedge of balls; both about chest high (the cover blocks were 160)
	constexpr float WallDie = 78.f;
	constexpr int32 WallLength = 4;
	constexpr float HedgeBall = 110.f;
	constexpr int32 HedgeLength = 5;

	// Tuned while looking at the game (console ~), defaults in Config/DefaultEngine.ini [SystemSettings]
	TAutoConsoleVariable<float> CVarSaturation(TEXT("rr.DiceLand.Saturation"), 1.15f,
		TEXT("Dice Land's colors: 1 as made, higher deeper, lower paler. Live."));
	TAutoConsoleVariable<float> CVarBrightness(TEXT("rr.DiceLand.Brightness"), 1.f,
		TEXT("Dice Land's colors: 1 as made, higher lighter, lower darker. Live."));
	// How far the fox's fur goes toward the land's pastels: a little less color, a touch of cream (mixed on linear
	// colors, where small amounts go a long way)
	TAutoConsoleVariable<float> CVarFoxDesaturate(TEXT("rr.DiceLand.FoxDesaturate"), 0.05f,
		TEXT("In Dice Land, how much color the fox's fur loses (0 none, 1 gray). Live."));
	TAutoConsoleVariable<float> CVarFoxCream(TEXT("rr.DiceLand.FoxCream"), 0.03f,
		TEXT("In Dice Land, how much cream the fox's fur takes (0 none, 1 all cream). Live."));

	constexpr int32 GroundSeed = 5151;
	constexpr int32 LayoutSeed = 6262;
	constexpr int32 SkySeed = 7373;

	// Macaron pastels (sRGB): deeper than they look, since the bright sky lifts everything toward white
	const FColor Pink(255, 143, 177);
	const FColor Peach(255, 178, 128);
	const FColor Lemon(255, 222, 110);
	const FColor Mint(122, 222, 166);
	const FColor Sky(115, 196, 255);
	const FColor Periwinkle(145, 155, 255);
	const FColor Lavender(195, 150, 255);
	const FColor Cream(255, 247, 236);
	const FColor Rainbow[] = { Pink, Peach, Lemon, Mint, Sky, Periwinkle, Lavender };
	// Tree tops, cover dice (white ones get colored pips), hedges, clouds
	const FColor Leaves[] = { FColor(255, 160, 195), FColor(196, 165, 255), FColor(255, 186, 140), FColor(140, 222, 185), FColor(150, 190, 255), FColor(255, 220, 120) };
	const FColor CoverDice[] = { FColor(255, 250, 245), FColor(255, 170, 195), FColor(190, 170, 255), FColor(160, 215, 255), FColor(255, 210, 140), FColor(160, 230, 190) };
	const FColor Greens[] = { FColor(120, 205, 150), FColor(105, 195, 140), FColor(140, 215, 165) };
	const FColor Puffs[] = { FColor(255, 250, 252), FColor(255, 222, 236), FColor(230, 220, 255), FColor(255, 214, 230), FColor(220, 234, 255) };
	// The cone under the island: peach at the top, pink, lilac at the point
	const FColor UnderTop(255, 165, 115);
	const FColor UnderMid(255, 130, 165);
	const FColor UnderBottom(160, 125, 255);

	// Minimap colors for what stands on the ground
	const FColor MapSky(190, 220, 255);
	const FColor MapCover(232, 150, 190);
	const FColor MapHedge(110, 190, 140);
	const FColor MapTree(150, 140, 220);
	const FColor MapPedestal(200, 170, 250);

	FColor Darker(const FColor& Color, float Share)
	{
		return FColor(uint8(Color.R * Share), uint8(Color.G * Share), uint8(Color.B * Share));
	}

	FColor Blend(const FColor& A, const FColor& B, float Alpha)
	{
		return FColor(uint8(FMath::Lerp(float(A.R), float(B.R), Alpha)), uint8(FMath::Lerp(float(A.G), float(B.G), Alpha)), uint8(FMath::Lerp(float(A.B), float(B.B), Alpha)));
	}

	/** The land's overall tone: the concept sketch's pastels, a little deeper. Every color goes through it. */
	FColor Toned(const FColor& Color)
	{
		FLinearColor HSV = FLinearColor(Color).LinearRGBToHSV();
		HSV.G = FMath::Min(1.f, HSV.G * 1.12f + 0.03f);
		HSV.B *= 0.97f;
		return HSV.HSVToLinearRGB().ToFColor(true);
	}

	/** A pastel a step richer, for the background (the arches, the rim, the sky islands), which the haze washes out. */
	FColor Richer(const FColor& Color)
	{
		FLinearColor HSV = FLinearColor(Color).LinearRGBToHSV();
		HSV.G = FMath::Min(1.f, HSV.G * 1.35f + 0.06f);
		HSV.B *= 0.95f;
		return HSV.HSVToLinearRGB().ToFColor(true);
	}

	/** A pastel's deeper shade, for pips on a die of that color. */
	FColor Deep(const FColor& Color)
	{
		FLinearColor HSV = FLinearColor(Color).LinearRGBToHSV();
		HSV.G = FMath::Min(1.f, HSV.G * 1.25f + 0.08f);
		HSV.B *= 0.82f;
		return HSV.HSVToLinearRGB().ToFColor(true);
	}

	UStaticMesh* LoadMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Environment/DiceLand/%s.%s"), Name, Name));
	}

	UMaterialInterface* LoadMaterial(const TCHAR* Name)
	{
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Environment/DiceLand/%s.%s"), Name, Name));
	}
}

ARRRandomDiceLand::ARRRandomDiceLand()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;

	// The host spawns it; clients build the same land from the same seeds (they need its walls to move)
	bReplicates = true;
	bAlwaysRelevant = true;
}

FBox2D ARRRandomDiceLand::GetMapBounds() const
{
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);
	return FBox2D(Center - FVector2D(MapHalf), Center + FVector2D(MapHalf));
}

FBox ARRRandomDiceLand::GetFlightArea() const
{
	// Over the land, the feet well over its tallest things (the dice trees, the rainbow, the spinning die over the plaza);
	// the rim's tall hidden wall keeps flyers over the island
	const FBox2D Map = GetMapBounds();
	return FBox(FVector(Map.Min, FloorZ - 1000.f), FVector(Map.Max, FloorZ + FlightCeiling));
}

float ARRRandomDiceLand::RimRadius(float Theta) const
{
	return BaseRadius + 170.f * FMath::Sin(3.f * Theta + 1.f) + 110.f * FMath::Sin(5.f * Theta + 2.3f) + 60.f * FMath::Sin(9.f * Theta);
}

FVector ARRRandomDiceLand::At(const FVector2D& Local, float Lift) const
{
	return FVector(GetActorLocation().X + Local.X, GetActorLocation().Y + Local.Y, FloorZ + Lift);
}

bool ARRRandomDiceLand::IsOnRoad(const FVector2D& Local, float Margin) const
{
	const float Half = LaneHalf + Margin;
	const bool bMain = FMath::Abs(Local.Y) < Half;
	const bool bTeamRow = FMath::Abs(FMath::Abs(Local.X) - TeamRowX) < Half && FMath::Abs(Local.Y) < SideLaneY + Half;
	const bool bSideLane = FMath::Abs(FMath::Abs(Local.Y) - SideLaneY) < Half && FMath::Abs(Local.X) < TeamRowX + Half;
	return bMain || bTeamRow || bSideLane;
}

FColor ARRRandomDiceLand::GroundColor(const FVector2D& Local, bool bChecker) const
{
	const float Distance = Local.Size();
	if (Distance < PlazaRadius)
	{
		return bChecker ? FColor(255, 200, 215) : FColor(255, 232, 205);
	}
	if (Distance < PlazaRing)
	{
		return bChecker ? FColor(200, 180, 255) : FColor(214, 198, 255);
	}
	if (IsOnRoad(Local, 0.f))
	{
		return bChecker ? FColor(255, 226, 170) : FColor(255, 214, 150);
	}
	// The woods: outside the side lanes, toward the far end on the left and the near end on the right
	const bool bWood = FMath::Abs(Local.Y) > SideLaneY + LaneHalf && (Local.Y < 0.f ? Local.X > 0.f : Local.X < 0.f);
	if (bWood)
	{
		return bChecker ? FColor(130, 210, 165) : FColor(146, 218, 178);
	}
	return bChecker ? FColor(150, 225, 180) : FColor(168, 232, 194);
}

bool ARRRandomDiceLand::IsClear(const FVector2D& Local, float Radius) const
{
	const float Distance = Local.Size();
	if (Distance + Radius > RimRadius(FMath::Atan2(Local.Y, Local.X)) - RimMargin || Distance < PlazaRing + Radius + 40.f || IsOnRoad(Local, Radius + 30.f))
	{
		return false;
	}
	for (const FVector3f& Other : Taken)
	{
		if (FVector2D::Distance(Local, FVector2D(Other.X, Other.Y)) < Other.Z + Radius + Spacing)
		{
			return false;
		}
	}
	return true;
}

UInstancedStaticMeshComponent* ARRRandomDiceLand::MakeInstances(UStaticMesh* Mesh, bool bSolid, int32 CustomFloats)
{
	UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(this);
	Instances->SetStaticMesh(Mesh);
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

void ARRRandomDiceLand::AddColored(UInstancedStaticMeshComponent* Instances, const FTransform& Transform, const FColor& Color, const FColor& PipColor) const
{
	if (!Instances)
	{
		return;
	}
	// The tint material reads the first three custom floats as a linear color, the pips the next three
	const int32 Index = Instances->AddInstance(Transform);
	const FLinearColor Body(Toned(Color));
	const FLinearColor Pips(PipColor == FColor::White ? PipColor : Toned(PipColor));
	const float Data[6] = { Body.R, Body.G, Body.B, Pips.R, Pips.G, Pips.B };
	Instances->SetCustomData(Index, MakeArrayView(Data, FMath::Min(6, Instances->NumCustomDataFloats)));
}

void ARRRandomDiceLand::AddCover(const FVector2D& Local, float Yaw, const FVector2D& Half, float Height, const FColor& MapColor)
{
	Solid->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), At(Local, Height * 0.5f), FVector(Half.X * 2.f / BasicSize, Half.Y * 2.f / BasicSize, Height / BasicSize)));
	Footprints.Add({ Local, Half, Yaw, MapColor, FMath::Max(1, FMath::RoundToInt(Height / 80.f)) });
	Taken.Add(FVector3f(Local.X, Local.Y, FMath::Max(Half.X, Half.Y)));
}

void ARRRandomDiceLand::AddDie(const FVector2D& Local, float Lift, float Size, const FRotator& Turn, const FColor& Color, const FColor& PipColor, bool bSolid)
{
	const FVector Middle = At(Local, Lift);
	AddColored(Dice, FTransform(Turn, Middle, FVector(Size / BasicSize)), Color, PipColor);
	if (bSolid)
	{
		Solid->AddInstance(FTransform(Turn, Middle, FVector(Size * 0.94f / BasicSize)));
		Footprints.Add({ Local, FVector2D(Size * 0.5f), float(Turn.Yaw), MapCover, 2 });
		Taken.Add(FVector3f(Local.X, Local.Y, Size * 0.6f));
	}
}

void ARRRandomDiceLand::Mirrored(TFunctionRef<void(const FVector2D&, float)> Place, const FVector2D& Local, float Yaw)
{
	Place(Local, Yaw);
	if (!Local.IsNearlyZero())
	{
		Place(-Local, Yaw + 180.f);
	}
}

void ARRRandomDiceLand::AddTree(const FVector2D& Local, float Yaw, float Size, int32 Kind)
{
	// A cream trunk, a little bent at random; dice for leaves: one big one tipped on top, a smaller one stacked over it,
	// or three around the top like a bunch
	const uint32 Hash = GetTypeHash(FIntPoint(FMath::RoundToInt(Local.X), FMath::RoundToInt(Local.Y)));
	FRandomStream Stream(static_cast<int32>(Hash));
	const float Height = (Kind == 2 ? 300.f : 240.f) * Size * Stream.FRandRange(0.9f, 1.1f);
	const float Girth = 0.62f * Size;
	AddColored(Trunks, FTransform(FRotator(Stream.FRandRange(-3.f, 3.f), Yaw, Stream.FRandRange(-3.f, 3.f)), At(Local, -5.f), FVector(Girth, Girth, Height / BasicSize)), FColor(235, 205, 170));
	const float Leaf = 250.f * Size;
	auto Top = [&](const FVector2D& Offset, float Lift, float DieSize)
	{
		const FColor Color = Leaves[Stream.RandRange(0, UE_ARRAY_COUNT(Leaves) - 1)];
		AddColored(Dice, FTransform(FRotator(Stream.FRandRange(-22.f, 22.f), Stream.FRandRange(0.f, 360.f), Stream.FRandRange(-22.f, 22.f)),
			At(Local + Offset, Lift), FVector(DieSize / BasicSize)), Color, FColor::White);
	};
	if (Kind == 2)
	{
		for (int32 Side = 0; Side < 3; ++Side)
		{
			const float Angle = FMath::DegreesToRadians(Yaw + Side * 120.f);
			Top(FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Leaf * 0.36f, Height + Leaf * 0.12f, Leaf * 0.78f);
		}
		Top(FVector2D::ZeroVector, Height + Leaf * 0.7f, Leaf * 0.8f);
	}
	else
	{
		Top(FVector2D::ZeroVector, Height + Leaf * 0.34f, Leaf);
		if (Kind == 1)
		{
			Top(FVector2D(Stream.FRandRange(-20.f, 20.f), Stream.FRandRange(-20.f, 20.f)), Height + Leaf * 0.92f, Leaf * 0.6f);
		}
	}
	// Only the trunk stops things; the leaves are looks only
	Solid->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), At(Local, Height * 0.5f), FVector(Girth * 0.8f, Girth * 0.8f, Height / BasicSize)));
	Footprints.Add({ Local, FVector2D(Leaf * 0.45f), Yaw, MapTree, 5 });
	Taken.Add(FVector3f(Local.X, Local.Y, Leaf * 0.35f));
}

void ARRRandomDiceLand::AddHedge(const FVector2D& Local, float Yaw)
{
	// A row of soft green balls, a few smaller ones on top, and little pink and yellow flowers stuck on
	const FVector2D Along(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	const uint32 Hash = GetTypeHash(FIntPoint(FMath::RoundToInt(Local.X), FMath::RoundToInt(Local.Y)));
	FRandomStream Stream(int32(Hash) ^ 0x5EED);
	const float Step = HedgeBall * 0.6f;
	for (int32 Index = 0; Index < HedgeLength; ++Index)
	{
		const float Size = HedgeBall * Stream.FRandRange(0.9f, 1.12f);
		const FVector2D Spot = Local + Along * (Index - (HedgeLength - 1) * 0.5f) * Step;
		AddColored(Balls, FTransform(FRotator::ZeroRotator, At(Spot, Size * 0.4f), FVector(Size / BasicSize, Size / BasicSize, Size * 0.85f / BasicSize)),
			Greens[Stream.RandRange(0, UE_ARRAY_COUNT(Greens) - 1)]);
		if (Index > 0 && Index < HedgeLength - 1)
		{
			const float Upper = Size * 0.75f;
			AddColored(Balls, FTransform(FRotator::ZeroRotator, At(Spot + Along * Step * 0.5f, Size * 0.85f), FVector(Upper / BasicSize)), Greens[Stream.RandRange(0, UE_ARRAY_COUNT(Greens) - 1)]);
		}
		const FVector2D Side(-Along.Y, Along.X);
		const float Flower = Stream.FRandRange(20.f, 28.f);
		AddColored(Balls, FTransform(FRotator::ZeroRotator, At(Spot + Side * Size * Stream.FRandRange(-0.42f, 0.42f), Size * Stream.FRandRange(0.7f, 1.05f)), FVector(Flower / BasicSize)),
			Stream.FRand() < 0.5f ? FColor(255, 170, 200) : FColor(255, 232, 140));
	}
	const float Length = (HedgeLength - 1) * Step + HedgeBall;
	AddCover(Local, Yaw, FVector2D(Length * 0.45f, HedgeBall * 0.42f), HedgeBall * 1.3f, MapHedge);
}

void ARRRandomDiceLand::AddWall(const FVector2D& Local, float Yaw)
{
	// Two neat rows of dice in turn of the rainbow, each a little askew
	const FVector2D Along(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	const uint32 Hash = GetTypeHash(FIntPoint(FMath::RoundToInt(Local.X), FMath::RoundToInt(Local.Y)));
	FRandomStream Stream(int32(Hash) ^ 0xD1CE);
	const float Step = WallDie * 1.02f;
	int32 Color = Stream.RandRange(0, UE_ARRAY_COUNT(Rainbow) - 1);
	for (int32 Row = 0; Row < 2; ++Row)
	{
		for (int32 Index = 0; Index < WallLength; ++Index)
		{
			const FVector2D Spot = Local + Along * (Index - (WallLength - 1) * 0.5f + Row * 0.12f) * Step;
			const FColor Body = Rainbow[Color++ % UE_ARRAY_COUNT(Rainbow)];
			AddColored(Dice, FTransform(FRotator(Stream.FRandRange(-2.f, 2.f), Yaw + Stream.FRandRange(-6.f, 6.f), Stream.FRandRange(-3.f, 3.f)),
				At(Spot, WallDie * (0.5f + Row * 0.98f)), FVector(WallDie / BasicSize)), Body, FColor::White);
		}
	}
	AddCover(Local, Yaw, FVector2D(WallLength * Step * 0.5f, WallDie * 0.5f), WallDie * 2.f, MapCover);
}

void ARRRandomDiceLand::AddBarricade(const FVector2D& Local, float Yaw)
{
	// Two dice side by side along Yaw and one on top, a little askew
	const FVector2D Along = FVector2D(FMath::Cos(FMath::DegreesToRadians(Yaw)), FMath::Sin(FMath::DegreesToRadians(Yaw)));
	const float Size = 135.f;
	AddDie(Local - Along * Size * 0.52f, Size * 0.5f, Size, FRotator(0.f, Yaw + 4.f, 0.f), Pink, Deep(Pink), true);
	AddDie(Local + Along * Size * 0.52f, Size * 0.5f, Size, FRotator(0.f, Yaw - 6.f, 0.f), Sky, Deep(Sky), true);
	AddDie(Local + Along * 6.f, Size * 1.45f, Size * 0.9f, FRotator(0.f, Yaw + 17.f, 0.f), Lemon, Deep(Lemon), false);
}

void ARRRandomDiceLand::AddBlock(const FVector2D& Local, float Size, float Yaw)
{
	// A big die sunk a third into the ground, tipped a little; white ones get pastel pips, colored ones deeper pips
	const int32 Pick = FMath::Abs(FMath::RoundToInt(Local.X * 7.f + Local.Y * 3.f)) % UE_ARRAY_COUNT(CoverDice);
	const FColor Color = CoverDice[Pick];
	const FColor Pips = Pick == 0 ? Deep(Rainbow[FMath::Abs(FMath::RoundToInt(Local.X + Local.Y)) % UE_ARRAY_COUNT(Rainbow)]) : Deep(Color);
	AddDie(Local, Size * 0.3f, Size, FRotator(4.f, Yaw, -3.f), Color, Pips, true);
}

void ARRRandomDiceLand::AddUnderside(const FVector& Middle, TFunctionRef<float(float)> Radius, int32 LayerCount, float DieSize)
{
	// Peach at the top, pink, lilac at the point; now and then a die shows its pips
	FRandomStream Stream(LayoutSeed + FMath::RoundToInt(Middle.X + Middle.Y));
	for (int32 Layer = 0; Layer <= LayerCount; ++Layer)
	{
		const float Share = 1.f - Layer * (0.86f / LayerCount);
		const float Z = Middle.Z - DieSize * 0.62f - Layer * DieSize * 0.78f;
		const float T = float(Layer) / LayerCount;
		const FColor Color = T < 0.5f ? Blend(UnderTop, UnderMid, T * 2.f) : Blend(UnderMid, UnderBottom, (T - 0.5f) * 2.f);
		for (const float Inset : { DieSize * 0.45f, DieSize * 1.4f })
		{
			const float First = Stream.FRandRange(0.f, 1.f);
			for (float Theta = First; Theta < First + UE_TWO_PI; )
			{
				const float Out = Radius(Theta) * Share - Inset;
				if (Out > DieSize * 0.4f)
				{
					const float Size = DieSize * Stream.FRandRange(0.88f, 1.12f);
					const FVector Spot(Middle.X + FMath::Cos(Theta) * Out, Middle.Y + FMath::Sin(Theta) * Out, Z + Stream.FRandRange(-0.12f, 0.12f) * DieSize);
					const FRotator Turn(Stream.FRandRange(-15.f, 15.f), Stream.FRandRange(0.f, 360.f), Stream.FRandRange(-15.f, 15.f));
					const FColor Tint = Blend(Color, Cream, Stream.FRandRange(0.f, 0.15f));
					const bool bPips = Stream.FRand() < 0.14f;
					AddColored(bPips ? Dice.Get() : BlankDice.Get(), FTransform(Turn, Spot, FVector(Size / BasicSize)), Tint, FColor::White);
				}
				Theta += DieSize * 0.92f / FMath::Max(Out, DieSize);
			}
		}
	}
	const float TipZ = Middle.Z - DieSize * 0.62f - (LayerCount + 1) * DieSize * 0.78f;
	AddColored(Dice, FTransform(FRotator(35.f, 20.f, 30.f), FVector(Middle.X, Middle.Y, TipZ), FVector(DieSize / BasicSize)), UnderBottom, FColor::White);
	AddColored(Dice, FTransform(FRotator(10.f, 50.f, 45.f), FVector(Middle.X + 20.f, Middle.Y - 10.f, TipZ - DieSize * 0.9f), FVector(DieSize * 0.6f / BasicSize)), Richer(Periwinkle), FColor::White);
}

void ARRRandomDiceLand::AddSkyIsland(const FVector& Middle, float Radius, int32 Kind)
{
	// Cushion tiles on a little disc, dice under it, and one thing on top; out of reach, so nothing on it is solid
	for (float X = -Radius; X <= Radius; X += TileSize)
	{
		for (float Y = -Radius; Y <= Radius; Y += TileSize)
		{
			if (FMath::Sqrt(X * X + Y * Y) > Radius - TileSize * 0.4f)
			{
				continue;
			}
			const bool bChecker = (FMath::RoundToInt(X / TileSize) + FMath::RoundToInt(Y / TileSize)) % 2 == 0;
			const FColor Color = Richer(bChecker ? FColor(150, 225, 180) : FColor(168, 232, 194));
			AddColored(Tiles, FTransform(FRotator::ZeroRotator, Middle + FVector(X, Y, 0.5f), FVector(1.f)), Color);
			AddColored(Underlay, FTransform(FRotator::ZeroRotator, Middle + FVector(X, Y, -UnderlayTop - UnderlayThickness * 0.5f),
				FVector(TileSize / BasicSize, TileSize / BasicSize, UnderlayThickness / BasicSize)), Darker(Color, 0.7f));
		}
	}
	AddUnderside(Middle, [Radius](float Theta) { return Radius + 40.f * FMath::Sin(4.f * Theta); }, 3, 150.f);
	switch (Kind)
	{
	case 0:
	{
		// A tree (its trunk box sits out of reach, harmless)
		const float SavedFloor = FloorZ;
		FloorZ = Middle.Z;
		AddTree(FVector2D(Middle.X - GetActorLocation().X, Middle.Y - GetActorLocation().Y), 40.f, 1.1f, 2);
		FloorZ = SavedFloor;
		Footprints.Pop();
		break;
	}
	case 1:
		// A tower of three dice
		AddColored(Dice, FTransform(FRotator(0.f, 10.f, 0.f), Middle + FVector(0.f, 0.f, 90.f), FVector(1.8f)), Richer(Lavender), FColor::White);
		AddColored(Dice, FTransform(FRotator(0.f, 35.f, 0.f), Middle + FVector(0.f, 0.f, 180.f + 70.f), FVector(1.4f)), Richer(Sky), FColor::White);
		AddColored(Dice, FTransform(FRotator(0.f, 70.f, 0.f), Middle + FVector(0.f, 0.f, 320.f + 52.f), FVector(1.05f)), Richer(Pink), FColor::White);
		break;
	default:
		for (int32 Index = 0; Index < 5; ++Index)
		{
			const float Angle = Index * 1.3f;
			const float Size = 120.f - Index * 12.f;
			AddColored(Balls, FTransform(FRotator::ZeroRotator, Middle + FVector(FMath::Cos(Angle) * 140.f, FMath::Sin(Angle) * 140.f, Size * 0.4f), FVector(Size / BasicSize)), Richer(Greens[Index % 3]));
		}
		AddColored(Dice, FTransform(FRotator(8.f, 25.f, -5.f), Middle + FVector(-60.f, 40.f, 60.f), FVector(1.2f)), Richer(Peach), Deep(Peach));
		break;
	}
}

void ARRRandomDiceLand::AddRainbowArch(const FVector& Base, const FVector& Across, float Radius, float DieSize)
{
	// Seven bands, pink outside to lavender inside, each a half circle of dice rolled along it; the feet go into the clouds
	FRandomStream Stream(SkySeed + FMath::RoundToInt(Base.X));
	for (int32 Band = 0; Band < UE_ARRAY_COUNT(Rainbow); ++Band)
	{
		const float Bend = Radius - Band * DieSize * 1.04f;
		const int32 Count = FMath::RoundToInt(PI * Bend / (DieSize * 1.02f));
		for (int32 Index = 0; Index <= Count; ++Index)
		{
			const float Angle = PI * Index / Count;
			const FVector Spot = Base + Across * (FMath::Cos(Angle) * Bend) + FVector::UpVector * (FMath::Sin(Angle) * Bend);
			if (Spot.Z < FloorZ - 1500.f)
			{
				continue;
			}
			const FQuat Roll(Across ^ FVector::UpVector, -Angle);
			const FQuat Jitter(FRotator(Stream.FRandRange(-12.f, 12.f), Stream.FRandRange(-12.f, 12.f), Stream.FRandRange(-12.f, 12.f)));
			AddColored(Dice, FTransform(Roll * Jitter, Spot, FVector(DieSize * Stream.FRandRange(0.92f, 1.04f) / BasicSize)), Richer(Rainbow[Band]), FColor::White);
		}
	}
}

void ARRRandomDiceLand::AddCloud(const FVector& Middle, float Size, float Flatten, FRandomStream& Stream)
{
	// A big ball in the middle and smaller ones heaped round it, flattened, all soft white with a hint of color
	const FColor Tint = Puffs[Stream.RandRange(0, UE_ARRAY_COUNT(Puffs) - 1)];
	const int32 Count = Stream.RandRange(4, 7);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Share = Index == 0 ? 1.f : Stream.FRandRange(0.5f, 0.8f);
		const float Angle = Stream.FRandRange(0.f, UE_TWO_PI);
		const FVector Offset = Index == 0 ? FVector::ZeroVector
			: FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Size * Stream.FRandRange(0.35f, 0.6f) + FVector(0.f, 0.f, -Size * Flatten * Stream.FRandRange(0.05f, 0.2f));
		const float Puff = Size * Share;
		AddColored(Clouds, FTransform(FRotator::ZeroRotator, Middle + Offset, FVector(Puff / BasicSize, Puff / BasicSize, Puff * Flatten / BasicSize)), Blend(Tint, FColor::White, Stream.FRandRange(0.f, 0.4f)));
	}
}

void ARRRandomDiceLand::BeginPlay()
{
	Super::BeginPlay();
	Build();
	bBuilt = true;
}

void ARRRandomDiceLand::Build()
{
	UWorld* World = GetWorld();
	// Loaded here rather than in the constructor, so the editor's tools still run before the assets are imported. Made by
	// char-pipeline/scripts/blender_dice_land.py and ue_import_dice_land.py (smooth shapes, tinted per instance)
	Die = LoadMesh(TEXT("SM_DLDie"));
	BlankDie = LoadMesh(TEXT("SM_DLDieBlank"));
	RainbowDie = LoadMesh(TEXT("SM_DLRainbowDie"));
	Tile = LoadMesh(TEXT("SM_DLTile"));
	Ball = LoadMesh(TEXT("SM_DLBall"));
	Trunk = LoadMesh(TEXT("SM_DLTrunk"));
	Ring = LoadMesh(TEXT("SM_DLRing"));
	Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	TileMaterial = LoadMaterial(TEXT("MI_DLTile"));
	CloudMaterial = LoadMaterial(TEXT("MI_DLCloud"));
	CandyMaterial = LoadMaterial(TEXT("M_DLCandy"));
	if (!Die || !BlankDie || !Tile || !Ball || !Trunk || !Cube)
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: dice land meshes missing (/Game/Environment/DiceLand)."));
		return;
	}

	// The floor under the actor: the land stands on it, and hides it (the tiles are the ground's looks; it stays solid)
	FHitResult Hit;
	const FVector Start = GetActorLocation() + FVector(0.f, 0.f, FloorSearchUp);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomDiceLand), false, this);
	if (!World->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, FloorSearchUp + FloorSearchDown), ECC_Visibility, Params) || !Hit.GetComponent())
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: no floor under the dice land at %s."), *GetActorLocation().ToString());
		return;
	}
	FloorZ = Hit.ImpactPoint.Z;
	Hit.GetComponent()->SetVisibility(false);
	const FVector2D Center(GetActorLocation().X, GetActorLocation().Y);

	Dice = MakeInstances(Die, false, 6);
	BlankDice = MakeInstances(BlankDie, false, 3);
	Tiles = MakeInstances(Tile, false, 3);
	Tiles->CastShadow = false;
	Underlay = MakeInstances(Cube, false, 3);
	Underlay->CastShadow = false;
	if (TileMaterial)
	{
		Underlay->SetMaterial(0, TileMaterial);
	}
	Balls = MakeInstances(Ball, false, 3);
	Trunks = MakeInstances(Trunk, false, 3);
	Clouds = MakeInstances(Ball, false, 3);
	Clouds->CastShadow = false;
	if (CloudMaterial)
	{
		Clouds->SetMaterial(0, CloudMaterial);
	}
	// Cover, trunks and props: hidden boxes, solid like cover
	Solid = MakeInstances(Cube, true, 0);
	Solid->SetVisibility(false);
	Solid->SetHiddenInGame(true);
	// The rim: a hidden wall that stops brawlers only; shots and the bots' sight (world static sweeps) pass
	RimWalls = MakeInstances(Cube, false, 0);
	RimWalls->SetVisibility(false);
	RimWalls->SetHiddenInGame(true);
	RimWalls->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	RimWalls->SetCollisionObjectType(ECC_WorldDynamic);
	RimWalls->SetCollisionResponseToAllChannels(ECR_Ignore);
	RimWalls->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Floaters = RainbowDie ? MakeInstances(RainbowDie, false, 0) : nullptr;

	// Ground: a cushion tile per cell on the island (checkered, now and then a lime, lemon or pink one on the grass), and
	// a darker slab under each so the seams between the puffy tiles read as grout and not as holes; flowers on the grass
	FRandomStream Ground(GroundSeed);
	int32 TileCount = 0;
	for (float X = -MapHalf; X <= MapHalf; X += TileSize)
	{
		for (float Y = -MapHalf; Y <= MapHalf; Y += TileSize)
		{
			const FVector2D Local(X, Y);
			if (Local.Size() > RimRadius(FMath::Atan2(Y, X)) - TileSize * 0.35f)
			{
				continue;
			}
			const bool bChecker = (FMath::RoundToInt(X / TileSize) + FMath::RoundToInt(Y / TileSize)) % 2 == 0;
			FColor Color = GroundColor(Local, bChecker);
			const bool bGrass = Local.Size() >= PlazaRing && !IsOnRoad(Local, 0.f);
			const float Roll = Ground.FRand();
			if (bGrass && Roll < 0.06f)
			{
				Color = FColor(200, 235, 140);
			}
			else if (bGrass && Roll < 0.085f)
			{
				Color = FColor(255, 228, 150);
			}
			else if (bGrass && Roll < 0.11f)
			{
				Color = FColor(255, 185, 210);
			}
			AddColored(Tiles, FTransform(FRotator(0.f, 90.f * Ground.RandRange(0, 3), 0.f), At(Local, 0.5f), FVector(1.f)), Color);
			AddColored(Underlay, FTransform(FRotator::ZeroRotator, At(Local, -UnderlayTop - UnderlayThickness * 0.5f),
				FVector(TileSize / BasicSize, TileSize / BasicSize, UnderlayThickness / BasicSize)), Darker(Color, 0.7f));
			++TileCount;
		}
	}

	// The rim: dice under it, a low row of little dice along it, and the hidden wall
	AddUnderside(FVector(Center, FloorZ), [this](float Theta) { return RimRadius(Theta); }, 7, 230.f);
	for (float Theta = 0.f; Theta < UE_TWO_PI; )
	{
		const float Out = RimRadius(Theta) - 110.f;
		const float Size = Ground.FRandRange(68.f, 92.f);
		const FVector Spot(Center.X + FMath::Cos(Theta) * Out, Center.Y + FMath::Sin(Theta) * Out, FloorZ + Size * 0.45f);
		AddColored(Dice, FTransform(FRotator(Ground.FRandRange(-6.f, 6.f), Ground.FRandRange(0.f, 360.f), Ground.FRandRange(-6.f, 6.f)), Spot, FVector(Size / BasicSize)),
			Richer(Rainbow[Ground.RandRange(0, UE_ARRAY_COUNT(Rainbow) - 1)]), FColor::White);
		Theta += (Size + 30.f) / Out;
	}
	for (int32 Segment = 0; Segment < RimWallSegments; ++Segment)
	{
		const float Theta = UE_TWO_PI * Segment / RimWallSegments;
		const float Out = RimRadius(Theta) - 40.f;
		const float Length = UE_TWO_PI * (Out + 100.f) / RimWallSegments * 1.15f;
		RimWalls->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Theta), 0.f),
			FVector(Center.X + FMath::Cos(Theta) * (Out + 50.f), Center.Y + FMath::Sin(Theta) * (Out + 50.f), FloorZ + RimWallHeight * 0.5f - 200.f),
			FVector(1.f, Length / BasicSize, RimWallHeight / BasicSize)));
	}

	// The plaza: two rounded slabs, a lilac one turned a quarter and a pink one on it, the rainbow die spinning over them in
	// two halos, and a ring of little dice with gaps for the roads
	const FVector2D Zero = FVector2D::ZeroVector;
	AddColored(BlankDice, FTransform(FRotator(0.f, 45.f, 0.f), At(Zero, 55.f), FVector(5.f, 5.f, 1.1f)), FColor(190, 165, 255));
	AddColored(BlankDice, FTransform(FRotator(0.f, 45.f, 0.f), At(Zero, 150.f), FVector(3.5f, 3.5f, 0.9f)), FColor(255, 165, 200));
	AddCover(Zero, 45.f, FVector2D(250.f), 195.f, MapPedestal);
	const float PedestalTop = 195.f;
	if (RainbowDie)
	{
		Centerpiece = NewObject<UStaticMeshComponent>(this);
		Centerpiece->SetStaticMesh(RainbowDie);
		Centerpiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Centerpiece->SetMobility(EComponentMobility::Movable);
		Centerpiece->SetupAttachment(RootComponent);
		Centerpiece->RegisterComponent();
		Centerpiece->SetWorldLocation(At(Zero, PedestalTop + 300.f));
		Centerpiece->SetWorldScale3D(FVector(2.4f));
	}
	if (Ring && CandyMaterial)
	{
		const FColor HaloColors[] = { FColor::White, FColor(226, 194, 255) };
		for (int32 Index = 0; Index < 2; ++Index)
		{
			UStaticMeshComponent* Halo = NewObject<UStaticMeshComponent>(this);
			Halo->SetStaticMesh(Ring);
			Halo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Halo->SetMobility(EComponentMobility::Movable);
			Halo->SetupAttachment(RootComponent);
			Halo->RegisterComponent();
			Halo->SetWorldLocation(At(Zero, PedestalTop + 300.f));
			Halo->SetWorldScale3D(FVector(Index == 0 ? 6.4f : 7.2f));
			UMaterialInstanceDynamic* Tint = UMaterialInstanceDynamic::Create(CandyMaterial, Halo);
			Tint->SetVectorParameterValue(TEXT("Tint"), FLinearColor(HaloColors[Index]));
			Tint->SetScalarParameterValue(TEXT("Glow"), 0.3f);
			Halo->SetMaterial(0, Tint);
			Halos.Add(Halo);
		}
	}
	for (int32 Post = 0; Post < 28; ++Post)
	{
		const float Theta = UE_TWO_PI * Post / 28.f;
		const FVector2D Local(FMath::Cos(Theta) * (PlazaRing - 40.f), FMath::Sin(Theta) * (PlazaRing - 40.f));
		if (IsOnRoad(Local, 40.f))
		{
			continue;
		}
		const float Size = 62.f + 10.f * (Post % 3);
		AddDie(Local, Size * 0.45f, Size, FRotator(0.f, Post * 23.f, 0.f), Rainbow[Post % UE_ARRAY_COUNT(Rainbow)], FColor::White, false);
		Solid->AddInstance(FTransform(FRotator::ZeroRotator, At(Local, Size * 0.45f), FVector(Size * 0.9f / BasicSize)));
	}

	// The field inside the ring road: cover for both teams, placed by hand on blue's half and turned round for red's
	// (X up the screen toward red, Y to the right; yaw 0 runs along X)
	Mirrored([this](const FVector2D& L, float Yaw) { AddHedge(L, Yaw); }, FVector2D(-1100.f, -1150.f), 0.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddHedge(L, Yaw); }, FVector2D(-1100.f, 1150.f), 0.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddWall(L, Yaw); }, FVector2D(-800.f, -800.f), 90.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddWall(L, Yaw); }, FVector2D(-800.f, 800.f), 90.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddWall(L, Yaw); }, FVector2D(-450.f, -1300.f), 0.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddWall(L, Yaw); }, FVector2D(-450.f, 1300.f), 0.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddWall(L, Yaw); }, FVector2D(0.f, -1100.f), 0.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddBlock(L, 190.f, Yaw); }, FVector2D(-1050.f, -700.f), 20.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddBlock(L, 190.f, Yaw); }, FVector2D(-1050.f, 700.f), -15.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddBlock(L, 160.f, Yaw); }, FVector2D(-200.f, -1500.f), 35.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddBlock(L, 160.f, Yaw); }, FVector2D(-200.f, 1500.f), 10.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddBarricade(L, Yaw); }, FVector2D(-850.f, -330.f), 0.f);
	Mirrored([this](const FVector2D& L, float Yaw) { AddBarricade(L, Yaw); }, FVector2D(-850.f, 330.f), 0.f);

	// Outside the side lanes and behind the starting rows, picked at random on blue's side of the middle and turned round
	FRandomStream Layout(LayoutSeed);
	auto Scatter = [&](int32 Count, const FBox2D& Area, float Radius, TFunctionRef<void(const FVector2D&, float)> Place)
	{
		for (int32 Try = 0, Placed = 0; Try < Count * 60 && Placed < Count; ++Try)
		{
			const FVector2D Local(Layout.FRandRange(Area.Min.X, Area.Max.X), Layout.FRandRange(Area.Min.Y, Area.Max.Y));
			if (IsClear(Local, Radius) && IsClear(-Local, Radius))
			{
				const float Yaw = Layout.FRandRange(0.f, 360.f);
				Mirrored(Place, Local, Yaw);
				++Placed;
			}
		}
	};
	const float Outside = SideLaneY + LaneHalf;
	auto Tree = [this, &Layout](const FVector2D& L, float Yaw) { AddTree(L, Yaw, Layout.FRandRange(0.85f, 1.15f), Layout.RandRange(0, 2)); };
	// The wood: left side, far end (and right side, near end)
	const FBox2D Wood(FVector2D(0.f, -MapHalf), FVector2D(MapHalf, -Outside));
	Scatter(4, Wood, 190.f, [this](const FVector2D& L, float Yaw) { AddHedge(L, Yaw); });
	Scatter(16, Wood, 105.f, Tree);
	Scatter(3, Wood, 120.f, [this, &Layout](const FVector2D& L, float Yaw) { AddBlock(L, Layout.FRandRange(150.f, 200.f), Yaw); });
	// The giant dice: left side, near end (and right side, far end)
	const FBox2D Giants(FVector2D(-MapHalf, -MapHalf), FVector2D(0.f, -Outside));
	Scatter(7, Giants, 200.f, [this, &Layout](const FVector2D& L, float Yaw) { AddBlock(L, Layout.FRandRange(230.f, 330.f), Yaw); });
	Scatter(2, Giants, 170.f, [this](const FVector2D& L, float Yaw) { AddBarricade(L, Yaw); });
	Scatter(2, Giants, 105.f, Tree);
	// Behind blue's starting row (and red's)
	const FBox2D Backyard(FVector2D(-MapHalf, -Outside), FVector2D(-TeamRowX - LaneHalf, Outside));
	Scatter(3, Backyard, 105.f, Tree);
	Scatter(3, Backyard, 190.f, [this](const FVector2D& L, float Yaw) { AddHedge(L, Yaw); });
	Scatter(3, Backyard, 100.f, [this, &Layout](const FVector2D& L, float Yaw) { AddBlock(L, Layout.FRandRange(120.f, 170.f), Yaw); });
	// Small bushes and flowers on the grass, looks only
	Scatter(10, FBox2D(FVector2D(-MapHalf), FVector2D(MapHalf, 0.f)), 60.f, [this, &Layout](const FVector2D& L, float Yaw)
	{
		const int32 Count = Layout.RandRange(1, 3);
		const FColor Color = Layout.FRand() < 0.75f ? Greens[Layout.RandRange(0, 2)] : Leaves[Layout.RandRange(0, UE_ARRAY_COUNT(Leaves) - 1)];
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Size = Layout.FRandRange(55.f, 95.f);
			const FVector2D Spot = L + FVector2D(Layout.FRandRange(-50.f, 50.f), Layout.FRandRange(-50.f, 50.f));
			AddColored(Balls, FTransform(FRotator::ZeroRotator, At(Spot, Size * 0.38f), FVector(Size / BasicSize, Size / BasicSize, Size * 0.85f / BasicSize)), Color);
		}
	});
	const FColor Petals[] = { FColor::White, FColor(255, 170, 200), FColor(255, 232, 140), FColor(221, 189, 255), FColor(255, 196, 160) };
	for (int32 Try = 0, Placed = 0; Try < 3000 && Placed < 220; ++Try)
	{
		const float Theta = Ground.FRandRange(0.f, UE_TWO_PI);
		const FVector2D Local = FVector2D(FMath::Cos(Theta), FMath::Sin(Theta)) * Ground.FRandRange(PlazaRing + 100.f, BaseRadius);
		if (Local.Size() > RimRadius(Theta) - 150.f || IsOnRoad(Local, 10.f) || !IsClear(Local, 5.f))
		{
			continue;
		}
		const float Size = Ground.FRandRange(14.f, 20.f);
		AddColored(Balls, FTransform(FRotator::ZeroRotator, At(Local, Size * 0.6f), FVector(Size / BasicSize)), Petals[Ground.RandRange(0, UE_ARRAY_COUNT(Petals) - 1)]);
		++Placed;
	}

	// Sky: a sea of ball clouds under and all round the island (bigger and higher the further out, so far off their tops
	// close the horizon while near the island they stay well under its rim), cumulus up high, an arch of rainbow dice
	// beyond each end, rainbow dice bobbing over the field, and three little islands out of reach
	FRandomStream Air(SkySeed);
	for (int32 Index = 0; Index < 170; ++Index)
	{
		const float Theta = Air.FRandRange(0.f, UE_TWO_PI);
		const float Far = FMath::Sqrt(Air.FRand());
		const float Out = FMath::Lerp(3000.f, 32000.f, Far);
		const float Size = FMath::Lerp(1300.f, 6500.f, Far) * Air.FRandRange(0.8f, 1.2f);
		const float Top = FloorZ + FMath::Lerp(-1300.f, 650.f, FMath::Square(Far));
		AddCloud(FVector(Center.X + FMath::Cos(Theta) * Out, Center.Y + FMath::Sin(Theta) * Out, Top - Size * 0.25f), Size, 0.5f, Air);
	}
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const float Theta = Air.FRandRange(0.f, UE_TWO_PI);
		const float Out = Air.FRandRange(7000.f, 15000.f);
		AddCloud(FVector(Center.X + FMath::Cos(Theta) * Out, Center.Y + FMath::Sin(Theta) * Out, FloorZ + Air.FRandRange(2200.f, 4500.f)), Air.FRandRange(500.f, 900.f), 0.78f, Air);
	}
	AddRainbowArch(FVector(Center.X + 14000.f, Center.Y - 1500.f, FloorZ - 1800.f), FVector(0.f, 1.f, 0.f), 7600.f, 300.f);
	AddRainbowArch(FVector(Center.X - 15000.f, Center.Y + 2500.f, FloorZ - 1800.f), FVector(0.f, -1.f, 0.f), 6800.f, 280.f);
	if (Floaters)
	{
		Floaters->SetMobility(EComponentMobility::Movable);
		for (int32 Index = 0; Index < 26; ++Index)
		{
			const float Theta = Air.FRandRange(0.f, UE_TWO_PI);
			const float Out = Air.FRandRange(500.f, 2700.f);
			const FTransform Rest(FRotator(Air.FRandRange(0.f, 360.f), Air.FRandRange(0.f, 360.f), Air.FRandRange(0.f, 360.f)),
				FVector(Center.X + FMath::Cos(Theta) * Out, Center.Y + FMath::Sin(Theta) * Out, FloorZ + Air.FRandRange(650.f, 1500.f)), FVector(Air.FRandRange(0.45f, 0.9f)));
			Floaters->AddInstance(Rest);
			FloaterRest.Add(Rest);
			FloaterMotion.Add(FVector4f(Air.FRandRange(30.f, 70.f), Air.FRandRange(0.5f, 1.f), Air.FRandRange(10.f, 40.f) * (Air.FRand() < 0.5f ? -1.f : 1.f), Air.FRandRange(0.f, UE_TWO_PI)));
		}
	}
	AddSkyIsland(FVector(Center.X + 3200.f, Center.Y + 3900.f, FloorZ + 450.f), 560.f, 1);
	AddSkyIsland(FVector(Center.X - 1500.f, Center.Y - 5000.f, FloorZ + 900.f), 480.f, 0);
	AddSkyIsland(FVector(Center.X - 4300.f, Center.Y + 3300.f, FloorZ + 250.f), 420.f, 2);

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

	UE_LOG(LogTemp, Log, TEXT("RRRandom: dice land %d tiles, %d dice, %d balls, %d cloud balls, %d trunks, %d solid boxes."), TileCount,
		Dice->GetInstanceCount() + BlankDice->GetInstanceCount(), Balls->GetInstanceCount(), Clouds->GetInstanceCount(), Trunks->GetInstanceCount(), Solid->GetInstanceCount());
}

void ARRRandomDiceLand::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}
	Clock += DeltaSeconds;
	// The brawlers' fur in the land's softer palette (the fox material's "Pastel", made by
	// char-pipeline/scripts/ue_fox_pastel.py); now and then, for brawlers who joined since
	PastelTimer -= DeltaSeconds;
	if (PastelTimer <= 0.f)
	{
		PastelTimer = 0.25f;
		for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
		{
			USkeletalMeshComponent* Body = It->GetMesh();
			for (int32 Slot = 0; Body && Slot < Body->GetNumMaterials(); ++Slot)
			{
				if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Body->GetMaterial(Slot)))
				{
					Material->SetScalarParameterValue(TEXT("Pastel"), 1.f);
					Material->SetScalarParameterValue(TEXT("PastelDesaturate"), CVarFoxDesaturate.GetValueOnGameThread());
					Material->SetScalarParameterValue(TEXT("PastelLift"), CVarFoxCream.GetValueOnGameThread());
				}
			}
		}
	}
	// The map's colors (every Dice Land material's "Saturation" and "Brightness"), when the console values change
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
	if (Centerpiece)
	{
		Centerpiece->SetWorldRotation(FRotator(25.f + 12.f * FMath::Sin(Clock * 0.7f), Clock * 35.f, 35.f));
		const FVector Bob = Centerpiece->GetComponentLocation();
		Centerpiece->SetWorldLocation(FVector(Bob.X, Bob.Y, FloorZ + 495.f + 25.f * FMath::Sin(Clock * 1.1f)));
	}
	for (int32 Index = 0; Index < Halos.Num(); ++Index)
	{
		// Tilted rings turning opposite ways round the die
		Halos[Index]->SetWorldRotation(FRotator(Index == 0 ? 70.f : 110.f, Clock * (Index == 0 ? 40.f : -30.f), Index == 0 ? 0.f : 30.f));
	}
	if (Floaters && FloaterRest.Num() > 0)
	{
		TArray<FTransform> Moved;
		Moved.Reserve(FloaterRest.Num());
		for (int32 Index = 0; Index < FloaterRest.Num(); ++Index)
		{
			const FVector4f& Motion = FloaterMotion[Index];
			FTransform Now = FloaterRest[Index];
			Now.AddToTranslation(FVector(0.f, 0.f, Motion.X * FMath::Sin(Clock * Motion.Y + Motion.W)));
			Now.SetRotation(FQuat(FRotator(Clock * Motion.Z * 0.6f, Clock * Motion.Z, 0.f)) * Now.GetRotation());
			Moved.Add(Now);
		}
		Floaters->BatchUpdateInstancesTransforms(0, Moved, false, true, false);
	}
}

FColor ARRRandomDiceLand::GetMapColor(const FVector2D& WorldLocation, int32& OutLevels) const
{
	const FVector2D Local = WorldLocation - FVector2D(GetActorLocation().X, GetActorLocation().Y);
	if (Local.Size() > RimRadius(FMath::Atan2(Local.Y, Local.X)) - TileSize * 0.35f)
	{
		OutLevels = -1;
		return MapSky;
	}
	for (const FFootprint& Print : Footprints)
	{
		const FVector2D Offset = (Local - Print.Center).GetRotated(-Print.Yaw);
		if (FMath::Abs(Offset.X) <= Print.Half.X && FMath::Abs(Offset.Y) <= Print.Half.Y)
		{
			OutLevels = Print.Levels;
			return Print.Color;
		}
	}
	OutLevels = 0;
	return GroundColor(Local, true);
}
