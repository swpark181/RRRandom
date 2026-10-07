#include "RRRandomTitleHUD.h"
#include "RRRandomSessionSubsystem.h"
#include "RRRandomTitlePlayerController.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/GameInstance.h"

namespace
{
	// Sizes for a 720 pixel tall screen; everything scales with the screen height
	constexpr float ReferenceHeight = 720.f;
	constexpr float ButtonWidth = 340.f;
	constexpr float ButtonHeight = 50.f;
	constexpr float ButtonGap = 14.f;
	constexpr float ButtonBorder = 2.f;

	// Picked as screen (sRGB) colors; the canvas takes linear ones
	const FLinearColor Background = FLinearColor::FromSRGBColor(FColor(14, 16, 24));
	const FLinearColor Accent = FLinearColor::FromSRGBColor(FColor(255, 196, 56));
	const FLinearColor ButtonFill = FLinearColor::FromSRGBColor(FColor(30, 33, 44));
	const FLinearColor ButtonEdge = FLinearColor::FromSRGBColor(FColor(72, 78, 96));
	const FLinearColor SelectedText = FLinearColor::FromSRGBColor(FColor(24, 18, 6));
	const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor(140, 146, 162));
	const FLinearColor StatusColor = FLinearColor::FromSRGBColor(FColor(255, 214, 110));

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}
}

int32 ARRRandomTitleHUD::GetButtonAt(const FVector2D& ScreenPoint) const
{
	return ButtonRects.IndexOfByPredicate([&ScreenPoint](const FBox2D& Rect) { return Rect.IsInside(ScreenPoint); });
}

float ARRRandomTitleHUD::DrawCenteredText(const FString& Text, const FLinearColor& Color, float Y, UFont* Font, float Scale)
{
	float Width;
	float Height;
	GetTextSize(Text, Width, Height, Font, Scale);
	const float X = (Canvas->ClipX - Width) * 0.5f;
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + Scale, Y + Scale, Font, Scale);
	DrawText(Text, Color, X, Y, Font, Scale);
	return Height;
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

	DrawRect(Background, 0.f, 0.f, ScreenW, ScreenH);

	// Name and a short underline
	float Y = ScreenH * 0.12f;
	Y += DrawCenteredText(TEXT("RRRANDOM"), Accent, Y, LargeFont, 4.f * S);
	DrawRect(Accent, (ScreenW - 160.f * S) * 0.5f, Y + 4.f * S, 160.f * S, 4.f * S);
	Y += 18.f * S;
	DrawCenteredText(TEXT("TEAM BRAWL PROTOTYPE"), Muted, Y, MediumFont, S);

	// Which online service a network game would use
	if (Menu->IsOnNetworkPage() && Sessions)
	{
		const FString Service = Sessions->IsUsingEOS()
			? TEXT("ONLINE VIA EPIC ONLINE SERVICES")
			: TEXT("ONLINE VIA LAN  (EOS keys not set: same PC or home network)");
		DrawCenteredText(Service, Muted, ScreenH * 0.36f, SmallFont, S);
	}

	// Buttons: lay them out, let a moving mouse select one, then draw
	const TArray<ERRTitleChoice> Choices = Menu->GetChoices();
	const float Left = (ScreenW - ButtonWidth * S) * 0.5f;
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
		DrawCenteredText(ARRRandomTitlePlayerController::GetDescription(Choices[Menu->GetSelected()]), Muted, Y, MediumFont, S);
	}
	Y += 32.f * S;
	if (Sessions && !Sessions->GetStatus().IsEmpty())
	{
		Y += DrawCenteredText(Sessions->GetStatus(), StatusColor, Y, MediumFont, S) + 6.f * S;
	}
	if (bBusy)
	{
		DrawCenteredText(TEXT("[ESC] CANCEL"), Muted, Y, SmallFont, S);
	}

	const FString Keys = Menu->IsOnNetworkPage()
		? TEXT("[W/S] SELECT     [ENTER] OK     [ESC] BACK")
		: TEXT("[W/S] SELECT     [ENTER] OK");
	DrawCenteredText(Keys, Muted, ScreenH - 40.f * S, SmallFont, S);
}
