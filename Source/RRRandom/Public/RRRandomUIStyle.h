#pragma once

#include "CoreMinimal.h"

class UCanvas;
class UFont;
class UTexture;
class UTexture2D;

/**
 * The pastel look the game HUD and the title share (the picked "combo" proposal, 49th round): airy text with a soft
 * white halo and thin rounded bars over the game, frosted white rounded panels on the results screen, and the
 * character cards on the title. Holds the palette, the Nunito font (Content/Fonts/Nunito, drawn crisp at any pixel
 * size through a runtime font) and canvas shapes cut from two white sprites: /Game/UI/T_UICircle (crisp rounded
 * corners and circles) and /Game/UI/T_UISoft (soft shadows, halos and washes), made by
 * char-pipeline/scripts/make_ui_shapes.py.
 *
 * Sizes are screen pixels; callers scale them with the screen (720 pixels tall = 1).
 */
namespace RRUI
{
	// Palette, picked as screen (sRGB) colors
	extern const FLinearColor Ink;
	extern const FLinearColor InkSoft;
	extern const FLinearColor White;
	extern const FLinearColor Pink;
	extern const FLinearColor PinkDeep;
	extern const FLinearColor Mint;
	extern const FLinearColor MintDeep;
	extern const FLinearColor Lavender;
	extern const FLinearColor LavenderDeep;
	extern const FLinearColor Butter;
	extern const FLinearColor Gold;
	extern const FLinearColor Sky;
	extern const FLinearColor SkyDeep;
	/** Frosted panel fill (white, slightly see-through). */
	extern const FLinearColor Panel;
	/** The soft shadow under panels. */
	extern const FLinearColor Shadow;

	enum class EWeight : uint8
	{
		SemiBold,
		ExtraBold,
		Black,
	};

	/** The shape sprites, handed over by each HUD (which keeps them referenced) before it draws. */
	void UseShapes(UTexture2D* Circle, UTexture2D* Soft);

	/** Team colors in the palette: blue team sky, red team pink; the deep ones read as text. */
	FLinearColor TeamColor(int32 Team);
	FLinearColor TeamColorDeep(int32 Team);
	/** A game color (stats, weapon tiers) brought into the palette: lighter and softer, for bars and chip marks. */
	FLinearColor Pastel(const FLinearColor& Color, float Amount = 0.35f);
	/** A game color darkened toward the ink so it reads as text on white. */
	FLinearColor Readable(const FLinearColor& Color);
	FLinearColor WithAlpha(FLinearColor Color, float Alpha);

	/** Width and line height of text at a pixel size; Tracking adds pixels between letters. */
	FVector2D MeasureText(const FString& Text, float Size, EWeight Weight, float Tracking = 0.f);
	/**
	 * Text with its top-left at Pos; returns its size. Zoom scales the drawn glyphs without making a new font size,
	 * for text that grows and shrinks every frame (pops, countdowns).
	 */
	FVector2D DrawText(UCanvas* Canvas, const FString& Text, FVector2D Pos, float Size, EWeight Weight, const FLinearColor& Color, float Tracking = 0.f, float Zoom = 1.f);
	/**
	 * A soft white plate behind text that sits over the game: a see-through pill hugging Box (the letters), feathered
	 * at its edge. Size is the text's pixel size. Draw it before the text; one plate can carry a group of texts.
	 */
	void DrawPlate(UCanvas* Canvas, const FBox2D& Box, float Size, float Alpha = 1.f);
	/** Text on its own soft white plate (DrawPlate) so it reads over the game. */
	FVector2D DrawGlowText(UCanvas* Canvas, const FString& Text, FVector2D Pos, float Size, EWeight Weight, const FLinearColor& Color, float Tracking = 0.f, float Zoom = 1.f, float Glow = 1.f);
	/** Text colored letter by letter along a gradient through Stops (VICTORY), with the white halo. */
	FVector2D DrawGradientText(UCanvas* Canvas, const FString& Text, FVector2D Pos, float Size, EWeight Weight, TArrayView<const FLinearColor> Stops, float Alpha, float Tracking = 0.f, float Zoom = 1.f);

	/** Plain rectangle. */
	void DrawBox(UCanvas* Canvas, const FBox2D& Rect, const FLinearColor& Color);
	void DrawRounded(UCanvas* Canvas, const FBox2D& Rect, float Radius, const FLinearColor& Color);
	/** Rounded rectangle with fully round ends. */
	void DrawPill(UCanvas* Canvas, const FBox2D& Rect, const FLinearColor& Color);
	/** A soft shadow under Rect, Blur pixels wide, moved down by Drop. */
	void DrawSoftShadow(UCanvas* Canvas, const FBox2D& Rect, float Radius, float Blur, float Drop, const FLinearColor& Color);
	/** Frosted panel: a soft shadow, then a rounded fill. Scale is the screen scale (shadow size). */
	void DrawPanel(UCanvas* Canvas, const FBox2D& Rect, float Radius, float Scale, const FLinearColor& Fill = Panel);
	/** A soft-edged ellipse, e.g. a white wash that lifts text off the world. */
	void DrawWash(UCanvas* Canvas, const FVector2D& Center, const FVector2D& Radii, const FLinearColor& Color);
	void DrawDisc(UCanvas* Canvas, const FVector2D& Center, float Radius, const FLinearColor& Color);
	void DrawRing(UCanvas* Canvas, const FVector2D& Center, float Radius, float Thickness, const FLinearColor& Color);
	/** A ring broken into Dashes arcs, each Fill of its slot. */
	void DrawDashedRing(UCanvas* Canvas, const FVector2D& Center, float Radius, float Thickness, int32 Dashes, float Fill, const FLinearColor& Color);
	void DrawTriangle(UCanvas* Canvas, const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Color);
	/** A texture clipped to a rounded rectangle (Radius half the side for a circle): portraits, the minimap. */
	void DrawTextureRounded(UCanvas* Canvas, UTexture* Texture, const FBox2D& Rect, float Radius, const FLinearColor& Tint);
	/** A thin rounded bar: the track, then Share of it filled. */
	void DrawBar(UCanvas* Canvas, const FBox2D& Rect, float Share, const FLinearColor& Track, const FLinearColor& Fill);
	/** A key cap like [E] with its top-left at Pos; returns its width. */
	float DrawKey(UCanvas* Canvas, const FString& Key, const FVector2D& Pos, float Height, float Alpha = 1.f);
	/**
	 * A tag in the air line style: a short colored bar on the left, the label, and an optional muted detail after it;
	 * top-left at Pos, returns its size.
	 */
	FVector2D DrawTag(UCanvas* Canvas, const FString& Label, const FString& Detail, const FVector2D& Pos, float Size, const FLinearColor& Mark, float Alpha = 1.f);
	FVector2D MeasureTag(const FString& Label, const FString& Detail, float Size);
}
