#include "RRRandomHUD.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomGameState.h"
#include "RRRandomOverheadDieComponent.h"
#include "RRRandomSessionSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "CanvasItem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "GlobalRenderResources.h"

namespace
{
	// Screen-corner dice panel
	constexpr float Margin = 20.f;
	constexpr float BarWidth = 220.f;
	constexpr float BarHeight = 18.f;
	constexpr float LineHeight = 20.f;

	// Over-the-head indicator, in screen pixels except HeadClearance (world units above the capsule)
	constexpr float HeadClearance = 40.f;
	// Extra screen-space lift so the bars clear the mannequin's head under the tilted camera
	constexpr float OverheadLift = 30.f;
	constexpr float HealthBarWidth = 80.f;
	constexpr float HealthBarHeight = 8.f;
	constexpr float AmmoBarHeight = 5.f;
	constexpr float GaugeBarHeight = 3.f;
	constexpr float DiceBoxSize = 15.f;
	// Friend-or-foe marker: a downward triangle between the bars and the head
	constexpr float MarkerWidth = 16.f;
	constexpr float MarkerHeight = 10.f;
	constexpr float MarkerBorder = 2.f;
	constexpr float ChipPadding = 3.f;
	// The roll's result label, from the moment its die lands; it goes when the die does
	constexpr float RollPopupTime = URRRandomOverheadDieComponent::HoldTime + URRRandomOverheadDieComponent::ExitTime;
	constexpr float RollPopupFadeTime = URRRandomOverheadDieComponent::ExitTime;
	// Buffs about to run out blink
	constexpr float BuffBlinkTime = 3.f;
	constexpr float DamagePopupRiseSpeed = 70.f;

	const FLinearColor Backing(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor DiceColor(1.f, 0.85f, 0.1f);
	const FLinearColor EmptyDiceColor(0.25f, 0.25f, 0.25f, 0.8f);
	const FLinearColor AmmoColor(1.f, 0.55f, 0.1f);
	const FLinearColor ReloadingAmmoColor(0.55f, 0.3f, 0.05f);
	const FLinearColor SelfHealthColor(0.3f, 1.f, 0.3f);
	const FLinearColor AllyHealthColor(0.25f, 0.6f, 1.f);
	const FLinearColor EnemyHealthColor(1.f, 0.25f, 0.2f);
	const FLinearColor FriendlyDamageColor(1.f, 0.3f, 0.3f);
	const FLinearColor WeaponDieColor(1.f, 1.f, 1.f);
	// Magazines bigger than this show as one bar instead of a segment per round
	constexpr int32 MaxAmmoSegments = 20;

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	FLinearColor ScoreColor(int32 Team)
	{
		return FMath::Lerp(ARRRandomCharacter::GetTeamColor(Team), FLinearColor::White, 0.35f);
	}
}

void ARRRandomHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const ARRRandomCharacter* Viewer = Cast<ARRRandomCharacter>(GetOwningPawn());
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		DrawOverhead(**It, Viewer);
	}

	DrawScore();
	DrawOnlinePanel();
	if (Viewer)
	{
		DrawDicePanel(*Viewer);
		if (Viewer->IsAlive())
		{
			DrawAmmoPanel(*Viewer);
		}
		else
		{
			DrawRespawnCountdown(*Viewer);
		}
	}
}

void ARRRandomHUD::DrawShadowedText(const FString& Text, const FLinearColor& Color, float X, float Y, UFont* Font)
{
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + 1.f, Y + 1.f, Font);
	DrawText(Text, Color, X, Y, Font);
}

void ARRRandomHUD::DrawFriendOrFoeMarker(float CenterX, float Top, const FLinearColor& Color)
{
	const float HalfWidth = MarkerWidth * 0.5f;
	// Border: the same triangle grown by MarkerBorder on every side, drawn first
	FCanvasTriangleItem Border(FVector2D(CenterX - HalfWidth - MarkerBorder * 1.7f, Top - MarkerBorder),
		FVector2D(CenterX + HalfWidth + MarkerBorder * 1.7f, Top - MarkerBorder),
		FVector2D(CenterX, Top + MarkerHeight + MarkerBorder * 1.9f), GWhiteTexture);
	Border.SetColor(Backing);
	Border.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Border);

	FCanvasTriangleItem Fill(FVector2D(CenterX - HalfWidth, Top), FVector2D(CenterX + HalfWidth, Top),
		FVector2D(CenterX, Top + MarkerHeight), GWhiteTexture);
	Fill.SetColor(Color);
	Canvas->DrawItem(Fill);
}

