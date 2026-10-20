// ARRRandomHUD, continued: the start countdown, the teammates' cards and the minimap.
#include "RRRandomHUD.h"
#include "RRRandomCharacter.h"
#include "RRRandomCover.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomDiceLand.h"
#include "RRRandomDiceTown.h"
#include "RRRandomFogOfWar.h"
#include "RRRandomGameState.h"
#include "RRRandomIsland.h"
#include "RRRandomMeteor.h"
#include "RRRandomOre.h"
#include "RRRandomPlayerController.h"
#include "RRRandomUIStyle.h"
#include "RRRandomWeapon.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"

using RRUI::EWeight;

namespace
{
	// Shared with the rest of the HUD (RRRandomHUD.cpp): screen margin, on a 720 pixel tall screen
	constexpr float PanelMargin = 22.f;
	const FLinearColor Track(1.f, 1.f, 1.f, 0.78f);
	
	// Countdown: the number for each second pops in this much bigger, and GO stays this long after the start
	constexpr float CountdownPop = 0.6f;
	constexpr float GoTime = 1.f;

	// Team cards, stacked over the ammo panel
	constexpr float CardWidth = 250.f;
	constexpr float CardHeight = 56.f;
	constexpr float CardGap = 8.f;
	constexpr float PortraitSize = 46.f;
	constexpr float CardBarHeight = 4.f;
	// Room left at the bottom right for the ammo panel (gun, rounds)
	constexpr float AmmoPanelRoom = 100.f;

	// Minimap: texture pixels per side, how often the fog shading is redone, the full-screen map's share of the screen,
	// how far fog veils never-seen and not-seen-now places, and the marker size on the corner map (scaled up full screen)
	constexpr int32 MapPixels = 288;
	constexpr float MapRefreshInterval = 0.12f;
	constexpr float ExpandedShare = 0.86f;
	constexpr float UnexploredFog = 0.82f;
	constexpr float ExploredFog = 0.45f;
	// The terrain is lightened this much toward white, so the map sits in the pastel HUD
	constexpr float MapPastel = 0.22f;
	constexpr float MarkerRadius = 4.5f;
	// A soft lavender haze for the fog of war on the map
	const FLinearColor MapFogColor = FLinearColor::FromSRGBColor(FColor(222, 214, 236));
	// See-through: the corner map shows the game under it; the full-screen one, over a veiled game, is solid
	constexpr float MinimapOpacity = 0.92f;
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
}

