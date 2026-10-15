// ARRRandomHUD, continued: the start countdown, the teammates' cards and the minimap.
#include "RRRandomHUD.h"
#include "RRRandomCharacter.h"
#include "RRRandomCover.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomDiceLand.h"
#include "RRRandomFogOfWar.h"
#include "RRRandomGameState.h"
#include "RRRandomIsland.h"
#include "RRRandomPlayerController.h"
#include "RRRandomWeapon.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "GlobalRenderResources.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Shared with the rest of the HUD (RRRandomHUD.cpp): screen margin, and the colors for me, my team and the others
	constexpr float PanelMargin = 20.f;
	const FLinearColor SelfColor(0.3f, 1.f, 0.3f);
	const FLinearColor AllyColor(0.25f, 0.6f, 1.f);
	const FLinearColor EnemyColor(1.f, 0.25f, 0.2f);
	const FLinearColor DiceColor(1.f, 0.85f, 0.1f);
	const FLinearColor GiantColor(0.45f, 0.85f, 1.f);
	const FLinearColor FlyColor(1.f, 0.55f, 0.85f);
	const FLinearColor ReloadColor(1.f, 0.55f, 0.1f);
	const FLinearColor CardBacking(0.f, 0.f, 0.f, 0.55f);
	const FLinearColor GoColor = FLinearColor::FromSRGBColor(FColor(255, 196, 56));

	// Countdown: the number for each second pops in this much bigger, and GO stays this long after the start
	constexpr float CountdownPop = 0.6f;
	constexpr float GoTime = 1.f;

	// Team cards
	constexpr float CardWidth = 270.f;
	constexpr float CardHeight = 62.f;
	constexpr float CardGap = 6.f;
	constexpr float PortraitSize = 54.f;
	constexpr float CardBarWidth = 150.f;
	constexpr float CardBarHeight = 9.f;
	// Room left at the bottom right for the ammo panel (gun, hint and rounds)
	constexpr float AmmoPanelRoom = 96.f;

	// Minimap: texture pixels per side, how often the fog shading is redone, the full-screen map's share of the screen,
	// how dark never-seen and not-seen-now places are, and the marker size on the corner map (scaled up full screen)
	constexpr int32 MapPixels = 288;
	constexpr float MapRefreshInterval = 0.12f;
	constexpr float ExpandedShare = 0.86f;
	constexpr float UnexploredShade = 0.22f;
	constexpr float ExploredShade = 0.55f;
	constexpr float MarkerRadius = 4.5f;
	// See-through: the corner map shows the game under it; the full-screen one, over a dimmed game, is more solid.
	// Framed by a thin light line.
	constexpr float MinimapOpacity = 0.7f;
	constexpr float ExpandedMapOpacity = 0.9f;
	const FLinearColor MapFrameColor(1.f, 1.f, 1.f, 0.45f);
	constexpr float MapFrameWidth = 1.f;
	// Map colors (sRGB): the island's cells, cover, and woods (cells of pines and round trees)
	const FColor WaterColor(70, 168, 205);
	const FColor SandColor(226, 205, 150);
	const FColor DockColor(150, 104, 58);
	const FColor FloorColor(92, 158, 58);
	const FColor FallsColor(175, 228, 245);
	const FColor CoverColor(48, 98, 38);
	const FColor WoodColor(34, 92, 44);
	// Houses, the windmill, crates, toy blocks and giant mushrooms (the solid props): a red-roofed patch
	const FColor BuildingColor(196, 72, 58);
	// A woods cell's trees cover this share of it, and a face down the screen (toward the camera) is shaded this dark
	constexpr float WoodShare = 0.42f;
	constexpr float CliffShade = 0.62f;

	FLinearColor Faded(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	FLinearColor TeamTint(int32 Team)
	{
		return FMath::Lerp(ARRRandomCharacter::GetTeamColor(Team), FLinearColor::White, 0.35f);
	}

	FColor Shaded(const FColor& Color, float Factor)
	{
		return FColor(FMath::Clamp(FMath::RoundToInt(Color.R * Factor), 0, 255), FMath::Clamp(FMath::RoundToInt(Color.G * Factor), 0, 255),
			FMath::Clamp(FMath::RoundToInt(Color.B * Factor), 0, 255), 255);
	}

	/** Higher hills are lighter and a little yellower, so the terraces read on the map. */
	FColor HillColor(int32 Levels)
	{
		const float T = FMath::Clamp(Levels / 9.f, 0.f, 1.f);
		return FColor(FMath::RoundToInt(FMath::Lerp(104.f, 196.f, T)), FMath::RoundToInt(FMath::Lerp(172.f, 208.f, T)), FMath::RoundToInt(FMath::Lerp(64.f, 104.f, T)), 255);
	}

	/** The name on a brawler's card, as on the results screen: YOU, a player's name, or BOT n within its team by name. */
	FString CardLabel(const UWorld* World, const ARRRandomCharacter& Brawler, const ARRRandomCharacter& Viewer)
	{
		if (&Brawler == &Viewer)
		{
			return TEXT("YOU");
		}
		if (const APlayerState* Player = Brawler.GetPlayerState())
		{
			return Player->GetPlayerName().Left(14).ToUpper();
		}
		TArray<FString> Bots;
		for (TActorIterator<ARRRandomCharacter> It(World); It; ++It)
		{
			if (It->GetTeam() == Brawler.GetTeam() && !It->GetPlayerState() && *It != &Viewer && !It->IsDecoy())
			{
				Bots.Add(It->GetName());
			}
		}
		Bots.Sort();
		return FString::Printf(TEXT("BOT %d"), Bots.IndexOfByKey(Brawler.GetName()) + 1);
	}

	/** A filled circle as a fan of triangles. */
	void DrawDisc(UCanvas* Canvas, const FVector2D& Center, float Radius, const FLinearColor& Color)
	{
		constexpr int32 Sides = 12;
		for (int32 Side = 0; Side < Sides; ++Side)
		{
			const float A = 2.f * PI * Side / Sides;
			const float B = 2.f * PI * (Side + 1) / Sides;
			FCanvasTriangleItem Slice(Center, Center + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius, Center + FVector2D(FMath::Cos(B), FMath::Sin(B)) * Radius, GWhiteTexture);
			Slice.SetColor(Color);
			Slice.BlendMode = SE_BLEND_Translucent;
			Canvas->DrawItem(Slice);
		}
	}

	void DrawTriangle(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
	{
		FCanvasTriangleItem Item(A, B, C, GWhiteTexture);
		Item.SetColor(Color);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
}

ARRRandomHUD::ARRRandomHUD()
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> FoxPortrait(TEXT("/Game/UI/T_FoxPortrait.T_FoxPortrait"));
	Portrait = FoxPortrait.Succeeded() ? FoxPortrait.Object : nullptr;
	static ConstructorHelpers::FObjectFinder<UTexture2D> HitEffectSheet(TEXT("/Game/UI/T_HitEffects.T_HitEffects"));
	HitEffects = HitEffectSheet.Succeeded() ? HitEffectSheet.Object : nullptr;
}

void ARRRandomHUD::DrawCountdown(const ARRRandomGameState& State)
{
	const float Remaining = State.GetCountdownRemaining();
	const float S = Canvas->ClipY / 720.f;
	UFont* Large = GEngine->GetLargeFont();
	const float Y = Canvas->ClipY * 0.3f;
	if (Remaining > 0.f)
	{
		// The number pops in at the start of each second and settles
		const int32 Number = FMath::CeilToInt(Remaining);
		const float Fresh = Remaining - (Number - 1);
		const float Pop = 1.f + CountdownPop * FMath::Pow(Fresh, 4.f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.3f), 0.f, Y - 40.f * S, Canvas->ClipX, 160.f * S);
		DrawCenteredText(TEXT("GET READY"), FLinearColor::White, Y - 30.f * S, GEngine->GetMediumFont(), 1.2f * S);
		DrawCenteredText(FString::FromInt(Number), GoColor, Y, Large, 5.f * S * Pop);
	}
	else if (-Remaining < GoTime)
	{
		const float T = -Remaining / GoTime;
		DrawCenteredText(TEXT("GO!"), Faded(GoColor, 1.f - T * T), Y, Large, 5.f * S * (1.f + 0.4f * T));
	}
}

