#include "RRRandomHUD.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomGameState.h"
#include "RRRandomOverheadDieComponent.h"
#include "RRRandomPlayerController.h"
#include "RRRandomSessionSubsystem.h"
#include "RRRandomUIStyle.h"
#include "RRRandomUltimate.h"
#include "RRRandomVehicle.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "UObject/ConstructorHelpers.h"

using RRUI::EWeight;

namespace
{
	// Everything is sized for a 720 pixel tall screen and scaled with it (ARRRandomHUD::UIScale)
	constexpr float Margin = 22.f;

	// Over-the-head indicator, in screen pixels except HeadClearance (world units above the capsule)
	constexpr float HeadClearance = 40.f;
	// Extra screen-space lift so the bars clear the mannequin's head under the tilted camera
	constexpr float OverheadLift = 30.f;
	constexpr float HealthBarWidth = 72.f;
	constexpr float HealthBarHeight = 6.f;
	constexpr float AmmoBarHeight = 3.f;
	constexpr float GaugeBarHeight = 2.5f;
	constexpr float ChipSize = 10.f;
	// Friend-or-foe marker: a downward triangle between the bars and the head
	constexpr float MarkerWidth = 13.f;
	constexpr float MarkerHeight = 8.f;
	constexpr float MarkerBorder = 1.5f;
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
	constexpr float HitEffectSizes[] = { 52.f, 46.f, 40.f, 60.f, 66.f, 96.f, 58.f, 84.f };

	// Bottom-left dice panel
	constexpr float DiceGaugeWidth = 150.f;
	constexpr float TagLine = 20.f;
	// Magazines bigger than this show as one bar instead of a segment per round
	constexpr int32 MaxAmmoSegments = 20;

	const FLinearColor Track(1.f, 1.f, 1.f, 0.78f);
		const FLinearColor WeaponDieColor = RRUI::Ink;

	FLinearColor WithAlpha(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	/** Blinking alpha for something about to run out. */
	float Blink(const UWorld* World)
	{
		return 0.35f + 0.65f * (0.5f + 0.5f * FMath::Cos(World->GetTimeSeconds() * 12.f));
	}

	/** Where text of a size sits so its baseline lands on Baseline (Nunito's ascent is about one em). */
	float TopForBaseline(float Baseline, float Size)
	{
		return Baseline - Size * 1.01f;
	}

	/** Text with a thin ink edge, for numbers over anything (damage). */
	void DrawEdgedText(UCanvas* Canvas, const FString& Text, const FVector2D& Pos, float Size, const FLinearColor& Color, const FLinearColor& Edge)
	{
		const float Offset = FMath::Max(1.f, Size * 0.08f);
		for (int32 Step = 0; Step < 8; ++Step)
		{
			const float Angle = Step * PI * 0.25f;
			RRUI::DrawText(Canvas, Text, Pos + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Offset, Size, EWeight::Black, Edge);
		}
		RRUI::DrawText(Canvas, Text, Pos, Size, EWeight::Black, Color);
	}
}

ARRRandomHUD::ARRRandomHUD()
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> FoxPortrait(TEXT("/Game/UI/T_FoxPortrait.T_FoxPortrait"));
	Portrait = FoxPortrait.Succeeded() ? FoxPortrait.Object : nullptr;
	static ConstructorHelpers::FObjectFinder<UTexture2D> HitEffectSheet(TEXT("/Game/UI/T_HitEffects.T_HitEffects"));
	HitEffects = HitEffectSheet.Succeeded() ? HitEffectSheet.Object : nullptr;
	static ConstructorHelpers::FObjectFinder<UTexture2D> CircleSprite(TEXT("/Game/UI/T_UICircle.T_UICircle"));
	UICircle = CircleSprite.Succeeded() ? CircleSprite.Object : nullptr;
	static ConstructorHelpers::FObjectFinder<UTexture2D> SoftSprite(TEXT("/Game/UI/T_UISoft.T_UISoft"));
	UISoft = SoftSprite.Succeeded() ? SoftSprite.Object : nullptr;
}

void ARRRandomHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}
	RRUI::UseShapes(UICircle, UISoft);
	UIScale = Canvas->ClipY / 720.f;

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

void ARRRandomHUD::DrawFriendOrFoeMarker(float CenterX, float Top, const FLinearColor& Color)
{
	const float S = UIScale;
	const float HalfWidth = MarkerWidth * 0.5f * S;
	const float Height = MarkerHeight * S;
	const float Border = MarkerBorder * S;
	// White border: the same triangle grown on every side, drawn first
	RRUI::DrawTriangle(Canvas, FVector2D(CenterX - HalfWidth - Border * 1.7f, Top - Border), FVector2D(CenterX + HalfWidth + Border * 1.7f, Top - Border),
		FVector2D(CenterX, Top + Height + Border * 1.9f), RRUI::White);
	RRUI::DrawTriangle(Canvas, FVector2D(CenterX - HalfWidth, Top), FVector2D(CenterX + HalfWidth, Top), FVector2D(CenterX, Top + Height), Color);
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

	const float S = UIScale;
	const bool bIsViewer = &Brawler == Viewer;
	const bool bIsAlly = Viewer && Brawler.GetTeam() == Viewer->GetTeam();
	const float BarWidth = HealthBarWidth * S;
	const float Left = Head.X - BarWidth * 0.5f;
	const float Top = Head.Y - OverheadLift * S;

	// Health: a thin rounded bar, mint for the player, sky for allies, pink for opponents, with the number above it
	const float HealthShare = FMath::Clamp(Brawler.GetHealth() / FMath::Max(1.f, Brawler.GetMaxHealth()), 0.f, 1.f);
	const FLinearColor HealthColor = bIsViewer ? RRUI::MintDeep : (bIsAlly ? RRUI::SkyDeep : RRUI::PinkDeep);
	const FBox2D BarRect(FVector2D(Left, Top), FVector2D(Left + BarWidth, Top + HealthBarHeight * S));
	RRUI::DrawPill(Canvas, BarRect.ExpandBy(FMath::Max(1.f, S)), WithAlpha(RRUI::Ink, 0.22f));
	RRUI::DrawBar(Canvas, BarRect, HealthShare, Track, HealthColor);

	const FString HealthText = FString::FromInt(FMath::CeilToInt(Brawler.GetHealth()));
	const float NumberSize = 11.f * S;
	const FVector2D HealthSize = RRUI::MeasureText(HealthText, NumberSize, EWeight::Black);
	const float HealthTextY = Top - HealthSize.Y + 1.f * S;
	RRUI::DrawGlowText(Canvas, HealthText, FVector2D(Head.X - HealthSize.X * 0.5f, HealthTextY), NumberSize, EWeight::Black, RRUI::Ink);

	// Stored dice: a small diamond and the count, right of the health bar (gold while one is the ultimate die)
	const URRRandomDiceBuffComponent* Dice = Brawler.GetDiceBuffs();
	const int32 Charges = Dice->GetCharges();
	const FLinearColor DiceMark = Dice->GetUltimateCharges() > 0 ? RRUI::Gold : Charges > 0 ? RRUI::LavenderDeep : WithAlpha(RRUI::Ink, 0.35f);
	const FVector2D Diamond(BarRect.Max.X + 9.f * S, BarRect.GetCenter().Y);
	const float Half = 4.5f * S;
	RRUI::DrawTriangle(Canvas, Diamond + FVector2D(0.f, -Half - S), Diamond + FVector2D(Half + S, 0.f), Diamond + FVector2D(-Half - S, 0.f), RRUI::White);
	RRUI::DrawTriangle(Canvas, Diamond + FVector2D(0.f, Half + S), Diamond + FVector2D(Half + S, 0.f), Diamond + FVector2D(-Half - S, 0.f), RRUI::White);
	RRUI::DrawTriangle(Canvas, Diamond + FVector2D(0.f, -Half), Diamond + FVector2D(Half, 0.f), Diamond + FVector2D(-Half, 0.f), DiceMark);
	RRUI::DrawTriangle(Canvas, Diamond + FVector2D(0.f, Half), Diamond + FVector2D(Half, 0.f), Diamond + FVector2D(-Half, 0.f), DiceMark);
	const FString ChargeText = FString::FromInt(Charges);
	const FVector2D ChargeSize = RRUI::MeasureText(ChargeText, NumberSize, EWeight::Black);
	RRUI::DrawGlowText(Canvas, ChargeText, FVector2D(Diamond.X + Half + 3.f * S, Diamond.Y - ChargeSize.Y * 0.5f), NumberSize, EWeight::Black, DiceMark);

	// Left of the health bar: the gun, if it isn't the starting pistol
	DrawWeaponChip(Brawler.GetWeapon(), Left - 5.f * S, BarRect.GetCenter().Y);

	// Below the health bar: ammo (own brawler only), then progress toward the next die
	float RowY = BarRect.Max.Y + 2.f * S;
	if (bIsViewer)
	{
		DrawAmmoBar(Brawler, Left, RowY, BarWidth, AmmoBarHeight * S);
		RowY += (AmmoBarHeight + 2.f) * S;
	}
	RRUI::DrawBar(Canvas, FBox2D(FVector2D(Left, RowY), FVector2D(Left + BarWidth, RowY + GaugeBarHeight * S)), Dice->GetGaugeProgress(), Track, RRUI::Lavender);

	// Below everything, pointing at the head: sky for the player and allies, pink for opponents
	DrawFriendOrFoeMarker(Head.X, RowY + (GaugeBarHeight + 4.f) * S, bIsAlly ? RRUI::SkyDeep : RRUI::PinkDeep);

	// Above the health number: active buffs, the power chips (giant, flight), then the latest roll
	float ChipsTop = DrawBuffChips(*Dice, Head.X, HealthTextY - 1.f * S);
	TArray<FString, TInlineAllocator<3>> PowerLabels;
	TArray<FLinearColor, TInlineAllocator<3>> PowerColors;
	TArray<float, TInlineAllocator<3>> PowerRemaining;
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
		PowerColors.Add(RRUI::Butter);
		PowerRemaining.Add(UE_BIG_NUMBER);
	}
	if (PowerLabels.Num() > 0)
	{
		ChipsTop = DrawPowerChips(PowerLabels, PowerColors, PowerRemaining, Head.X, ChipsTop);
	}
	ChipsTop = DrawRollPopup(*Dice, Head.X, ChipsTop);

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