void ARRRandomHUD::DrawCountdown(const ARRRandomGameState& State)
{
	const float Remaining = State.GetCountdownRemaining();
	const float S = UIScale;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.28f;
	const float NumberSize = 96.f * S;
	if (Remaining > 0.f)
	{
		// GET READY over a big number that pops in at the start of each second and settles
		const int32 Number = FMath::CeilToInt(Remaining);
		const float Fresh = Remaining - (Number - 1);
		const float Pop = 1.f + CountdownPop * FMath::Pow(Fresh, 4.f);
		const float LabelSize = 14.f * S;
		const FString Label = TEXT("GET READY");
		const FVector2D LabelBox = RRUI::MeasureText(Label, LabelSize, EWeight::ExtraBold, LabelSize * 0.3f);
		RRUI::DrawGlowText(Canvas, Label, FVector2D(CenterX - LabelBox.X * 0.5f, Y - LabelBox.Y), LabelSize, EWeight::ExtraBold, RRUI::InkSoft, LabelSize * 0.3f);
		const FString Text = FString::FromInt(Number);
		const FVector2D Box = RRUI::MeasureText(Text, NumberSize, EWeight::Black);
		const FLinearColor Stops[] = { RRUI::LavenderDeep, RRUI::PinkDeep };
		RRUI::DrawGradientText(Canvas, Text, FVector2D(CenterX - Box.X * Pop * 0.5f, Y + Box.Y * (1.f - Pop) * 0.5f), NumberSize, EWeight::Black, Stops, 1.f, 0.f, Pop);
	}
	else if (-Remaining < GoTime)
	{
		const float T = -Remaining / GoTime;
		const float Zoom = 1.f + 0.4f * T;
		const FString Text = TEXT("GO!");
		const FVector2D Box = RRUI::MeasureText(Text, NumberSize, EWeight::Black, NumberSize * 0.06f);
		const FLinearColor Stops[] = { RRUI::PinkDeep, RRUI::Gold };
		RRUI::DrawGradientText(Canvas, Text, FVector2D(CenterX - Box.X * Zoom * 0.5f, Y + Box.Y * (1.f - Zoom) * 0.5f), NumberSize, EWeight::Black, Stops, 1.f - T * T, NumberSize * 0.06f, Zoom);
	}

	// Meteor Rush: a team holding the goal (or overtime's golden crystal) counts down to its win, high on the screen in
	// its color, each second popping in like the start's
	const int32 HoldTeam = State.GetHoldTeam();
	if (State.IsMeteorRush() && HoldTeam != INDEX_NONE && State.HasPlayStarted())
	{
		const float Left = FMath::Max(0.f, State.GetHoldRemaining());
		const int32 Number = FMath::Max(1, FMath::CeilToInt(Left));
		const float Fresh = FMath::Clamp(Left - (Number - 1), 0.f, 1.f);
		const float Pop = 1.f + CountdownPop * 0.6f * FMath::Pow(Fresh, 4.f);
		const float HoldY = Canvas->ClipY * 0.17f;
		const float LabelSize = 13.f * S;
		const FString TeamName = HoldTeam == 0 ? TEXT("BLUE") : TEXT("RED");
		const FString Label = State.IsOvertime() ? TeamName + TEXT(" HAS THE GOLDEN CRYSTAL") : TeamName + TEXT(" HOLDS THE ORE");
		const FVector2D LabelBox = RRUI::MeasureText(Label, LabelSize, EWeight::Black, LabelSize * 0.24f);
		RRUI::DrawGlowText(Canvas, Label, FVector2D(CenterX - LabelBox.X * 0.5f, HoldY - LabelBox.Y), LabelSize, EWeight::Black, RRUI::TeamColorDeep(HoldTeam), LabelSize * 0.24f);
		const float HoldSize = 64.f * S;
		const FString Text = FString::FromInt(Number);
		const FVector2D Box = RRUI::MeasureText(Text, HoldSize, EWeight::Black);
		const FLinearColor Stops[] = { RRUI::TeamColorDeep(HoldTeam), State.IsOvertime() ? RRUI::Gold : RRUI::LavenderDeep };
		RRUI::DrawGradientText(Canvas, Text, FVector2D(CenterX - Box.X * Pop * 0.5f, HoldY + Box.Y * (1.f - Pop) * 0.5f), HoldSize, EWeight::Black, Stops, 1.f, 0.f, Pop);
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
	if (Mates.Num() == 0)
	{
		return;
	}

	// Stacked upward from over the ammo panel, the first teammate on top
	const float S = UIScale;
	const float Left = Canvas->ClipX - (PanelMargin + CardWidth) * S;
	const float Height = Mates.Num() * (CardHeight + CardGap) * S - CardGap * S;
	float Top = Canvas->ClipY - (PanelMargin + AmmoPanelRoom) * S - Height;
	for (const ARRRandomCharacter* Mate : Mates)
	{
		DrawTeamCard(*Mate, Viewer, Left, Top);
		Top += (CardHeight + CardGap) * S;
	}
}

void ARRRandomHUD::DrawTeamCard(const ARRRandomCharacter& Mate, const ARRRandomCharacter& Viewer, float Left, float Top)
{
	// No box: a round portrait in a white and team-colored ring, the name, a thin health bar and what it has going
	const float S = UIScale;
	const bool bAlive = Mate.IsAlive();
	const float Face = PortraitSize * S;
	const FVector2D FaceCenter(Left + Face * 0.5f + 4.f * S, Top + CardHeight * 0.5f * S);
	RRUI::DrawDisc(Canvas, FaceCenter, Face * 0.5f + 4.f * S, bAlive ? RRUI::TeamColor(Mate.GetTeam()) : RRUI::InkSoft);
	RRUI::DrawDisc(Canvas, FaceCenter, Face * 0.5f + 2.f * S, RRUI::White);
	// The mate's own species (the fox's picture if its own is missing), greyed with KO over it while knocked out
	if (UTexture2D* Picture = Mate.GetPortrait() ? Mate.GetPortrait() : Portrait.Get())
	{
		RRUI::DrawTextureRounded(Canvas, Picture, FBox2D(FaceCenter - FVector2D(Face * 0.5f), FaceCenter + FVector2D(Face * 0.5f)), Face * 0.5f,
			bAlive ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.4f));
	}
	if (!bAlive)
	{
		const float Size = 18.f * S;
		const FVector2D Box = RRUI::MeasureText(TEXT("KO"), Size, EWeight::Black);
		RRUI::DrawGlowText(Canvas, TEXT("KO"), FaceCenter - Box * 0.5f, Size, EWeight::Black, RRUI::PinkDeep);
	}

	// Name and health on the first line, the bar, then the tags, on one soft white plate
	const float TextX = FaceCenter.X + Face * 0.5f + 12.f * S;
	const float Right = Left + CardWidth * S;
	const float NameSize = 13.f * S;
	const float SmallSize = 10.f * S;
	RRUI::DrawPlate(Canvas, FBox2D(FVector2D(TextX, Top + 6.f * S), FVector2D(Right, Top + (CardHeight - 8.f) * S)), 14.f * S);
	float Y = Top + 3.f * S;
	const FVector2D Name = RRUI::DrawText(Canvas, CardLabel(GetWorld(), Mate, Viewer), FVector2D(TextX, Y), NameSize, EWeight::ExtraBold, bAlive ? RRUI::Ink : RRUI::InkSoft, NameSize * 0.04f);
	const FString Health = FString::FromInt(FMath::CeilToInt(Mate.GetHealth()));
	const FVector2D HealthBox = RRUI::MeasureText(Health, SmallSize, EWeight::ExtraBold);
	RRUI::DrawText(Canvas, Health, FVector2D(Right - HealthBox.X, Y + (Name.Y - HealthBox.Y) * 0.6f), SmallSize, EWeight::ExtraBold, RRUI::InkSoft);
	Y += Name.Y + 1.f * S;
	const float Share = bAlive ? FMath::Clamp(Mate.GetHealth() / FMath::Max(1.f, Mate.GetMaxHealth()), 0.f, 1.f) : 0.f;
	RRUI::DrawBar(Canvas, FBox2D(FVector2D(TextX, Y), FVector2D(Right, Y + CardBarHeight * S)), Share, RRUI::WithAlpha(RRUI::Ink, 0.12f), RRUI::MintDeep);
	Y += CardBarHeight * S + 3.f * S;

	// What it has going: dice, giant, flight, its gun, reloading, e.g. "DICE 1 · CANNON"
	TArray<FString> Going;
	if (const URRRandomDiceBuffComponent* Dice = Mate.GetDiceBuffs())
	{
		Going.Add(FString::Printf(TEXT("DICE %d"), Dice->GetCharges()));
	}
	if (Mate.IsGiant())
	{
		Going.Add(FString::Printf(TEXT("GIANT %ds"), FMath::CeilToInt(Mate.GetGiantRemaining())));
	}
	if (Mate.CanFly())
	{
		Going.Add(FString::Printf(TEXT("FLY %ds"), FMath::CeilToInt(Mate.GetFlightRemaining())));
	}
	if (!Mate.GetWeapon().bIsDefault)
	{
		Going.Add(Mate.GetWeapon().Name);
	}
	if (Mate.IsReloading())
	{
		Going.Add(TEXT("RELOAD"));
	}
	RRUI::DrawText(Canvas, FString::Join(Going, TEXT(" · ")), FVector2D(TextX, Y), SmallSize, EWeight::ExtraBold, RRUI::InkSoft, SmallSize * 0.06f);
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
	TActorIterator<ARRRandomDiceTown> TownIt(GetWorld());
	const ARRRandomDiceTown* Town = !Island && !Land && TownIt && TownIt->IsBuilt() ? *TownIt : nullptr;
	if (!Island && !Land && !Town)
	{
		return false;
	}
	MapBounds = Island ? Island->GetMapBounds() : Land ? Land->GetMapBounds() : Town->GetMapBounds();

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
			else if (Town)
			{
				Color = Town->GetMapColor(World, Levels);
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

	// The terrain lightened into the pastel palette; clear where the team sees now, under a light lavender haze where it
	// has looked before, and a thick one where it never has
	const ARRRandomPlayerController* Controller = Cast<ARRRandomPlayerController>(PlayerOwner);
	const ARRRandomFogOfWar* Fog = Controller ? Controller->GetFogOfWar() : nullptr;
	const FColor FogColor = MapFogColor.ToFColor(true);
	FColor* Pixels = new FColor[MapPixels * MapPixels];
	for (int32 Y = 0; Y < MapPixels; ++Y)
	{
		for (int32 X = 0; X < MapPixels; ++X)
		{
			bool bExplored = true;
			const FVector2D World = MapPixelToWorld(X, Y);
			const float Sight = Fog ? Fog->GetSightAt(FVector(World.X, World.Y, 0.f), &bExplored) : 1.f;
			const FColor& Base = MapBase[Y * MapPixels + X];
			const float Veil = (1.f - Sight) * (bExplored ? ExploredFog : UnexploredFog);
			auto Mix = [&](uint8 Channel, uint8 Haze)
			{
				const float Light = FMath::Lerp(float(Channel), 255.f, MapPastel);
				return uint8(FMath::Clamp(FMath::RoundToInt(FMath::Lerp(Light, float(Haze), Veil)), 0, 255));
			};
			Pixels[Y * MapPixels + X] = FColor(Mix(Base.R, FogColor.R), Mix(Base.G, FogColor.G), Mix(Base.B, FogColor.B), 255);
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
	const float S = UIScale;
	MinimapRect = FBox2D(FVector2D(MinimapMargin, MinimapMargin) * S, FVector2D(MinimapMargin + MinimapSize, MinimapMargin + MinimapSize) * S);
	if (!MapTexture)
	{
		return;
	}

	if (bMapExpanded)
	{
		// Full screen: the game under a light veil, the whole square map on a frosted panel with rounded corners
		const float Side = FMath::Min(Canvas->ClipX, Canvas->ClipY) * ExpandedShare;
		const FVector2D Min((Canvas->ClipX - Side) * 0.5f, (Canvas->ClipY - Side) * 0.5f);
		const FBox2D Rect(Min, Min + FVector2D(Side, Side));
		RRUI::DrawBox(Canvas, FBox2D(FVector2D::ZeroVector, FVector2D(Canvas->ClipX, Canvas->ClipY)), FLinearColor::FromSRGBColor(FColor(244, 240, 252, 150)));
		RRUI::DrawPanel(Canvas, Rect.ExpandBy(6.f * S), 26.f * S, S, RRUI::White);
		RRUI::DrawTextureRounded(Canvas, MapTexture, Rect, 20.f * S, FLinearColor::White);
		const float Size = 11.f * S;
		RRUI::DrawGlowText(Canvas, TEXT("MAP · M OR CLICK TO CLOSE"), FVector2D(Rect.Min.X, Rect.Min.Y - 12.f * S - Size * 1.36f), Size, EWeight::ExtraBold, RRUI::Ink, Size * 0.12f);
		DrawMapMarkers(Rect, Viewer);
		return;
	}

	// The corner map is round: a faint white halo, the map in the circle, a white rim
	const FVector2D Center = MinimapRect.GetCenter();
	const float Radius = MinimapRect.GetSize().X * 0.5f;
	RRUI::DrawRing(Canvas, Center, Radius + 4.f * S, 6.f * S, FLinearColor(1.f, 1.f, 1.f, 0.35f));
	RRUI::DrawTextureRounded(Canvas, MapTexture, MinimapRect, Radius, FLinearColor(1.f, 1.f, 1.f, MinimapOpacity));
	RRUI::DrawRing(Canvas, Center, Radius, 2.f * S, RRUI::White);
	DrawMapMarkers(MinimapRect, Viewer);
}

void ARRRandomHUD::DrawMapMarkers(const FBox2D& Rect, const ARRRandomCharacter* Viewer)
{
	const bool bThirdPersonGame = ARRRandomGameState::IsThirdPersonMode(this);
	const FVector2D Size = Rect.GetSize();
	const FVector2D Area = MapBounds.GetSize();
	const float Scale = Size.X / MinimapSize;
	const FVector2D Center = Rect.GetCenter();
	const float Edge = Size.X * 0.5f - MarkerRadius * Scale;
	auto ToScreen = [&](const FVector& World)
	{
		const FVector2D Point(Rect.Min.X + (World.Y - MapBounds.Min.Y) / Area.Y * Size.X, Rect.Min.Y + (MapBounds.Max.X - World.X) / Area.X * Size.Y);
		if (!bMapExpanded)
		{
			// Kept inside the round corner map
			const FVector2D Offset = Point - Center;
			return Offset.Size() > Edge ? Center + Offset.GetSafeNormal() * Edge : Point;
		}
		return FVector2D(FMath::Clamp(Point.X, Rect.Min.X, Rect.Max.X), FMath::Clamp(Point.Y, Rect.Min.Y, Rect.Max.Y));
	};

	// Teammates always; opponents only while the team sees them (as on screen and for the bots)
	const int32 Team = Viewer ? Viewer->GetTeam() : INDEX_NONE;
	const float Radius = MarkerRadius * Scale;

	// Meteor Rush: where meteors are coming down (a pulsing ring), the ore lying about (small dots in its color) and
	// anyone carrying a crystal, seen or not (a gold ring)
	if (ARRRandomGameState::IsMeteorRush(this))
	{
		for (TActorIterator<ARRRandomOre> It(GetWorld()); It; ++It)
		{
			const FVector2D Point = ToScreen(It->GetActorLocation());
			const bool bCrystal = RROre::IsCrystal(It->GetKind());
			RRUI::DrawDisc(Canvas, Point, (bCrystal ? 3.6f : 2.4f) * Scale + 1.2f * Scale, RRUI::White);
			RRUI::DrawDisc(Canvas, Point, (bCrystal ? 3.6f : 2.4f) * Scale, RROre::GetColor(It->GetKind()));
		}
		// Meteors: coming down, a ring pulsing out round a dot with the seconds left beside it; landed and throwing, a
		// solid disc in a white rim
		for (TActorIterator<ARRRandomMeteor> It(GetWorld()); It; ++It)
		{
			const float Left = It->GetTimeToImpact();
			const FLinearColor Color = It->IsGolden() ? RRUI::Gold : RRUI::PinkDeep;
			const FVector2D Point = ToScreen(It->GetActorLocation());
			if (Left > 0.f)
			{
				const float Pulse = FMath::Fmod(GetWorld()->GetTimeSeconds() * 1.5f, 1.f);
				RRUI::DrawRing(Canvas, Point, (6.f + 10.f * Pulse) * Scale, 2.f * Scale, RRUI::WithAlpha(Color, 1.f - Pulse));
				RRUI::DrawRing(Canvas, Point, 7.f * Scale, 2.4f * Scale, Color);
				RRUI::DrawDisc(Canvas, Point, 3.5f * Scale, Color);
				const FString Seconds = FString::FromInt(FMath::CeilToInt(Left));
				const float TextSize = 10.f * Scale;
				RRUI::DrawGlowText(Canvas, Seconds, Point + FVector2D(9.f * Scale, -TextSize * 0.7f), TextSize, EWeight::Black, Color);
			}
			else if (It->IsBursting())
			{
				RRUI::DrawDisc(Canvas, Point, 7.5f * Scale, RRUI::White);
				RRUI::DrawDisc(Canvas, Point, 6.f * Scale, Color);
			}
		}
		for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
		{
			const URRRandomOreCarryComponent* Carry = It->GetOreCarry();
			if (*It != Viewer && It->IsAlive() && Carry && Carry->HasCrystal())
			{
				const FVector2D Point = ToScreen(It->GetActorLocation());
				RRUI::DrawRing(Canvas, Point, Radius + 4.f * Scale, 2.4f * Scale, Carry->HasGolden() ? RRUI::Butter : RRUI::Gold);
				if (It->GetTeam() != Team)
				{
					RRUI::DrawDisc(Canvas, Point, Radius + 1.8f * Scale, RRUI::White);
					RRUI::DrawDisc(Canvas, Point, Radius, RRUI::PinkDeep);
				}
			}
		}
	}
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		const ARRRandomCharacter* Brawler = *It;
		const bool bAlly = Brawler->GetTeam() == Team;
		if (Brawler == Viewer)
		{
			continue;
		}
		// Opponents: only those the team sees; in a third-person game none (no fog of war, so no shared sight to show)
		if (!bAlly && (bThirdPersonGame || !Brawler->IsAlive() || !Brawler->IsNoticedByTeam(Team)))
		{
			continue;
		}
		const FVector2D Point = ToScreen(Brawler->GetActorLocation());
		if (!Brawler->IsAlive())
		{
			// A knocked-out teammate: a soft ink cross
			const float Arm = Radius * 0.9f;
			DrawLine(Point.X - Arm, Point.Y - Arm, Point.X + Arm, Point.Y + Arm, RRUI::InkSoft, 2.f * Scale);
			DrawLine(Point.X - Arm, Point.Y + Arm, Point.X + Arm, Point.Y - Arm, RRUI::InkSoft, 2.f * Scale);
			continue;
		}
		RRUI::DrawDisc(Canvas, Point, Radius + 1.8f * Scale, RRUI::White);
		RRUI::DrawDisc(Canvas, Point, Radius, bAlly ? RRUI::SkyDeep : RRUI::PinkDeep);
	}

	// Me last, on top: a white arrow with an ink edge, the way I face
	if (Viewer)
	{
		const FVector2D Point = ToScreen(Viewer->GetActorLocation());
		const float Yaw = FMath::DegreesToRadians(Viewer->GetActorRotation().Yaw);
		// World +X is screen up and +Y screen right
		const FVector2D Ahead(FMath::Sin(Yaw), -FMath::Cos(Yaw));
		const FVector2D Side(-Ahead.Y, Ahead.X);
		const float Length = Radius * 1.9f;
		const FLinearColor Color = Viewer->IsAlive() ? RRUI::White : FLinearColor(0.8f, 0.8f, 0.8f);
		const float Rim = 1.6f * Scale;
		RRUI::DrawTriangle(Canvas, Point + Ahead * (Length + Rim * 2.f), Point - Ahead * (Length * 0.6f + Rim) + Side * (Length * 0.75f + Rim),
			Point - Ahead * (Length * 0.6f + Rim) - Side * (Length * 0.75f + Rim), RRUI::WithAlpha(RRUI::Ink, 0.85f));
		RRUI::DrawTriangle(Canvas, Point + Ahead * Length, Point - Ahead * Length * 0.6f + Side * Length * 0.75f, Point - Ahead * Length * 0.6f - Side * Length * 0.75f, Color);
	}
}

bool ARRRandomHUD::IsOnCornerMap(const FVector2D& ScreenPoint) const
{
	return MinimapRect.bIsValid && FVector2D::Distance(ScreenPoint, MinimapRect.GetCenter()) <= MinimapRect.GetSize().X * 0.5f;
}

bool ARRRandomHUD::HandleMapClick(const FVector2D& ScreenPoint)
{
	if (bMapExpanded)
	{
		bMapExpanded = false;
		return true;
	}
	if (IsOnCornerMap(ScreenPoint))
	{
		bMapExpanded = true;
		return true;
	}
	return false;
}

bool ARRRandomHUD::IsPointOnMap(const FVector2D& ScreenPoint) const
{
	return bMapExpanded || IsOnCornerMap(ScreenPoint);
}