void ARRRandomHUD::DrawTeamPanel(const ARRRandomCharacter& Viewer)
{
	TArray<const ARRRandomCharacter*> Mates;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (*It != &Viewer && It->GetTeam() == Viewer.GetTeam() && !It->IsDecoy())
		{
			Mates.Add(*It);
		}
	}
	Mates.Sort([](const ARRRandomCharacter& A, const ARRRandomCharacter& B) { return A.GetName() < B.GetName(); });

	// Stacked upward from over the ammo panel, the first teammate on top
	const float Left = Canvas->ClipX - PanelMargin - CardWidth;
	float Top = Canvas->ClipY - PanelMargin - AmmoPanelRoom - Mates.Num() * (CardHeight + CardGap) + CardGap;
	for (const ARRRandomCharacter* Mate : Mates)
	{
		DrawTeamCard(*Mate, Viewer, Left, Top);
		Top += CardHeight + CardGap;
	}
}

void ARRRandomHUD::DrawTeamCard(const ARRRandomCharacter& Mate, const ARRRandomCharacter& Viewer, float Left, float Top)
{
	UFont* Small = GEngine->GetSmallFont();
	const bool bAlive = Mate.IsAlive();
	const FLinearColor Tint = TeamTint(Mate.GetTeam());
	DrawRect(CardBacking, Left, Top, CardWidth, CardHeight);
	DrawRect(Tint, Left, Top, 3.f, CardHeight);

	// Portrait in a team colored frame, greyed with KO over it while knocked out
	const float PortraitX = Left + 8.f;
	const float PortraitY = Top + (CardHeight - PortraitSize) * 0.5f;
	DrawRect(bAlive ? Tint : FLinearColor(0.35f, 0.35f, 0.35f), PortraitX - 2.f, PortraitY - 2.f, PortraitSize + 4.f, PortraitSize + 4.f);
	DrawRect(FLinearColor(0.08f, 0.09f, 0.12f), PortraitX, PortraitY, PortraitSize, PortraitSize);
	// The mate's own species (the fox's picture if its own is missing)
	UTexture2D* Face = Mate.GetPortrait() ? Mate.GetPortrait() : Portrait.Get();
	if (Face)
	{
		DrawTexture(Face, PortraitX, PortraitY, PortraitSize, PortraitSize, 0.f, 0.f, 1.f, 1.f, bAlive ? FLinearColor::White : FLinearColor(0.3f, 0.3f, 0.3f));
	}
	if (!bAlive)
	{
		float W;
		float H;
		GetTextSize(TEXT("KO"), W, H, GEngine->GetLargeFont());
		DrawShadowedText(TEXT("KO"), EnemyColor, PortraitX + (PortraitSize - W) * 0.5f, PortraitY + (PortraitSize - H) * 0.5f, GEngine->GetLargeFont());
	}

	// Name, health bar and number
	const float TextX = PortraitX + PortraitSize + 10.f;
	DrawShadowedText(CardLabel(GetWorld(), Mate, Viewer), bAlive ? FLinearColor::White : FLinearColor::Gray, TextX, Top + 5.f, Small);
	const float Share = FMath::Clamp(Mate.GetHealth() / FMath::Max(1.f, Mate.GetMaxHealth()), 0.f, 1.f);
	const float BarY = Top + 24.f;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.7f), TextX - 1.f, BarY - 1.f, CardBarWidth + 2.f, CardBarHeight + 2.f);
	if (bAlive)
	{
		DrawRect(AllyColor, TextX, BarY, CardBarWidth * Share, CardBarHeight);
	}
	DrawShadowedText(FString::FromInt(FMath::CeilToInt(Mate.GetHealth())), FLinearColor::White, TextX + CardBarWidth + 8.f, BarY - 3.f, Small);

	// What it has going: dice, giant, flight, its gun, reloading
	float X = TextX;
	const float ChipY = Top + 40.f;
	auto Chip = [&](const FString& Label, const FLinearColor& Color)
	{
		float W;
		float H;
		GetTextSize(Label, W, H, Small);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), X, ChipY, W + 8.f, H + 2.f);
		DrawRect(Color, X, ChipY + H + 1.f, W + 8.f, 1.f);
		DrawText(Label, Color, X + 4.f, ChipY + 1.f, Small);
		X += W + 12.f;
	};
	if (const URRRandomDiceBuffComponent* Dice = Mate.GetDiceBuffs())
	{
		Chip(FString::Printf(TEXT("DICE %d"), Dice->GetCharges()), DiceColor);
	}
	if (Mate.IsGiant())
	{
		Chip(FString::Printf(TEXT("GIANT %ds"), FMath::CeilToInt(Mate.GetGiantRemaining())), GiantColor);
	}
	if (Mate.CanFly())
	{
		Chip(FString::Printf(TEXT("FLY %ds"), FMath::CeilToInt(Mate.GetFlightRemaining())), FlyColor);
	}
	if (!Mate.GetWeapon().bIsDefault)
	{
		Chip(Mate.GetWeapon().Name, FRRWeapon::GetTierColor(Mate.GetWeapon().Tier));
	}
	if (Mate.IsReloading())
	{
		Chip(TEXT("RELOAD"), ReloadColor);
	}
}