float ARRRandomHUD::DrawPowerChips(TArrayView<const FString> Labels, TArrayView<const FLinearColor> Colors, TArrayView<const float> Remaining, float CenterX, float Bottom)
{
	// Tags side by side, e.g. | GIANT  | FLY
	const float S = UIScale;
	const float Size = ChipSize * S;
	const float Gap = 6.f * S;
	float TotalWidth = 0.f;
	float Height = 0.f;
	for (const FString& Label : Labels)
	{
		const FVector2D Tag = RRUI::MeasureTag(Label, FString(), Size);
		TotalWidth += Tag.X + (TotalWidth > 0.f ? Gap : 0.f);
		Height = Tag.Y;
	}
	const float Y = Bottom - Height;
	float X = CenterX - TotalWidth * 0.5f;
	for (int32 Index = 0; Index < Labels.Num(); ++Index)
	{
		// Blinks like a buff when about to run out
		const float Alpha = Remaining[Index] < BuffBlinkTime ? Blink(GetWorld()) : 1.f;
		X += RRUI::DrawTag(Canvas, Labels[Index], FString(), FVector2D(X, Y), Size, RRUI::Pastel(Colors[Index], 0.2f), Alpha).X + Gap;
	}
	return Y - 1.f * S;
}

void ARRRandomHUD::DrawAmmoBar(const ARRRandomCharacter& Brawler, float Left, float Top, float Width, float Height)
{
	const int32 MaxAmmo = Brawler.GetMaxAmmo();
	if (MaxAmmo <= 0)
	{
		return;
	}
	const FBox2D Rect(FVector2D(Left, Top), FVector2D(Left + Width, Top + Height));

	// While reloading, one bar fills up across the whole width instead of the rounds
	if (Brawler.IsReloading())
	{
		RRUI::DrawBar(Canvas, Rect, Brawler.GetReloadProgress(), Track, RRUI::Butter);
		return;
	}

	// Too many rounds for separate segments: one bar
	if (MaxAmmo > MaxAmmoSegments)
	{
		RRUI::DrawBar(Canvas, Rect, float(Brawler.GetAmmo()) / MaxAmmo, Track, RRUI::Gold);
		return;
	}

	const float Gap = FMath::Max(1.f, UIScale);
	const float SegmentWidth = (Width - Gap * (MaxAmmo - 1)) / MaxAmmo;
	for (int32 Index = 0; Index < MaxAmmo; ++Index)
	{
		const float X = Left + Index * (SegmentWidth + Gap);
		RRUI::DrawRounded(Canvas, FBox2D(FVector2D(X, Top), FVector2D(X + SegmentWidth, Top + Height)), Height * 0.5f, Index < Brawler.GetAmmo() ? RRUI::Gold : Track);
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
	const FString Text = Ridden ? TEXT("GET OFF")
		: FString::Printf(TEXT("RIDE %s%s"), Near->GetHandling().Name, Near->IsNextSeatRear() ? TEXT(" (BACK SEAT)") : TEXT(""));
	const FVector Feet = Brawler.GetActorLocation() - FVector(0.f, 0.f, Brawler.GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FVector Screen = Project(Feet);
	if (Screen.Z <= 0.f)
	{
		return;
	}
	// [F] key cap, then the words
	const float S = UIScale;
	const float Size = 12.f * S;
	const float KeyHeight = 18.f * S;
	const float Gap = 6.f * S;
	const FVector2D Label = RRUI::MeasureText(Text, Size, EWeight::ExtraBold, Size * 0.06f);
	const float KeyWidth = FMath::Max(KeyHeight, RRUI::MeasureText(TEXT("F"), KeyHeight * 0.62f, EWeight::Black).X + KeyHeight * 0.5f);
	const float X = Screen.X - (KeyWidth + Gap + Label.X) * 0.5f;
	const float Y = Screen.Y + 8.f * S;
	RRUI::DrawKey(Canvas, TEXT("F"), FVector2D(X, Y), KeyHeight);
	RRUI::DrawGlowText(Canvas, Text, FVector2D(X + KeyWidth + Gap, Y + (KeyHeight - Label.Y) * 0.5f), Size, EWeight::ExtraBold, Ridden ? RRUI::InkSoft : RRUI::Ink, Size * 0.06f);
}

void ARRRandomHUD::DrawAmmoPanel(const ARRRandomCharacter& Brawler)
{
	// Bottom right on one soft white plate: "PISTOL  12 / 15  [R]", or the reload filling up
	const float S = UIScale;
	const float Right = Canvas->ClipX - Margin * S;
	const float Baseline = Canvas->ClipY - Margin * S - 6.f * S;
	const FRRWeapon& Weapon = Brawler.GetWeapon();
	const float NameSize = 11.f * S;
	const float CountSize = 40.f * S;
	const float MaxSize = 16.f * S;
	const float KeyHeight = 20.f * S;
	const FString Short = Weapon.bFoamFinger ? TEXT("FINGER") : Weapon.Name;
	const float ShortWidth = RRUI::MeasureText(Short, NameSize, EWeight::ExtraBold, NameSize * 0.2f).X;
	const FString Count = FString::FromInt(Brawler.GetAmmo());
	const FString MaxText = FString::Printf(TEXT("/ %d"), Brawler.GetMaxAmmo());
	const float CountWidth = RRUI::MeasureText(Count, CountSize, EWeight::Black).X;
	const float MaxWidth = RRUI::MeasureText(MaxText, MaxSize, EWeight::ExtraBold).X;
	const float KeyWidth = FMath::Max(KeyHeight, RRUI::MeasureText(TEXT("R"), KeyHeight * 0.62f, EWeight::Black).X + KeyHeight * 0.5f);
	const float ReloadWidth = 120.f * S;
	const bool bReloading = Brawler.IsReloading();
	const float RowWidth = ShortWidth + 10.f * S + (bReloading ? ReloadWidth : CountWidth + 6.f * S + MaxWidth + 8.f * S + KeyWidth);
	RRUI::DrawPlate(Canvas, FBox2D(FVector2D(Right - RowWidth, Baseline - CountSize * 0.72f), FVector2D(Right, Baseline + 2.f * S)), 22.f * S);

	float X = Right;
	if (bReloading)
	{
		// The bar, and RELOADING over its left end
		const FBox2D Bar(FVector2D(Right - ReloadWidth, Baseline - 4.f * S), FVector2D(Right, Baseline));
		RRUI::DrawBar(Canvas, Bar, Brawler.GetReloadProgress(), WithAlpha(RRUI::Ink, 0.12f), RRUI::Butter);
		const float Size = 14.f * S;
		RRUI::DrawText(Canvas, TEXT("RELOADING"), FVector2D(Bar.Min.X, Bar.Min.Y - Size * 1.5f), Size, EWeight::Black, RRUI::Ink, Size * 0.1f);
		X = Bar.Min.X;
	}
	else
	{
		X -= KeyWidth;
		RRUI::DrawKey(Canvas, TEXT("R"), FVector2D(X, Baseline - KeyHeight), KeyHeight);
		X -= 8.f * S + MaxWidth;
		RRUI::DrawText(Canvas, MaxText, FVector2D(X, TopForBaseline(Baseline, MaxSize)), MaxSize, EWeight::ExtraBold, RRUI::InkSoft);
		X -= 6.f * S + CountWidth;
		// Pink when nearly empty
		const FLinearColor CountColor = Brawler.GetAmmo() * 4 <= Brawler.GetMaxAmmo() ? RRUI::PinkDeep : RRUI::Ink;
		RRUI::DrawText(Canvas, Count, FVector2D(X, TopForBaseline(Baseline, CountSize)), CountSize, EWeight::Black, CountColor);
	}
	X -= 10.f * S;
	RRUI::DrawText(Canvas, Short, FVector2D(X - ShortWidth, TopForBaseline(Baseline, NameSize)), NameSize, EWeight::ExtraBold, RRUI::InkSoft, NameSize * 0.2f);

	// A new gun and how it differs from the pistol, e.g. "EPIC CANNON  DMG x2.3 · SHOT SPD x0.5", over the rest
	if (!Weapon.bIsDefault)
	{
		const FString Label = Weapon.bFoamFinger ? TEXT("FOAM FINGER") : FString::Printf(TEXT("%s %s"), *FRRWeapon::GetTierName(Weapon.Tier), *Weapon.Name);
		const FString Detail = Weapon.bFoamFinger ? TEXT("DMG 1 · BACK POKE = KO")
			: FString::Printf(TEXT("DMG x%.1f · SHOT SPD x%.1f"), Weapon.DamageMultiplier, Weapon.ProjectileSpeedMultiplier);
		const float Size = 11.f * S;
		const FVector2D Tag = RRUI::MeasureTag(Label, Detail, Size);
		RRUI::DrawTag(Canvas, Label, Detail, FVector2D(Right - Tag.X, Baseline - 40.f * S - Tag.Y - 6.f * S), Size, RRUI::Pastel(FRRWeapon::GetTierColor(Weapon.Tier), 0.15f));
	}
}

void ARRRandomHUD::DrawWeaponChip(const FRRWeapon& Weapon, float Right, float CenterY)
{
	if (Weapon.bIsDefault)
	{
		return;
	}
	// The gun's name in its tier's color, ending at Right
	const float Size = 10.f * UIScale;
	const FVector2D Label = RRUI::MeasureText(Weapon.Name, Size, EWeight::Black, Size * 0.06f);
	RRUI::DrawGlowText(Canvas, Weapon.Name, FVector2D(Right - Label.X, CenterY - Label.Y * 0.5f), Size, EWeight::Black, RRUI::Readable(FRRWeapon::GetTierColor(Weapon.Tier)), Size * 0.06f);
}

float ARRRandomHUD::DrawBuffChips(const URRRandomDiceBuffComponent& Dice, float CenterX, float Bottom)
{
	// One tag per stat with the summed bonus, e.g. "| ATK+25"
	constexpr int32 StatCount = 3;
	float TotalBonus[StatCount] = {};
	float SoonestExpiry[StatCount] = { UE_BIG_NUMBER, UE_BIG_NUMBER, UE_BIG_NUMBER };
	for (const FRRDiceBuff& Buff : Dice.GetActiveBuffs())
	{
		const int32 Index = static_cast<int32>(Buff.Stat);
		TotalBonus[Index] += Buff.Bonus;
		SoonestExpiry[Index] = FMath::Min(SoonestExpiry[Index], Buff.RemainingTime);
	}

	const float S = UIScale;
	const float Size = ChipSize * S;
	const float Gap = 6.f * S;
	TArray<int32, TInlineAllocator<StatCount>> Shown;
	TArray<FString, TInlineAllocator<StatCount>> Labels;
	float TotalWidth = 0.f;
	float Height = 0.f;
	for (int32 Index = 0; Index < StatCount; ++Index)
	{
		if (TotalBonus[Index] <= 0.f)
		{
			continue;
		}
		const FString Label = FString::Printf(TEXT("%s+%d"), *URRRandomDiceBuffComponent::GetStatShortName(static_cast<ERRDiceBuffStat>(Index)), FMath::RoundToInt(TotalBonus[Index] * 100.f));
		const FVector2D Tag = RRUI::MeasureTag(Label, FString(), Size);
		Shown.Add(Index);
		Labels.Add(Label);
		TotalWidth += Tag.X + (Shown.Num() > 1 ? Gap : 0.f);
		Height = Tag.Y;
	}
	if (Shown.Num() == 0)
	{
		return Bottom;
	}

	const float Y = Bottom - Height;
	float X = CenterX - TotalWidth * 0.5f;
	for (int32 Chip = 0; Chip < Shown.Num(); ++Chip)
	{
		const int32 Index = Shown[Chip];
		const float Alpha = SoonestExpiry[Index] < BuffBlinkTime ? Blink(GetWorld()) : 1.f;
		const FLinearColor Mark = RRUI::Pastel(URRRandomDiceBuffComponent::GetStatColor(static_cast<ERRDiceBuffStat>(Index)), 0.25f);
		X += RRUI::DrawTag(Canvas, Labels[Chip], FString(), FVector2D(X, Y), Size, Mark, Alpha).X + Gap;
	}
	return Y - 1.f * S;
}

float ARRRandomHUD::DrawRollPopup(const URRRandomDiceBuffComponent& Dice, float CenterX, float Bottom)
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
	const float Size = 13.f * UIScale;
	const FVector2D TextSize = RRUI::MeasureText(Text, Size, EWeight::Black, Size * 0.04f);
	const float Y = Bottom - TextSize.Y;
	if (Landed >= 0.f)
	{
		const FLinearColor DieColor = Die.bUltimate ? URRRandomDiceBuffComponent::GetUltimateColor() : Die.bWeapon ? WeaponDieColor
			: Die.bGiant ? URRRandomDiceBuffComponent::GetGiantColor()
			: Die.bFlight ? URRRandomDiceBuffComponent::GetFlightColor() : URRRandomDiceBuffComponent::GetStatColor(Die.Stat);
		const float Alpha = FMath::Clamp((RollPopupTime - Landed) / RollPopupFadeTime, 0.f, 1.f);
		RRUI::DrawGlowText(Canvas, Text, FVector2D(CenterX - TextSize.X * 0.5f, Y), Size, EWeight::Black, WithAlpha(RRUI::Readable(DieColor), Alpha), Size * 0.04f);
	}
	return Y - 1.f * UIScale;
}

void ARRRandomHUD::DrawDamagePopups(const ARRRandomCharacter& Brawler, const ARRRandomCharacter* Viewer)
{
	if (Brawler.GetDamagePopups().Num() == 0)
	{
		return;
	}

	// Beside the body so the numbers don't cover the health bar; pink when our side gets hurt
	const FVector Body = Project(Brawler.GetActorLocation());
	if (Body.Z <= 0.f)
	{
		return;
	}
	const float S = UIScale;
	const bool bFriendly = Viewer && Brawler.GetTeam() == Viewer->GetTeam();

	// The skull sticker of a foam finger's knockout first, under the numbers: popping in with an overshoot, holding, then
	// fading and shrinking. The guns' stickers aren't drawn any more; the glowing hit bursts in the world
	// (ARRRandomEffects) took their place.
	if (HitEffects)
	{
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
			const float Size = HitEffectSizes[Cell] * S * Bounce * FMath::Lerp(0.8f, 1.f, Fade);
			// A skull drifts up off the fallen body
			const float Rise = Popup.Age * 30.f * S;
			const FVector2D Center(Body.X + Popup.Nudge.X * S, Body.Y + Popup.Nudge.Y * S - Rise);
			DrawTexture(HitEffects, Center.X - Size * 0.5f, Center.Y - Size * 0.5f, Size, Size,
				float(Cell % HitEffectColumns) / HitEffectColumns, float(Cell / HitEffectColumns) / HitEffectRows, 1.f / HitEffectColumns, 1.f / HitEffectRows,
				FLinearColor(1.f, 1.f, 1.f, Fade), BLEND_Translucent, 1.f, false, 0.f, FVector2D(0.5f, 0.5f));
		}
	}

	const float Size = 18.f * S;
	for (const FRRDamagePopup& Popup : Brawler.GetDamagePopups())
	{
		const FString Text = FString::FromInt(FMath::RoundToInt(Popup.Amount));
		const FVector2D TextSize = RRUI::MeasureText(Text, Size, EWeight::Black);
		const float Alpha = FMath::Max(0.f, 1.f - Popup.Age / ARRRandomCharacter::DamagePopupLifetime);
		const FVector2D Pos(Body.X + (50.f + Popup.OffsetX) * S - TextSize.X * 0.5f, Body.Y - Popup.Age * DamagePopupRiseSpeed * S);
		// White numbers with an ink edge on opponents; pink with a white edge when it's our side
		DrawEdgedText(Canvas, Text, Pos, Size, WithAlpha(bFriendly ? RRUI::PinkDeep : RRUI::White, Alpha), WithAlpha(bFriendly ? RRUI::White : RRUI::Ink, 0.7f * Alpha));
	}
}

