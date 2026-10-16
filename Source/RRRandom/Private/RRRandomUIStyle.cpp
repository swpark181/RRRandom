#include "RRRandomUIStyle.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GlobalRenderResources.h"
#include "Misc/Paths.h"
#include "Rendering/SlateRenderer.h"
#include "UObject/Package.h"

namespace RRUI
{
	const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(74, 63, 92));
	const FLinearColor InkSoft = FLinearColor::FromSRGBColor(FColor(122, 112, 138));
	const FLinearColor White = FLinearColor::White;
	const FLinearColor Pink = FLinearColor::FromSRGBColor(FColor(247, 168, 196));
	const FLinearColor PinkDeep = FLinearColor::FromSRGBColor(FColor(242, 127, 166));
	const FLinearColor Mint = FLinearColor::FromSRGBColor(FColor(143, 221, 190));
	const FLinearColor MintDeep = FLinearColor::FromSRGBColor(FColor(124, 203, 172));
	const FLinearColor Lavender = FLinearColor::FromSRGBColor(FColor(189, 166, 240));
	const FLinearColor LavenderDeep = FLinearColor::FromSRGBColor(FColor(154, 123, 224));
	const FLinearColor Butter = FLinearColor::FromSRGBColor(FColor(247, 216, 138));
	const FLinearColor Gold = FLinearColor::FromSRGBColor(FColor(233, 169, 63));
	const FLinearColor Sky = FLinearColor::FromSRGBColor(FColor(151, 201, 245));
	const FLinearColor SkyDeep = FLinearColor::FromSRGBColor(FColor(102, 165, 230));
	const FLinearColor Panel = FLinearColor::FromSRGBColor(FColor(255, 255, 255, 214));
	const FLinearColor Shadow = FLinearColor::FromSRGBColor(FColor(90, 74, 110, 46));
}

namespace
{
	// The HUD's sprites for this frame (kept referenced by the HUD) and the font, built once and kept for the session
	UTexture2D* CircleSprite = nullptr;
	UTexture2D* SoftSprite = nullptr;
	UFont* Nunito = nullptr;

	const FName WeightNames[] = { TEXT("SemiBold"), TEXT("ExtraBold"), TEXT("Black") };
	const TCHAR* const WeightFiles[] = { TEXT("Nunito-SemiBold.ttf"), TEXT("Nunito-ExtraBold.ttf"), TEXT("Nunito-Black.ttf") };
	// Slate sizes fonts in points at 96 DPI; a pixel size is three quarters of that in points
	constexpr float PointsPerPixel = 0.75f;
	// Rounded corners and circles are drawn as fans with this many steps per quarter turn
	constexpr int32 ArcSteps = 8;