FVector2D ARRRandomHUD::MapPixelToWorld(int32 X, int32 Y) const
{
	// Up the texture is up the screen (+X), right is +Y
	const FVector2D Size = MapBounds.GetSize();
	return FVector2D(MapBounds.Max.X - (Y + 0.5f) / MapPixels * Size.X, MapBounds.Min.Y + (X + 0.5f) / MapPixels * Size.Y);
}

bool ARRRandomHUD::BuildMinimapBase()
{
	TActorIterator<ARRRandomIsland> IslandIt(GetWorld());
	const ARRRandomIsland* Island = IslandIt && IslandIt->TypeRows.Num() > 0 ? *IslandIt : nullptr;
	// The third person mode's map instead: it has no cells, so it says what each spot shows
	TActorIterator<ARRRandomDiceLand> LandIt(GetWorld());
	const ARRRandomDiceLand* Land = !Island && LandIt && LandIt->IsBuilt() ? *LandIt : nullptr;
	if (!Island && !Land)
	{
		return false;
	}
	MapBounds = Island ? Island->GetMapBounds() : Land->GetMapBounds();

	// Each pixel's cell: its color, and its height so faces down the screen can be shaded
	MapBase.SetNumUninitialized(MapPixels * MapPixels);
	TArray<int32> Heights;
	Heights.SetNumZeroed(MapPixels * MapPixels);
	for (int32 Y = 0; Y < MapPixels; ++Y)
	{
		for (int32 X = 0; X < MapPixels; ++X)
		{
			const FVector2D World = MapPixelToWorld(X, Y);
			FColor Color = WaterColor;
			int32 Levels = -1;
			int32 Row;
			int32 Col;
			if (Land)
			{
				Color = Land->GetMapColor(World, Levels);
			}
			else if (Island->GetCellAt(World, Row, Col))
			{
				const TCHAR Type = Island->TypeRows[Row][Col];
				const TCHAR Digit = Island->HeightRows[Row][Col];
				Levels = FChar::IsDigit(Digit) ? Digit - TEXT('0') : 0;
				switch (Type)
				{
				case TEXT('g'):
				case TEXT('r'):
					Color = HillColor(Levels);
					break;
				case TEXT('s'):
					Color = SandColor;
					break;
				case TEXT('d'):
				case TEXT('b'):
					Color = DockColor;
					break;
				case TEXT('F'):
					Color = FallsColor;
					break;
				case TEXT('w'):
					Color = WaterColor;
					Levels = -1;
					break;
				default:
					Color = FloorColor;
					break;
				}
				// Woods: a round dark patch in the middle of each pine or round tree cell
				const TCHAR Prop = Island->PropRows.IsValidIndex(Row) && Island->PropRows[Row].IsValidIndex(Col) ? Island->PropRows[Row][Col] : TEXT('.');
				if (Prop == TEXT('P') || Prop == TEXT('T'))
				{
					const FVector2D CellMid(MapBounds.Max.X - (Row + 0.5f) * Island->CellSize, MapBounds.Min.Y + (Col + 0.5f) * Island->CellSize);
					if (FVector2D::Distance(World, CellMid) < Island->CellSize * WoodShare)
					{
						Color = WoodColor;
					}
				}
				// Buildings and other solid props: a smaller square patch
				else if (FCString::Strchr(TEXT("HVCKM"), Prop))
				{
					const FVector2D CellMid(MapBounds.Max.X - (Row + 0.5f) * Island->CellSize, MapBounds.Min.Y + (Col + 0.5f) * Island->CellSize);
					const float Half = Island->CellSize * (Prop == TEXT('H') || Prop == TEXT('V') ? 0.9f : 0.3f);
					if (FMath::Abs(World.X - CellMid.X) < Half && FMath::Abs(World.Y - CellMid.Y) < Half)
					{
						Color = BuildingColor;
					}
				}
			}
			MapBase[Y * MapPixels + X] = Color;
			Heights[Y * MapPixels + X] = Levels;
		}
	}

	// Cover blocks, as dark green raised patches
	MapCoverCount = 0;
	const FVector2D Size = MapBounds.GetSize();
	for (TActorIterator<ARRRandomCover> It(GetWorld()); It; ++It)
	{
		++MapCoverCount;
		const FBox Box = It->GetComponentsBoundingBox(true);
		const int32 X0 = FMath::FloorToInt((Box.Min.Y - MapBounds.Min.Y) / Size.Y * MapPixels);
		const int32 X1 = FMath::CeilToInt((Box.Max.Y - MapBounds.Min.Y) / Size.Y * MapPixels);
		const int32 Y0 = FMath::FloorToInt((MapBounds.Max.X - Box.Max.X) / Size.X * MapPixels);
		const int32 Y1 = FMath::CeilToInt((MapBounds.Max.X - Box.Min.X) / Size.X * MapPixels);
		for (int32 Y = FMath::Max(0, Y0); Y < FMath::Min(MapPixels, Y1); ++Y)
		{
			for (int32 X = FMath::Max(0, X0); X < FMath::Min(MapPixels, X1); ++X)
			{
				MapBase[Y * MapPixels + X] = CoverColor;
				Heights[Y * MapPixels + X] = 2;
			}
		}
	}

	// A step down toward the camera (down the screen) shows its face: shade the pixel above the drop
	for (int32 Y = 0; Y < MapPixels - 1; ++Y)
	{
		for (int32 X = 0; X < MapPixels; ++X)
		{
			if (Heights[Y * MapPixels + X] > Heights[(Y + 1) * MapPixels + X])
			{
				MapBase[Y * MapPixels + X] = Shaded(MapBase[Y * MapPixels + X], CliffShade);
			}
		}
	}
	return true;
}