void ARRRandomHUD::DrawOverhead(const ARRRandomCharacter& Brawler, const ARRRandomCharacter* Viewer)
{
	DrawDamagePopups(Brawler, Viewer);
	if (!Brawler.IsAlive())
	{
		return;
	}

	const float HalfHeight = Brawler.GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector HeadWorld = Brawler.GetActorLocation() + FVector(0.f, 0.f, HalfHeight + HeadClearance);
	const FVector Head = Project(HeadWorld);
	if (Head.Z <= 0.f)
	{
		return;
	}

	const bool bIsViewer = &Brawler == Viewer;
	const bool bIsAlly = Viewer && Brawler.GetTeam() == Viewer->GetTeam();
	UFont* Font = GEngine->GetSmallFont();
	const float Left = Head.X - HealthBarWidth * 0.5f;
	const float Top = Head.Y - OverheadLift;

	// Health bar: green for the player, blue for allies, red for opponents, with the number above it
	const float HealthShare = FMath::Clamp(Brawler.GetHealth() / FMath::Max(1.f, Brawler.GetMaxHealth()), 0.f, 1.f);
	const FLinearColor HealthColor = bIsViewer ? SelfHealthColor : (bIsAlly ? AllyHealthColor : EnemyHealthColor);
	DrawRect(Backing, Left - 1.f, Top - 1.f, HealthBarWidth + 2.f, HealthBarHeight + 2.f);
	DrawRect(HealthColor, Left, Top, HealthBarWidth * HealthShare, HealthBarHeight);

	const FString HealthText = FString::FromInt(FMath::CeilToInt(Brawler.GetHealth()));
	float TextWidth = 0.f;
	float TextHeight = 0.f;
	GetTextSize(HealthText, TextWidth, TextHeight, Font);
	const float HealthTextY = Top - TextHeight - 1.f;
	DrawShadowedText(HealthText, FLinearColor::White, Head.X - TextWidth * 0.5f, HealthTextY, Font);

	// Stored dice: a yellow box with the count, right of the health bar
	const URRRandomDiceBuffComponent* Dice = Brawler.GetDiceBuffs();
	const int32 Charges = Dice->GetCharges();
	const float BoxX = Left + HealthBarWidth + 4.f;
	const float BoxY = Top + HealthBarHeight * 0.5f - DiceBoxSize * 0.5f;
	DrawRect(Backing, BoxX - 1.f, BoxY - 1.f, DiceBoxSize + 2.f, DiceBoxSize + 2.f);
	DrawRect(Charges > 0 ? DiceColor : EmptyDiceColor, BoxX, BoxY, DiceBoxSize, DiceBoxSize);
	const FString ChargeText = FString::FromInt(Charges);
	GetTextSize(ChargeText, TextWidth, TextHeight, Font);
	DrawText(ChargeText, Charges > 0 ? FLinearColor::Black : FLinearColor::Gray, BoxX + (DiceBoxSize - TextWidth) * 0.5f, BoxY + (DiceBoxSize - TextHeight) * 0.5f, Font);

	// Left of the health bar: the gun, if it isn't the starting pistol
	DrawWeaponChip(Brawler.GetWeapon(), Left - 4.f, Top + HealthBarHeight * 0.5f, Font);

	// Below the health bar: ammo (own brawler only), then progress toward the next die
	float RowY = Top + HealthBarHeight + 2.f;
	if (bIsViewer)
	{
		DrawAmmoBar(Brawler, Left, RowY);
		RowY += AmmoBarHeight + 2.f;
	}
	DrawRect(Backing, Left, RowY, HealthBarWidth, GaugeBarHeight);
	DrawRect(DiceColor, Left, RowY, HealthBarWidth * FMath::Clamp(Dice->GetGaugeProgress(), 0.f, 1.f), GaugeBarHeight);

	// Below everything, pointing at the head: blue for the player and allies, red for opponents
	DrawFriendOrFoeMarker(Head.X, RowY + GaugeBarHeight + 3.f, bIsAlly ? AllyHealthColor : EnemyHealthColor);

	// Above the health number: active buffs, the power chips (giant, flight), then the latest roll
	float ChipsTop = DrawBuffChips(*Dice, Head.X, HealthTextY - 2.f, Font);
	TArray<FString, TInlineAllocator<2>> PowerLabels;
	TArray<FLinearColor, TInlineAllocator<2>> PowerColors;
	TArray<float, TInlineAllocator<2>> PowerRemaining;
	if (Brawler.IsGiant())
	{
		PowerLabels.Add(TEXT("GIANT"));
		PowerColors.Add(URRRandomDiceBuffComponent::GetGiantColor());
		PowerRemaining.Add(Brawler.GetGiantRemaining());
	}
	if (Brawler.CanFly())
	{
		PowerLabels.Add(TEXT("FLY"));
		PowerColors.Add(URRRandomDiceBuffComponent::GetFlightColor());
		PowerRemaining.Add(Brawler.GetFlightRemaining());
	}
	if (PowerLabels.Num() > 0)
	{
		ChipsTop = DrawPowerChips(PowerLabels, PowerColors, PowerRemaining, Head.X, ChipsTop, Font);
	}
	ChipsTop = DrawRollPopup(*Dice, Head.X, ChipsTop, Font);

	// The 3D die floats on top of all this: hand it the stack's top edge, carried from the screen back into the
	// world along the camera's up (the camera is nearly orthographic, so one scale factor fits the whole stack)
	if (URRRandomOverheadDieComponent* Die = Brawler.GetOverheadDie(); Die && PlayerOwner && PlayerOwner->PlayerCameraManager)
	{
		const FVector ScreenUp = PlayerOwner->PlayerCameraManager->GetCameraRotation().Quaternion().GetUpVector();
		const float PixelsPerUnit = (Head.Y - Project(HeadWorld + ScreenUp * 100.f).Y) / 100.f;
		if (PixelsPerUnit > KINDA_SMALL_NUMBER)
		{
			Die->SetStackTop(HeadWorld + ScreenUp * ((Head.Y - ChipsTop) / PixelsPerUnit));
		}
	}
}