void ARRRandomHUD::DrawDicePanel(const ARRRandomCharacter& Viewer)
{
	const URRRandomDiceBuffComponent& Dice = *Viewer.GetDiceBuffs();
	const float S = UIScale;

	// My ultimate die: a butter flash and big words across the middle of the screen while it tumbles and as it lands
	const FRRDiceRoll& Roll = Dice.GetLastRoll();
	const float RollAge = Dice.GetTimeSinceLastRoll();
	const float Spin = Dice.GetSpinTime();
	if (Roll.bUltimate && RollAge < Spin + UltimateBannerTime)
	{
		FString Banner;
		float Zoom;
		float Alpha = 1.f;
		if (RollAge < Spin)
		{
			Banner = TEXT("ULTIMATE DIE!");
			Zoom = 1.f + 0.1f * FMath::Sin(RollAge * 12.f);
			Alpha = FMath::Clamp(RollAge / 0.15f, 0.f, 1.f);
		}
		else
		{
			const float Landed = RollAge - Spin;
			Banner = RRSpecies::GetUltimateBanner(Viewer.GetSpecies(), Roll.Face);
			// Slams in big, settles, fades out at the end
			Zoom = 1.12f + 0.6f * FMath::Max(0.f, 1.f - Landed / 0.2f);
			Alpha = FMath::Clamp((UltimateBannerTime - Landed) / 0.4f, 0.f, 1.f);
			RRUI::DrawBox(Canvas, FBox2D(FVector2D::ZeroVector, FVector2D(Canvas->ClipX, Canvas->ClipY)), WithAlpha(RRUI::Butter, 0.45f * FMath::Max(0.f, 1.f - Landed / 0.35f)));
		}
		const float Size = 64.f * S;
		const FVector2D BannerSize = RRUI::MeasureText(Banner, Size, EWeight::Black, Size * 0.06f) * Zoom;
		// Below the crosshair, clear of the labels over the brawlers' heads
		const FVector2D Pos((Canvas->ClipX - BannerSize.X) * 0.5f, Canvas->ClipY * 0.66f - BannerSize.Y * 0.5f);
		const FLinearColor Stops[] = { RRUI::Gold, RRUI::PinkDeep, RRUI::LavenderDeep };
		RRUI::DrawGradientText(Canvas, Banner, Pos, Size, EWeight::Black, Stops, Alpha, Size * 0.06f, Zoom);
	}

	// Bottom left on one soft white plate: the big count of stored dice, the gauge toward the next one, and [E]
	const float Left = Margin * S;
	const float Baseline = Canvas->ClipY - Margin * S - 6.f * S;
	const bool bUltimate = Dice.GetUltimateCharges() > 0;
	const float CountSize = 30.f * S;
	const float LabelSize = 10.f * S;
	const float KeyHeight = 20.f * S;
	const FString Count = FString::FromInt(Dice.GetCharges());
	const float CountWidth = RRUI::MeasureText(Count, CountSize, EWeight::Black).X;
	const float LabelWidth = RRUI::MeasureText(TEXT("DICE"), LabelSize, EWeight::ExtraBold, LabelSize * 0.2f).X;
	const float KeyWidth = FMath::Max(KeyHeight, RRUI::MeasureText(TEXT("E"), KeyHeight * 0.62f, EWeight::Black).X + KeyHeight * 0.5f);
	const float RowWidth = CountWidth + 4.f * S + LabelWidth + 12.f * S + DiceGaugeWidth * S + 10.f * S + KeyWidth;
	RRUI::DrawPlate(Canvas, FBox2D(FVector2D(Left, Baseline - CountSize * 0.72f), FVector2D(Left + RowWidth, Baseline + 2.f * S)), 20.f * S);

	float X = Left;
	X += RRUI::DrawText(Canvas, Count, FVector2D(X, TopForBaseline(Baseline, CountSize)), CountSize, EWeight::Black,
		bUltimate ? RRUI::Gold : Dice.GetCharges() > 0 ? RRUI::Ink : RRUI::InkSoft).X;
	X += 4.f * S;
	X += RRUI::DrawText(Canvas, TEXT("DICE"), FVector2D(X, TopForBaseline(Baseline, LabelSize)), LabelSize, EWeight::ExtraBold, RRUI::InkSoft, LabelSize * 0.2f).X;
	X += 12.f * S;
	const FBox2D Gauge(FVector2D(X, Baseline - 7.f * S), FVector2D(X + DiceGaugeWidth * S, Baseline - 3.f * S));
	RRUI::DrawBar(Canvas, Gauge, Dice.GetGaugeProgress(), WithAlpha(RRUI::Ink, 0.12f), RRUI::Lavender);
	X = Gauge.Max.X + 10.f * S;
	X += RRUI::DrawKey(Canvas, TEXT("E"), FVector2D(X, Baseline - KeyHeight + 2.f * S), KeyHeight);
	if (bUltimate)
	{
		const float Size = 11.f * S;
		RRUI::DrawTag(Canvas, TEXT("ULTIMATE"), RRSpecies::GetUltimateName(Viewer.GetSpecies()), FVector2D(X + 14.f * S, TopForBaseline(Baseline, Size)), Size, RRUI::Gold);
	}

	// Powers (the ultimate, flight, giant), then active buffs, stacked upward as tags
	const float TagSize = 12.f * S;
	float LineY = TopForBaseline(Baseline, CountSize) - TagLine * S;
	auto Line = [&](const FString& Label, const FString& Detail, const FLinearColor& Mark, float Alpha)
	{
		RRUI::DrawTag(Canvas, Label, Detail, FVector2D(Left, LineY), TagSize, Mark, Alpha);
		LineY -= TagLine * S;
	};
	if (const ARRRandomUltimate* Ultimate = Viewer.GetUltimate())
	{
		Line(Ultimate->GetStatusLine(), FString(), RRUI::Gold, 1.f);
	}
	if (Viewer.CanFly())
	{
		const TCHAR* Dash = Viewer.IsDashing() ? TEXT("DASHING") : Viewer.HasDash() ? TEXT("SPACE x2 HOLD: DASH") : TEXT("DASH AFTER LANDING");
		Line(TEXT("FLY"), FString::Printf(TEXT("HOLD SPACE: RISE · %s · %.0fs"), Dash, FMath::CeilToFloat(Viewer.GetFlightRemaining())),
			RRUI::Pastel(URRRandomDiceBuffComponent::GetFlightColor(), 0.2f), Viewer.GetFlightRemaining() < BuffBlinkTime ? Blink(GetWorld()) : 1.f);
	}
	if (Viewer.IsGiant())
	{
		Line(TEXT("GIANT"), FString::Printf(TEXT("ATK x%.1f · DMG TAKEN x%.1f · %.0fs"), Viewer.GetGiantDamageMultiplier(), Viewer.GetGiantDamageTakenMultiplier(), FMath::CeilToFloat(Viewer.GetGiantRemaining())),
			RRUI::Pastel(URRRandomDiceBuffComponent::GetGiantColor(), 0.2f), Viewer.GetGiantRemaining() < BuffBlinkTime ? Blink(GetWorld()) : 1.f);
	}
	for (const FRRDiceBuff& Buff : Dice.GetActiveBuffs())
	{
		Line(FString::Printf(TEXT("%s +%d%%"), *URRRandomDiceBuffComponent::GetStatName(Buff.Stat).ToString().ToUpper(), FMath::RoundToInt(Buff.Bonus * 100.f)),
			FString::Printf(TEXT("%.0fs"), FMath::CeilToFloat(Buff.RemainingTime)),
			RRUI::Pastel(URRRandomDiceBuffComponent::GetStatColor(Buff.Stat), 0.25f), Buff.RemainingTime < BuffBlinkTime ? Blink(GetWorld()) : 1.f);
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
	case NM_ListenServer: Connection = FString::Printf(TEXT("HOSTING (%s) · %d PLAYER%s"), Service, Players, Players == 1 ? TEXT("") : TEXT("S")); break;
	case NM_Client: Connection = FString::Printf(TEXT("ONLINE (%s) · %d PLAYER%s"), Service, Players, Players == 1 ? TEXT("") : TEXT("S")); break;
	default: Connection = FString::Printf(TEXT("SOLO · ONLINE VIA %s"), Service); break;
	}

	const float S = UIScale;
	const float Size = 10.f * S;
	const float Tracking = Size * 0.06f;
	const float Left = Margin * S;
	const float Step = Size * 1.6f;
	float Y = (MinimapMargin + MinimapSize + 30.f) * S;
	RRUI::DrawGlowText(Canvas, Connection, FVector2D(Left, Y), Size, EWeight::ExtraBold, RRUI::Ink, Tracking);
	Y += Step;
	RRUI::DrawGlowText(Canvas, TEXT("F1 HOST · F2 JOIN · F3 LEAVE · ESC MENU · M MAP"), FVector2D(Left, Y), Size, EWeight::ExtraBold, RRUI::InkSoft, Tracking);
	Y += Step;
#if !UE_BUILD_SHIPPING
	RRUI::DrawGlowText(Canvas, TEXT("CHEATS · F6 WIN · F7 GIANT · F8 FLY · F9 3RD PERSON · F10 GOD · U ULTIMATE DIE"), FVector2D(Left, Y), Size, EWeight::ExtraBold, RRUI::InkSoft, Tracking);
	Y += Step;
	RRUI::DrawGlowText(Canvas, TEXT("1-7 GUNS · PISTOL MINIGUN SMG RIFLE BLASTER CANNON FINGER"), FVector2D(Left, Y), Size, EWeight::ExtraBold, RRUI::InkSoft, Tracking);
	Y += Step;
#endif
	if (!Sessions->GetStatus().IsEmpty())
	{
		RRUI::DrawGlowText(Canvas, Sessions->GetStatus(), FVector2D(Left, Y), Size, EWeight::ExtraBold, RRUI::PinkDeep, Tracking);
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

	// "BLUE  3 · 2  RED" centered at the top, a thin line filling from both ends toward the goal, and what wins under
	// it, all on one soft white plate
	const float S = UIScale;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Top = 14.f * S;
	const float NumberSize = 30.f * S;
	const float TeamSize = 11.f * S;
	const float GoalSize = 9.f * S;
	const float Gap = 14.f * S;
	const FString Blue = FString::FromInt(State->GetTeamScore(0));
	const FString Red = FString::FromInt(State->GetTeamScore(1));
	const float BlueLabel = RRUI::MeasureText(TEXT("BLUE"), TeamSize, EWeight::Black, TeamSize * 0.2f).X;
	const float RedLabel = RRUI::MeasureText(TEXT("RED"), TeamSize, EWeight::Black, TeamSize * 0.2f).X;
	const float BlueWidth = RRUI::MeasureText(Blue, NumberSize, EWeight::Black).X;
	const float RedWidth = RRUI::MeasureText(Red, NumberSize, EWeight::Black).X;
	const float DotWidth = RRUI::MeasureText(TEXT("·"), NumberSize, EWeight::Black).X;
	const float Baseline = Top + NumberSize * 1.01f;
	const float LineWidth = 150.f * S;
	const float LineY = Baseline + 8.f * S;
	const bool bGoal = State->GetScoreToWin() > 0;
	const float GroupLeft = CenterX - DotWidth * 0.5f - Gap * 0.6f - BlueWidth - Gap - BlueLabel;
	const float GroupRight = CenterX + DotWidth * 0.5f + Gap * 0.6f + RedWidth + Gap + RedLabel;
	const float GroupBottom = bGoal ? LineY + 8.f * S + GoalSize * 1.2f : LineY + 4.f * S;
	RRUI::DrawPlate(Canvas, FBox2D(FVector2D(GroupLeft, Top + NumberSize * 0.3f), FVector2D(GroupRight, GroupBottom)), 18.f * S);

	// The dot sits in the middle of the screen
	float X = CenterX - DotWidth * 0.5f;
	RRUI::DrawText(Canvas, TEXT("·"), FVector2D(X, Top), NumberSize, EWeight::Black, WithAlpha(RRUI::Ink, 0.4f));
	X -= Gap * 0.6f + BlueWidth;
	RRUI::DrawText(Canvas, Blue, FVector2D(X, Top), NumberSize, EWeight::Black, RRUI::Ink);
	X -= Gap + BlueLabel;
	RRUI::DrawText(Canvas, TEXT("BLUE"), FVector2D(X, TopForBaseline(Baseline, TeamSize)), TeamSize, EWeight::Black, RRUI::SkyDeep, TeamSize * 0.2f);
	X = CenterX + DotWidth * 0.5f + Gap * 0.6f;
	RRUI::DrawText(Canvas, Red, FVector2D(X, Top), NumberSize, EWeight::Black, RRUI::Ink);
	X += RedWidth + Gap;
	RRUI::DrawText(Canvas, TEXT("RED"), FVector2D(X, TopForBaseline(Baseline, TeamSize)), TeamSize, EWeight::Black, RRUI::PinkDeep, TeamSize * 0.2f);

	const FBox2D Line(FVector2D(CenterX - LineWidth * 0.5f, LineY), FVector2D(CenterX + LineWidth * 0.5f, LineY + 4.f * S));
	RRUI::DrawPill(Canvas, Line, WithAlpha(RRUI::Ink, 0.12f));
	if (bGoal)
	{
		// Each team fills its half toward the goal
		const float Half = LineWidth * 0.5f;
		const float BlueShare = FMath::Clamp(float(State->GetTeamScore(0)) / State->GetScoreToWin(), 0.f, 1.f);
		const float RedShare = FMath::Clamp(float(State->GetTeamScore(1)) / State->GetScoreToWin(), 0.f, 1.f);
		if (BlueShare > 0.f)
		{
			RRUI::DrawPill(Canvas, FBox2D(Line.Min, FVector2D(Line.Min.X + FMath::Max(Line.GetSize().Y, Half * BlueShare), Line.Max.Y)), RRUI::Sky);
		}
		if (RedShare > 0.f)
		{
			RRUI::DrawPill(Canvas, FBox2D(FVector2D(Line.Max.X - FMath::Max(Line.GetSize().Y, Half * RedShare), Line.Min.Y), Line.Max), RRUI::Pink);
		}

		const FString Goal = FString::Printf(TEXT("FIRST TO %d"), State->GetScoreToWin());
		const float GoalWidth = RRUI::MeasureText(Goal, GoalSize, EWeight::ExtraBold, GoalSize * 0.22f).X;
		RRUI::DrawText(Canvas, Goal, FVector2D(CenterX - GoalWidth * 0.5f, Line.Max.Y + 3.f * S), GoalSize, EWeight::ExtraBold, RRUI::InkSoft, GoalSize * 0.22f);
	}
}

void ARRRandomHUD::DrawRespawnCountdown(const ARRRandomCharacter& Viewer)
{
	// KNOCKED OUT, and a big count to getting back up
	const float S = UIScale;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Top = Canvas->ClipY * 0.28f;
	const float LabelSize = 13.f * S;
	const FString Label = TEXT("KNOCKED OUT · BACK IN");
	const FVector2D LabelBox = RRUI::MeasureText(Label, LabelSize, EWeight::ExtraBold, LabelSize * 0.24f);
	RRUI::DrawGlowText(Canvas, Label, FVector2D(CenterX - LabelBox.X * 0.5f, Top), LabelSize, EWeight::ExtraBold, RRUI::InkSoft, LabelSize * 0.24f);
	const float NumberSize = 52.f * S;
	const FString Number = FString::FromInt(FMath::CeilToInt(Viewer.GetRespawnRemaining()));
	const float NumberWidth = RRUI::MeasureText(Number, NumberSize, EWeight::Black).X;
	RRUI::DrawGlowText(Canvas, Number, FVector2D(CenterX - NumberWidth * 0.5f, Top + LabelBox.Y), NumberSize, EWeight::Black, RRUI::Ink);
}

namespace
{
	// Results screen, sized for a 720 pixel tall screen and scaled with it
	// The text and buttons fade in once the camera has mostly closed in
	constexpr float ResultsTextDelay = 0.9f;
	constexpr float ResultsFadeTime = 0.6f;
	constexpr float ResultsButtonWidth = 200.f;
	constexpr float ResultsButtonHeight = 50.f;
	constexpr float ResultsButtonGap = 16.f;
	constexpr float BoardWidth = 250.f;
	constexpr float BoardRowHeight = 30.f;

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

	/** Text centered on CenterX with its top at Y; returns its size. */
	FVector2D DrawCentered(UCanvas* Canvas, const FString& Text, float CenterX, float Y, float Size, EWeight Weight, const FLinearColor& Color, float Tracking = 0.f, bool bGlow = false)
	{
		const FVector2D Box = RRUI::MeasureText(Text, Size, Weight, Tracking);
		const FVector2D Pos(CenterX - Box.X * 0.5f, Y);
		return bGlow ? RRUI::DrawGlowText(Canvas, Text, Pos, Size, Weight, Color, Tracking) : RRUI::DrawText(Canvas, Text, Pos, Size, Weight, Color, Tracking);
	}
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
	// A frosted panel: the team and its score over a line in the team color, then a row per brawler with thin
	// dividers: name (YOU in lavender, the MVP tagged in gold) and knockouts
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	const TArray<TPair<const ARRRandomCharacter*, FString>> Board = GetTeamBoard(GetWorld(), Team, Viewer);
	const float Row = BoardRowHeight * Scale;
	const float PadX = 14.f * Scale;
	const float PadY = 8.f * Scale;
	const float Height = Row * (Board.Num() + 1) + PadY * 2.f;
	RRUI::DrawPanel(Canvas, FBox2D(FVector2D(X, Top), FVector2D(X + Width, Top + Height)), 20.f * Scale, Scale, WithAlpha(RRUI::Panel, Alpha));

	const float Left = X + PadX;
	const float Right = X + Width - PadX;
	const float HeaderSize = 12.f * Scale;
	float Y = Top + PadY;
	const float TextY = (Row - RRUI::MeasureText(TEXT("0"), HeaderSize, EWeight::Black).Y) * 0.5f;
	RRUI::DrawText(Canvas, Team == 0 ? TEXT("BLUE") : TEXT("RED"), FVector2D(Left, Y + TextY), HeaderSize, EWeight::Black, WithAlpha(RRUI::Ink, Alpha), HeaderSize * 0.16f);
	const FString Score = FString::FromInt(State ? State->GetTeamScore(Team) : 0);
	const float ScoreWidth = RRUI::MeasureText(Score, HeaderSize, EWeight::Black).X;
	RRUI::DrawText(Canvas, Score, FVector2D(Right - ScoreWidth, Y + TextY), HeaderSize, EWeight::Black, WithAlpha(RRUI::Ink, Alpha));
	Y += Row;
	RRUI::DrawPill(Canvas, FBox2D(FVector2D(Left, Y - 3.f * Scale), FVector2D(Right, Y)), WithAlpha(RRUI::TeamColor(Team), Alpha));

	const float NameSize = 14.f * Scale;
	const float RowText = (Row - RRUI::MeasureText(TEXT("0"), NameSize, EWeight::ExtraBold).Y) * 0.5f;
	for (int32 Index = 0; Index < Board.Num(); ++Index)
	{
		const TPair<const ARRRandomCharacter*, FString>& Entry = Board[Index];
		if (Index > 0)
		{
			RRUI::DrawBox(Canvas, FBox2D(FVector2D(Left, Y), FVector2D(Right, Y + FMath::Max(1.f, Scale))), WithAlpha(RRUI::Ink, 0.12f * Alpha));
		}
		const bool bIsYou = Entry.Key == Viewer;
		float NameX = Left + 4.f * Scale;
		NameX += RRUI::DrawText(Canvas, Entry.Value, FVector2D(NameX, Y + RowText), NameSize, EWeight::ExtraBold, WithAlpha(bIsYou ? RRUI::LavenderDeep : RRUI::Ink, Alpha)).X;
		if (Entry.Key == MVP)
		{
			const float TagSize = 10.f * Scale;
			RRUI::DrawText(Canvas, TEXT("MVP"), FVector2D(NameX + 8.f * Scale, Y + (Row - RRUI::MeasureText(TEXT("MVP"), TagSize, EWeight::Black).Y) * 0.5f),
				TagSize, EWeight::Black, WithAlpha(RRUI::Gold, Alpha), TagSize * 0.14f);
		}
		const FString Kills = FString::Printf(TEXT("%d KO"), Entry.Key->GetKills());
		const float KillsWidth = RRUI::MeasureText(Kills, NameSize, EWeight::ExtraBold).X;
		RRUI::DrawText(Canvas, Kills, FVector2D(Right - 4.f * Scale - KillsWidth, Y + RowText), NameSize, EWeight::ExtraBold, WithAlpha(RRUI::InkSoft, Alpha));
		Y += Row;
	}
	return Top + Height;
}

void ARRRandomHUD::DrawResults(const ARRRandomGameState& State, const ARRRandomCharacter* Viewer)
{
	// Straight over the game (no shading, no bars): the headline, the score in a pill, the two team boards, the MVP
	// and the buttons, all fading in once the camera has closed in on the MVP
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (ResultsShownAt < 0.f)
	{
		ResultsShownAt = Now;
	}
	const float Age = Now - ResultsShownAt;
	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float S = UIScale;
	const float CenterX = ScreenW * 0.5f;

	const float Fade = EaseOut((Age - ResultsTextDelay) / ResultsFadeTime);
	if (Fade <= 0.f)
	{
		ResultsButtons.Reset();
		return;
	}

	// Headline: how it went for this screen's team (or who won, for a screen without a brawler), letter by letter
	// along a gradient: lavender, pink and gold for a win, sky to lavender for a loss
	const int32 Winner = State.GetWinningTeam();
	const bool bWon = Viewer && Viewer->GetTeam() == Winner;
	const FString Headline = Viewer ? (bWon ? TEXT("VICTORY") : TEXT("DEFEAT")) : (Winner == 0 ? TEXT("BLUE WINS") : TEXT("RED WINS"));
	const float Pop = 1.f + 0.3f * (1.f - Fade);
	const float HeadSize = 86.f * S;
	const float HeadTracking = HeadSize * 0.14f;
	const FVector2D HeadBox = RRUI::MeasureText(Headline, HeadSize, EWeight::Black, HeadTracking);
	const float HeadTop = 62.f * S;
	const FVector2D HeadPos(CenterX - HeadBox.X * Pop * 0.5f, HeadTop + HeadBox.Y * (1.f - Pop) * 0.5f);
	const FLinearColor WinStops[] = { RRUI::LavenderDeep, RRUI::PinkDeep, RRUI::Gold };
	const FLinearColor LossStops[] = { RRUI::SkyDeep, RRUI::LavenderDeep };
	const FLinearColor TeamStops[] = { RRUI::TeamColorDeep(Winner), RRUI::LavenderDeep };
	const TArrayView<const FLinearColor> Stops = !Viewer ? TArrayView<const FLinearColor>(TeamStops) : bWon ? TArrayView<const FLinearColor>(WinStops) : TArrayView<const FLinearColor>(LossStops);
	RRUI::DrawGradientText(Canvas, Headline, HeadPos, HeadSize, EWeight::Black, Stops, Fade, HeadTracking, Pop);

	// The score in a frosted pill
	const float ResultSize = 13.f * S;
	const FString Result = FString::Printf(TEXT("%s WINS · %d : %d"), Winner == 0 ? TEXT("BLUE") : TEXT("RED"), State.GetTeamScore(0), State.GetTeamScore(1));
	const FVector2D ResultBox = RRUI::MeasureText(Result, ResultSize, EWeight::ExtraBold, ResultSize * 0.24f);
	const float PillTop = HeadTop + HeadBox.Y + 4.f * S;
	const FBox2D Pill(FVector2D(CenterX - ResultBox.X * 0.5f - 20.f * S, PillTop), FVector2D(CenterX + ResultBox.X * 0.5f + 20.f * S, PillTop + ResultBox.Y + 8.f * S));
	RRUI::DrawPanel(Canvas, Pill, Pill.GetSize().Y * 0.5f, S, WithAlpha(RRUI::Panel, Fade));
	RRUI::DrawText(Canvas, Result, FVector2D(CenterX - ResultBox.X * 0.5f, PillTop + 4.f * S), ResultSize, EWeight::ExtraBold, WithAlpha(RRUI::Ink, Fade), ResultSize * 0.24f);

	// The teams on either side, the MVP in the middle of the screen between them
	const ARRRandomCharacter* MVP = State.GetMVP();
	const float BoardTop = 210.f * S;
	DrawTeamBoard(0, Viewer, MVP, 50.f * S, BoardTop, BoardWidth * S, Fade, S);
	DrawTeamBoard(1, Viewer, MVP, ScreenW - (50.f + BoardWidth) * S, BoardTop, BoardWidth * S, Fade, S);

	// MVP pill under the hero: round portrait in a butter ring, MVP, the name and its knockouts
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
		const float Face = 64.f * S;
		const float NameSize = 26.f * S;
		const float TagSize = 10.f * S;
		const float KOSize = 14.f * S;
		const FString KO = FString::Printf(TEXT("%d KO"), MVP->GetKills());
		const float NameWidth = RRUI::MeasureText(MVPName, NameSize, EWeight::Black).X;
		const float KOWidth = RRUI::MeasureText(KO, KOSize, EWeight::ExtraBold).X;
		const float Inner = Face + 14.f * S + NameWidth + 8.f * S + KOWidth;
		const float Pad = 8.f * S;
		const float Top = ScreenH * 0.6f;
		const FBox2D Box(FVector2D(CenterX - Inner * 0.5f - Pad, Top), FVector2D(CenterX + Inner * 0.5f + Pad + 22.f * S, Top + Face + Pad * 2.f));
		RRUI::DrawPanel(Canvas, Box, Box.GetSize().Y * 0.5f, S, WithAlpha(RRUI::Panel, Fade));
		const FVector2D FaceCenter(Box.Min.X + Pad + Face * 0.5f, Box.GetCenter().Y);
		RRUI::DrawDisc(Canvas, FaceCenter, Face * 0.5f + 5.f * S, WithAlpha(RRUI::Butter, Fade));
		RRUI::DrawDisc(Canvas, FaceCenter, Face * 0.5f + 3.f * S, WithAlpha(RRUI::White, Fade));
		if (UTexture2D* Picture = MVP->GetPortrait() ? MVP->GetPortrait() : Portrait.Get())
		{
			RRUI::DrawTextureRounded(Canvas, Picture, FBox2D(FaceCenter - FVector2D(Face * 0.5f), FaceCenter + FVector2D(Face * 0.5f)), Face * 0.5f, WithAlpha(FLinearColor::White, Fade));
		}
		const float TextX = FaceCenter.X + Face * 0.5f + 14.f * S;
		const float NameBaseline = Box.GetCenter().Y + NameSize * 0.55f;
		RRUI::DrawText(Canvas, TEXT("MVP"), FVector2D(TextX, TopForBaseline(NameBaseline - NameSize * 0.95f, TagSize)), TagSize, EWeight::Black, WithAlpha(RRUI::Gold, Fade), TagSize * 0.3f);
		RRUI::DrawText(Canvas, MVPName, FVector2D(TextX, TopForBaseline(NameBaseline, NameSize)), NameSize, EWeight::Black, WithAlpha(RRUI::Ink, Fade));
		RRUI::DrawText(Canvas, KO, FVector2D(TextX + NameWidth + 8.f * S, TopForBaseline(NameBaseline, KOSize)), KOSize, EWeight::ExtraBold, WithAlpha(RRUI::InkSoft, Fade));
	}

	// Buttons: a new game (the host decides on a network game) and back to the title
	const bool bCanRestart = GetWorld()->GetAuthGameMode() != nullptr;
	ResultsButtons.Reset();
	const float ButtonW = ResultsButtonWidth * S;
	const float ButtonH = ResultsButtonHeight * S;
	const float Left = CenterX - ButtonW - ResultsButtonGap * S * 0.5f;
	const float Top = ScreenH - 60.f * S - ButtonH;
	ResultsButtons.Add({ ERRResultsButton::NewGame, FBox2D(FVector2D(Left, Top), FVector2D(Left + ButtonW, Top + ButtonH)), bCanRestart });
	const float Right = Left + ButtonW + ResultsButtonGap * S;
	ResultsButtons.Add({ ERRResultsButton::Title, FBox2D(FVector2D(Right, Top), FVector2D(Right + ButtonW, Top + ButtonH)), true });
	for (const FResultsButtonRect& Button : ResultsButtons)
	{
		const bool bNewGame = Button.Button == ERRResultsButton::NewGame;
		DrawMenuButton(Button, bNewGame ? (bCanRestart ? TEXT("NEW GAME") : TEXT("WAITING FOR HOST")) : TEXT("TITLE"), bNewGame, Fade, S);
	}

	const float KeySize = 10.f * S;
	DrawCentered(Canvas, bCanRestart ? TEXT("ENTER  NEW GAME   ·   ESC  TITLE") : TEXT("ESC  TITLE"), CenterX, ScreenH - 30.f * S, KeySize, EWeight::ExtraBold,
		WithAlpha(RRUI::InkSoft, Fade), KeySize * 0.16f, true);
}