void ARRRandomHUD::RefreshMinimap()
{
	if (!MapTexture)
	{
		if (!BuildMinimapBase())
		{
			return;
		}
		MapTexture = UTexture2D::CreateTransient(MapPixels, MapPixels, PF_B8G8R8A8);
		MapTexture->SRGB = true;
		MapTexture->Filter = TF_Bilinear;
		MapTexture->AddressX = TA_Clamp;
		MapTexture->AddressY = TA_Clamp;
		MapTexture->UpdateResource();
	}
	else
	{
		// Cover reaches clients a little after the island
		int32 Covers = 0;
		for (TActorIterator<ARRRandomCover> It(GetWorld()); It; ++It)
		{
			++Covers;
		}
		if (Covers != MapCoverCount)
		{
			BuildMinimapBase();
		}
	}

	// Lit where the team sees now, dim where it has looked before, dark where it never has
	const ARRRandomPlayerController* Controller = Cast<ARRRandomPlayerController>(PlayerOwner);
	const ARRRandomFogOfWar* Fog = Controller ? Controller->GetFogOfWar() : nullptr;
	FColor* Pixels = new FColor[MapPixels * MapPixels];
	for (int32 Y = 0; Y < MapPixels; ++Y)
	{
		for (int32 X = 0; X < MapPixels; ++X)
		{
			bool bExplored = true;
			const FVector2D World = MapPixelToWorld(X, Y);
			const float Sight = Fog ? Fog->GetSightAt(FVector(World.X, World.Y, 0.f), &bExplored) : 1.f;
			Pixels[Y * MapPixels + X] = Shaded(MapBase[Y * MapPixels + X], FMath::Lerp(bExplored ? ExploredShade : UnexploredShade, 1.f, Sight));
		}
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, MapPixels, MapPixels);
	MapTexture->UpdateTextureRegions(0, 1, Region, MapPixels * sizeof(FColor), sizeof(FColor), reinterpret_cast<uint8*>(Pixels),
		[](uint8* Data, const FUpdateTextureRegion2D* Regions)
		{
			delete[] reinterpret_cast<FColor*>(Data);
			delete Regions;
		});
}