float ARRRandomHUD::DrawPowerChips(TArrayView<const FString> Labels, TArrayView<const FLinearColor> Colors, TArrayView<const float> Remaining, float CenterX, float Bottom, UFont* Font)
{
	// Outlined chips side by side, e.g. [GIANT] [FLY]
	constexpr float Gap = 4.f;
	TArray<float, TInlineAllocator<2>> Widths;
	float TotalWidth = 0.f;
	float TextHeight = 0.f;
	for (const FString& Label : Labels)
	{
		float Width = 0.f;
		GetTextSize(Label, Width, TextHeight, Font);
		Widths.Add(Width + ChipPadding * 2.f);
		TotalWidth += Widths.Last() + (Widths.Num() > 1 ? Gap : 0.f);
	}

	const float ChipHeight = TextHeight + 2.f;
	const float Y = Bottom - ChipHeight;
	const float Blink = 0.35f + 0.65f * (0.5f + 0.5f * FMath::Cos(GetWorld()->GetTimeSeconds() * 12.f));
	float X = CenterX - TotalWidth * 0.5f;
	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		// Blinks like a buff when about to run out
		const float Alpha = Remaining[Index] < BuffBlinkTime ? Blink : 1.f;
		DrawRect(WithAlpha(Colors[Index], Alpha), X - 1.f, Y - 1.f, Widths[Index] + 2.f, ChipHeight + 2.f);
		DrawRect(Backing, X, Y, Widths[Index], ChipHeight);
		DrawText(Labels[Index], WithAlpha(Colors[Index], Alpha), X + ChipPadding, Y + 1.f, Font);
		X += Widths[Index] + Gap;
	}
	return Y - 3.f;
}