	UFont* GetFont()
	{
		if (!Nunito)
		{
			// A runtime font over the TTF files, so every size is rasterized sharp (the canvas's offline fonts only scale)
			Nunito = NewObject<UFont>(GetTransientPackage(), TEXT("RRRandomNunito"));
			Nunito->AddToRoot();
			Nunito->FontCacheType = EFontCacheType::Runtime;
			Nunito->LegacyFontSize = 16;
			const FString Folder = FPaths::ProjectContentDir() / TEXT("Fonts/Nunito");
			FTypeface& Typeface = Nunito->GetMutableInternalCompositeFont().DefaultTypeface;
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(WeightNames); ++Index)
			{
				Typeface.AppendFont(WeightNames[Index], Folder / WeightFiles[Index], EFontHinting::Default, EFontLoadingPolicy::LazyLoad);
			}
			Nunito->LegacyFontName = WeightNames[1];
		}
		return Nunito;
	}

	FSlateFontInfo MakeFont(float Size, RRUI::EWeight Weight)
	{
		// Whole points only, so the font cache keeps a few sizes instead of one per frame
		return FSlateFontInfo(GetFont(), FMath::Max(1.f, FMath::RoundToFloat(Size * PointsPerPixel)), WeightNames[static_cast<int32>(Weight)]);
	}

	FTexture* ResourceOf(UTexture2D* Texture)
	{
		FTexture* Resource = Texture ? Texture->GetResource() : nullptr;
		return Resource ? Resource : GWhiteTexture;
	}

	void Tile(UCanvas* Canvas, FTexture* Texture, float X, float Y, float W, float H, float U0, float V0, float U1, float V1, const FLinearColor& Color)
	{
		if (W <= 0.f || H <= 0.f || Color.A <= 0.f)
		{
			return;
		}
		FCanvasTileItem Item(FVector2D(X, Y), Texture, FVector2D(W, H), FVector2D(U0, V0), FVector2D(U1, V1), Color);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}

	/**
	 * Nine slices of a round sprite over Rect: its quarters at the corners (Corner pixels square) and its middle lines
	 * stretched along the edges and over the middle. No piece overlaps another, so see-through colors stay even.
	 */
	void NineSlice(UCanvas* Canvas, UTexture2D* Sprite, const FBox2D& Rect, float Corner, const FLinearColor& Color)
	{
		const FVector2D Size = Rect.GetSize();
		if (Size.X <= 0.f || Size.Y <= 0.f)
		{
			return;
		}
		FTexture* Texture = ResourceOf(Sprite);
		const float R = FMath::Min(Corner, FMath::Min(Size.X, Size.Y) * 0.5f);
		const float X0 = Rect.Min.X;
		const float X1 = X0 + R;
		const float X3 = Rect.Max.X;
		const float X2 = X3 - R;
		const float Y0 = Rect.Min.Y;
		const float Y1 = Y0 + R;
		const float Y3 = Rect.Max.Y;
		const float Y2 = Y3 - R;
		Tile(Canvas, Texture, X0, Y0, R, R, 0.f, 0.f, 0.5f, 0.5f, Color);
		Tile(Canvas, Texture, X2, Y0, R, R, 0.5f, 0.f, 1.f, 0.5f, Color);
		Tile(Canvas, Texture, X0, Y2, R, R, 0.f, 0.5f, 0.5f, 1.f, Color);
		Tile(Canvas, Texture, X2, Y2, R, R, 0.5f, 0.5f, 1.f, 1.f, Color);
		Tile(Canvas, Texture, X1, Y0, X2 - X1, R, 0.5f, 0.f, 0.5f, 0.5f, Color);
		Tile(Canvas, Texture, X1, Y2, X2 - X1, R, 0.5f, 0.5f, 0.5f, 1.f, Color);
		Tile(Canvas, Texture, X0, Y1, R, Y2 - Y1, 0.f, 0.5f, 0.5f, 0.5f, Color);
		Tile(Canvas, Texture, X2, Y1, R, Y2 - Y1, 0.5f, 0.5f, 1.f, 0.5f, Color);
		Tile(Canvas, Texture, X1, Y1, X2 - X1, Y2 - Y1, 0.5f, 0.5f, 0.5f, 0.5f, Color);
	}

	FBox2D Snapped(const FBox2D& Rect)
	{
		return FBox2D(FVector2D(FMath::RoundToFloat(Rect.Min.X), FMath::RoundToFloat(Rect.Min.Y)), FVector2D(FMath::RoundToFloat(Rect.Max.X), FMath::RoundToFloat(Rect.Max.Y)));
	}

	void DrawTris(UCanvas* Canvas, TArray<FCanvasUVTri>& Triangles, FTexture* Texture)
	{
		if (Triangles.Num() == 0)
		{
			return;
		}
		FCanvasTriangleItem Item(Triangles, Texture);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}
}

namespace RRUI
{
	void UseShapes(UTexture2D* Circle, UTexture2D* Soft)
	{
		CircleSprite = Circle;
		SoftSprite = Soft;
	}

	FLinearColor TeamColor(int32 Team)
	{
		return Team == 0 ? Sky : Pink;
	}

	FLinearColor TeamColorDeep(int32 Team)
	{
		return Team == 0 ? SkyDeep : PinkDeep;
	}

	FLinearColor Pastel(const FLinearColor& Color, float Amount)
	{
		FLinearColor Out = FMath::Lerp(Color, FLinearColor::White, Amount);
		Out.A = Color.A;
		return Out;
	}

