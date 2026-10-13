#include "RRRandomTitleHUD.h"
#include "RRRandomSessionSubsystem.h"
#include "RRRandomTitlePlayerController.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Sizes for a 720 pixel tall screen; everything scales with the screen height
	constexpr float ReferenceHeight = 720.f;
	constexpr float ButtonWidth = 340.f;
	constexpr float ButtonHeight = 50.f;
	constexpr float ButtonGap = 14.f;
	constexpr float ButtonBorder = 2.f;
	// The menu column's middle, as a share of the screen width (the fox stands on the right of the art)
	constexpr float ColumnShare = 0.27f;
	// The art is darkened from the left edge to this share of the screen, so the menu reads over it
	constexpr float ShadeShare = 0.58f;
	constexpr float ShadeAlpha = 0.5f;
	constexpr int32 ShadeStrips = 32;

	// Picked as screen (sRGB) colors; the canvas takes linear ones
	const FLinearColor Background = FLinearColor::FromSRGBColor(FColor(14, 16, 24));
	const FLinearColor Accent = FLinearColor::FromSRGBColor(FColor(255, 196, 56));
	const FLinearColor ButtonFill = FLinearColor::FromSRGBColor(FColor(30, 33, 44));
	const FLinearColor ButtonEdge = FLinearColor::FromSRGBColor(FColor(72, 78, 96));
	const FLinearColor SelectedText = FLinearColor::FromSRGBColor(FColor(24, 18, 6));
	const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor(200, 206, 222));
	const FLinearColor StatusColor = FLinearColor::FromSRGBColor(FColor(255, 214, 110));
	const FLinearColor FlashColor(1.f, 0.93f, 0.75f);

	// Where things are in the title art, in its own pixels (1376 x 768): the painted die and the rifle's muzzle
	const FVector2D ArtSize(1376.f, 768.f);
	const FVector2D DieSpot(1186.f, 116.f);
	const FVector2D MuzzleSpot(921.f, 314.f);

	// The spinning die: frames in the sheet, its grid, how fast it turns, and its size in art pixels (it covers the
	// painted die; a frame is wider than the die inside it)
	constexpr int32 SpinFrames = 24;
	constexpr int32 SpinColumns = 6;
	constexpr int32 SpinRows = 4;
	constexpr float SpinFramesPerSecond = 16.f;
	constexpr float SpinSize = 178.f;
	// The flash behind it: size, a slow pulse, and a bright burst every so often
	constexpr float GlowSize = 330.f;
	constexpr float PulseRate = 1.3f;
	constexpr float BurstEvery = 2.6f;
	constexpr float BurstTime = 0.35f;
	// Sparkles around the die: spots (art pixels from the die) and size
	const FVector2D SparkleSpots[] = { FVector2D(-78.f, -52.f), FVector2D(64.f, -70.f), FVector2D(82.f, 38.f), FVector2D(-86.f, 34.f), FVector2D(-18.f, -96.f), FVector2D(46.f, 86.f) };
	constexpr float SparkleSize = 46.f;
	// Smoke from the muzzle: puffs alive at once, how long each lasts, how fast it rises and drifts, its size and
	// how thick it is
	constexpr int32 PuffCount = 9;
	constexpr float PuffLife = 3.2f;
	constexpr float PuffRise = 48.f;
	constexpr float PuffDrift = -6.f;
	constexpr float PuffSway = 14.f;
	const FVector2D PuffSize(12.f, 64.f);
	constexpr float PuffAlpha = 0.32f;

	// The fox comes alive on its own layers (char-pipeline/scripts/make_title_layers.py, from Tripo edits of the art):
	// the scene without it, its tail, and the rest of it, all at the art's size. The body is drawn as a fine grid of
	// triangles whose corners are nudged (puppet style); only the fox is on that layer, so nothing behind it bends.
	// The patch it covers and its cell size, in art pixels.
	const FBox2D FoxPatch(FVector2D(860.f, 140.f), FVector2D(1376.f, 720.f));
	constexpr float PatchCell = 12.f;
	// Breathing: the fox rises and settles, most at the top of the head and not at all at the feet; pixels at the top,
	// seconds a breath
	constexpr float FeetY = 690.f;
	constexpr float HeadTopY = 200.f;
	constexpr float BreathAmount = 4.f;
	constexpr float BreathPeriod = 3.4f;
	// The head tilts a little about the neck and now and then the right ear flicks (degrees, seconds; each turns what
	// lies within a soft circle)
	const FVector2D NeckPivot(1165.f, 385.f);
	const FVector2D HeadCenter(1185.f, 270.f);
	constexpr float HeadRadius = 140.f;
	constexpr float HeadTilt = 1.6f;
	constexpr float HeadPeriod = 5.1f;
	// The tail swings whole about its root, hidden behind the hip: its layer's part of the art (with the tail's colors
	// carried on under the body, so no gap opens at the root), degrees each way, seconds a swing, and how much of a
	// second, faster swing rides on it so it doesn't tick like a metronome
	const FBox2D TailBox(FVector2D(1190.f, 420.f), FVector2D(1376.f, 625.f));
	const FVector2D TailPivot(1222.f, 578.f);
	constexpr float TailSwing = 6.f;
	constexpr float TailPeriod = 2.2f;
	constexpr float TailFlutter = 0.25f;
	const FVector2D EarPivot(1255.f, 255.f);
	const FVector2D EarCenter(1285.f, 205.f);
	constexpr float EarRadius = 55.f;
	constexpr float EarSwing = 7.f;
	constexpr float EarEvery = 4.3f;
	constexpr float EarTime = 0.3f;
	// Each soft circle fades out over this many art pixels past its radius
	constexpr float PartFalloff = 45.f;

	/** 1 within Radius, easing to 0 over Falloff past it. */
	float Soft(float Distance, float Radius, float Falloff)
	{
		return 1.f - FMath::SmoothStep(Radius, Radius + Falloff, Distance);
	}

	/** How far turning about Pivot by Degrees moves Point. */
	FVector2D Turning(const FVector2D& Point, const FVector2D& Pivot, float Degrees)
	{
		return Pivot + (Point - Pivot).GetRotated(Degrees) - Point;
	}

	/** Where the fox's art pixel at Point is drawn this moment, as a nudge in art pixels. */
	FVector2D FoxNudge(const FVector2D& Point, float Time)
	{
		FVector2D Nudge = FVector2D::ZeroVector;
		const float Height = FMath::Clamp((FeetY - Point.Y) / (FeetY - HeadTopY), 0.f, 1.f);
		Nudge.Y -= BreathAmount * Height * (0.5f - 0.5f * FMath::Cos(2.f * PI * Time / BreathPeriod));
		Nudge += Turning(Point, NeckPivot, HeadTilt * FMath::Sin(2.f * PI * Time / HeadPeriod + 1.f)) * Soft(float(FVector2D::Distance(Point, HeadCenter)), HeadRadius, PartFalloff);
		const float SinceFlick = FMath::Fmod(Time, EarEvery);
		const float Flick = SinceFlick < EarTime ? FMath::Sin(PI * SinceFlick / EarTime) : 0.f;
		Nudge += Turning(Point, EarPivot, -EarSwing * Flick) * Soft(float(FVector2D::Distance(Point, EarCenter)), EarRadius, PartFalloff);
		return Nudge;
	}

	/** The tail's turn this moment, in degrees. */
	float TailTurn(float Time)
	{
		const float Phase = 2.f * PI * Time / TailPeriod;
		return TailSwing * (FMath::Sin(Phase) + TailFlutter * FMath::Sin(2.f * Phase + 0.7f)) / (1.f + TailFlutter);
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	/** For additive sprites (premultiplied, and additive ignores alpha): how bright is the color itself. */
	FLinearColor Brightened(const FLinearColor& Color, float Amount)
	{
		return FLinearColor(Color.R * Amount, Color.G * Amount, Color.B * Amount, 1.f);
	}
}