void ARRRandomHUD::DrawAmmoBar(const ARRRandomCharacter& Brawler, float Left, float Top)
{
	const int32 MaxAmmo = Brawler.GetMaxAmmo();
	if (MaxAmmo <= 0)
	{
		return;
	}

	// While reloading, one bar fills up across the whole width instead of the rounds
	if (Brawler.IsReloading())
	{
		DrawRect(Backing, Left, Top, HealthBarWidth, AmmoBarHeight);
		DrawRect(ReloadingAmmoColor, Left, Top, HealthBarWidth * Brawler.GetReloadProgress(), AmmoBarHeight);
		return;
	}

	// Too many rounds for separate segments: one bar
	if (MaxAmmo > MaxAmmoSegments)
	{
		DrawRect(Backing, Left, Top, HealthBarWidth, AmmoBarHeight);
		DrawRect(AmmoColor, Left, Top, HealthBarWidth * Brawler.GetAmmo() / MaxAmmo, AmmoBarHeight);
		return;
	}

	constexpr float Gap = 1.f;
	const float SegmentWidth = (HealthBarWidth - Gap * (MaxAmmo - 1)) / MaxAmmo;
	for (int32 Index = 0; Index < MaxAmmo; ++Index)
	{
		const float X = Left + Index * (SegmentWidth + Gap);
		DrawRect(Index < Brawler.GetAmmo() ? AmmoColor : Backing, X, Top, SegmentWidth, AmmoBarHeight);
	}
}

void ARRRandomHUD::DrawAmmoPanel(const ARRRandomCharacter& Brawler)
{
	// "12 / 15", or the reload progress, in the bottom-right corner
	UFont* Font = GEngine->GetLargeFont();
	const FString Text = Brawler.IsReloading()
		? FString::Printf(TEXT("RELOADING %d%%"), FMath::FloorToInt(Brawler.GetReloadProgress() * 100.f))
		: FString::Printf(TEXT("%d / %d"), Brawler.GetAmmo(), Brawler.GetMaxAmmo());
	const FLinearColor Color = Brawler.IsReloading() ? ReloadingAmmoColor : (Brawler.GetAmmo() * 4 <= Brawler.GetMaxAmmo() ? EnemyHealthColor : FLinearColor::White);
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font);
	const float Y = Canvas->ClipY - Margin - Height;
	DrawShadowedText(Text, Color, Canvas->ClipX - Margin - Width, Y, Font);

	const FString Hint = TEXT("AMMO  [R] RELOAD");
	UFont* HintFont = GEngine->GetSmallFont();
	GetTextSize(Hint, Width, Height, HintFont);
	const float HintY = Y - Height - 2.f;
	DrawShadowedText(Hint, FLinearColor::Gray, Canvas->ClipX - Margin - Width, HintY, HintFont);

	// The gun and how it differs from the pistol, e.g. "EPIC CANNON  DMG x2.3  SHOT SPD x0.5"
	const FRRWeapon& Weapon = Brawler.GetWeapon();
	const FString WeaponText = Weapon.bIsDefault
		? Weapon.Name
		: FString::Printf(TEXT("%s %s  DMG x%.1f  SHOT SPD x%.1f"), *FRRWeapon::GetTierName(Weapon.Tier), *Weapon.Name, Weapon.DamageMultiplier, Weapon.ProjectileSpeedMultiplier);
	UFont* WeaponFont = GEngine->GetMediumFont();
	GetTextSize(WeaponText, Width, Height, WeaponFont);
	DrawShadowedText(WeaponText, FRRWeapon::GetTierColor(Weapon.Tier), Canvas->ClipX - Margin - Width, HintY - Height - 2.f, WeaponFont);
}

void ARRRandomHUD::DrawWeaponChip(const FRRWeapon& Weapon, float Right, float CenterY, UFont* Font)
{
	if (Weapon.bIsDefault)
	{
		return;
	}

	// Gun name in its tier color, framed in the same color
	float TextWidth = 0.f;
	float TextHeight = 0.f;
	GetTextSize(Weapon.Name, TextWidth, TextHeight, Font);
	const FLinearColor Color = FRRWeapon::GetTierColor(Weapon.Tier);
	const float Width = TextWidth + ChipPadding * 2.f;
	const float Height = TextHeight + 2.f;
	const float X = Right - Width;
	const float Y = CenterY - Height * 0.5f;
	DrawRect(Color, X - 1.f, Y - 1.f, Width + 2.f, Height + 2.f);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.85f), X, Y, Width, Height);
	DrawText(Weapon.Name, Color, X + ChipPadding, Y + 1.f, Font);
}