void ARRRandomHUD::DrawMinimap(const ARRRandomCharacter* Viewer)
{
	MapRefreshTimer -= GetWorld()->GetDeltaSeconds();
	if (MapRefreshTimer <= 0.f || !MapTexture)
	{
		MapRefreshTimer = MapRefreshInterval;
		RefreshMinimap();
	}
	MinimapRect = FBox2D(FVector2D(MinimapMargin, MinimapMargin), FVector2D(MinimapMargin + MinimapSize, MinimapMargin + MinimapSize));
	if (!MapTexture)
	{
		return;
	}

	FBox2D Rect = MinimapRect;
	if (bMapExpanded)
	{
		// Full screen: the game dimmed behind a big square map
		const float Side = FMath::Min(Canvas->ClipX, Canvas->ClipY) * ExpandedShare;
		const FVector2D Min((Canvas->ClipX - Side) * 0.5f, (Canvas->ClipY - Side) * 0.5f);
		Rect = FBox2D(Min, Min + FVector2D(Side, Side));
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		DrawShadowedText(TEXT("MAP   [M] OR CLICK TO CLOSE"), FLinearColor::White, Rect.Min.X, Rect.Min.Y - 22.f, GEngine->GetSmallFont());
	}
	const FVector2D Size = Rect.GetSize();
	DrawTexture(MapTexture, Rect.Min.X, Rect.Min.Y, Size.X, Size.Y, 0.f, 0.f, 1.f, 1.f,
		FLinearColor(1.f, 1.f, 1.f, bMapExpanded ? ExpandedMapOpacity : MinimapOpacity), BLEND_Translucent);
	DrawRect(MapFrameColor, Rect.Min.X - MapFrameWidth, Rect.Min.Y - MapFrameWidth, Size.X + MapFrameWidth * 2.f, MapFrameWidth);
	DrawRect(MapFrameColor, Rect.Min.X - MapFrameWidth, Rect.Max.Y, Size.X + MapFrameWidth * 2.f, MapFrameWidth);
	DrawRect(MapFrameColor, Rect.Min.X - MapFrameWidth, Rect.Min.Y, MapFrameWidth, Size.Y);
	DrawRect(MapFrameColor, Rect.Max.X, Rect.Min.Y, MapFrameWidth, Size.Y);
	DrawMapMarkers(Rect, Viewer);
	if (!bMapExpanded)
	{
		DrawShadowedText(TEXT("[M] / CLICK: MAP"), FLinearColor(0.75f, 0.75f, 0.75f), Rect.Min.X, Rect.Max.Y + 4.f, GEngine->GetSmallFont());
	}
}