void ARRRandomHUD::DrawMenuButton(const FResultsButtonRect& Button, const FString& Label, bool bPrimary, float Alpha, float Scale)
{
	float MouseX = -1.f;
	float MouseY = -1.f;
	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	const bool bHovered = Button.bEnabled && Button.Rect.IsInside(FVector2D(MouseX, MouseY));
	const float ButtonAlpha = Alpha * (Button.bEnabled ? 1.f : 0.45f);
	// Lifts a little under the mouse; the main choice is mint on a darker mint edge, the other frosted white
	const FBox2D Rect = Button.Rect.ShiftBy(FVector2D(0.f, bHovered ? -2.f * Scale : 0.f));
	const float Radius = Rect.GetSize().Y * 0.5f;
	if (bPrimary)
	{
		RRUI::DrawSoftShadow(Canvas, Rect, Radius, 10.f * Scale, 4.f * Scale, WithAlpha(RRUI::Shadow, ButtonAlpha));
		RRUI::DrawRounded(Canvas, Rect.ShiftBy(FVector2D(0.f, 4.f * Scale)), Radius, WithAlpha(RRUI::MintDeep, ButtonAlpha));
		RRUI::DrawRounded(Canvas, Rect, Radius, WithAlpha(RRUI::Mint, ButtonAlpha));
	}
	else
	{
		RRUI::DrawPanel(Canvas, Rect, Radius, Scale, WithAlpha(bHovered ? FLinearColor::White : RRUI::Panel, ButtonAlpha));
	}
	const float Size = (Button.bEnabled ? 15.f : 12.f) * Scale;
	const float Tracking = Size * 0.16f;
	const FVector2D Box = RRUI::MeasureText(Label, Size, EWeight::Black, Tracking);
	RRUI::DrawText(Canvas, Label, Rect.GetCenter() - Box * 0.5f, Size, EWeight::Black, WithAlpha(RRUI::Ink, ButtonAlpha), Tracking);
}