float ARRRandomHUD::DrawBuffChips(const URRRandomDiceBuffComponent& Dice, float CenterX, float Bottom, UFont* Font)
{
	// One chip per stat with the summed bonus, e.g. "ATK+25"
	constexpr int32 StatCount = 3;
	float TotalBonus[StatCount] = {};
	float SoonestExpiry[StatCount] = { UE_BIG_NUMBER, UE_BIG_NUMBER, UE_BIG_NUMBER };
	for (const FRRDiceBuff& Buff : Dice.GetActiveBuffs())
	{
		const int32 Index = static_cast<int32>(Buff.Stat);
		TotalBonus[Index] += Buff.Bonus;
		SoonestExpiry[Index] = FMath::Min(SoonestExpiry[Index], Buff.RemainingTime);
	}

	TArray<int32, TInlineAllocator<StatCount>> Shown;
	TArray<FString, TInlineAllocator<StatCount>> Labels;
	TArray<float, TInlineAllocator<StatCount>> Widths;
	float TotalWidth = 0.f;
	float TextHeight = 0.f;
	for (int32 Index = 0; Index < StatCount; ++Index)
	{
		if (TotalBonus[Index] <= 0.f)
		{
			continue;
		}
		const FString Label = FString::Printf(TEXT("%s+%d"), *URRRandomDiceBuffComponent::GetStatShortName(static_cast<ERRDiceBuffStat>(Index)), FMath::RoundToInt(TotalBonus[Index] * 100.f));
		float Width = 0.f;
		GetTextSize(Label, Width, TextHeight, Font);
		Shown.Add(Index);
		Labels.Add(Label);
		Widths.Add(Width + ChipPadding * 2.f);
		TotalWidth += Widths.Last() + (Shown.Num() > 1 ? 2.f : 0.f);
	}
	if (Shown.Num() == 0)
	{
		return Bottom;
	}

	const float ChipHeight = TextHeight + 2.f;
	const float ChipY = Bottom - ChipHeight;
	const float Time = GetWorld()->GetTimeSeconds();
	float X = CenterX - TotalWidth * 0.5f;
	for (int32 Chip = 0; Chip < Shown.Num(); ++Chip)
	{
		const int32 Index = Shown[Chip];
		const float Alpha = SoonestExpiry[Index] < BuffBlinkTime ? 0.35f + 0.65f * (0.5f + 0.5f * FMath::Cos(Time * 12.f)) : 1.f;
		DrawRect(Backing, X, ChipY, Widths[Chip], ChipHeight);
		DrawText(Labels[Chip], WithAlpha(URRRandomDiceBuffComponent::GetStatColor(static_cast<ERRDiceBuffStat>(Index)), Alpha), X + ChipPadding, ChipY + 1.f, Font);
		X += Widths[Chip] + 2.f;
	}
	return ChipY - 2.f;
}

float ARRRandomHUD::DrawRollPopup(const URRRandomDiceBuffComponent& Dice, float CenterX, float Bottom, UFont* Font)
{
	// While the 3D die is up its result gets a row: empty while it tumbles, "5 ATK" (colored by stat) once it lands
	const float Landed = Dice.GetTimeSinceLastRoll() - Dice.RollSpinTime;
	if (Landed >= RollPopupTime)
	{
		return Bottom;
	}

	const FRRDiceRoll& Die = Dice.GetLastRoll();
	const FString Text = FString::Printf(TEXT("%d %s"), Die.Face, Die.bWeapon ? TEXT("GUN") : Die.bGiant ? TEXT("GIANT") : Die.bFlight ? TEXT("FLY") : *URRRandomDiceBuffComponent::GetStatShortName(Die.Stat));
	float Width = 0.f;
	float TextHeight = 0.f;
	GetTextSize(Text, Width, TextHeight, Font);
	const float Y = Bottom - TextHeight;
	if (Landed >= 0.f)
	{
		const FLinearColor DieColor = Die.bWeapon ? WeaponDieColor
			: Die.bGiant ? URRRandomDiceBuffComponent::GetGiantColor()
			: Die.bFlight ? URRRandomDiceBuffComponent::GetFlightColor() : URRRandomDiceBuffComponent::GetStatColor(Die.Stat);
		const float Alpha = FMath::Clamp((RollPopupTime - Landed) / RollPopupFadeTime, 0.f, 1.f);
		DrawShadowedText(Text, WithAlpha(DieColor, Alpha), CenterX - Width * 0.5f, Y, Font);
	}
	return Y - 2.f;
}

