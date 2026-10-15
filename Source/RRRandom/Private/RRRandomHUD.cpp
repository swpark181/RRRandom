#include "RRRandomHUD.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomGameState.h"
#include "RRRandomOverheadDieComponent.h"
#include "RRRandomPlayerController.h"
#include "RRRandomSessionSubsystem.h"
#include "RRRandomUltimate.h"
#include "RRRandomVehicle.h"
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
	// Hit stickers (/Game/UI/T_HitEffects): the sheet's grid, how long they take to pop in, and each one's size on a
	// 720 pixel tall screen, in ERRHitEffect order (the cannon's blast and the skull biggest, rapid fire's sparks small)
	constexpr int32 HitEffectColumns = 4;
	constexpr int32 HitEffectRows = 2;
	constexpr float HitEffectPopTime = 0.1f;
	constexpr float HitEffectReferenceHeight = 720.f;
	constexpr float HitEffectSizes[] = { 52.f, 46.f, 40.f, 60.f, 66.f, 96.f, 58.f, 84.f };

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
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (State && State->IsMatchOver())
	{
		DrawResults(*State, Viewer);
		return;
	}
	ResultsShownAt = -1.f;
	ResultsButtons.Reset();

	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		DrawOverhead(**It, Viewer);
	}

	DrawScore();
	DrawOnlinePanel();
	if (Viewer)
	{
		DrawDicePanel(*Viewer);
		DrawTeamPanel(*Viewer);
		if (Viewer->IsAlive())
		{
			DrawAmmoPanel(*Viewer);
			DrawRidePrompt(*Viewer);
			DrawCrosshair(*Viewer);
		}
		else
		{
			DrawRespawnCountdown(*Viewer);
		}
	}
	if (State)
	{
		DrawCountdown(*State);
	}
	// Last, so the full-screen map covers the rest
	DrawMinimap(Viewer);

	const ARRRandomPlayerController* Controller = Cast<ARRRandomPlayerController>(PlayerOwner);
	if (Controller && Controller->IsPauseMenuOpen())
	{
		DrawPauseMenu();
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
	// Hidden in the fog of war on this screen (ARRRandomFogOfWar): nothing over it either
	if (Brawler.GetMesh()->bHiddenInGame)
	{
		return;
	}
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
	// Gold while one of them is the tiger's ultimate die
	DrawRect(Dice->GetUltimateCharges() > 0 ? URRRandomDiceBuffComponent::GetUltimateColor() : Charges > 0 ? DiceColor : EmptyDiceColor, BoxX, BoxY, DiceBoxSize, DiceBoxSize);
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
	if (Brawler.IsInvincible())
	{
		// The cheat: never runs out, so it never blinks
		PowerLabels.Add(TEXT("GOD"));
		PowerColors.Add(FLinearColor::FromSRGBColor(FColor(255, 255, 255)));
		PowerRemaining.Add(UE_BIG_NUMBER);
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

void ARRRandomHUD::DrawRidePrompt(const ARRRandomCharacter& Brawler)
{
	// Under the brawler's feet: what F does right now
	const ARRRandomVehicle* Ridden = Brawler.GetVehicle();
	// Giants and flyers can't get on, so no offer
	const ARRRandomVehicle* Near = Ridden || Brawler.IsGiant() || Brawler.CanFly() ? nullptr : Brawler.FindVehicleInReach();
	if (!Ridden && !Near)
	{
		return;
	}
	const FString Text = Ridden ? TEXT("[F] GET OFF")
		: FString::Printf(TEXT("[F] RIDE %s%s"), Near->GetHandling().Name, Near->IsNextSeatRear() ? TEXT(" (BACK SEAT)") : TEXT(""));
	const FVector Feet = Brawler.GetActorLocation() - FVector(0.f, 0.f, Brawler.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Screen = Project(Feet);
	if (Screen.Z <= 0.f)
	{
		return;
	}
	UFont* Font = GEngine->GetSmallFont();
	float Width = 0.f;
	float Height = 0.f;
	GetTextSize(Text, Width, Height, Font);
	DrawShadowedText(Text, Ridden ? FLinearColor(0.75f, 0.75f, 0.75f) : FLinearColor::FromSRGBColor(FColor(255, 214, 110)), Screen.X - Width * 0.5f, Screen.Y + 6.f, Font);
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
	const FString WeaponText = Weapon.bIsDefault ? Weapon.Name
		: Weapon.bFoamFinger ? TEXT("FOAM FINGER  DMG 1  BACK POKE = KO")
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
	const float Landed = Dice.GetTimeSinceLastRoll() - Dice.GetSpinTime();
	if (Landed >= RollPopupTime)
	{
		return Bottom;
	}

	const FRRDiceRoll& Die = Dice.GetLastRoll();
	const ARRRandomCharacter* Roller = Cast<ARRRandomCharacter>(Dice.GetOwner());
	const FString UltimateName = RRSpecies::GetUltimateName(Roller ? Roller->GetSpecies() : RRSpecies::Tiger);
	const FString Text = FString::Printf(TEXT("%d %s"), Die.Face, Die.bUltimate ? *UltimateName : Die.bWeapon ? TEXT("GUN") : Die.bGiant ? TEXT("GIANT") : Die.bFlight ? TEXT("FLY") : *URRRandomDiceBuffComponent::GetStatShortName(Die.Stat));
	float Width = 0.f;
	float TextHeight = 0.f;
	GetTextSize(Text, Width, TextHeight, Font);
	const float Y = Bottom - TextHeight;
	if (Landed >= 0.f)
	{
		const FLinearColor DieColor = Die.bUltimate ? URRRandomDiceBuffComponent::GetUltimateColor() : Die.bWeapon ? WeaponDieColor
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

	// The skull sticker of a foam finger's knockout first, under the numbers: popping in with an overshoot, holding, then
	// fading and shrinking. The guns' stickers aren't drawn any more; the glowing hit bursts in the world
	// (ARRRandomEffects) took their place.
	if (HitEffects)
	{
		const float Scale = Canvas->ClipY / HitEffectReferenceHeight;
		for (const FRRDamagePopup& Popup : Brawler.GetDamagePopups())
		{
			if (Popup.Effect != ERRHitEffect::Skull)
			{
				continue;
			}
			const int32 Cell = FMath::Clamp(static_cast<int32>(Popup.Effect), 0, static_cast<int32>(ERRHitEffect::Count) - 1);
			const float T = Popup.Age / FMath::Max(Popup.Lifetime, KINDA_SMALL_NUMBER);
			const float PopIn = FMath::Clamp(Popup.Age / HitEffectPopTime, 0.f, 1.f);
			// 0 -> 1.25 -> 1, a cartoon bounce
			const float Bounce = PopIn < 1.f ? 1.25f * FMath::Sin(PopIn * PI * 0.5f) : 1.f + 0.25f * FMath::Max(0.f, 1.f - (Popup.Age - HitEffectPopTime) / HitEffectPopTime);
			const float Fade = 1.f - FMath::SmoothStep(0.55f, 1.f, T);
			const float Size = HitEffectSizes[Cell] * Scale * Bounce * FMath::Lerp(0.8f, 1.f, Fade);
			// A skull drifts up off the fallen body
			const float Rise = Popup.Effect == ERRHitEffect::Skull ? Popup.Age * 30.f * Scale : 0.f;
			const FVector2D Center(Body.X + Popup.Nudge.X * Scale, Body.Y + Popup.Nudge.Y * Scale - Rise);
			DrawTexture(HitEffects, Center.X - Size * 0.5f, Center.Y - Size * 0.5f, Size, Size,
				float(Cell % HitEffectColumns) / HitEffectColumns, float(Cell / HitEffectColumns) / HitEffectRows, 1.f / HitEffectColumns, 1.f / HitEffectRows,
				FLinearColor(1.f, 1.f, 1.f, Fade), BLEND_Translucent, 1.f, false, Popup.Effect == ERRHitEffect::Skull ? 0.f : Popup.Spin, FVector2D(0.5f, 0.5f));
		}
	}

	for (const FRRDamagePopup& Popup : Brawler.GetDamagePopups())
	{
		const FString Text = FString::FromInt(FMath::RoundToInt(Popup.Amount));
		float Width = 0.f;
		float Height = 0.f;
		GetTextSize(Text, Width, Height, Font);
		const float Alpha = FMath::Max(0.f, 1.f - Popup.Age / ARRRandomCharacter::DamagePopupLifetime);
		DrawShadowedText(Text, WithAlpha(Color, Alpha), Body.X + 50.f + Popup.OffsetX - Width * 0.5f, Body.Y - Popup.Age * DamagePopupRiseSpeed, Font);
	}
}

void ARRRandomHUD::DrawDicePanel(const ARRRandomCharacter& Viewer)
{
	const URRRandomDiceBuffComponent& Dice = *Viewer.GetDiceBuffs();
	UFont* Font = GEngine->GetMediumFont();

	// My ultimate die: a gold flash and big words across the middle of the screen while it tumbles and as it lands
	const FRRDiceRoll& Roll = Dice.GetLastRoll();
	const float RollAge = Dice.GetTimeSinceLastRoll();
	const float Spin = Dice.GetSpinTime();
	if (Roll.bUltimate && RollAge < Spin + UltimateBannerTime)
	{
		const float S = Canvas->ClipY / 720.f;
		const FLinearColor Ult = URRRandomDiceBuffComponent::GetUltimateColor();
		FString Banner;
		float Scale;
		float Alpha = 1.f;
		if (RollAge < Spin)
		{
			Banner = TEXT("ULTIMATE DIE!");
			Scale = (3.f + 0.35f * FMath::Sin(RollAge * 12.f)) * S;
			Alpha = FMath::Clamp(RollAge / 0.15f, 0.f, 1.f);
		}
		else
		{
			const float Landed = RollAge - Spin;
			Banner = RRSpecies::GetUltimateBanner(Viewer.GetSpecies(), Roll.Face);
			// Slams in big, settles, fades out at the end
			Scale = (3.4f + 2.f * FMath::Max(0.f, 1.f - Landed / 0.2f)) * S;
			Alpha = FMath::Clamp((UltimateBannerTime - Landed) / 0.4f, 0.f, 1.f);
			DrawRect(WithAlpha(Ult, 0.45f * FMath::Max(0.f, 1.f - Landed / 0.35f)), 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		}
		UFont* Big = GEngine->GetLargeFont();
		float Width;
		float Height;
		GetTextSize(Banner, Width, Height, Big, Scale);
		const float X = (Canvas->ClipX - Width) * 0.5f;
		// Below the crosshair, clear of the labels over the brawlers' heads
		const float Y = Canvas->ClipY * 0.66f - Height * 0.5f;
		DrawText(Banner, WithAlpha(FLinearColor(0.15f, 0.05f, 0.f), Alpha * 0.8f), X + 3.f * S, Y + 3.f * S, Big, Scale);
		DrawText(Banner, WithAlpha(Ult, Alpha), X, Y, Big, Scale);
	}
	const float BarY = Canvas->ClipY - Margin - BarHeight;

	// Gauge: dark track, yellow fill, stored dice count to the right
	DrawRect(Backing, Margin, BarY, BarWidth, BarHeight);
	DrawRect(DiceColor, Margin, BarY, BarWidth * FMath::Clamp(Dice.GetGaugeProgress(), 0.f, 1.f), BarHeight);
	const FLinearColor CountColor = Dice.GetCharges() > 0 ? FLinearColor::White : FLinearColor::Gray;
	DrawText(FString::Printf(TEXT("DICE x%d  [E]"), Dice.GetCharges()), CountColor, Margin + BarWidth + 10.f, BarY, Font);
	if (Dice.GetUltimateCharges() > 0)
	{
		float CountWidth;
		float CountHeight;
		GetTextSize(FString::Printf(TEXT("DICE x%d  [E]"), Dice.GetCharges()), CountWidth, CountHeight, Font);
		DrawText(TEXT("  ULTIMATE DIE: ") + RRSpecies::GetUltimateName(Viewer.GetSpecies()) + TEXT("!"), URRRandomDiceBuffComponent::GetUltimateColor(), Margin + BarWidth + 10.f + CountWidth, BarY, Font);
	}

	// Powers (the ultimate, flight, giant), then active buffs, stacked upward from the gauge
	float LineY = BarY - LineHeight - 4.f;
	if (const ARRRandomUltimate* Ultimate = Viewer.GetUltimate())
	{
		DrawText(Ultimate->GetStatusLine(), URRRandomDiceBuffComponent::GetUltimateColor(), Margin, LineY, Font);
		LineY -= LineHeight;
	}
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
	// Top left under the minimap: how this game is connected, the keys, and the last thing the online code did
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
	float Y = MinimapMargin + MinimapSize + 26.f;
	DrawShadowedText(Connection, FLinearColor::White, Margin, Y, Font);
	Y += LineHeight;
	DrawShadowedText(TEXT("[F1] HOST   [F2] JOIN   [F3] LEAVE   [ESC] MENU   [M] MAP"), FLinearColor::Gray, Margin, Y, Font);
	Y += LineHeight;
#if !UE_BUILD_SHIPPING
	DrawShadowedText(TEXT("CHEATS  [F6] WIN   [F7] GIANT   [F8] FLY   [F9] 3RD PERSON   [F10] GOD   [U] ULTIMATE DIE   [1-7] PISTOL MINIGUN SMG RIFLE BLASTER CANNON FINGER"), FLinearColor::Gray, Margin, Y, Font);
	Y += LineHeight;
#endif
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

	// What wins, under the score
	if (State->GetScoreToWin() > 0)
	{
		UFont* Small = GEngine->GetSmallFont();
		const FString Goal = FString::Printf(TEXT("FIRST TO %d"), State->GetScoreToWin());
		float GoalWidth;
		float GoalHeight;
		GetTextSize(Goal, GoalWidth, GoalHeight, Small);
		DrawShadowedText(Goal, FLinearColor(0.8f, 0.8f, 0.8f), (Canvas->ClipX - GoalWidth) * 0.5f, Margin + Height + 6.f, Small);
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

namespace
{
	// Results screen, sized for a 720 pixel tall screen and scaled with it
	constexpr float ResultsReferenceHeight = 720.f;
	constexpr float LetterboxShare = 0.07f;
	constexpr float LetterboxTime = 0.8f;
	// The text and buttons fade in once the camera has mostly closed in
	constexpr float ResultsTextDelay = 0.9f;
	constexpr float ResultsFadeTime = 0.6f;
	constexpr float ResultsButtonWidth = 230.f;
	constexpr float ResultsButtonHeight = 40.f;
	constexpr float ResultsButtonGap = 24.f;
	constexpr float BoardWidth = 230.f;
	constexpr float BoardRowHeight = 26.f;
	const FLinearColor VictoryColor = FLinearColor::FromSRGBColor(FColor(255, 196, 56));
	const FLinearColor DefeatColor = FLinearColor::FromSRGBColor(FColor(235, 90, 80));
	const FLinearColor ButtonFill = FLinearColor::FromSRGBColor(FColor(30, 33, 44));
	const FLinearColor ButtonEdge = FLinearColor::FromSRGBColor(FColor(72, 78, 96));
	const FLinearColor HoveredText = FLinearColor::FromSRGBColor(FColor(24, 18, 6));
	const FLinearColor Muted = FLinearColor::FromSRGBColor(FColor(150, 156, 172));

	float EaseOut(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return 1.f - FMath::Square(1.f - T);
	}

	/** A team's brawlers, numbered bots in a fixed order (by name), then sorted by knockouts. */
	TArray<TPair<const ARRRandomCharacter*, FString>> GetTeamBoard(const UWorld* World, int32 Team, const ARRRandomCharacter* Viewer)
	{
		TArray<const ARRRandomCharacter*> Members;
		for (TActorIterator<ARRRandomCharacter> It(World); It; ++It)
		{
			if (It->GetTeam() == Team && !It->IsDecoy())
			{
				Members.Add(*It);
			}
		}
		Members.Sort([](const ARRRandomCharacter& A, const ARRRandomCharacter& B) { return A.GetName() < B.GetName(); });
		TArray<TPair<const ARRRandomCharacter*, FString>> Board;
		int32 Bots = 0;
		for (const ARRRandomCharacter* Member : Members)
		{
			// Players have a player state, bots have none
			FString Label;
			if (Member == Viewer)
			{
				Label = TEXT("YOU");
			}
			else if (const APlayerState* Player = Member->GetPlayerState())
			{
				Label = Player->GetPlayerName().Left(14).ToUpper();
			}
			else
			{
				Label = FString::Printf(TEXT("BOT %d"), ++Bots);
			}
			Board.Emplace(Member, Label);
		}
		Board.StableSort([](const TPair<const ARRRandomCharacter*, FString>& A, const TPair<const ARRRandomCharacter*, FString>& B)
		{
			return A.Key->GetKills() > B.Key->GetKills();
		});
		return Board;
	}
}

float ARRRandomHUD::DrawCenteredText(const FString& Text, const FLinearColor& Color, float Y, UFont* Font, float Scale)
{
	float Width;
	float Height;
	GetTextSize(Text, Width, Height, Font, Scale);
	const float X = (Canvas->ClipX - Width) * 0.5f;
	const float Shadow = FMath::Max(1.f, Scale);
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.8f), X + Shadow, Y + Shadow, Font, Scale);
	DrawText(Text, Color, X, Y, Font, Scale);
	return Height;
}

ERRResultsButton ARRRandomHUD::GetResultsButtonAt(const FVector2D& ScreenPoint) const
{
	for (const FResultsButtonRect& Button : ResultsButtons)
	{
		if (Button.bEnabled && Button.Rect.IsInside(ScreenPoint))
		{
			return Button.Button;
		}
	}
	return ERRResultsButton::None;
}

float ARRRandomHUD::DrawTeamBoard(int32 Team, const ARRRandomCharacter* Viewer, const ARRRandomCharacter* MVP, float X, float Top, float Width, float Alpha, float Scale)
{
	UFont* Font = GEngine->GetMediumFont();
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	const TArray<TPair<const ARRRandomCharacter*, FString>> Board = GetTeamBoard(GetWorld(), Team, Viewer);
	const float Row = BoardRowHeight * Scale;
	const float Pad = 10.f * Scale;

	// Header: team name and its score
	DrawRect(WithAlpha(FLinearColor(0.f, 0.f, 0.f, 0.55f), Alpha), X, Top, Width, Row * (Board.Num() + 1) + Pad);
	DrawRect(WithAlpha(ScoreColor(Team), Alpha), X, Top, Width, 3.f * Scale);
	const FString Header = Team == 0 ? TEXT("BLUE") : TEXT("RED");
	DrawText(Header, WithAlpha(ScoreColor(Team), Alpha), X + Pad, Top + 6.f * Scale, Font, Scale);
	const FString Score = FString::FromInt(State ? State->GetTeamScore(Team) : 0);
	float W;
	float H;
	GetTextSize(Score, W, H, Font, Scale);
	DrawText(Score, WithAlpha(FLinearColor::White, Alpha), X + Width - Pad - W, Top + 6.f * Scale, Font, Scale);

	// One row per brawler: name (MVP tagged), knockouts
	float Y = Top + Row + 4.f * Scale;
	for (const TPair<const ARRRandomCharacter*, FString>& Entry : Board)
	{
		const bool bIsMVP = Entry.Key == MVP;
		const FLinearColor NameColor = Entry.Key == Viewer ? FLinearColor(0.55f, 1.f, 0.55f) : FLinearColor::White;
		FString Name = Entry.Value;
		if (bIsMVP)
		{
			Name += TEXT("  MVP");
		}
		DrawText(Name, WithAlpha(bIsMVP ? VictoryColor : NameColor, Alpha), X + Pad, Y, Font, Scale);
		const FString Kills = FString::Printf(TEXT("%d KO"), Entry.Key->GetKills());
		GetTextSize(Kills, W, H, Font, Scale);
		DrawText(Kills, WithAlpha(FLinearColor(0.85f, 0.85f, 0.85f), Alpha), X + Width - Pad - W, Y, Font, Scale);
		Y += Row;
	}
	return Y;
}

void ARRRandomHUD::DrawResults(const ARRRandomGameState& State, const ARRRandomCharacter* Viewer)
{
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (ResultsShownAt < 0.f)
	{
		ResultsShownAt = Now;
	}
	const float Age = Now - ResultsShownAt;
	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float S = ScreenH / ResultsReferenceHeight;
	UFont* LargeFont = GEngine->GetLargeFont();
	UFont* MediumFont = GEngine->GetMediumFont();
	UFont* SmallFont = GEngine->GetSmallFont();

	// Letterbox bars slide in like a cutscene
	const float Bar = ScreenH * LetterboxShare * EaseOut(Age / LetterboxTime);
	DrawRect(FLinearColor::Black, 0.f, 0.f, ScreenW, Bar);
	DrawRect(FLinearColor::Black, 0.f, ScreenH - Bar, ScreenW, Bar);

	const float Fade = EaseOut((Age - ResultsTextDelay) / ResultsFadeTime);
	if (Fade <= 0.f)
	{
		ResultsButtons.Reset();
		return;
	}

	// Headline: how it went for this screen's team (or who won, for a screen without a brawler)
	const int32 Winner = State.GetWinningTeam();
	const bool bWon = Viewer && Viewer->GetTeam() == Winner;
	const FString Headline = Viewer ? (bWon ? TEXT("VICTORY") : TEXT("DEFEAT")) : (Winner == 0 ? TEXT("BLUE WINS") : TEXT("RED WINS"));
	const float Pop = 1.f + 0.35f * (1.f - Fade);
	float Y = ScreenH * 0.12f;
	Y += DrawCenteredText(Headline, WithAlpha(Viewer && !bWon ? DefeatColor : VictoryColor, Fade), Y, LargeFont, 3.2f * S * Pop);
	const FString Result = FString::Printf(TEXT("%s TEAM WINS   %d : %d"), Winner == 0 ? TEXT("BLUE") : TEXT("RED"), State.GetTeamScore(0), State.GetTeamScore(1));
	DrawCenteredText(Result, WithAlpha(ScoreColor(Winner), Fade), Y + 4.f * S, MediumFont, 1.1f * S);

	// The teams on either side, the MVP in the middle of the screen between them
	const ARRRandomCharacter* MVP = State.GetMVP();
	const float BoardTop = ScreenH * 0.3f;
	DrawTeamBoard(0, Viewer, MVP, 30.f * S, BoardTop, BoardWidth * S, Fade, S);
	DrawTeamBoard(1, Viewer, MVP, ScreenW - (30.f + BoardWidth) * S, BoardTop, BoardWidth * S, Fade, S);

	// MVP banner under the hero
	if (MVP)
	{
		FString MVPName;
		for (const TPair<const ARRRandomCharacter*, FString>& Entry : GetTeamBoard(GetWorld(), MVP->GetTeam(), Viewer))
		{
			if (Entry.Key == MVP)
			{
				MVPName = Entry.Value;
			}
		}
		const FString Banner = FString::Printf(TEXT("MVP   %s   %d KO"), *MVPName, MVP->GetKills());
		float W;
		float H;
		GetTextSize(Banner, W, H, LargeFont, 1.3f * S);
		const float BannerY = ScreenH * 0.76f;
		const float PadX = 18.f * S;
		DrawRect(WithAlpha(FLinearColor(0.f, 0.f, 0.f, 0.6f), Fade), (ScreenW - W) * 0.5f - PadX, BannerY - 6.f * S, W + PadX * 2.f, H + 12.f * S);
		DrawRect(WithAlpha(VictoryColor, Fade), (ScreenW - W) * 0.5f - PadX, BannerY - 6.f * S, W + PadX * 2.f, 3.f * S);
		DrawCenteredText(Banner, WithAlpha(ScoreColor(MVP->GetTeam()), Fade), BannerY, LargeFont, 1.3f * S);
	}

	// Buttons: a new game (the host decides on a network game) and back to the title
	const bool bCanRestart = GetWorld()->GetAuthGameMode() != nullptr;
	ResultsButtons.Reset();
	const float ButtonW = ResultsButtonWidth * S;
	const float ButtonH = ResultsButtonHeight * S;
	const float Left = (ScreenW - ButtonW * 2.f - ResultsButtonGap * S) * 0.5f;
	const float Top = ScreenH * 0.85f;
	ResultsButtons.Add({ ERRResultsButton::NewGame, FBox2D(FVector2D(Left, Top), FVector2D(Left + ButtonW, Top + ButtonH)), bCanRestart });
	const float Right = Left + ButtonW + ResultsButtonGap * S;
	ResultsButtons.Add({ ERRResultsButton::Title, FBox2D(FVector2D(Right, Top), FVector2D(Right + ButtonW, Top + ButtonH)), true });

	for (const FResultsButtonRect& Button : ResultsButtons)
	{
		DrawMenuButton(Button, Button.Button == ERRResultsButton::NewGame ? (bCanRestart ? TEXT("NEW GAME") : TEXT("WAITING FOR HOST")) : TEXT("TITLE"), Fade, S);
	}

	const FString Keys = bCanRestart ? TEXT("[ENTER] NEW GAME      [ESC] TITLE") : TEXT("[ESC] TITLE");
	float W;
	float H;
	GetTextSize(Keys, W, H, SmallFont, S);
	DrawText(Keys, WithAlpha(Muted, Fade), (ScreenW - W) * 0.5f, ScreenH - Bar + (Bar - H) * 0.5f, SmallFont, S);
}

void ARRRandomHUD::DrawMenuButton(const FResultsButtonRect& Button, const FString& Label, float Alpha, float Scale)
{
	float MouseX = -1.f;
	float MouseY = -1.f;
	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	UFont* Font = GEngine->GetLargeFont();
	const FVector2D Size = Button.Rect.GetSize();
	const bool bHovered = Button.bEnabled && Button.Rect.IsInside(FVector2D(MouseX, MouseY));
	const float ButtonAlpha = Alpha * (Button.bEnabled ? 1.f : 0.45f);
	const float Border = 2.f * Scale;
	DrawRect(WithAlpha(bHovered ? VictoryColor : ButtonEdge, ButtonAlpha), Button.Rect.Min.X, Button.Rect.Min.Y, Size.X, Size.Y);
	if (!bHovered)
	{
		DrawRect(WithAlpha(ButtonFill, ButtonAlpha), Button.Rect.Min.X + Border, Button.Rect.Min.Y + Border, Size.X - Border * 2.f, Size.Y - Border * 2.f);
	}
	const float LabelScale = (Button.bEnabled ? 1.1f : 0.8f) * Scale;
	float W;
	float H;
	GetTextSize(Label, W, H, Font, LabelScale);
	DrawText(Label, WithAlpha(bHovered ? HoveredText : FLinearColor::White, ButtonAlpha), Button.Rect.Min.X + (Size.X - W) * 0.5f, Button.Rect.Min.Y + (Size.Y - H) * 0.5f, Font, LabelScale);
}

void ARRRandomHUD::DrawPauseMenu()
{
	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float S = ScreenH / ResultsReferenceHeight;

	// The game dimmed behind the menu
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), 0.f, 0.f, ScreenW, ScreenH);
	float Y = ScreenH * 0.3f;
	Y += DrawCenteredText(TEXT("PAUSED"), VictoryColor, Y, GEngine->GetLargeFont(), 2.6f * S);
	// An online game can't stop for one player
	if (GetWorld()->GetNetMode() != NM_Standalone)
	{
		DrawCenteredText(TEXT("ONLINE GAME KEEPS RUNNING"), Muted, Y + 6.f * S, GEngine->GetMediumFont(), S);
	}

	const float ButtonW = ResultsButtonWidth * S;
	const float ButtonH = ResultsButtonHeight * S;
	const float Left = (ScreenW - ButtonW) * 0.5f;
	const float Top = ScreenH * 0.48f;
	ResultsButtons.Reset();
	ResultsButtons.Add({ ERRResultsButton::Resume, FBox2D(FVector2D(Left, Top), FVector2D(Left + ButtonW, Top + ButtonH)), true });
	const float Second = Top + ButtonH + 14.f * S;
	ResultsButtons.Add({ ERRResultsButton::Title, FBox2D(FVector2D(Left, Second), FVector2D(Left + ButtonW, Second + ButtonH)), true });
	DrawMenuButton(ResultsButtons[0], TEXT("RESUME"), 1.f, S);
	DrawMenuButton(ResultsButtons[1], TEXT("TITLE"), 1.f, S);

	float W;
	float H;
	const FString Keys = TEXT("[ESC] RESUME      [ENTER] TITLE");
	GetTextSize(Keys, W, H, GEngine->GetSmallFont(), S);
	DrawText(Keys, Muted, (ScreenW - W) * 0.5f, Second + ButtonH + 24.f * S, GEngine->GetSmallFont(), S);
}

void ARRRandomHUD::DrawCrosshair(const ARRRandomCharacter& Brawler)
{
	const ARRRandomPlayerController* Controller = Cast<ARRRandomPlayerController>(GetOwningPlayerController());
	if (!Controller || !Controller->IsThirdPerson())
	{
		return;
	}
	// Four short ticks around a dot; the gap closes in the sights and opens briefly after a shot
	const float Scale = Canvas->ClipY / 1080.f;
	const FVector2D Middle(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const float Kick = FMath::Clamp(1.f - Brawler.GetTimeSinceShot() / 0.18f, 0.f, 1.f);
	const float Gap = (FMath::Lerp(14.f, 6.f, Brawler.GetSightsBlend()) + 10.f * Kick) * Scale;
	const float Length = 10.f * Scale;
	const float Thickness = FMath::Max(2.f, 2.f * Scale);
	const FLinearColor Ink(1.f, 1.f, 1.f, 0.95f);
	const FLinearColor Edge(0.f, 0.f, 0.f, 0.6f);
	const FVector2D Directions[] = { FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f) };
	for (const FLinearColor& Color : { Edge, Ink })
	{
		const float Grow = Color == Edge ? 1.5f * Scale : 0.f;
		for (const FVector2D& Direction : Directions)
		{
			const FVector2D From = Middle + Direction * (Gap - Grow);
			const FVector2D To = Middle + Direction * (Gap + Length + Grow);
			Canvas->K2_DrawLine(From, To, Thickness + Grow * 2.f, Color);
		}
		const float Dot = (Thickness + Grow * 2.f) * 0.5f;
		DrawRect(Color, Middle.X - Dot, Middle.Y - Dot, Dot * 2.f, Dot * 2.f);
	}
}
