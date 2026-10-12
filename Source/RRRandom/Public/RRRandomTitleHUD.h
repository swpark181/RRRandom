#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RRRandomTitleHUD.generated.h"

class UFont;

/**
 * Title screen drawn on the canvas (no widgets or assets): the game's name, the menu buttons of
 * ARRRandomTitlePlayerController, what the selected one does, and the online status while hosting or joining.
 * Moving the mouse over a button selects it.
 */
UCLASS()
class RRRANDOM_API ARRRandomTitleHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/** The button under a screen point as last drawn, or INDEX_NONE. */
	int32 GetButtonAt(const FVector2D& ScreenPoint) const;

private:
	/** Text centered across the screen with its top at Y; returns its height. */
	float DrawCenteredText(const FString& Text, const FLinearColor& Color, float Y, UFont* Font, float Scale);

	/** Button rectangles from the last frame, in screen pixels. */
	TArray<FBox2D> ButtonRects;

	/** Where the mouse was last frame; only moving it changes the selection, so the keys aren't overridden by a resting mouse. */
	FVector2D LastMouse = FVector2D(-1.f, -1.f);
};