void ARRRandomHUD::DrawPauseMenu()
{
	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float S = UIScale;
	const float CenterX = ScreenW * 0.5f;

	// The game behind a light lavender veil, then PAUSED, and RESUME / TITLE pills
	RRUI::DrawBox(Canvas, FBox2D(FVector2D::ZeroVector, FVector2D(ScreenW, ScreenH)), FLinearColor::FromSRGBColor(FColor(244, 240, 252, 150)));
	const float HeadSize = 56.f * S;
	const FString Head = TEXT("PAUSED");
	const FVector2D HeadBox = RRUI::MeasureText(Head, HeadSize, EWeight::Black, HeadSize * 0.14f);
	const FLinearColor Stops[] = { RRUI::LavenderDeep, RRUI::PinkDeep };
	float Y = ScreenH * 0.28f;
	RRUI::DrawGradientText(Canvas, Head, FVector2D(CenterX - HeadBox.X * 0.5f, Y), HeadSize, EWeight::Black, Stops, 1.f, HeadSize * 0.14f);
	Y += HeadBox.Y;
	// An online game can't stop for one player
	if (GetWorld()->GetNetMode() != NM_Standalone)
	{
		const float Size = 12.f * S;
		DrawCentered(Canvas, TEXT("ONLINE GAME KEEPS RUNNING"), CenterX, Y, Size, EWeight::ExtraBold, RRUI::InkSoft, Size * 0.2f);
	}

	const float ButtonW = ResultsButtonWidth * S;
	const float ButtonH = ResultsButtonHeight * S;
	const float Left = CenterX - ButtonW * 0.5f;
	const float Top = ScreenH * 0.5f;
	ResultsButtons.Reset();
	ResultsButtons.Add({ ERRResultsButton::Resume, FBox2D(FVector2D(Left, Top), FVector2D(Left + ButtonW, Top + ButtonH)), true });
	const float Second = Top + ButtonH + 16.f * S;
	ResultsButtons.Add({ ERRResultsButton::Title, FBox2D(FVector2D(Left, Second), FVector2D(Left + ButtonW, Second + ButtonH)), true });
	DrawMenuButton(ResultsButtons[0], TEXT("RESUME"), true, 1.f, S);
	DrawMenuButton(ResultsButtons[1], TEXT("TITLE"), false, 1.f, S);

	const float KeySize = 10.f * S;
	DrawCentered(Canvas, TEXT("ESC  RESUME   ·   ENTER  TITLE"), CenterX, Second + ButtonH + 24.f * S, KeySize, EWeight::ExtraBold, RRUI::InkSoft, KeySize * 0.16f);
}