	FLinearColor Readable(const FLinearColor& Color)
	{
		FLinearColor Out = FMath::Lerp(Color, Ink, 0.35f);
		Out.A = Color.A;
		return Out;
	}

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	FVector2D MeasureText(const FString& Text, float Size, EWeight Weight, float Tracking)
	{
		if (Text.IsEmpty() || !FSlateApplication::IsInitialized())
		{
			return FVector2D(0.f, Size * 1.36f);
		}
		const FVector2D Measured = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, MakeFont(Size, Weight), 1.f);
		return FVector2D(Measured.X + Tracking * FMath::Max(0, Text.Len() - 1), Measured.Y);
	}

	FVector2D DrawText(UCanvas* Canvas, const FString& Text, FVector2D Pos, float Size, EWeight Weight, const FLinearColor& Color, float Tracking, float Zoom)
	{
		if (Text.IsEmpty())
		{
			return FVector2D::ZeroVector;
		}
		if (Color.A > 0.f)
		{
			FCanvasTextItem Item(FVector2D(FMath::RoundToFloat(Pos.X), FMath::RoundToFloat(Pos.Y)), FText::FromString(Text), MakeFont(Size, Weight), Color);
			Item.Scale = FVector2D(Zoom, Zoom);
			// The item scales the letter spacing along with the glyphs
			Item.HorizSpacingAdjust = Tracking;
			Canvas->DrawItem(Item);
		}
		return MeasureText(Text, Size, Weight, Tracking) * Zoom;
	}

	/** A ring of white copies of the text Radius pixels out, Taps of them. */
	void DrawTextRing(UCanvas* Canvas, const FString& Text, FVector2D Pos, float Size, EWeight Weight, const FLinearColor& Color, float Radius, int32 Taps, float Tracking, float Zoom)
	{
		for (int32 Step = 0; Step < Taps; ++Step)
		{
			const float Angle = (Step + 0.5f) * 2.f * PI / Taps;
			DrawText(Canvas, Text, Pos + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius, Size, Weight, Color, Tracking, Zoom);
		}
	}

	void DrawPlate(UCanvas* Canvas, const FBox2D& Box, float Size, float Alpha)
	{
		// A see-through white pill hugging the text, feathered out with a soft edge so it reads as a glow, not a box
		if (Alpha <= 0.f || !Box.bIsValid)
		{
			return;
		}
		const FBox2D Core(Box.Min - FVector2D(Size * 0.35f, Size * 0.02f), Box.Max + FVector2D(Size * 0.35f, Size * 0.02f));
		NineSlice(Canvas, SoftSprite, Core.ExpandBy(FVector2D(Size * 0.8f, Size * 0.55f)), Size * 1.1f, FLinearColor(1.f, 1.f, 1.f, 0.4f * Alpha));
		DrawPill(Canvas, Core, FLinearColor(1.f, 1.f, 1.f, 0.5f * Alpha));
	}

	FVector2D DrawGlowText(UCanvas* Canvas, const FString& Text, FVector2D Pos, float Size, EWeight Weight, const FLinearColor& Color, float Tracking, float Zoom, float Glow)
	{
		const FVector2D Box = MeasureText(Text, Size, Weight, Tracking) * Zoom;
		// The plate covers the letters' body (the line box has room above and below them)
		DrawPlate(Canvas, FBox2D(Pos + FVector2D(0.f, Box.Y * 0.14f), Pos + FVector2D(Box.X, Box.Y * 0.86f)), Size * Zoom, Color.A * Glow);
		return DrawText(Canvas, Text, Pos, Size, Weight, Color, Tracking, Zoom);
	}

	FVector2D DrawGradientText(UCanvas* Canvas, const FString& Text, FVector2D Pos, float Size, EWeight Weight, TArrayView<const FLinearColor> Stops, float Alpha, float Tracking, float Zoom)
	{
		// A soft ink shadow and a smooth white rim for the whole word first, so letters don't cover each other's edge
		const float Rim = FMath::Max(1.f, Size * Zoom * 0.045f);
		DrawText(Canvas, Text, Pos + FVector2D(0.f, Size * Zoom * 0.06f), Size, Weight, WithAlpha(Ink, 0.2f * Alpha), Tracking, Zoom);
		DrawTextRing(Canvas, Text, Pos, Size, Weight, FLinearColor(1.f, 1.f, 1.f, 0.25f * Alpha), Rim * 2.f, 12, Tracking, Zoom);
		DrawTextRing(Canvas, Text, Pos, Size, Weight, FLinearColor(1.f, 1.f, 1.f, 0.9f * Alpha), Rim, 12, Tracking, Zoom);
		const int32 Count = Text.Len();
		for (int32 Index = 0; Index < Count && Stops.Num() > 0; ++Index)
		{
			const float T = Count > 1 ? float(Index) / (Count - 1) : 0.f;
			const float Spot = T * (Stops.Num() - 1);
			const int32 From = FMath::Min(FMath::FloorToInt(Spot), Stops.Num() - 1);
			const int32 To = FMath::Min(From + 1, Stops.Num() - 1);
			const FLinearColor Color = WithAlpha(FMath::Lerp(Stops[From], Stops[To], Spot - From), Alpha);
			// Where this letter starts: the width of the text before it, with its spacing
			const float Before = Index > 0 ? MeasureText(Text.Left(Index), Size, Weight, Tracking).X + Tracking : 0.f;
			DrawText(Canvas, Text.Mid(Index, 1), Pos + FVector2D(Before * Zoom, 0.f), Size, Weight, Color, Tracking, Zoom);
		}
		return MeasureText(Text, Size, Weight, Tracking) * Zoom;
	}

	void DrawBox(UCanvas* Canvas, const FBox2D& Rect, const FLinearColor& Color)
	{
		const FBox2D Box = Snapped(Rect);
		Tile(Canvas, GWhiteTexture, Box.Min.X, Box.Min.Y, Box.GetSize().X, Box.GetSize().Y, 0.f, 0.f, 1.f, 1.f, Color);
	}

	void DrawRounded(UCanvas* Canvas, const FBox2D& Rect, float Radius, const FLinearColor& Color)
	{
		NineSlice(Canvas, CircleSprite, Snapped(Rect), FMath::RoundToFloat(Radius), Color);
	}

	void DrawPill(UCanvas* Canvas, const FBox2D& Rect, const FLinearColor& Color)
	{
		DrawRounded(Canvas, Rect, Rect.GetSize().Y * 0.5f, Color);
	}

	void DrawSoftShadow(UCanvas* Canvas, const FBox2D& Rect, float Radius, float Blur, float Drop, const FLinearColor& Color)
	{
		const FBox2D Spread(Rect.Min - FVector2D(Blur, Blur - Drop), Rect.Max + FVector2D(Blur, Blur + Drop));
		NineSlice(Canvas, SoftSprite, Spread, Radius + Blur * 2.f, Color);
	}

	void DrawPanel(UCanvas* Canvas, const FBox2D& Rect, float Radius, float Scale, const FLinearColor& Fill)
	{
		DrawSoftShadow(Canvas, Rect, Radius, 12.f * Scale, 4.f * Scale, WithAlpha(Shadow, Fill.A));
		DrawRounded(Canvas, Rect, Radius, Fill);
	}

	void DrawWash(UCanvas* Canvas, const FVector2D& Center, const FVector2D& Radii, const FLinearColor& Color)
	{
		Tile(Canvas, ResourceOf(SoftSprite), Center.X - Radii.X, Center.Y - Radii.Y, Radii.X * 2.f, Radii.Y * 2.f, 0.f, 0.f, 1.f, 1.f, Color);
	}

	void DrawDisc(UCanvas* Canvas, const FVector2D& Center, float Radius, const FLinearColor& Color)
	{
		Tile(Canvas, ResourceOf(CircleSprite), Center.X - Radius, Center.Y - Radius, Radius * 2.f, Radius * 2.f, 0.f, 0.f, 1.f, 1.f, Color);
	}

	void DrawDashedRing(UCanvas* Canvas, const FVector2D& Center, float Radius, float Thickness, int32 Dashes, float Fill, const FLinearColor& Color)
	{
		// Each dash (or the whole ring) as a strip of quads between the inner and outer circles
		const float Inner = FMath::Max(0.f, Radius - Thickness * 0.5f);
		const float Outer = Radius + Thickness * 0.5f;
		const int32 Slots = FMath::Max(1, Dashes);
		const int32 Steps = FMath::Max(2, ArcSteps * 4 / Slots);
		TArray<FCanvasUVTri> Triangles;
		for (int32 Slot = 0; Slot < Slots; ++Slot)
		{
			const float Start = 2.f * PI * Slot / Slots;
			const float Span = 2.f * PI / Slots * FMath::Clamp(Fill, 0.f, 1.f);
			for (int32 Step = 0; Step < Steps; ++Step)
			{
				const float A = Start + Span * Step / Steps;
				const float B = Start + Span * (Step + 1) / Steps;
				const FVector2D DA(FMath::Cos(A), FMath::Sin(A));
				const FVector2D DB(FMath::Cos(B), FMath::Sin(B));
				FCanvasUVTri& First = Triangles.AddDefaulted_GetRef();
				First.V0_Pos = Center + DA * Inner;
				First.V1_Pos = Center + DA * Outer;
				First.V2_Pos = Center + DB * Outer;
				FCanvasUVTri& Second = Triangles.AddDefaulted_GetRef();
				Second.V0_Pos = Center + DA * Inner;
				Second.V1_Pos = Center + DB * Outer;
				Second.V2_Pos = Center + DB * Inner;
			}
		}
		for (FCanvasUVTri& Triangle : Triangles)
		{
			Triangle.V0_Color = Triangle.V1_Color = Triangle.V2_Color = Color;
		}
		DrawTris(Canvas, Triangles, GWhiteTexture);
	}

	void DrawRing(UCanvas* Canvas, const FVector2D& Center, float Radius, float Thickness, const FLinearColor& Color)
	{
		DrawDashedRing(Canvas, Center, Radius, Thickness, 1, 1.f, Color);
	}

	void DrawTriangle(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color)
	{
		FCanvasTriangleItem Item(A, B, C, GWhiteTexture);
		Item.SetColor(Color);
		Item.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(Item);
	}

	void DrawTextureRounded(UCanvas* Canvas, UTexture* Texture, const FBox2D& Rect, float Radius, const FLinearColor& Tint)
	{
		FTexture* Resource = Texture ? Texture->GetResource() : nullptr;
		const FVector2D Size = Rect.GetSize();
		if (!Resource || Size.X <= 0.f || Size.Y <= 0.f)
		{
			return;
		}
		// The outline: a quarter arc at each corner (they meet in a circle when Radius is half the side), filled as a
		// fan from the middle; each point takes the texture where it sits in the rectangle
		const float R = FMath::Min(Radius, FMath::Min(Size.X, Size.Y) * 0.5f);
		const FVector2D Corners[] = { FVector2D(Rect.Max.X - R, Rect.Max.Y - R), FVector2D(Rect.Min.X + R, Rect.Max.Y - R),
			FVector2D(Rect.Min.X + R, Rect.Min.Y + R), FVector2D(Rect.Max.X - R, Rect.Min.Y + R) };
		TArray<FVector2D> Outline;
		for (int32 Corner = 0; Corner < 4; ++Corner)
		{
			for (int32 Step = 0; Step <= ArcSteps; ++Step)
			{
				const float Angle = (Corner + float(Step) / ArcSteps) * PI * 0.5f;
				Outline.Add(Corners[Corner] + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * R);
			}
		}
		const FVector2D Middle = Rect.GetCenter();
		auto UV = [&Rect, &Size](const FVector2D& Point) { return (Point - Rect.Min) / Size; };
		TArray<FCanvasUVTri> Triangles;
		for (int32 Index = 0; Index < Outline.Num(); ++Index)
		{
			const FVector2D& A = Outline[Index];
			const FVector2D& B = Outline[(Index + 1) % Outline.Num()];
			FCanvasUVTri& Triangle = Triangles.AddDefaulted_GetRef();
			Triangle.V0_Pos = Middle;
			Triangle.V1_Pos = A;
			Triangle.V2_Pos = B;
			Triangle.V0_UV = UV(Middle);
			Triangle.V1_UV = UV(A);
			Triangle.V2_UV = UV(B);
			Triangle.V0_Color = Triangle.V1_Color = Triangle.V2_Color = Tint;
		}
		DrawTris(Canvas, Triangles, Resource);
	}

	void DrawBar(UCanvas* Canvas, const FBox2D& Rect, float Share, const FLinearColor& Track, const FLinearColor& Fill)
	{
		DrawPill(Canvas, Rect, Track);
		const FVector2D Size = Rect.GetSize();
		const float Clamped = FMath::Clamp(Share, 0.f, 1.f);
		if (Clamped <= 0.f)
		{
			return;
		}
		// Never thinner than round: a nearly empty bar is a dot at the start
		const float Width = FMath::Max(Size.Y, Size.X * Clamped);
		DrawPill(Canvas, FBox2D(Rect.Min, FVector2D(Rect.Min.X + Width, Rect.Max.Y)), Fill);
	}

	float DrawKey(UCanvas* Canvas, const FString& Key, const FVector2D& Pos, float Height, float Alpha)
	{
		const float TextSize = Height * 0.62f;
		const FVector2D Label = MeasureText(Key, TextSize, EWeight::Black);
		const float Width = FMath::Max(Height, Label.X + Height * 0.5f);
		const FBox2D Rect(Pos, Pos + FVector2D(Width, Height));
		const float Edge = FMath::Max(1.f, Height * 0.075f);
		DrawRounded(Canvas, Rect, Height * 0.26f, WithAlpha(Ink, 0.85f * Alpha));
		DrawRounded(Canvas, FBox2D(Rect.Min + FVector2D(Edge, Edge), Rect.Max - FVector2D(Edge, Edge)), Height * 0.26f - Edge, WithAlpha(FLinearColor(1.f, 1.f, 1.f, 0.92f), Alpha));
		DrawText(Canvas, Key, Pos + FVector2D((Width - Label.X) * 0.5f, (Height - Label.Y) * 0.5f), TextSize, EWeight::Black, WithAlpha(Ink, Alpha));
		return Width;
	}

	FVector2D MeasureTag(const FString& Label, const FString& Detail, float Size)
	{
		const float Tracking = Size * 0.08f;
		const FVector2D Main = MeasureText(Label, Size, EWeight::ExtraBold, Tracking);
		float Width = Size * 0.75f + Main.X;
		if (!Detail.IsEmpty())
		{
			Width += Size * 0.45f + MeasureText(Detail, Size, EWeight::ExtraBold, Tracking).X;
		}
		return FVector2D(Width, Main.Y);
	}

	FVector2D DrawTag(UCanvas* Canvas, const FString& Label, const FString& Detail, const FVector2D& Pos, float Size, const FLinearColor& Mark, float Alpha)
	{
		const float Tracking = Size * 0.08f;
		const FVector2D Whole = MeasureTag(Label, Detail, Size);
		DrawPlate(Canvas, FBox2D(Pos + FVector2D(0.f, Whole.Y * 0.14f), Pos + FVector2D(Whole.X, Whole.Y * 0.86f)), Size, Alpha);
		const float Bar = FMath::Max(2.f, Size * 0.25f);
		DrawRounded(Canvas, FBox2D(Pos + FVector2D(0.f, Whole.Y * 0.14f), Pos + FVector2D(Bar, Whole.Y * 0.86f)), Bar * 0.5f, WithAlpha(Mark, Alpha));
		float X = Pos.X + Size * 0.75f;
		X += DrawText(Canvas, Label, FVector2D(X, Pos.Y), Size, EWeight::ExtraBold, WithAlpha(Ink, Alpha), Tracking).X;
		if (!Detail.IsEmpty())
		{
			X += Size * 0.45f;
			X += DrawText(Canvas, Detail, FVector2D(X, Pos.Y), Size, EWeight::ExtraBold, WithAlpha(InkSoft, Alpha), Tracking).X;
		}
		return FVector2D(X - Pos.X, Whole.Y);
	}
}