void ARRRandomHUD::DrawDamagePopups(const ARRRandomCharacter& Brawler, const ARRRandomCharacter* Viewer)
{
	if (Brawler.GetDamagePopups().Num() == 0)
	{
		return;
	}

	// Beside the body so the numbers don't cover the health bar; red when our side gets hurt
	const FVector Body = Project(Brawler.GetActorLocation());
	if (Body.Z <= 0.f)
	{
		return;
	}
	UFont* Font = GEngine->GetMediumFont();
	const bool bFriendly = Viewer && Brawler.GetTeam() == Viewer->GetTeam();
	const FLinearColor Color = bFriendly ? FriendlyDamageColor : FLinearColor::White;
	for (const FRRDamagePopup& Popup : Brawler.GetDamagePopups())
	{
		const FString Text = FString::FromInt(FMath::RoundToInt(Popup.Amount));
		float Width = 0.f;
		float Height = 0.f;
		GetTextSize(Text, Width, Height, Font);
		const float Alpha = 1.f - Popup.Age / ARRRandomCharacter::DamagePopupLifetime;
		DrawShadowedText(Text, WithAlpha(Color, Alpha), Body.X + 50.f + Popup.OffsetX - Width * 0.5f, Body.Y - Popup.Age * DamagePopupRiseSpeed, Font);
	}
}

void ARRRandomHUD::DrawDicePanel(const ARRRandomCharacter& Viewer)
{
	const URRRandomDiceBuffComponent& Dice = *Viewer.GetDiceBuffs();
	UFont* Font = GEngine->GetMediumFont();
	const float BarY = Canvas->ClipY - Margin - BarHeight;

	// Gauge: dark track, yellow fill, stored dice count to the right
	DrawRect(Backing, Margin, BarY, BarWidth, BarHeight);
	DrawRect(DiceColor, Margin, BarY, BarWidth * FMath::Clamp(Dice.GetGaugeProgress(), 0.f, 1.f), BarHeight);
	const FLinearColor CountColor = Dice.GetCharges() > 0 ? FLinearColor::White : FLinearColor::Gray;
	DrawText(FString::Printf(TEXT("DICE x%d  [E]"), Dice.GetCharges()), CountColor, Margin + BarWidth + 10.f, BarY, Font);

	// Powers (flight, giant), then active buffs, stacked upward from the gauge
	float LineY = BarY - LineHeight - 4.f;
	if (Viewer.CanFly())
	{
		const TCHAR* Dash = Viewer.IsDashing() ? TEXT("DASHING") : Viewer.HasDash() ? TEXT("[SPACE x2 HOLD] DASH") : TEXT("DASH AFTER LANDING");
		const FString Line = FString::Printf(TEXT("FLY  HOLD [SPACE] RISE  %s  %.0fs"), Dash, FMath::CeilToFloat(Viewer.GetFlightRemaining()));
		DrawText(Line, URRRandomDiceBuffComponent::GetFlightColor(), Margin, LineY, Font);
		LineY -= LineHeight;
	}
	if (Viewer.IsGiant())
	{
		const FString Line = FString::Printf(TEXT("GIANT  ATK x%.1f  DMG TAKEN x%.1f  %.0fs"),
			Viewer.GetGiantDamageMultiplier(), Viewer.GetGiantDamageTakenMultiplier(), FMath::CeilToFloat(Viewer.GetGiantRemaining()));
		DrawText(Line, URRRandomDiceBuffComponent::GetGiantColor(), Margin, LineY, Font);
		LineY -= LineHeight;
	}
	for (const FRRDiceBuff& Buff : Dice.GetActiveBuffs())
	{
		const FString Line = FString::Printf(TEXT("%s +%d%%  %.0fs"),
			*URRRandomDiceBuffComponent::GetStatName(Buff.Stat).ToString(), FMath::RoundToInt(Buff.Bonus * 100.f), FMath::CeilToFloat(Buff.RemainingTime));
		DrawText(Line, URRRandomDiceBuffComponent::GetStatColor(Buff.Stat), Margin, LineY, Font);
		LineY -= LineHeight;
	}
}