void ARRRandomHUD::DrawMapMarkers(const FBox2D& Rect, const ARRRandomCharacter* Viewer)
{
	const bool bThirdPersonGame = ARRRandomGameState::IsThirdPersonMode(this);
	const FVector2D Size = Rect.GetSize();
	const FVector2D Area = MapBounds.GetSize();
	const float Scale = Size.X / MinimapSize;
	auto ToScreen = [&](const FVector& World)
	{
		const FVector2D Point(Rect.Min.X + (World.Y - MapBounds.Min.Y) / Area.Y * Size.X, Rect.Min.Y + (MapBounds.Max.X - World.X) / Area.X * Size.Y);
		return FVector2D(FMath::Clamp(Point.X, Rect.Min.X, Rect.Max.X), FMath::Clamp(Point.Y, Rect.Min.Y, Rect.Max.Y));
	};

	// Teammates always; opponents only while the team sees them (as on screen and for the bots)
	const int32 Team = Viewer ? Viewer->GetTeam() : INDEX_NONE;
	const float Radius = MarkerRadius * Scale;
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		const ARRRandomCharacter* Brawler = *It;
		const bool bAlly = Brawler->GetTeam() == Team;
		if (Brawler == Viewer)
		{
			continue;
		}
		// Opponents: only those the team sees; in a third-person game none (no fog of war, so no shared sight to show)
		if (!bAlly && (bThirdPersonGame || !Brawler->IsAlive() || !ARRRandomCharacter::IsSeenByTeam(GetWorld(), Team, Brawler->GetActorLocation())))
		{
			continue;
		}
		const FVector2D Point = ToScreen(Brawler->GetActorLocation());
		if (!Brawler->IsAlive())
		{
			// A knocked-out teammate: a grey cross
			const float Arm = Radius * 0.9f;
			DrawLine(Point.X - Arm, Point.Y - Arm, Point.X + Arm, Point.Y + Arm, FLinearColor(0.6f, 0.6f, 0.6f), 2.f * Scale);
			DrawLine(Point.X - Arm, Point.Y + Arm, Point.X + Arm, Point.Y - Arm, FLinearColor(0.6f, 0.6f, 0.6f), 2.f * Scale);
			continue;
		}
		DrawDisc(Canvas, Point, Radius + 1.5f * Scale, FLinearColor(0.f, 0.f, 0.f, 0.85f));
		DrawDisc(Canvas, Point, Radius, bAlly ? AllyColor : EnemyColor);
	}

	// Me last, on top: an arrow the way I face
	if (Viewer)
	{
		const FVector2D Point = ToScreen(Viewer->GetActorLocation());
		const float Yaw = FMath::DegreesToRadians(Viewer->GetActorRotation().Yaw);
		// World +X is screen up and +Y screen right
		const FVector2D Ahead(FMath::Sin(Yaw), -FMath::Cos(Yaw));
		const FVector2D Side(-Ahead.Y, Ahead.X);
		const float Length = Radius * 1.9f;
		const FLinearColor Color = Viewer->IsAlive() ? SelfColor : FLinearColor(0.6f, 0.6f, 0.6f);
		const float Edge = 1.6f * Scale;
		DrawTriangle(Canvas, Point + Ahead * (Length + Edge * 2.f), Point - Ahead * (Length * 0.6f + Edge) + Side * (Length * 0.75f + Edge),
			Point - Ahead * (Length * 0.6f + Edge) - Side * (Length * 0.75f + Edge), FLinearColor(0.f, 0.f, 0.f, 0.85f));
		DrawTriangle(Canvas, Point + Ahead * Length, Point - Ahead * Length * 0.6f + Side * Length * 0.75f, Point - Ahead * Length * 0.6f - Side * Length * 0.75f, Color);
	}
}

bool ARRRandomHUD::HandleMapClick(const FVector2D& ScreenPoint)
{
	if (bMapExpanded)
	{
		bMapExpanded = false;
		return true;
	}
	if (MinimapRect.IsInside(ScreenPoint))
	{
		bMapExpanded = true;
		return true;
	}
	return false;
}

bool ARRRandomHUD::IsPointOnMap(const FVector2D& ScreenPoint) const
{
	return bMapExpanded || MinimapRect.IsInside(ScreenPoint);
}