ARRRandomTitleHUD::ARRRandomTitleHUD()
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> ArtTexture(TEXT("/Game/UI/T_TitleArt.T_TitleArt"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> SpinTexture(TEXT("/Game/UI/T_TitleDiceSpin.T_TitleDiceSpin"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> GlowTexture(TEXT("/Game/UI/T_TitleGlow.T_TitleGlow"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> SparkleTexture(TEXT("/Game/UI/T_TitleSparkle.T_TitleSparkle"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> PuffTexture(TEXT("/Game/UI/T_TitlePuff.T_TitlePuff"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> BackTexture(TEXT("/Game/UI/T_TitleBack.T_TitleBack"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> FoxBodyTexture(TEXT("/Game/UI/T_TitleFoxBody.T_TitleFoxBody"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> FoxTailTexture(TEXT("/Game/UI/T_TitleFoxTail.T_TitleFoxTail"));
	Art = ArtTexture.Succeeded() ? ArtTexture.Object : nullptr;
	Back = BackTexture.Succeeded() ? BackTexture.Object : nullptr;
	FoxBody = FoxBodyTexture.Succeeded() ? FoxBodyTexture.Object : nullptr;
	FoxTail = FoxTailTexture.Succeeded() ? FoxTailTexture.Object : nullptr;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaterMaterial(TEXT("/Game/UI/M_TitleWater.M_TitleWater"));
	Water = WaterMaterial.Succeeded() ? WaterMaterial.Object : nullptr;
	DiceSpin = SpinTexture.Succeeded() ? SpinTexture.Object : nullptr;
	Glow = GlowTexture.Succeeded() ? GlowTexture.Object : nullptr;
	Sparkle = SparkleTexture.Succeeded() ? SparkleTexture.Object : nullptr;
	Puff = PuffTexture.Succeeded() ? PuffTexture.Object : nullptr;
}

int32 ARRRandomTitleHUD::GetButtonAt(const FVector2D& ScreenPoint) const
{
	return ButtonRects.IndexOfByPredicate([&ScreenPoint](const FBox2D& Rect) { return Rect.IsInside(ScreenPoint); });
}

float ARRRandomTitleHUD::DrawCenteredText(const FString& Text, const FLinearColor& Color, float CenterX, float Y, UFont* Font, float Scale)
{
	float Width;
	float Height;
	GetTextSize(Text, Width, Height, Font, Scale);
	const float X = CenterX - Width * 0.5f;
	const float Shadow = FMath::Max(1.f, Scale);
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + Shadow, Y + Shadow, Font, Scale);
	DrawText(Text, Color, X, Y, Font, Scale);
	return Height;
}

FVector2D ARRRandomTitleHUD::ArtToScreen(const FVector2D& ArtPoint) const
{
	return ArtOrigin + ArtPoint * ArtScale;
}

void ARRRandomTitleHUD::DrawArt(float Time)
{
	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	if (!Art)
	{
		DrawRect(Background, 0.f, 0.f, ScreenW, ScreenH);
		return;
	}

	// Cover the screen, cropping the art evenly on the long side
	ArtScale = FMath::Max(ScreenW / ArtSize.X, ScreenH / ArtSize.Y);
	ArtOrigin = FVector2D((ScreenW - ArtSize.X * ArtScale) * 0.5f, (ScreenH - ArtSize.Y * ArtScale) * 0.5f);
	// The fox on its own layers, moving; without them, the art as one still picture
	const bool bLayers = Back && FoxBody && FoxTail;
	DrawTexture(bLayers ? Back : Art, ArtOrigin.X, ArtOrigin.Y, ArtSize.X * ArtScale, ArtSize.Y * ArtScale, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, BLEND_Opaque);
	if (bLayers)
	{
		// The sea moves under the fox: the material covers the whole art and shows only where the water is
		if (Water)
		{
			DrawMaterial(Water, ArtOrigin.X, ArtOrigin.Y, ArtSize.X * ArtScale, ArtSize.Y * ArtScale, 0.f, 0.f, 1.f, 1.f);
		}
		DrawFox(Time);
	}

	// Smoke: puffs leave the muzzle (moving with the fox's breath) one after another, rise, sway, swell and thin out
	if (Puff)
	{
		const FVector2D Muzzle = MuzzleSpot + (bLayers ? FoxNudge(MuzzleSpot, Time) : FVector2D::ZeroVector);
		for (int32 Index = 0; Index < PuffCount; ++Index)
		{
			const float Age = FMath::Fmod(Time + Index * PuffLife / PuffCount, PuffLife);
			const float T = Age / PuffLife;
			const FVector2D Spot = Muzzle + FVector2D(PuffDrift * Age + PuffSway * FMath::Sin(Age * 2.1f + Index) * T, -PuffRise * Age);
			const float Size = FMath::Lerp(PuffSize.X, PuffSize.Y, FMath::Sqrt(T)) * ArtScale;
			const float Alpha = PuffAlpha * FMath::Min(1.f, T * 8.f) * FMath::Square(1.f - T);
			const FVector2D Center = ArtToScreen(Spot);
			DrawTexture(Puff, Center.X - Size * 0.5f, Center.Y - Size * 0.5f, Size, Size, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.f, 1.f, 1.f, Alpha),
				BLEND_Translucent, 1.f, false, Age * 25.f + Index * 40.f, FVector2D(0.5f, 0.5f));
		}
	}

	// The die's flash: a slow pulse with a bright burst every few seconds
	const FVector2D DieCenter = ArtToScreen(DieSpot);
	const float Burst = FMath::Max(0.f, 1.f - FMath::Fmod(Time, BurstEvery) / BurstTime);
	if (Glow)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Time * 2.f * PI * PulseRate);
		const float Size = GlowSize * ArtScale * (1.f + 0.12f * Pulse + 0.45f * Burst);
		DrawTexture(Glow, DieCenter.X - Size * 0.5f, DieCenter.Y - Size * 0.5f, Size, Size, 0.f, 0.f, 1.f, 1.f,
			Brightened(FlashColor, 0.55f + 0.25f * Pulse + 0.6f * Burst), BLEND_Additive);
	}

	// The game's own die, spinning over the painted one
	if (DiceSpin)
	{
		const int32 Frame = FMath::FloorToInt(Time * SpinFramesPerSecond) % SpinFrames;
		const float Size = SpinSize * ArtScale;
		DrawTexture(DiceSpin, DieCenter.X - Size * 0.5f, DieCenter.Y - Size * 0.5f, Size, Size,
			float(Frame % SpinColumns) / SpinColumns, float(Frame / SpinColumns) / SpinRows, 1.f / SpinColumns, 1.f / SpinRows);
	}

	// Sparkles twinkling around it, each in its own rhythm, brighter in a burst
	if (Sparkle)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(SparkleSpots); ++Index)
		{
			const float Twinkle = FMath::Max(0.f, FMath::Sin(Time * (2.4f + Index * 0.37f) + Index * 1.9f));
			const float Size = SparkleSize * ArtScale * (0.45f + 0.75f * Twinkle + 0.3f * Burst);
			const FVector2D Center = ArtToScreen(DieSpot + SparkleSpots[Index]);
			DrawTexture(Sparkle, Center.X - Size * 0.5f, Center.Y - Size * 0.5f, Size, Size, 0.f, 0.f, 1.f, 1.f,
				Brightened(FlashColor, FMath::Min(1.f, Twinkle + Burst)), BLEND_Additive, 1.f, false, Time * 20.f + Index * 15.f, FVector2D(0.5f, 0.5f));
		}
	}

	// Darken toward the left so the menu reads over the sky and the island
	const float ShadeWidth = ScreenW * ShadeShare;
	for (int32 Strip = 0; Strip < ShadeStrips; ++Strip)
	{
		// Whole-pixel edges that meet exactly: overlapping strips would show as darker lines
		const float T = (Strip + 0.5f) / ShadeStrips;
		const float X0 = FMath::RoundToFloat(ShadeWidth * Strip / ShadeStrips);
		const float X1 = FMath::RoundToFloat(ShadeWidth * (Strip + 1) / ShadeStrips);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, ShadeAlpha * FMath::Square(1.f - T)), X0, 0.f, X1 - X0, ScreenH);
	}
}

void ARRRandomTitleHUD::DrawFox(float Time)
{
	FTextureResource* BodyResource = FoxBody->GetResource();
	if (!BodyResource)
	{
		return;
	}

	// The tail first, behind the body: its part of the layer turned whole about its root, which rides on the breath
	const float Turn = TailTurn(Time);
	const FVector2D TailSize = TailBox.GetSize();
	const FVector2D TailCorner = ArtToScreen(TailBox.Min + FoxNudge(TailPivot, Time));
	DrawTexture(FoxTail, TailCorner.X, TailCorner.Y, TailSize.X * ArtScale, TailSize.Y * ArtScale, TailBox.Min.X / ArtSize.X, TailBox.Min.Y / ArtSize.Y,
		TailSize.X / ArtSize.X, TailSize.Y / ArtSize.Y, FLinearColor::White, BLEND_Translucent, 1.f, false, Turn, (TailPivot - TailBox.Min) / TailSize);

	// The body as a grid over the fox: each corner keeps its place in the art (the texture coordinate) but is drawn
	// where FoxNudge moves it.
	const int32 Columns = FMath::CeilToInt(FoxPatch.GetSize().X / PatchCell);
	const int32 Rows = FMath::CeilToInt(FoxPatch.GetSize().Y / PatchCell);
	TArray<FVector2D> Corners;
	TArray<FVector2D> Spots;
	Corners.SetNumUninitialized((Columns + 1) * (Rows + 1));
	Spots.SetNumUninitialized(Corners.Num());
	for (int32 Row = 0; Row <= Rows; ++Row)
	{
		for (int32 Column = 0; Column <= Columns; ++Column)
		{
			const FVector2D Point = FVector2D::Min(FoxPatch.Min + FVector2D(Column, Row) * PatchCell, FoxPatch.Max);
			const int32 Index = Row * (Columns + 1) + Column;
			Spots[Index] = Point / ArtSize;
			Corners[Index] = ArtToScreen(Point + FoxNudge(Point, Time));
		}
	}
	TArray<FCanvasUVTri> Triangles;
	Triangles.Reserve(Columns * Rows * 2);
	auto AddTriangle = [&](int32 A, int32 B, int32 C)
	{
		FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
		Triangle.V0_Pos = Corners[A];
		Triangle.V1_Pos = Corners[B];
		Triangle.V2_Pos = Corners[C];
		Triangle.V0_UV = Spots[A];
		Triangle.V1_UV = Spots[B];
		Triangle.V2_UV = Spots[C];
		Triangle.V0_Color = Triangle.V1_Color = Triangle.V2_Color = FLinearColor::White;
	};
	for (int32 Row = 0; Row < Rows; ++Row)
	{
		for (int32 Column = 0; Column < Columns; ++Column)
		{
			const int32 TopLeft = Row * (Columns + 1) + Column;
			const int32 BottomLeft = TopLeft + Columns + 1;
			AddTriangle(TopLeft, TopLeft + 1, BottomLeft + 1);
			AddTriangle(TopLeft, BottomLeft + 1, BottomLeft);
		}
	}
	FCanvasTriangleItem Item(Triangles, BodyResource);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ARRRandomTitleHUD::DrawHUD()
{
	Super::DrawHUD();

	ARRRandomTitlePlayerController* Menu = Cast<ARRRandomTitlePlayerController>(PlayerOwner);
	if (!Canvas || !Menu)
	{
		return;
	}

	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float S = ScreenH / ReferenceHeight;
	UFont* LargeFont = GEngine->GetLargeFont();
	UFont* MediumFont = GEngine->GetMediumFont();
	UFont* SmallFont = GEngine->GetSmallFont();

	DrawArt(GetWorld()->GetRealTimeSeconds());
	const float ColumnX = Art ? ScreenW * ColumnShare : ScreenW * 0.5f;

	// Name and a short underline
	float Y = ScreenH * 0.12f;
	Y += DrawCenteredText(TEXT("RRRANDOM"), Accent, ColumnX, Y, LargeFont, 4.f * S);
	DrawRect(Accent, ColumnX - 80.f * S, Y + 4.f * S, 160.f * S, 4.f * S);
	Y += 18.f * S;
	DrawCenteredText(TEXT("TEAM BRAWL PROTOTYPE"), Muted, ColumnX, Y, MediumFont, S);

	// The view page asks how to play
	if (Menu->IsOnViewPage())
	{
		DrawCenteredText(TEXT("HOW DO YOU WANT TO PLAY?"), Muted, ColumnX, ScreenH * 0.36f, SmallFont, S);
	}
	// Which online service a network game would use
	else if (Menu->IsOnNetworkPage() && Sessions)
	{
		const FString Service = Sessions->IsUsingEOS()
			? TEXT("ONLINE VIA EPIC ONLINE SERVICES")
			: TEXT("ONLINE VIA LAN  (EOS keys not set: same PC or home network)");
		DrawCenteredText(Service, Muted, ColumnX, ScreenH * 0.36f, SmallFont, S);
	}

	// Buttons: lay them out, let a moving mouse select one, then draw
	const TArray<ERRTitleChoice> Choices = Menu->GetChoices();
	const float Left = ColumnX - ButtonWidth * S * 0.5f;
	const float Top = ScreenH * 0.42f;
	ButtonRects.Reset();
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		const FVector2D Min(Left, Top + Index * (ButtonHeight + ButtonGap) * S);
		ButtonRects.Add(FBox2D(Min, Min + FVector2D(ButtonWidth, ButtonHeight) * S));
	}

	float MouseX;
	float MouseY;
	if (PlayerOwner->GetMousePosition(MouseX, MouseY))
	{
		const FVector2D Mouse(MouseX, MouseY);
		if (!Mouse.Equals(LastMouse))
		{
			LastMouse = Mouse;
			const int32 Hovered = GetButtonAt(Mouse);
			if (Hovered != INDEX_NONE)
			{
				Menu->SetSelected(Hovered);
			}
		}
	}

	// Buttons fade while a host or join is under way
	const bool bBusy = Menu->IsBusy();
	const float ButtonAlpha = bBusy ? 0.35f : 1.f;
	for (int32 Index = 0; Index < Choices.Num(); ++Index)
	{
		const FBox2D& Rect = ButtonRects[Index];
		const FVector2D Size = Rect.GetSize();
		const bool bSelected = Index == Menu->GetSelected();
		const float Border = ButtonBorder * S;

		DrawRect(WithAlpha(bSelected ? Accent : ButtonEdge, ButtonAlpha), Rect.Min.X, Rect.Min.Y, Size.X, Size.Y);
		if (!bSelected)
		{
			DrawRect(WithAlpha(ButtonFill, ButtonAlpha), Rect.Min.X + Border, Rect.Min.Y + Border, Size.X - Border * 2.f, Size.Y - Border * 2.f);
		}

		const FString Label = ARRRandomTitlePlayerController::GetLabel(Choices[Index]);
		float TextW;
		float TextH;
		GetTextSize(Label, TextW, TextH, LargeFont, 1.2f * S);
		DrawText(Label, WithAlpha(bSelected ? SelectedText : FLinearColor::White, ButtonAlpha),
			Rect.Min.X + (Size.X - TextW) * 0.5f, Rect.Min.Y + (Size.Y - TextH) * 0.5f, LargeFont, 1.2f * S);
	}

	// What the selected button does, then the online status
	Y = (ButtonRects.Num() > 0 ? ButtonRects.Last().Max.Y : Top) + 22.f * S;
	if (Choices.IsValidIndex(Menu->GetSelected()) && !bBusy)
	{
		DrawCenteredText(ARRRandomTitlePlayerController::GetDescription(Choices[Menu->GetSelected()]), Muted, ColumnX, Y, MediumFont, S);
	}
	Y += 32.f * S;
	if (Sessions && !Sessions->GetStatus().IsEmpty())
	{
		Y += DrawCenteredText(Sessions->GetStatus(), StatusColor, ColumnX, Y, MediumFont, S) + 6.f * S;
	}
	if (bBusy)
	{
		DrawCenteredText(TEXT("[ESC] CANCEL"), Muted, ColumnX, Y, SmallFont, S);
	}

	const FString Keys = Menu->IsOnNetworkPage()
		? TEXT("[W/S] SELECT     [ENTER] OK     [ESC] BACK")
		: TEXT("[W/S] SELECT     [ENTER] OK");
	DrawCenteredText(Keys, Muted, ColumnX, ScreenH - 40.f * S, SmallFont, S);
}