void ARRRandomHUD::DrawOnlinePanel()
{
	// Top left: how this game is connected, the keys, and the last thing the online code did
	const URRRandomSessionSubsystem* Sessions = GetGameInstance()->GetSubsystem<URRRandomSessionSubsystem>();
	if (!Sessions)
	{
		return;
	}

	const AGameStateBase* State = GetWorld()->GetGameState();
	const int32 Players = State ? State->PlayerArray.Num() : 1;
	const TCHAR* Service = Sessions->IsUsingEOS() ? TEXT("EOS") : TEXT("LAN");
	FString Connection;
	switch (GetNetMode())
	{
	case NM_ListenServer: Connection = FString::Printf(TEXT("HOSTING (%s)  %d PLAYER%s"), Service, Players, Players == 1 ? TEXT("") : TEXT("S")); break;
	case NM_Client: Connection = FString::Printf(TEXT("ONLINE (%s)  %d PLAYER%s"), Service, Players, Players == 1 ? TEXT("") : TEXT("S")); break;
	default: Connection = FString::Printf(TEXT("SOLO  -  ONLINE VIA %s"), Service); break;
	}

	UFont* Font = GEngine->GetSmallFont();
	float Y = Margin;
	DrawShadowedText(Connection, FLinearColor::White, Margin, Y, Font);
	Y += LineHeight;
	DrawShadowedText(TEXT("[F1] HOST   [F2] JOIN   [F3] LEAVE   [ESC] TITLE"), FLinearColor::Gray, Margin, Y, Font);
	Y += LineHeight;
	if (!Sessions->GetStatus().IsEmpty())
	{
		DrawShadowedText(Sessions->GetStatus(), FLinearColor(1.f, 0.85f, 0.4f), Margin, Y, Font);
	}
}

void ARRRandomHUD::DrawScore()
{
	// The game mode lives on the server only; the score reaches clients through the game state
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (!State)
	{
		return;
	}

	// "BLUE  2 : 1  RED" centered at the top
	UFont* Font = GEngine->GetLargeFont();
	const FString Parts[] = { TEXT("BLUE  "), FString::Printf(TEXT("%d : %d"), State->GetTeamScore(0), State->GetTeamScore(1)), TEXT("  RED") };
	const FLinearColor Colors[] = { ScoreColor(0), FLinearColor::White, ScoreColor(1) };
	float Widths[3] = {};
	float TotalWidth = 0.f;
	float Height = 0.f;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		GetTextSize(Parts[Index], Widths[Index], Height, Font);
		TotalWidth += Widths[Index];
	}

	float X = (Canvas->ClipX - TotalWidth) * 0.5f;
	DrawRect(Backing, X - 12.f, Margin - 4.f, TotalWidth + 24.f, Height + 8.f);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		DrawText(Parts[Index], Colors[Index], X, Margin, Font);
		X += Widths[Index];
	}
}

void ARRRandomHUD::DrawRespawnCountdown(const ARRRandomCharacter& Viewer)
{
	UFont* Font = GEngine->GetLargeFont();
	const FString Text = FString::Printf(TEXT("KNOCKED OUT  -  BACK IN %d"), FMath::CeilToInt(Viewer.GetRespawnRemaining()));
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font);
	DrawShadowedText(Text, FLinearColor::White, (Canvas->ClipX - Width) * 0.5f, Canvas->ClipY * 0.3f, Font);
}