void ARRRandomHUD::DrawCrosshair(const ARRRandomCharacter& Brawler)
{
	const ARRRandomPlayerController* Controller = Cast<ARRRandomPlayerController>(GetOwningPlayerController());
	if (!Controller || !Controller->IsThirdPerson())
	{
		return;
	}
	// A dot in a dashed ring; the ring closes in the sights and opens briefly after a shot
	const float S = UIScale;
	const FVector2D Middle(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const float Kick = FMath::Clamp(1.f - Brawler.GetTimeSinceShot() / 0.18f, 0.f, 1.f);
	const float Radius = (FMath::Lerp(13.f, 8.f, Brawler.GetSightsBlend()) + 8.f * Kick) * S;
	const float Thickness = FMath::Max(1.5f, 1.6f * S);
	RRUI::DrawDashedRing(Canvas, Middle, Radius, Thickness + 2.f, 8, 0.55f, WithAlpha(RRUI::Ink, 0.45f));
	RRUI::DrawDashedRing(Canvas, Middle, Radius, Thickness, 8, 0.5f, RRUI::White);
	RRUI::DrawDisc(Canvas, Middle, 3.2f * S, WithAlpha(RRUI::Ink, 0.8f));
	RRUI::DrawDisc(Canvas, Middle, 2.f * S, RRUI::White);
}
