#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RRRandomTitleHUD.generated.h"

class UFont;
class UMaterialInterface;
class UTexture2D;

/**
 * Title screen drawn on the canvas: the title art (a Tripo illustration of the fox blowing the smoke off its rifle
 * under a glowing die) filling the screen, brought to life with the game's own die spinning over the painted one
 * (a sprite sheet rendered in Blender) in a pulsing flash with twinkling sparkles, smoke drifting up from the
 * muzzle, the sea rippling and glinting, and the fox itself breathing, tilting its head, swinging its tail and flicking an ear (drawn on its own
 * layers over the scene without it, the tail turning whole and the body warped puppet style); then, in a column on the left
 * over a white wash, in the pastel style of RRRandomUIStyle: the game's name, the menu buttons of
 * ARRRandomTitlePlayerController (pills, or a row of portrait cards on the brawler page), what the selected one does, and
 * the online status while hosting or joining. Moving the mouse over a button selects it.
 * Without the art it falls back to a plain dark background.
 */
UCLASS()
class RRRANDOM_API ARRRandomTitleHUD : public AHUD
{
	GENERATED_BODY()

public:
	ARRRandomTitleHUD();

	virtual void DrawHUD() override;

	/** The button under a screen point as last drawn, or INDEX_NONE. */
	int32 GetButtonAt(const FVector2D& ScreenPoint) const;

private:
	/** Text in the pastel style's font (Weight is an RRUI::EWeight) centered on CenterX with its top at Y; returns its size. */
	FVector2D DrawCenteredText(const FString& Text, const FLinearColor& Color, float CenterX, float Y, float Size, int32 Weight, float Tracking);

	/** The art scaled to cover the screen (cropped evenly), and the animation on top of it. */
	void DrawArt(float Time);
	/** The fox on its layers: the tail swinging whole behind it, then the body breathing, tilting its head and flicking an ear (warped). */
	void DrawFox(float Time);
	/** A point of the art (in its own pixels) on screen, and the art's scale. */
	FVector2D ArtToScreen(const FVector2D& ArtPoint) const;

	UPROPERTY()
	TObjectPtr<UTexture2D> Art;

	/** The game's die spinning, frames in a grid (char-pipeline/scripts/blender_dice_spin_sheet.py). */
	UPROPERTY()
	TObjectPtr<UTexture2D> DiceSpin;

	/** White sprites for the flash, the sparkles and the smoke (char-pipeline/scripts/make_title_fx_textures.py). */
	UPROPERTY()
	TObjectPtr<UTexture2D> Glow;

	UPROPERTY()
	TObjectPtr<UTexture2D> Sparkle;

	UPROPERTY()
	TObjectPtr<UTexture2D> Puff;

	/** The art in layers, for the fox to move (char-pipeline/scripts/make_title_layers.py): the scene without it, its body and its tail. */
	UPROPERTY()
	TObjectPtr<UTexture2D> Back;

	UPROPERTY()
	TObjectPtr<UTexture2D> FoxBody;

	UPROPERTY()
	TObjectPtr<UTexture2D> FoxTail;

	/** Each brawler's HUD card picture (RRSpecies order), shown on the brawler page's buttons. */
	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> Portraits;

	/** White sprites for rounded shapes and soft shadows (RRRandomUIStyle). */
	UPROPERTY()
	TObjectPtr<UTexture2D> UICircle;

	UPROPERTY()
	TObjectPtr<UTexture2D> UISoft;

	/** The sea in the background rippling, flowing and glinting (char-pipeline/scripts/ue_make_title_water.py). */
	UPROPERTY()
	TObjectPtr<UMaterialInterface> Water;

	/** Where the art lands on screen this frame: its top-left corner and scale. */
	FVector2D ArtOrigin = FVector2D::ZeroVector;
	float ArtScale = 1.f;

	/** Button rectangles from the last frame, in screen pixels. */
	TArray<FBox2D> ButtonRects;

	/** Where the mouse was last frame; only moving it changes the selection, so the keys aren't overridden by a resting mouse. */
	FVector2D LastMouse = FVector2D(-1.f, -1.f);
};
