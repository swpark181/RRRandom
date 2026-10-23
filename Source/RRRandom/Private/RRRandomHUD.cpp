#include "RRRandomHUD.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomGameState.h"
#include "RRRandomKit.h"
#include "RRRandomMeteor.h"
#include "RRRandomOre.h"
#include "RRRandomOverheadDieComponent.h"
#include "RRRandomPlayerController.h"
#include "RRRandomProjectile.h"
#include "RRRandomSessionSubsystem.h"
#include "RRRandomStarRobot.h"
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

	DrawMeteorCartoons();
	DrawMeteorWarnings();
	DrawStarRobots(Viewer);
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
	UpdateOreFeedback(Viewer);
	DrawOreFeedback(Viewer);
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
	// Hidden in the fog of war on this screen (ARRRandomFogOfWar), or inside the star robot: nothing over it either
	if (Brawler.GetMesh()->bHiddenInGame || Brawler.IsHidden())
	{
		return;
	}
	DrawDamagePopups(Brawler, Viewer);
	// An invisible opponent (the fox's own die) has nothing over it but the numbers of hits on it
	if (!Brawler.IsAlive() || Brawler.GetKit()->IsCloakedOnThisScreen())
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
	// The powers of its own die (FRRKit)
	const URRRandomKitComponent& Kit = *Brawler.GetKit();
	const FLinearColor OwnColor = RRKit::Get(Brawler.GetSpecies()).DieColor;
	if (Kit.IsCloaked())
	{
		PowerLabels.Add(TEXT("INVISIBLE"));
		PowerColors.Add(OwnColor);
		PowerRemaining.Add(Kit.GetCloakRemaining());
	}
	if (Kit.GetSuperHops() > 0)
	{
		PowerLabels.Add(FString::Printf(TEXT("HOP x%d"), Kit.GetSuperHops()));
		PowerColors.Add(OwnColor);
		PowerRemaining.Add(UE_BIG_NUMBER);
	}
	if (Kit.GetShield() > 0.f)
	{
		PowerLabels.Add(FString::Printf(TEXT("SHIELD %d"), FMath::CeilToInt(Kit.GetShield())));
		PowerColors.Add(OwnColor);
		PowerRemaining.Add(Kit.GetShieldRemaining());
	}
	if (Brawler.IsInvincible())
	{
		// The cheat: never runs out, so it never blinks
		PowerLabels.Add(TEXT("GOD"));
		PowerColors.Add(RRUI::Butter);
		PowerRemaining.Add(UE_BIG_NUMBER);
	}
	// Meteor Rush: the ore carried (gold with a crystal)
	if (const URRRandomOreCarryComponent* Carry = Brawler.GetOreCarry(); Carry && Carry->GetPoints() > 0)
	{
		PowerLabels.Add(FString::Printf(TEXT("ORE %d"), Carry->GetPoints()));
		PowerColors.Add(Carry->HasGolden() ? RRUI::Butter : Carry->HasCrystal() ? RRUI::Gold : RRUI::MintDeep);
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
	// Under the brawler's feet: what F does right now (the star robot's own sign says it when F is for the robot)
	const ARRRandomStarRobot* Robot = ARRRandomStarRobot::Get(this, Brawler.GetTeam());
	if (Brawler.GetRobot() || ARRRandomStarRobot::CanStartCombine(&Brawler) || (Robot && !Brawler.GetVehicle() && Robot->CanBoard(&Brawler)))
	{
		return;
	}
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
	const int32 RollerSpecies = Roller ? Roller->GetSpecies() : RRSpecies::Tiger;
	const FString UltimateName = RRSpecies::GetUltimateName(RollerSpecies);
	const FRRKit& RollerKit = RRKit::Get(RollerSpecies);
	const FString Text = FString::Printf(TEXT("%d %s"), Die.Face, Die.bUltimate ? *UltimateName : Die.bWeapon ? TEXT("GUN") : Die.bGiant ? TEXT("GIANT") : Die.bFlight ? TEXT("FLY")
		: Die.bOwn ? RollerKit.DieWord : *URRRandomDiceBuffComponent::GetStatShortName(Die.Stat));
	const float Size = 13.f * UIScale;
	const FVector2D TextSize = RRUI::MeasureText(Text, Size, EWeight::Black, Size * 0.04f);
	const float Y = Bottom - TextSize.Y;
	if (Landed >= 0.f)
	{
		const FLinearColor DieColor = Die.bUltimate ? URRRandomDiceBuffComponent::GetUltimateColor() : Die.bWeapon ? WeaponDieColor
			: Die.bGiant ? URRRandomDiceBuffComponent::GetGiantColor()
			: Die.bFlight ? URRRandomDiceBuffComponent::GetFlightColor() : Die.bOwn ? RollerKit.DieColor : URRRandomDiceBuffComponent::GetStatColor(Die.Stat);
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

	for (const FRRDamagePopup& Popup : Brawler.GetDamagePopups())
	{
		// A fox's backstab: bigger, gold, popping in with a bounce
		const float Pop = Popup.bBackstab ? 1.f + 0.35f * FMath::Max(0.f, 1.f - Popup.Age / 0.15f) : 1.f;
		const float Size = (Popup.bBackstab ? 25.f : 18.f) * S * Pop;
		const FString Text = FString::FromInt(FMath::RoundToInt(Popup.Amount)) + (Popup.bBackstab ? TEXT("!") : TEXT(""));
		const FVector2D TextSize = RRUI::MeasureText(Text, Size, EWeight::Black);
		const float Alpha = FMath::Max(0.f, 1.f - Popup.Age / ARRRandomCharacter::DamagePopupLifetime);
		const FVector2D Pos(Body.X + (50.f + Popup.OffsetX) * S - TextSize.X * 0.5f, Body.Y - Popup.Age * DamagePopupRiseSpeed * S);
		// White numbers with an ink edge on opponents; pink with a white edge when it's our side
		const FLinearColor Fill = Popup.bBackstab ? RRUI::Gold : bFriendly ? RRUI::PinkDeep : RRUI::White;
		const FLinearColor Edge = Popup.bBackstab ? RRUI::White : bFriendly ? RRUI::White : RRUI::Ink;
		DrawEdgedText(Canvas, Text, Pos, Size, WithAlpha(Fill, Alpha), WithAlpha(Edge, 0.8f * Alpha));
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

	// Meteor Rush: the ore I carry, right over the dice
	float PanelTop = TopForBaseline(Baseline, CountSize);
	if (ARRRandomGameState::IsMeteorRush(this))
	{
		PanelTop = DrawOrePanel(Viewer, Left, PanelTop - 8.f * S);
	}

	// Powers (the ultimate, flight, giant), then active buffs, stacked upward as tags
	const float TagSize = 12.f * S;
	float LineY = PanelTop - TagLine * S;
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
	// The powers of my own die (FRRKit)
	const URRRandomKitComponent& Kit = *Viewer.GetKit();
	const FLinearColor OwnMark = RRUI::Pastel(RRKit::Get(Viewer.GetSpecies()).DieColor, 0.2f);
	if (Kit.IsCloaked())
	{
		Line(TEXT("INVISIBLE"), FString::Printf(TEXT("SHOOTING ENDS IT · FIRST SHOT x%.2f · %.0fs"), RRKit::BackstabMultiplier, FMath::CeilToFloat(Kit.GetCloakRemaining())),
			OwnMark, Kit.GetCloakRemaining() < BuffBlinkTime ? Blink(GetWorld()) : 1.f);
	}
	if (Kit.GetSuperHops() > 0)
	{
		Line(FString::Printf(TEXT("SUPER HOP x%d"), Kit.GetSuperHops()), TEXT("SPACE IN THE AIR"), OwnMark, 1.f);
	}
	if (Kit.GetShield() > 0.f)
	{
		Line(FString::Printf(TEXT("SHIELD %d"), FMath::CeilToInt(Kit.GetShield())), FString::Printf(TEXT("%.0fs"), FMath::CeilToFloat(Kit.GetShieldRemaining())),
			OwnMark, Kit.GetShieldRemaining() < BuffBlinkTime ? Blink(GetWorld()) : 1.f);
	}
	if (Kit.GetMagnetRemaining() > 0.f)
	{
		Line(TEXT("ORE MAGNET"), FString::Printf(TEXT("%.0fs"), FMath::CeilToFloat(Kit.GetMagnetRemaining())), OwnMark, Kit.GetMagnetRemaining() < BuffBlinkTime ? Blink(GetWorld()) : 1.f);
	}
	for (const FRRDiceBuff& Buff : Dice.GetActiveBuffs())
	{
		Line(FString::Printf(TEXT("%s +%d%%"), *URRRandomDiceBuffComponent::GetStatName(Buff.Stat).ToString().ToUpper(), FMath::RoundToInt(Buff.Bonus * 100.f)),
			FString::Printf(TEXT("%.0fs"), FMath::CeilToFloat(Buff.RemainingTime)),
			RRUI::Pastel(URRRandomDiceBuffComponent::GetStatColor(Buff.Stat), 0.25f), Buff.RemainingTime < BuffBlinkTime ? Blink(GetWorld()) : 1.f);
	}
}

namespace
{
	// The ore panel's team bar, and how long a gain takes to pop over my head and then fly into the panel
	constexpr float OreBarWidth = 230.f;
	constexpr float OreGainPop = 0.4f;
	constexpr float OreGainFly = 0.45f;
	constexpr float OreDroppedTime = 2.6f;
	// Below this share of health, carrying ore, the panel warns that it could all be dropped
	constexpr float OreRiskHealthShare = 0.35f;

	/** A little cut gem (a diamond) with a white rim, for the ore panel. */
	void DrawGem(UCanvas* Canvas, const FVector2D& Center, float Half, const FLinearColor& Color)
	{
		const float Rim = FMath::Max(1.f, Half * 0.28f);
		const FVector2D Up(0.f, -Half * 1.15f), Down(0.f, Half * 1.15f), Side(Half, 0.f);
		const FVector2D RimUp(0.f, -Rim * 1.2f), RimDown(0.f, Rim * 1.2f), RimSide(Rim, 0.f);
		RRUI::DrawTriangle(Canvas, Center + Up + RimUp, Center + Side + RimSide, Center - Side - RimSide, RRUI::WithAlpha(RRUI::White, Color.A));
		RRUI::DrawTriangle(Canvas, Center + Down + RimDown, Center + Side + RimSide, Center - Side - RimSide, RRUI::WithAlpha(RRUI::White, Color.A));
		RRUI::DrawTriangle(Canvas, Center + Up, Center + Side, Center - Side, Color);
		RRUI::DrawTriangle(Canvas, Center + Down, Center + Side, Center - Side, Color);
		// A glint on the upper left face
		RRUI::DrawTriangle(Canvas, Center + Up * 0.8f, Center - Side * 0.15f, Center - Side * 0.65f, RRUI::WithAlpha(RRUI::White, 0.55f * Color.A));
	}

	FLinearColor GainColor(int32 Amount)
	{
		return Amount >= 6 ? RRUI::Gold : Amount >= 3 ? RRUI::PinkDeep : RRUI::MintDeep;
	}
}

float ARRRandomHUD::DrawOrePanel(const ARRRandomCharacter& Viewer, float Left, float Bottom)
{
	const URRRandomOreCarryComponent* Carry = Viewer.GetOreCarry();
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (!Carry || !State)
	{
		return Bottom;
	}
	const float S = UIScale;
	const float Now = GetWorld()->GetRealTimeSeconds();
	const int32 Points = Carry->GetPoints();
	const int32 Goal = FMath::Max(1, State->GetScoreToWin());
	const int32 Team = Viewer.GetTeam();
	const int32 TeamPoints = State->GetTeamScore(Team);

	// Bottom up: the team bar with TEAM n / goal after it, then the row with the count, ORE, and a gem and a count for each
	// kind carried; the plate as wide as the longer of the two
	const float BarW = OreBarWidth * S;
	const FBox2D Bar(FVector2D(Left, Bottom - 7.f * S), FVector2D(Left + BarW, Bottom - 1.f * S));
	const float Baseline = Bar.Min.Y - 10.f * S;
	const float CountSize = 30.f * S;
	const float Top = Baseline - CountSize * 0.95f;
	const FString TeamText = FString::Printf(TEXT("TEAM %d / %d"), TeamPoints, Goal);
	const float TeamSize = 9.f * S;
	const FVector2D TeamBox = RRUI::MeasureText(TeamText, TeamSize, EWeight::ExtraBold, TeamSize * 0.15f);
	const bool bAtRisk = Points > 0 && Viewer.IsAlive() && Viewer.GetHealth() < Viewer.GetMaxHealth() * OreRiskHealthShare;
	const float RowWidth = 230.f * S + (bAtRisk ? 62.f * S : 0.f) + (Carry->HasGolden() ? 34.f * S : 0.f);
	const FBox2D Panel(FVector2D(Left - 4.f * S, Top), FVector2D(Left + FMath::Max(RowWidth, BarW + 8.f * S + TeamBox.X) + 6.f * S, Bottom + 2.f * S));

	// Hurt with ore on me: a pink glow round the panel, blinking
	if (bAtRisk)
	{
		RRUI::DrawRounded(Canvas, Panel.ExpandBy(10.f * S), 22.f * S, WithAlpha(RRUI::PinkDeep, 0.45f * Blink(GetWorld())));
	}
	RRUI::DrawPlate(Canvas, Panel, 20.f * S);

	// The count: bouncing big when it has just grown (a gain landing in it)
	const float SinceBounce = Now - OreBounceAt;
	const float Bounce = SinceBounce >= 0.f && SinceBounce < 0.35f ? FMath::Sin(SinceBounce / 0.35f * PI) * FMath::Exp(-SinceBounce * 4.f) : 0.f;
	const float Size = CountSize * (1.f + 0.45f * Bounce);
	const FString Count = FString::FromInt(Points);
	const FLinearColor CountColor = bAtRisk ? RRUI::PinkDeep : Carry->HasGolden() ? RRUI::Gold : Carry->HasCrystal() ? RRUI::Gold : Points > 0 ? RRUI::Ink : RRUI::InkSoft;
	float X = Left;
	const FVector2D CountBox = RRUI::DrawText(Canvas, Count, FVector2D(X, TopForBaseline(Baseline, Size)), Size, EWeight::Black, CountColor);
	OreCountAt = FVector2D(X + CountBox.X * 0.5f, Baseline - CountSize * 0.4f);
	X += FMath::Max(CountBox.X, RRUI::MeasureText(Count, CountSize, EWeight::Black).X) + 4.f * S;
	const float LabelSize = 10.f * S;
	X += RRUI::DrawText(Canvas, TEXT("ORE"), FVector2D(X, TopForBaseline(Baseline, LabelSize)), LabelSize, EWeight::ExtraBold, RRUI::InkSoft, LabelSize * 0.2f).X;

	// What I carry, kind by kind: a gem in its color and xN (dim when none)
	X += 12.f * S;
	struct FKind
	{
		int32 Count;
		ERROreKind Kind;
	};
	const FKind Kinds[] = { { Carry->GetShards(), ERROreKind::Shard }, { Carry->GetOres(), ERROreKind::Ore }, { Carry->GetCrystals(), ERROreKind::Big }, { Carry->HasGolden() ? 1 : 0, ERROreKind::Golden } };
	const float KindSize = 12.f * S;
	for (const FKind& Kind : Kinds)
	{
		if (Kind.Kind == ERROreKind::Golden && Kind.Count == 0)
		{
			continue;
		}
		const float Alpha = Kind.Count > 0 ? 1.f : 0.3f;
		const float Half = (Kind.Kind == ERROreKind::Shard ? 4.5f : Kind.Kind == ERROreKind::Ore ? 5.5f : 6.5f) * S;
		DrawGem(Canvas, FVector2D(X + Half, Baseline - 7.f * S), Half, WithAlpha(RROre::GetColor(Kind.Kind), Alpha));
		X += Half * 2.f + 3.f * S;
		X += RRUI::DrawText(Canvas, FString::Printf(TEXT("x%d"), Kind.Count), FVector2D(X, TopForBaseline(Baseline, KindSize)), KindSize, EWeight::Black, WithAlpha(RRUI::Ink, Alpha)).X + 9.f * S;
	}
	if (bAtRisk)
	{
		const float RiskSize = 10.f * S;
		RRUI::DrawText(Canvas, TEXT("AT RISK!"), FVector2D(X + 2.f * S, TopForBaseline(Baseline, RiskSize)), RiskSize, EWeight::Black, WithAlpha(RRUI::PinkDeep, Blink(GetWorld())), RiskSize * 0.15f);
	}

	// The team's ore toward the goal (its color, lighter), my share of it at the start (deeper), a tick at the goal; the
	// numbers over its right end
	RRUI::DrawPill(Canvas, Bar, WithAlpha(RRUI::Ink, 0.12f));
	const float TeamShare = FMath::Clamp(float(TeamPoints) / Goal, 0.f, 1.f);
	const float MyShare = FMath::Clamp(float(FMath::Min(Points, TeamPoints)) / Goal, 0.f, 1.f);
	if (TeamShare > 0.f)
	{
		RRUI::DrawPill(Canvas, FBox2D(Bar.Min, FVector2D(Bar.Min.X + FMath::Max(Bar.GetSize().Y, BarW * TeamShare), Bar.Max.Y)), RRUI::TeamColor(Team));
	}
	if (MyShare > 0.f)
	{
		RRUI::DrawPill(Canvas, FBox2D(Bar.Min, FVector2D(Bar.Min.X + FMath::Max(Bar.GetSize().Y, BarW * MyShare), Bar.Max.Y)), RRUI::TeamColorDeep(Team));
	}
	RRUI::DrawText(Canvas, TeamText, FVector2D(Bar.Max.X + 8.f * S, Bar.GetCenter().Y - TeamBox.Y * 0.55f), TeamSize, EWeight::ExtraBold, RRUI::InkSoft, TeamSize * 0.15f);
	return Panel.Min.Y;
}

void ARRRandomHUD::UpdateOreFeedback(const ARRRandomCharacter* Viewer)
{
	const URRRandomOreCarryComponent* Carry = Viewer ? Viewer->GetOreCarry() : nullptr;
	if (!Carry || !ARRRandomGameState::IsMeteorRush(this))
	{
		OreGains.Reset();
		LastOrePoints = 0;
		OreViewer = nullptr;
		return;
	}
	// A new brawler to watch (a respawned pawn is the same one): start from what it carries
	if (OreViewer.Get() != Viewer)
	{
		OreViewer = Viewer;
		LastOrePoints = Carry->GetPoints();
		return;
	}
	const int32 Points = Carry->GetPoints();
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (Points > LastOrePoints)
	{
		// Picked up: pops over my head
		FOreGain& Gain = OreGains.AddDefaulted_GetRef();
		Gain.Amount = Points - LastOrePoints;
		Gain.Color = GainColor(Gain.Amount);
		Gain.Start = Now;
		const FVector Head = Project(Viewer->GetActorLocation() + FVector(0.f, 0.f, Viewer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 60.f));
		Gain.From = Head.Z > 0.f ? FVector2D(Head.X, Head.Y) : FVector2D(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.45f);
		// Two gains close together sit side by side
		Gain.From.X += (OreGains.Num() - 1) % 3 * 34.f * UIScale;
	}
	else if (Points < LastOrePoints)
	{
		// Knocked out (or fallen) with ore on me; the squirrel's cheek pouch keeps some
		DroppedOre = LastOrePoints - Points;
		KeptOre = Points;
		DroppedAt = Now;
		OreGains.Reset();
	}
	LastOrePoints = Points;
}

void ARRRandomHUD::DrawOreFeedback(const ARRRandomCharacter* Viewer)
{
	if (!ARRRandomGameState::IsMeteorRush(this))
	{
		return;
	}
	const float S = UIScale;
	const float Now = GetWorld()->GetRealTimeSeconds();
	// Gains: "+3" popping up over my head, then shooting down into the panel's count, which bounces as it lands
	for (FOreGain& Gain : OreGains)
	{
		const float Age = Now - Gain.Start;
		FVector2D At;
		float Scale;
		if (Age < OreGainPop)
		{
			const float T = Age / OreGainPop;
			Scale = T < 0.3f ? FMath::Lerp(0.3f, 1.35f, T / 0.3f) : FMath::Lerp(1.35f, 1.f, FMath::Min(1.f, (T - 0.3f) / 0.4f));
			At = Gain.From - FVector2D(0.f, 40.f * S * FMath::Sin(T * PI * 0.5f));
		}
		else
		{
			const float T = FMath::Clamp((Age - OreGainPop) / OreGainFly, 0.f, 1.f);
			const FVector2D Lifted = Gain.From - FVector2D(0.f, 40.f * S);
			// Easing in, along a slight arc
			const float Ease = T * T;
			At = FMath::Lerp(Lifted, OreCountAt, Ease) - FVector2D(0.f, 60.f * S * FMath::Sin(T * PI));
			Scale = FMath::Lerp(1.f, 0.55f, Ease);
			if (T >= 1.f && !Gain.bArrived)
			{
				Gain.bArrived = true;
				OreBounceAt = Now;
			}
		}
		if (Gain.bArrived)
		{
			continue;
		}
		const FString Text = FString::Printf(TEXT("+%d"), Gain.Amount);
		const float Size = (Gain.Amount >= 6 ? 34.f : 28.f) * S * Scale;
		const FVector2D Box = RRUI::MeasureText(Text, Size, EWeight::Black);
		DrawGem(Canvas, At - FVector2D(Box.X * 0.5f + 9.f * S * Scale, 0.f), 7.f * S * Scale, Gain.Color);
		DrawEdgedText(Canvas, Text, At - FVector2D(Box.X * 0.5f - 6.f * S * Scale, Box.Y * 0.5f), Size, Gain.Color, RRUI::White);
	}
	OreGains.RemoveAll([](const FOreGain& Gain) { return Gain.bArrived; });

	// Dropped: under the knockout count, pink, shaking as it comes in
	const float SinceDrop = Now - DroppedAt;
	if (DroppedOre > 0 && SinceDrop >= 0.f && SinceDrop < OreDroppedTime)
	{
		const float Alpha = FMath::Clamp((OreDroppedTime - SinceDrop) / 0.4f, 0.f, 1.f);
		const float Shake = SinceDrop < 0.4f ? FMath::Sin(SinceDrop * 60.f) * 6.f * S * (1.f - SinceDrop / 0.4f) : 0.f;
		const float Zoom = SinceDrop < 0.15f ? FMath::Lerp(1.6f, 1.f, SinceDrop / 0.15f) : 1.f;
		const FString Text = FString::Printf(TEXT("-%d ORE DROPPED"), DroppedOre);
		const float Size = 24.f * S * Zoom;
		const FVector2D Box = RRUI::MeasureText(Text, Size, EWeight::Black, Size * 0.06f);
		const float Y = Canvas->ClipY * 0.28f + 96.f * S;
		RRUI::DrawGlowText(Canvas, Text, FVector2D((Canvas->ClipX - Box.X) * 0.5f + Shake, Y), Size, EWeight::Black, WithAlpha(RRUI::PinkDeep, Alpha), Size * 0.06f);
		const FString Hint = KeptOre > 0 ? FString::Printf(TEXT("CHEEK POUCH KEPT %d · GRAB THE REST BACK!"), KeptOre) : FString(TEXT("GRAB IT BACK!"));
		const float HintSize = 11.f * S;
		const FVector2D HintBox = RRUI::MeasureText(Hint, HintSize, EWeight::ExtraBold, HintSize * 0.2f);
		RRUI::DrawGlowText(Canvas, Hint, FVector2D((Canvas->ClipX - HintBox.X) * 0.5f, Y + Box.Y + 4.f * S), HintSize, EWeight::ExtraBold, WithAlpha(RRUI::InkSoft, Alpha), HintSize * 0.2f);
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
	RRUI::DrawGlowText(Canvas, TEXT("CHEATS · CTRL+0 STAR CARS · CTRL+8 CLASH TEST · F6 WIN · F7 GIANT · F8 FLY · F9 3RD PERSON · F10 GOD · U ULTIMATE DIE"), FVector2D(Left, Y), Size, EWeight::ExtraBold, RRUI::InkSoft, Tracking);
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

		// Meteor Rush: the points to hold and the clock (overtime: the golden crystal)
		FString Goal = FString::Printf(TEXT("FIRST TO %d"), State->GetScoreToWin());
		if (State->IsMeteorRush())
		{
			const int32 Seconds = FMath::Max(0, FMath::CeilToInt(State->HasPlayStarted() ? State->GetMatchTimeRemaining() : State->GetMatchTimeRemaining() - State->GetCountdownRemaining()));
			Goal = State->IsOvertime() ? FString(TEXT("OVERTIME · GOLDEN CRYSTAL"))
				: FString::Printf(TEXT("HOLD %d · %d:%02d"), State->GetScoreToWin(), Seconds / 60, Seconds % 60);
		}
		const float GoalWidth = RRUI::MeasureText(Goal, GoalSize, EWeight::ExtraBold, GoalSize * 0.22f).X;
		RRUI::DrawText(Canvas, Goal, FVector2D(CenterX - GoalWidth * 0.5f, Line.Max.Y + 3.f * S), GoalSize, EWeight::ExtraBold, State->IsOvertime() ? RRUI::Gold : RRUI::InkSoft, GoalSize * 0.22f);

		// A meteor on its way: a pill under the score with the seconds to go, its edge blinking
		if (State->IsMeteorRush())
		{
			for (TActorIterator<ARRRandomMeteor> It(GetWorld()); It; ++It)
			{
				const float Left = It->GetTimeToImpact();
				if (Left > 0.f)
				{
					const FString Warning = FString::Printf(TEXT("%s INCOMING  %d"), It->IsGolden() ? TEXT("GOLDEN METEOR") : TEXT("METEOR"), FMath::CeilToInt(Left));
					const float WarnSize = 15.f * S;
					const FVector2D WarnBox = RRUI::MeasureText(Warning, WarnSize, EWeight::Black, WarnSize * 0.16f);
					const FLinearColor Color = It->IsGolden() ? RRUI::Gold : RRUI::PinkDeep;
					const FBox2D Pill(FVector2D(CenterX - WarnBox.X * 0.5f - 16.f * S, GroupBottom + 8.f * S), FVector2D(CenterX + WarnBox.X * 0.5f + 16.f * S, GroupBottom + 14.f * S + WarnBox.Y));
					RRUI::DrawPill(Canvas, Pill.ExpandBy(2.5f * S), WithAlpha(Color, Blink(GetWorld())));
					RRUI::DrawPill(Canvas, Pill, RRUI::White);
					RRUI::DrawText(Canvas, Warning, FVector2D(CenterX - WarnBox.X * 0.5f, Pill.Min.Y + 3.f * S), WarnSize, EWeight::Black, Color, WarnSize * 0.16f);
					break;
				}
			}
		}
	}
}

namespace
{
	// Comic words stay up this long (as the meteor keeps them)
	constexpr float ComicWordLife = 0.9f;

	/** A spiky comic burst: Points spikes, the dips InnerShare of the way in, turned by Tilt degrees. */
	void DrawStarburst(UCanvas* Canvas, const FVector2D& Center, FVector2D Radius, int32 Points, float InnerShare, float Tilt, const FLinearColor& Color)
	{
		const int32 Corners = Points * 2;
		auto Corner = [&](int32 Index)
		{
			const float Angle = FMath::DegreesToRadians(Tilt) + Index * PI / Points;
			const float Reach = Index % 2 == 0 ? 1.f : InnerShare;
			return Center + FVector2D(FMath::Cos(Angle) * Radius.X, FMath::Sin(Angle) * Radius.Y) * Reach;
		};
		for (int32 Index = 0; Index < Corners; ++Index)
		{
			RRUI::DrawTriangle(Canvas, Center, Corner(Index), Corner((Index + 1) % Corners), Color);
		}
	}

	/** A cartoon star (five points), with an ink edge Edge wide. */
	void DrawCartoonStar(UCanvas* Canvas, const FVector2D& Center, float Radius, float Turn, const FLinearColor& Color, float Edge)
	{
		DrawStarburst(Canvas, Center, FVector2D(Radius + Edge * 1.8f), 5, 0.46f, Turn - 90.f, RRUI::WithAlpha(RRUI::Ink, Color.A));
		DrawStarburst(Canvas, Center, FVector2D(Radius), 5, 0.46f, Turn - 90.f, Color);
	}
}

void ARRRandomHUD::DrawMeteorCartoons()
{
	if (!ARRRandomGameState::IsMeteorRush(this) || !PlayerOwner || !PlayerOwner->PlayerCameraManager)
	{
		return;
	}
	const float S = UIScale;
	const float Now = GetWorld()->GetTimeSeconds();
	const FVector CameraRight = PlayerOwner->PlayerCameraManager->GetCameraRotation().Quaternion().GetRightVector();
	for (TActorIterator<ARRRandomMeteor> It(GetWorld()); It; ++It)
	{
		const ARRRandomMeteor* Meteor = *It;
		// How big the meteor is on the screen (its half width in pixels), from its top
		const FVector Top = Meteor->GetTopLocation();
		const FVector TopOnScreen = Project(Top);
		const FVector SideOnScreen = Project(Top + CameraRight * ARRRandomMeteor::GetHeight() * 0.5f);
		const float Reach = TopOnScreen.Z > 0.f ? FVector2D::Distance(FVector2D(TopOnScreen), FVector2D(SideOnScreen)) : 0.f;

		// Spinning: three speed lines whirling round its middle
		if (Meteor->IsSpinning() && Reach > 4.f && Meteor->GetSpinSpeedShare() > 0.15f)
		{
			const FVector Middle = Project(FMath::Lerp(Meteor->GetActorLocation(), Top, 0.45f));
			const float Alpha = FMath::Clamp(Meteor->GetSpinSpeedShare(), 0.f, 1.f);
			// Each a tapering stroke of triangles along an ellipse round it: the ink edge first, then the white
			for (int32 Pass = 0; Pass < 2; ++Pass)
			{
				for (int32 Line = 0; Line < 3; ++Line)
				{
					const float From = Now * 13.f * Alpha + Line * 2.f * PI / 3.f;
					constexpr int32 Steps = 10;
					auto At = [&](int32 Step, float Out)
					{
						const float Angle = From + Step * 0.11f;
						const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
						// Thick in the middle of the stroke, thin at its ends
						const float Half = ((Pass == 0 ? 1.5f : 0.f) + 3.f * FMath::Sin(Step / float(Steps) * PI)) * S;
						return FVector2D(Middle.X + Dir.X * (Reach * 1.35f + Out * Half), Middle.Y + Dir.Y * (Reach * 0.5f + Out * Half * 0.6f));
					};
					const FLinearColor LineColor = Pass == 0 ? RRUI::WithAlpha(RRUI::Ink, 0.6f * Alpha) : RRUI::WithAlpha(RRUI::White, Alpha);
					for (int32 Step = 0; Step < Steps; ++Step)
					{
						RRUI::DrawTriangle(Canvas, At(Step, 1.f), At(Step, -1.f), At(Step + 1, 1.f), LineColor);
						RRUI::DrawTriangle(Canvas, At(Step, -1.f), At(Step + 1, -1.f), At(Step + 1, 1.f), LineColor);
					}
				}
			}
		}

		// Dizzy: little stars circling over its head
		if (Meteor->GetDizzyAlpha() > 0.f && TopOnScreen.Z > 0.f)
		{
			const FVector Crown = Project(Top + FVector(0.f, 0.f, 45.f));
			const float Alpha = Meteor->GetDizzyAlpha();
			const FLinearColor StarColors[] = { RRUI::Butter, RRUI::Pink, RRUI::Mint };
			for (int32 Star = 0; Star < 3; ++Star)
			{
				const float Angle = Now * 3.6f + Star * 2.f * PI / 3.f;
				// Further back round the circle: smaller
				const float Near = 0.75f + 0.25f * FMath::Sin(Angle);
				const FVector2D At(Crown.X + FMath::Cos(Angle) * FMath::Max(Reach * 0.75f, 26.f * S), Crown.Y + FMath::Sin(Angle) * FMath::Max(Reach * 0.22f, 8.f * S));
				DrawCartoonStar(Canvas, At, 15.f * S * Near, Now * 140.f + Star * 50.f, RRUI::WithAlpha(StarColors[Star], Alpha), 1.6f * S);
			}
		}

		// Comic words: popping in big, settling, drifting up and fading, each in a white starburst with an ink edge
		for (const FRRComicPop& Pop : Meteor->GetComicPops())
		{
			const float Age = Now - Pop.Start;
			const FVector OnScreen = Project(Pop.Where);
			if (Age < 0.f || Age > ComicWordLife || OnScreen.Z <= 0.f)
			{
				continue;
			}
			const float Scale = Age < 0.12f ? FMath::Lerp(0.25f, 1.25f, Age / 0.12f) : Age < 0.24f ? FMath::Lerp(1.25f, 1.f, (Age - 0.12f) / 0.12f) : 1.f;
			const float Alpha = 1.f - FMath::Clamp((Age - (ComicWordLife - 0.25f)) / 0.25f, 0.f, 1.f);
			const FVector2D Center(OnScreen.X, OnScreen.Y - Age * 45.f * S);
			const float Size = 22.f * S * Pop.Size * Scale;
			const FVector2D Box = RRUI::MeasureText(Pop.Text, Size, EWeight::Black);
			const FVector2D Burst(Box.X * 0.78f + 14.f * S * Scale, Box.Y * 0.95f + 12.f * S * Scale);
			DrawStarburst(Canvas, Center, Burst + FVector2D(3.f * S), 11, 0.74f, Pop.Tilt, RRUI::WithAlpha(RRUI::Ink, Alpha));
			DrawStarburst(Canvas, Center, Burst, 11, 0.74f, Pop.Tilt, RRUI::WithAlpha(RRUI::White, Alpha));
			DrawEdgedText(Canvas, Pop.Text, Center - Box * 0.5f, Size, RRUI::WithAlpha(Pop.Color, Alpha), RRUI::WithAlpha(RRUI::Ink, Alpha));
		}
	}
}

void ARRRandomHUD::DrawMeteorWarnings()
{
	if (!ARRRandomGameState::IsMeteorRush(this))
	{
		return;
	}
	const float S = UIScale;
	// Markers stay this far inside the screen's edges (clear of the score and its warning at the top, the panels below)
	const FVector2D InsetMin(70.f * S, 150.f * S);
	const FVector2D InsetMax(70.f * S, 110.f * S);
	for (TActorIterator<ARRRandomMeteor> It(GetWorld()); It; ++It)
	{
		const float Left = It->GetTimeToImpact();
		const bool bIncoming = Left > 0.f;
		if (!bIncoming && !It->IsBursting())
		{
			continue;
		}
		const FLinearColor Color = It->IsGolden() ? RRUI::Gold : RRUI::PinkDeep;
		// Over the spot, up where the falling meteor will be seen
		const FVector Projected = Project(It->GetActorLocation() + FVector(0.f, 0.f, 380.f));
		FVector2D Point(Projected.X, Projected.Y);
		const bool bBehind = Projected.Z <= 0.f;
		const FBox2D Allowed(InsetMin, FVector2D(Canvas->ClipX, Canvas->ClipY) - InsetMax);
		const bool bOnScreen = !bBehind && Allowed.IsInside(Point);
		FVector2D Out(0.f, 1.f);
		if (!bOnScreen)
		{
			// Off the screen (or under the score): slid along the way from the middle of the allowed area to its edge (turned
			// round when behind the camera)
			const FVector2D Middle = Allowed.GetCenter();
			Out = (Point - Middle) * (bBehind ? -1.f : 1.f);
			Out = Out.IsNearlyZero() ? FVector2D(0.f, 1.f) : Out.GetSafeNormal();
			const FVector2D Half = Allowed.GetExtent();
			const float Reach = FMath::Min(Out.X != 0.f ? Half.X / FMath::Abs(Out.X) : UE_BIG_NUMBER, Out.Y != 0.f ? Half.Y / FMath::Abs(Out.Y) : UE_BIG_NUMBER);
			Point = Middle + Out * Reach;
		}
		const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * (bIncoming ? 6.f + 10.f * (1.f - Left / ARRRandomMeteor::WarnTime) : 12.f));
		const float Radius = (20.f + 3.f * Pulse) * S;
		// A pointer: down at the spot on the screen, out toward it at the edge
		const FVector2D Tip = Point + Out * (Radius + 12.f * S);
		const FVector2D Side(-Out.Y, Out.X);
		RRUI::DrawTriangle(Canvas, Tip + Out * 2.f * S, Point + Out * Radius * 0.6f + Side * (Radius * 0.62f + 2.f * S), Point + Out * Radius * 0.6f - Side * (Radius * 0.62f + 2.f * S), RRUI::White);
		RRUI::DrawTriangle(Canvas, Tip, Point + Out * Radius * 0.6f + Side * Radius * 0.55f, Point + Out * Radius * 0.6f - Side * Radius * 0.55f, Color);
		RRUI::DrawRing(Canvas, Point, Radius + 6.f * S + 6.f * S * Pulse, 2.f * S, WithAlpha(Color, 0.6f * (1.f - Pulse)));
		RRUI::DrawDisc(Canvas, Point, Radius + 2.5f * S, RRUI::White);
		RRUI::DrawDisc(Canvas, Point, Radius, Color);
		const FString Text = bIncoming ? FString::FromInt(FMath::CeilToInt(Left)) : FString(TEXT("ORE!"));
		const float Size = (bIncoming ? 20.f : 11.f) * S;
		const FVector2D Box = RRUI::MeasureText(Text, Size, EWeight::Black);
		RRUI::DrawText(Canvas, Text, Point - Box * 0.5f, Size, EWeight::Black, RRUI::White);
		// The label under (or beside, at the edge) the marker
		const FString Label = bIncoming ? (It->IsGolden() ? TEXT("GOLDEN METEOR") : TEXT("METEOR")) : TEXT("GRAB THE ORE");
		const float LabelSize = 10.f * S;
		const FVector2D LabelBox = RRUI::MeasureText(Label, LabelSize, EWeight::Black, LabelSize * 0.16f);
		RRUI::DrawGlowText(Canvas, Label, FVector2D(Point.X - LabelBox.X * 0.5f, Point.Y - Radius - 6.f * S - LabelBox.Y), LabelSize, EWeight::Black, Color, LabelSize * 0.16f);
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

	int32 GetOreCollected(const ARRRandomCharacter* Brawler)
	{
		return Brawler->GetOreCarry() ? Brawler->GetOreCarry()->GetCollected() : 0;
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
		// (Meteor Rush: by the ore picked up)
		const bool bRush = ARRRandomGameState::IsMeteorRush(World);
		Board.StableSort([bRush](const TPair<const ARRRandomCharacter*, FString>& A, const TPair<const ARRRandomCharacter*, FString>& B)
		{
			return bRush ? GetOreCollected(A.Key) > GetOreCollected(B.Key) : A.Key->GetKills() > B.Key->GetKills();
		});
		return Board;
	}

	/** What a brawler's row and the MVP pill show: knockouts, or in Meteor Rush the ore picked up. */
	FString GetTally(const ARRRandomCharacter* Brawler)
	{
		return ARRRandomGameState::IsMeteorRush(Brawler) ? FString::Printf(TEXT("%d ORE"), GetOreCollected(Brawler)) : FString::Printf(TEXT("%d KO"), Brawler->GetKills());
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
		const FString Kills = GetTally(Entry.Key);
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
		const FString KO = GetTally(MVP);
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
	// At the wheel the gun is put away: nothing to aim
	if (!Controller || Brawler.IsDriving())
	{
		MuzzleBlockedBlend = 0.f;
		return;
	}
	// The muzzle buried in a wall the brawler hugs: the gun won't fire (ARRRandomCharacter::GetShotStart), and the aim says so
	FVector Target;
	FTransform Shot;
	const bool bBlocked = !Brawler.IsInRobot() && Controller->GetFireTarget(Target) && !Brawler.GetShotStart(Target, Shot);
	MuzzleBlockedBlend = FMath::FInterpConstantTo(MuzzleBlockedBlend, bBlocked ? 1.f : 0.f, GetWorld()->GetDeltaSeconds(), 8.f);
	const float Warn = MuzzleBlockedBlend;
	const float S = UIScale;
	const float Thickness = FMath::Max(1.5f, 1.6f * S);
	// A small pink cross with BLOCKED under it
	auto DrawBlocked = [this, Warn, S, Thickness](const FVector2D& Center, float Below)
	{
		const float Arm = 4.5f * S;
		for (const FVector2D& Way : { FVector2D(1.f, 1.f), FVector2D(1.f, -1.f) })
		{
			Canvas->K2_DrawLine(Center - Way * Arm, Center + Way * Arm, Thickness + 2.f, WithAlpha(RRUI::Ink, 0.6f * Warn));
			Canvas->K2_DrawLine(Center - Way * Arm, Center + Way * Arm, Thickness, WithAlpha(RRUI::Pink, Warn));
		}
		const float Size = 9.f * S;
		const FString Label = TEXT("BLOCKED");
		const FVector2D Box = RRUI::MeasureText(Label, Size, EWeight::Black, Size * 0.12f);
		RRUI::DrawGlowText(Canvas, Label, FVector2D(Center.X - Box.X * 0.5f, Center.Y + Below), Size, EWeight::Black, WithAlpha(RRUI::PinkDeep, Warn), Size * 0.12f);
	};

	if (!Controller->IsThirdPerson())
	{
		// From above there is no crosshair: the warning sits just under the cursor
		float MouseX;
		float MouseY;
		if (Warn > 0.f && Controller->GetMousePosition(MouseX, MouseY))
		{
			DrawBlocked(FVector2D(MouseX, MouseY + 22.f * S), 8.f * S);
		}
		return;
	}
	// A dot in a dashed ring; the ring closes in the sights and opens briefly after a shot. Blocked, it turns pink and the
	// dot becomes a cross.
	const FVector2D Middle(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const float Kick = FMath::Clamp(1.f - Brawler.GetTimeSinceShot() / 0.18f, 0.f, 1.f);
	const float Radius = (FMath::Lerp(13.f, 8.f, Brawler.GetSightsBlend()) + 8.f * Kick) * S;
	RRUI::DrawDashedRing(Canvas, Middle, Radius, Thickness + 2.f, 8, 0.55f, WithAlpha(RRUI::Ink, 0.45f));
	RRUI::DrawDashedRing(Canvas, Middle, Radius, Thickness, 8, 0.5f, FMath::Lerp(RRUI::White, RRUI::Pink, Warn));
	if (Warn < 1.f)
	{
		RRUI::DrawDisc(Canvas, Middle, 3.2f * S, WithAlpha(RRUI::Ink, 0.8f * (1.f - Warn)));
		RRUI::DrawDisc(Canvas, Middle, 2.f * S, WithAlpha(RRUI::White, 1.f - Warn));
	}
	if (Warn > 0.f)
	{
		DrawBlocked(Middle, Radius + 5.f * S);
	}
}

namespace
{
	// The star robot's comic words while it combines: when (seconds in), what, how big, which way tilted, where on it
	// (a share of its height)
	struct FRobotWord
	{
		float Time;
		const TCHAR* Text;
		float Size;
		float Tilt;
		float Height;
	};
	const FRobotWord RobotWords[] =
	{
		{ 1.05f, TEXT("WHOOSH!"), 1.f, -8.f, 0.9f },
		{ 4.2f, TEXT("SHING!"), 1.1f, 6.f, 1.f },
		{ 5.6f, TEXT("THOOM!"), 1.2f, -5.f, 0.15f },
		{ 6.8f, TEXT("KA-CHUNK!"), 1.f, 7.f, 0.55f },
		{ 7.4f, TEXT("CLANK!"), 1.f, -9.f, 0.8f },
		// The fists shooting out of the arms, the head springing up out of the chest (ARRRandomStarRobot's timeline)
		{ 7.6f, TEXT("SHA-CHAK!"), 1.f, 8.f, 0.3f },
		{ 8.9f, TEXT("BOING!"), 0.95f, 10.f, 1.05f },
	};
	constexpr float RobotWordLife = 1.1f;
	const TCHAR* const RobotSeatNames[] = { TEXT("MOVE"), TEXT("ATTACK"), TEXT("DEFEND") };
	const TCHAR* const RobotSeatHelp[] = { TEXT("WALK IT AROUND"), TEXT("LEFT: SWORD COMBO  ·  RIGHT: MISSILES"), TEXT("HOLD FIRE: SHIELD") };

	/** [F] then the words, centered on CenterX with its top at Top. */
	void DrawKeyPrompt(UCanvas* Canvas, const FString& Text, float CenterX, float Top, float S, const FLinearColor& Color)
	{
		const float Size = 13.f * S;
		const float KeyHeight = 19.f * S;
		const float Gap = 6.f * S;
		const FVector2D Label = RRUI::MeasureText(Text, Size, EWeight::Black, Size * 0.06f);
		const float KeyWidth = FMath::Max(KeyHeight, RRUI::MeasureText(TEXT("F"), KeyHeight * 0.62f, EWeight::Black).X + KeyHeight * 0.5f);
		const float X = CenterX - (KeyWidth + Gap + Label.X) * 0.5f;
		RRUI::DrawKey(Canvas, TEXT("F"), FVector2D(X, Top), KeyHeight);
		RRUI::DrawGlowText(Canvas, Text, FVector2D(X + KeyWidth + Gap, Top + (KeyHeight - Label.Y) * 0.5f), Size, EWeight::Black, Color, Size * 0.06f);
	}
}

void ARRRandomHUD::DrawStarRobot(const ARRRandomCharacter* Viewer, const ARRRandomStarRobot* Robot)
{
	const UWorld* World = GetWorld();
	if (!World || !Canvas)
	{
		return;
	}
	const float S = UIScale;
	const float Now = World->GetTimeSeconds();

	// No robot yet: over the team's gathered cars, a pulsing sign
	if (!Robot)
	{
		ARRRandomVehicle* Cars[3];
		FVector Center;
		if (!ARRRandomStarRobot::FindGathered(this, Viewer ? Viewer->GetTeam() : 0, Cars, Center))
		{
			return;
		}
		// A little star over each car
		for (const ARRRandomVehicle* Car : Cars)
		{
			const FVector Over = Project(Car->GetActorLocation() + FVector(0.f, 0.f, 230.f));
			if (Over.Z > 0.f)
			{
				DrawCartoonStar(Canvas, FVector2D(Over.X, Over.Y + FMath::Sin(Now * 5.f + Over.X) * 4.f * S), 11.f * S, Now * 90.f, RRUI::Butter, 1.5f * S);
			}
		}
		const FVector Sign = Project(Center + FVector(0.f, 0.f, 330.f));
		if (Sign.Z <= 0.f)
		{
			return;
		}
		const float Pulse = 1.f + 0.07f * FMath::Sin(Now * 6.f);
		const FVector2D Middle(Sign.X, Sign.Y);
		DrawStarburst(Canvas, Middle, FVector2D(118.f, 46.f) * S * Pulse + FVector2D(3.f * S), 14, 0.78f, Now * 20.f, RRUI::WithAlpha(RRUI::Ink, 0.9f));
		DrawStarburst(Canvas, Middle, FVector2D(118.f, 46.f) * S * Pulse, 14, 0.78f, Now * 20.f, RRUI::Butter);
		const float TitleSize = 21.f * S * Pulse;
		const FString Title = TEXT("STAR ROBOT");
		const FVector2D TitleBox = RRUI::MeasureText(Title, TitleSize, EWeight::Black);
		DrawEdgedText(Canvas, Title, Middle - FVector2D(TitleBox.X * 0.5f, TitleBox.Y * 0.62f), TitleSize, RRUI::SkyDeep, RRUI::White);
		const float SubSize = 10.f * S;
		const FString Sub = TEXT("READY TO COMBINE");
		const FVector2D SubBox = RRUI::MeasureText(Sub, SubSize, EWeight::ExtraBold, SubSize * 0.15f);
		RRUI::DrawText(Canvas, Sub, FVector2D(Middle.X - SubBox.X * 0.5f, Middle.Y + TitleBox.Y * 0.3f), SubSize, EWeight::ExtraBold, RRUI::Ink, SubSize * 0.15f);
		if (Viewer && ARRRandomStarRobot::CanStartCombine(Viewer))
		{
			DrawKeyPrompt(Canvas, TEXT("COMBINE!"), Middle.X, Middle.Y + 56.f * S, S, RRUI::Ink);
		}
		return;
	}

	// Over its head, and a little higher on the screen so the plate clears its shoulders seen from above
	const FVector TopOnScreen = Project(Robot->GetTopLocation() + FVector(0.f, 0.f, 120.f)) - FVector(0.f, 45.f * S, 0.f);
	const FVector Feet = Robot->GetActorLocation();
	if (!Robot->IsAssembled())
	{
		const float Progress = Robot->GetCombineProgress();
		const float Time = Progress * ARRRandomStarRobot::CombineSeconds;
		// Manga speed lines rushing in at it from all round, flickering, while the parts fly together
		const FVector MiddleOnScreen = Project(Feet + FVector(0.f, 0.f, 320.f));
		if (MiddleOnScreen.Z > 0.f && Time > 1.f)
		{
			const FVector2D Middle(MiddleOnScreen.X, MiddleOnScreen.Y);
			const float Outer = FVector2D(Canvas->ClipX, Canvas->ClipY).Size();
			const float Inner = 230.f * S;
			const float Strength = FMath::Clamp((Time - 1.f) / 1.f, 0.f, 1.f) * FMath::Clamp((ARRRandomStarRobot::CombineSeconds - Time) / 0.6f, 0.f, 1.f);
			const uint32 Seed = static_cast<uint32>(FMath::FloorToInt(Now * 20.f));
			for (uint32 Line = 0; Line < 40; ++Line)
			{
				const uint32 Hash = (Line * 2654435761u) ^ (Seed * 40503u);
				const float Angle = (Line + (Hash % 100) / 100.f) * 2.f * PI / 40.f;
				const float Reach = Inner * (1.f + (Hash % 37) / 37.f * 0.8f);
				const float Half = (1.5f + (Hash % 5)) * S;
				const FVector2D Dir(FMath::Cos(Angle), FMath::Sin(Angle));
				const FVector2D Side(-Dir.Y, Dir.X);
				RRUI::DrawTriangle(Canvas, Middle + Dir * Outer + Side * Half * 3.f, Middle + Dir * Outer - Side * Half * 3.f, Middle + Dir * Reach,
					RRUI::WithAlpha(RRUI::Ink, 0.22f * Strength));
			}
		}
		// The comic words
		for (const FRobotWord& Word : RobotWords)
		{
			const float Age = Time - Word.Time;
			const FVector OnScreen = Project(Feet + FVector(0.f, 0.f, 640.f * Word.Height));
			if (Age < 0.f || Age > RobotWordLife || OnScreen.Z <= 0.f)
			{
				continue;
			}
			const float Scale = Age < 0.12f ? FMath::Lerp(0.25f, 1.3f, Age / 0.12f) : Age < 0.24f ? FMath::Lerp(1.3f, 1.f, (Age - 0.12f) / 0.12f) : 1.f;
			const float Alpha = 1.f - FMath::Clamp((Age - (RobotWordLife - 0.25f)) / 0.25f, 0.f, 1.f);
			const FVector2D Center(OnScreen.X + (Word.Tilt > 0.f ? 1.f : -1.f) * 150.f * S, OnScreen.Y - Age * 35.f * S);
			const float Size = 26.f * S * Word.Size * Scale;
			const FVector2D Box = RRUI::MeasureText(Word.Text, Size, EWeight::Black);
			const FVector2D Burst(Box.X * 0.78f + 16.f * S * Scale, Box.Y * 0.95f + 14.f * S * Scale);
			DrawStarburst(Canvas, Center, Burst + FVector2D(3.f * S), 12, 0.74f, Word.Tilt * 3.f, RRUI::WithAlpha(RRUI::Ink, Alpha));
			DrawStarburst(Canvas, Center, Burst, 12, 0.74f, Word.Tilt * 3.f, RRUI::WithAlpha(RRUI::Butter, Alpha));
			DrawEdgedText(Canvas, Word.Text, Center - Box * 0.5f, Size, RRUI::WithAlpha(RRUI::PinkDeep, Alpha), RRUI::WithAlpha(RRUI::White, Alpha));
		}
		// The gauge over it
		if (TopOnScreen.Z > 0.f)
		{
			const float Width = 190.f * S;
			const float Height = 12.f * S;
			const FVector2D Top(TopOnScreen.X - Width * 0.5f, TopOnScreen.Y);
			const FString Label = FString::Printf(TEXT("COMBINING  %d%%"), FMath::FloorToInt(Progress * 100.f));
			const float Size = 12.f * S;
			const FVector2D LabelBox = RRUI::MeasureText(Label, Size, EWeight::Black, Size * 0.1f);
			RRUI::DrawPlate(Canvas, FBox2D(Top - FVector2D(10.f * S, LabelBox.Y + 10.f * S), Top + FVector2D(Width + 10.f * S, Height + 8.f * S)), 18.f * S);
			RRUI::DrawText(Canvas, Label, FVector2D(TopOnScreen.X - LabelBox.X * 0.5f, Top.Y - LabelBox.Y - 4.f * S), Size, EWeight::Black, RRUI::Ink, Size * 0.1f);
			RRUI::DrawBar(Canvas, FBox2D(Top, Top + FVector2D(Width, Height)), Progress, WithAlpha(RRUI::Ink, 0.12f), FMath::Fmod(Now * 4.f, 1.f) < 0.5f ? RRUI::Gold : RRUI::Butter);
		}
		// The one combining it: a reminder to hang on
		if (Viewer && Viewer == Robot->GetCombiner())
		{
			const FString Text = TEXT("COMBINING... DON'T GET KNOCKED OUT!");
			const float Size = 15.f * S;
			const FVector2D Box = RRUI::MeasureText(Text, Size, EWeight::Black, Size * 0.06f);
			const FVector2D Pos(Canvas->ClipX * 0.5f - Box.X * 0.5f, Canvas->ClipY * 0.72f);
			RRUI::DrawPlate(Canvas, FBox2D(Pos - FVector2D(14.f, 8.f) * S, Pos + Box + FVector2D(14.f, 8.f) * S), 22.f * S);
			RRUI::DrawText(Canvas, Text, Pos, Size, EWeight::Black, Blink(World) > 0.5f ? RRUI::PinkDeep : RRUI::Ink, Size * 0.06f);
		}
		return;
	}

	// Done: a banner for a moment (the team's own)
	const float Since = Robot->GetSecondsSinceAssembled();
	if (Since < 2.8f && (!Viewer || Viewer->GetTeam() == Robot->GetTeam()) && !Robot->GetClashRival())
	{
		const float Scale = Since < 0.15f ? FMath::Lerp(0.3f, 1.25f, Since / 0.15f) : Since < 0.3f ? FMath::Lerp(1.25f, 1.f, (Since - 0.15f) / 0.15f) : 1.f;
		const float Alpha = 1.f - FMath::Clamp((Since - 2.4f) / 0.4f, 0.f, 1.f);
		const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.2f);
		const float Size = 44.f * S * Scale;
		const FString Text = TEXT("STAR ROBOT, COMBINE!");
		const FVector2D Box = RRUI::MeasureText(Text, Size, EWeight::Black);
		DrawStarburst(Canvas, Center, FVector2D(Box.X * 0.66f, Box.Y * 1.15f) + FVector2D(4.f * S), 18, 0.8f, Since * 30.f, RRUI::WithAlpha(RRUI::Ink, Alpha));
		DrawStarburst(Canvas, Center, FVector2D(Box.X * 0.66f, Box.Y * 1.15f), 18, 0.8f, Since * 30.f, RRUI::WithAlpha(RRUI::Butter, Alpha));
		const FLinearColor Stops[] = { RRUI::SkyDeep, RRUI::LavenderDeep, RRUI::PinkDeep };
		RRUI::DrawGradientText(Canvas, Text, Center - Box * 0.5f, Size, EWeight::Black, Stops, Alpha);
	}

	// Over it: health, shield, the three seats
	if (TopOnScreen.Z > 0.f)
	{
		const float Width = 170.f * S;
		const FVector2D Top(TopOnScreen.X - Width * 0.5f, TopOnScreen.Y);
		const FLinearColor TeamColor = RRUI::TeamColor(Robot->GetTeam());
		const float ChipHeight = 17.f * S;
		const float ChipGap = 4.f * S;
		const float ChipWidth = (Width - ChipGap * 2.f) / 3.f;
		RRUI::DrawPlate(Canvas, FBox2D(Top - FVector2D(8.f * S, 8.f * S), Top + FVector2D(Width + 8.f * S, 16.f * S + ChipHeight + 8.f * S)), 18.f * S);
		RRUI::DrawBar(Canvas, FBox2D(Top, Top + FVector2D(Width, 10.f * S)), Robot->GetHealth() / FMath::Max(1.f, Robot->GetMaxHealth()), WithAlpha(RRUI::Ink, 0.12f), TeamColor);
		if (Robot->GetCrew(ERRRobotSeat::Defend))
		{
			RRUI::DrawBar(Canvas, FBox2D(Top + FVector2D(0.f, 11.5f * S), Top + FVector2D(Width, 14.5f * S)), Robot->GetShieldEnergy(), WithAlpha(RRUI::Ink, 0.08f),
				Robot->IsShieldBroken() ? RRUI::PinkDeep : Robot->IsShieldUp() ? RRUI::Sky : RRUI::SkyDeep);
		}
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const ERRRobotSeat Seat = Index == 0 ? ERRRobotSeat::Move : Index == 1 ? ERRRobotSeat::Attack : ERRRobotSeat::Defend;
			const bool bTaken = Robot->GetCrew(Seat) != nullptr;
			const FBox2D Chip(Top + FVector2D(Index * (ChipWidth + ChipGap), 16.f * S), Top + FVector2D(Index * (ChipWidth + ChipGap) + ChipWidth, 16.f * S + ChipHeight));
			RRUI::DrawPill(Canvas, Chip, bTaken ? TeamColor : WithAlpha(RRUI::Ink, 0.1f));
			const float Size = 8.5f * S;
			const FVector2D Box = RRUI::MeasureText(RobotSeatNames[Index], Size, EWeight::Black, Size * 0.1f);
			RRUI::DrawText(Canvas, RobotSeatNames[Index], Chip.GetCenter() - Box * 0.5f, Size, EWeight::Black, bTaken ? RRUI::White : RRUI::InkSoft, Size * 0.1f);
		}
	}
	if (!Viewer || !Viewer->IsAlive())
	{
		return;
	}

	// The crew: which seat, what it does, who is missing (not in a clash: its row of keys has the screen then)
	if (Viewer->GetRobot() == Robot && Viewer->IsInRobot() && !Robot->IsClashing())
	{
		const int32 Index = Viewer->GetRobotSeat() == ERRRobotSeat::Move ? 0 : Viewer->GetRobotSeat() == ERRRobotSeat::Attack ? 1 : 2;
		const int32 Missing = 3 - Robot->CountCrew();
		const FString Title = FString::Printf(TEXT("YOU: %s"), RobotSeatNames[Index]);
		const FString Help = Missing > 0 && Index == 0 ? FString::Printf(TEXT("NEED %d MORE CREW TO WALK"), Missing) : FString(RobotSeatHelp[Index]);
		const float TitleSize = 18.f * S;
		const float HelpSize = 11.f * S;
		const FVector2D TitleBox = RRUI::MeasureText(Title, TitleSize, EWeight::Black, TitleSize * 0.06f);
		const FVector2D HelpBox = RRUI::MeasureText(Help, HelpSize, EWeight::ExtraBold, HelpSize * 0.1f);
		const float Width = FMath::Max(TitleBox.X, HelpBox.X) + 36.f * S;
		const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.8f);
		const FBox2D Plate(Center - FVector2D(Width * 0.5f, 26.f * S), Center + FVector2D(Width * 0.5f, 30.f * S));
		RRUI::DrawPlate(Canvas, Plate, 22.f * S);
		RRUI::DrawText(Canvas, Title, FVector2D(Center.X - TitleBox.X * 0.5f, Plate.Min.Y + 6.f * S), TitleSize, EWeight::Black, RRUI::Readable(RRUI::TeamColor(Robot->GetTeam())), TitleSize * 0.06f);
		RRUI::DrawText(Canvas, Help, FVector2D(Center.X - HelpBox.X * 0.5f, Plate.Min.Y + 8.f * S + TitleBox.Y), HelpSize, EWeight::ExtraBold,
			Missing > 0 && Index == 0 ? RRUI::PinkDeep : RRUI::InkSoft, HelpSize * 0.1f);
		// Over it: the ATTACK seat's two attacks (ready or cooling down; the sword's combo so far), the DEFEND seat's shield
		const float CardWidth = 200.f * S;
		const float CardHeight = 34.f * S;
		const float CardTop = Plate.Min.Y - 10.f * S - CardHeight;
		const auto DrawGauge = [&](float Left, const TCHAR* Key, const TCHAR* Name, int32 Combo, const FString& Status, bool bReady, float Share, const FLinearColor& Fill)
		{
			const FBox2D Card(FVector2D(Left, CardTop), FVector2D(Left + CardWidth, CardTop + CardHeight));
			RRUI::DrawPlate(Canvas, Card, 14.f * S);
			const float Size = 11.f * S;
			const float Line = Card.Min.Y + 2.f * S;
			float X = Card.Min.X + 4.f * S;
			X += RRUI::DrawKey(Canvas, Key, FVector2D(X, Line), 16.f * S) + 6.f * S;
			X += RRUI::DrawText(Canvas, Name, FVector2D(X, Line + 1.f * S), Size, EWeight::Black, RRUI::Ink, Size * 0.08f).X + 7.f * S;
			// The combo's hits: one lit for each swung
			for (int32 Hit = 0; Combo >= 0 && Hit < 3; ++Hit)
			{
				RRUI::DrawDisc(Canvas, FVector2D(X + 4.f * S + Hit * 11.f * S, Line + 8.5f * S), 4.f * S, Hit < Combo ? RRUI::Gold : WithAlpha(RRUI::Ink, 0.15f));
			}
			const FVector2D StatusBox = RRUI::MeasureText(Status, Size, EWeight::Black, Size * 0.08f);
			RRUI::DrawText(Canvas, Status, FVector2D(Card.Max.X - 4.f * S - StatusBox.X, Line + 1.f * S), Size, EWeight::Black, bReady ? RRUI::MintDeep : RRUI::InkSoft, Size * 0.08f);
			RRUI::DrawBar(Canvas, FBox2D(FVector2D(Card.Min.X + 4.f * S, Card.Max.Y - 9.f * S), FVector2D(Card.Max.X - 4.f * S, Card.Max.Y - 4.f * S)), Share, WithAlpha(RRUI::Ink, 0.12f), Fill);
		};
		const auto Seconds = [](float Left) { return FString::Printf(TEXT("%.1fs"), Left); };
		// (Not in a clash: its own row of keys is what counts then)
		if (Index == 1 && !Robot->IsClashing())
		{
			const float SwordIn = Robot->GetSwordReadyIn();
			const float MissileIn = Robot->GetMissileReadyIn();
			const bool bBlocked = Robot->IsShieldUp();
			const bool bSwordReady = SwordIn <= 0.f && !bBlocked;
			const bool bMissileReady = MissileIn <= 0.f && !bBlocked;
			DrawGauge(Center.X - CardWidth - 5.f * S, TEXT("LMB"), TEXT("SWORD"), Robot->GetComboStage(), bBlocked ? FString(TEXT("SHIELD")) : SwordIn > 0.f ? Seconds(SwordIn) : FString(TEXT("READY")),
				bSwordReady, 1.f - SwordIn / ARRRandomStarRobot::SwordCooldown, bSwordReady ? RRUI::Sky : RRUI::LavenderDeep);
			DrawGauge(Center.X + 5.f * S, TEXT("RMB"), TEXT("MISSILES"), -1, bBlocked ? FString(TEXT("SHIELD")) : MissileIn > 0.f ? Seconds(MissileIn) : FString(TEXT("READY")),
				bMissileReady, 1.f - MissileIn / ARRRandomStarRobot::MissileCooldown, bMissileReady ? RRUI::Gold : RRUI::Butter);
		}
		else if (Index == 2 && !Robot->IsClashing())
		{
			const bool bBroken = Robot->IsShieldBroken();
			DrawGauge(Center.X - CardWidth * 0.5f, TEXT("LMB"), TEXT("SHIELD"), -1,
				bBroken ? FString(TEXT("BROKEN")) : Robot->GetComboStage() > 0 ? FString(TEXT("ATTACKING")) : FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Robot->GetShieldHealth()), FMath::RoundToInt(ARRRandomStarRobot::ShieldMaxHealth)),
				!bBroken, Robot->GetShieldEnergy(), bBroken ? RRUI::PinkDeep : Robot->IsShieldUp() ? RRUI::Sky : RRUI::SkyDeep);
		}
		DrawKeyPrompt(Canvas, TEXT("GET OUT"), Center.X, Plate.Max.Y + 8.f * S, S, RRUI::InkSoft);
		return;
	}
	// Next to it with a seat free: get in
	if (!Viewer->GetVehicle() && Robot->CanBoard(Viewer))
	{
		const ERRRobotSeat Seat = Robot->GetFreeSeat();
		const int32 Index = Seat == ERRRobotSeat::Move ? 0 : Seat == ERRRobotSeat::Attack ? 1 : 2;
		const FVector Under = Project(Viewer->GetActorLocation() - FVector(0.f, 0.f, Viewer->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		if (Under.Z > 0.f)
		{
			DrawKeyPrompt(Canvas, FString::Printf(TEXT("GET IN: %s"), RobotSeatNames[Index]), Under.X, Under.Y + 8.f * S, S, RRUI::Ink);
		}
	}
}

void ARRRandomHUD::DrawStarRobots(const ARRRandomCharacter* Viewer)
{
	// The viewer's team's robot (or the sign over its gathered cars), then the other team's
	const int32 MyTeam = Viewer ? Viewer->GetTeam() : 0;
	DrawStarRobot(Viewer, ARRRandomStarRobot::Get(this, MyTeam));
	for (TActorIterator<ARRRandomStarRobot> It(GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->GetTeam() != MyTeam)
		{
			DrawStarRobot(Viewer, *It);
		}
	}
	DrawRobotDuel(Viewer);
}

void ARRRandomHUD::DrawRobotDuel(const ARRRandomCharacter* Viewer)
{
	const UWorld* World = GetWorld();
	if (!World || !Canvas)
	{
		return;
	}
	const float S = UIScale;
	const float Now = World->GetTimeSeconds();
	// A comic word in a starburst, popping in and fading out at the end of its life
	const auto Word = [&](const FString& Text, const FVector2D& Center, float Age, float Life, float Size, float Tilt, const FLinearColor& Fill)
	{
		const float Scale = Age < 0.12f ? FMath::Lerp(0.25f, 1.3f, Age / 0.12f) : Age < 0.24f ? FMath::Lerp(1.3f, 1.f, (Age - 0.12f) / 0.12f) : 1.f;
		const float Alpha = 1.f - FMath::Clamp((Age - (Life - 0.25f)) / 0.25f, 0.f, 1.f);
		const float Height = Size * S * Scale;
		const FVector2D Box = RRUI::MeasureText(Text, Height, EWeight::Black);
		const FVector2D Burst(Box.X * 0.78f + 16.f * S * Scale, Box.Y * 0.95f + 14.f * S * Scale);
		DrawStarburst(Canvas, Center, Burst + FVector2D(3.f * S), 12, 0.74f, Tilt * 3.f, RRUI::WithAlpha(RRUI::Ink, Alpha));
		DrawStarburst(Canvas, Center, Burst, 12, 0.74f, Tilt * 3.f, RRUI::WithAlpha(Fill, Alpha));
		DrawEdgedText(Canvas, Text, Center - Box * 0.5f, Height, RRUI::WithAlpha(RRUI::PinkDeep, Alpha), RRUI::WithAlpha(RRUI::White, Alpha));
	};
	const auto Centered = [&](const FString& Text, const FVector2D& Middle, float Size, const FLinearColor& Color)
	{
		const FVector2D Box = RRUI::MeasureText(Text, Size, EWeight::Black, Size * 0.06f);
		RRUI::DrawGlowText(Canvas, Text, Middle - Box * 0.5f, Size, EWeight::Black, Color, Size * 0.06f);
	};
	const ARRRandomStarRobot* Mine = Viewer && Viewer->IsInRobot() ? Viewer->GetRobot() : nullptr;

	// Over every robot: its last loud moment, and kneeling or reeling
	static const TCHAR* const ShoutWords[] = { TEXT(""), TEXT("PARRY!"), TEXT("GUARD BREAK!"), TEXT("CLASH!"), TEXT("WIN!"), TEXT("DOWN!"), TEXT("GET UP!"), TEXT("FINISH!") };
	for (TActorIterator<ARRRandomStarRobot> It(World); It; ++It)
	{
		const ARRRandomStarRobot* Robot = *It;
		const FVector Top = IsValid(Robot) && Robot->IsAssembled() ? Project(Robot->GetTopLocation() + FVector(0.f, 0.f, 60.f)) : FVector::ZeroVector;
		if (Top.Z <= 0.f)
		{
			continue;
		}
		float Age = 0.f;
		const ERRRobotShout Shout = Robot->GetShout(Age);
		const float Life = Shout == ERRRobotShout::Finish ? 1.8f : 1.1f;
		if (Shout != ERRRobotShout::None && Age < Life)
		{
			const bool bBig = Shout == ERRRobotShout::Finish || Shout == ERRRobotShout::Clash;
			const bool bBad = Shout == ERRRobotShout::Down || Shout == ERRRobotShout::GuardBreak;
			Word(ShoutWords[static_cast<int32>(Shout)], FVector2D(Top.X, Top.Y - 40.f * S - Age * 30.f * S), Age, Life, bBig ? 34.f : 26.f,
				Shout == ERRRobotShout::Parry ? -8.f : 8.f, bBad ? RRUI::Pink : RRUI::Butter);
		}
		if (Robot->IsDowned())
		{
			// For the rival's crew: what finishes it
			const bool bTheirs = Mine && Mine->GetTeam() != Robot->GetTeam();
			Centered(bTheirs ? TEXT("SLAM IT TO FINISH!") : TEXT("DOWN!"), FVector2D(Top.X, Top.Y - 100.f * S), 15.f * S, Blink(World) > 0.5f ? RRUI::PinkDeep : RRUI::Ink);
		}
		else if (Robot->IsStaggered() && !Robot->GetClashRival())
		{
			Centered(TEXT("DIZZY"), FVector2D(Top.X, Top.Y - 100.f * S), 12.f * S, RRUI::LavenderDeep);
		}
	}
	if (!Mine)
	{
		return;
	}

	// The rival winding up a blow at this robot: a warning over it, big for the slam
	const ARRRandomStarRobot* Rival = Mine->GetRival();
	if (Rival && !Mine->GetClashRival())
	{
		float Into = 0.f;
		const int32 Swing = Rival->GetShownSwing(Into);
		const FVector Top = Project(Rival->GetTopLocation() + FVector(0.f, 0.f, 60.f));
		if (Swing > 0 && Into < 0.45f && Top.Z > 0.f && FVector::Dist2D(Rival->GetActorLocation(), Mine->GetActorLocation()) < 1500.f)
		{
			const bool bSlam = Swing == 3;
			const float Radius = (bSlam ? 30.f : 20.f) * S * (1.f + 0.15f * FMath::Sin(Now * 30.f));
			const FVector2D At(Top.X, Top.Y - 75.f * S);
			RRUI::DrawDisc(Canvas, At, Radius + 3.f * S, RRUI::Ink);
			RRUI::DrawDisc(Canvas, At, Radius, bSlam ? RRUI::PinkDeep : RRUI::Gold);
			const float Mark = Radius * 1.3f;
			const FVector2D MarkBox = RRUI::MeasureText(TEXT("!"), Mark, EWeight::Black);
			RRUI::DrawText(Canvas, TEXT("!"), At - MarkBox * 0.5f, Mark, EWeight::Black, RRUI::White);
			if (bSlam)
			{
				Centered(TEXT("SLAM!"), At + FVector2D(0.f, Radius + 14.f * S), 14.f * S, RRUI::PinkDeep);
			}
		}
	}
	// The rival's missiles coming in at this robot
	int32 Incoming = 0;
	for (TActorIterator<ARRRandomProjectile> It(World); It; ++It)
	{
		const ARRRandomCharacter* By = Cast<ARRRandomCharacter>(It->GetInstigator());
		if (It->bMissile && It->IsActorTickEnabled() && By && By->GetTeam() != Mine->GetTeam() && FVector::Dist2D(It->MissileTarget, Mine->GetActorLocation()) < 900.f)
		{
			++Incoming;
		}
	}
	if (Incoming > 0)
	{
		Centered(FString::Printf(TEXT("MISSILES INCOMING  x%d"), Incoming), FVector2D(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.24f), 16.f * S,
			Blink(World) > 0.5f ? RRUI::PinkDeep : RRUI::Ink);
	}

	// The clash: both sides' progress, the time left, this seat's row of keys, then how it ended
	const ARRRandomStarRobot* Foe = Mine->GetClashRival();
	if (!Foe)
	{
		return;
	}
	const int32 Seat = Viewer->GetRobotSeat() == ERRRobotSeat::Move ? 0 : Viewer->GetRobotSeat() == ERRRobotSeat::Attack ? 1 : 2;
	const int32 Outcome = Mine->GetClashOutcome();
	const float Time = Mine->GetClashTime();
	// (Up top and down low: the middle of the screen is left to the blades)
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.25f);
	static const TCHAR* const Outcomes[] = { TEXT("CLASH!"), TEXT("YOU WIN THE CLASH!"), TEXT("CLASH LOST - WIDE OPEN!"), TEXT("EVEN!") };
	Word(Outcomes[FMath::Clamp(Outcome, 0, 3)], Center - FVector2D(0.f, 78.f * S), FMath::Min(Time, 0.3f), 100.f, Outcome == 0 ? 36.f : 28.f, -4.f,
		Outcome == 2 ? RRUI::Pink : RRUI::Butter);
	const float Width = 380.f * S;
	const float LabelSize = 11.f * S;
	RRUI::DrawPlate(Canvas, FBox2D(Center - FVector2D(Width * 0.5f + 70.f * S, 34.f * S), Center + FVector2D(Width * 0.5f + 20.f * S, 22.f * S)), 18.f * S);
	const auto Side = [&](const TCHAR* Label, float Share, const FLinearColor& Color, float Y)
	{
		RRUI::DrawText(Canvas, Label, FVector2D(Center.X - Width * 0.5f - 62.f * S, Y - 7.f * S), LabelSize, EWeight::Black, RRUI::Ink, LabelSize * 0.08f);
		RRUI::DrawBar(Canvas, FBox2D(FVector2D(Center.X - Width * 0.5f, Y - 5.f * S), FVector2D(Center.X + Width * 0.5f, Y + 5.f * S)), Share, RRUI::WithAlpha(RRUI::Ink, 0.12f), Color);
	};
	Side(TEXT("YOU"), Mine->GetClashShare(), RRUI::TeamColor(Mine->GetTeam()), Center.Y - 18.f * S);
	Side(TEXT("RIVAL"), Foe->GetClashShare(), RRUI::TeamColor(Foe->GetTeam()), Center.Y);
	RRUI::DrawBar(Canvas, FBox2D(FVector2D(Center.X - Width * 0.5f, Center.Y + 12.f * S), FVector2D(Center.X + Width * 0.5f, Center.Y + 15.f * S)),
		Outcome == 0 ? 1.f - Time / ARRRandomStarRobot::ClashSeconds : 0.f, RRUI::WithAlpha(RRUI::Ink, 0.08f), RRUI::Gold);
	if (Outcome != 0)
	{
		return;
	}
	// A new clash starts the count of wrong keys afresh; a wrong one shakes the key to press, red
	const int32 Done = Mine->GetClashDone(Seat);
	const int32 Misses = Mine->GetClashMisses(Seat);
	if (Time < 0.2f || Misses < ClashMissesSeen)
	{
		ClashMissesSeen = Misses;
	}
	if (Misses > ClashMissesSeen)
	{
		ClashMissesSeen = Misses;
		ClashMissTime = Now;
	}
	const bool bShake = Now - ClashMissTime < 0.3f;
	static const TCHAR* const Letters[] = { TEXT("W"), TEXT("A"), TEXT("S"), TEXT("D") };
	const float Cap = 32.f * S;
	const float Gap = 8.f * S;
	const int32 Count = ARRRandomStarRobot::ClashKeyCount;
	const float RowLeft = Center.X - (Count * Cap + (Count - 1) * Gap) * 0.5f;
	const float RowTop = Canvas->ClipY * 0.74f;
	RRUI::DrawPlate(Canvas, FBox2D(FVector2D(RowLeft - 12.f * S, RowTop - 12.f * S), FVector2D(RowLeft + Count * (Cap + Gap) + 4.f * S, RowTop + Cap + 30.f * S)), 18.f * S);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const bool bNext = Index == Done;
		const float Height = Cap * (bNext ? 1.25f + 0.08f * FMath::Sin(Now * 14.f) : 1.f);
		FVector2D Pos(RowLeft + Index * (Cap + Gap) + (Cap - Height) * 0.5f, RowTop + (Cap - Height) * 0.5f);
		if (bNext)
		{
			Pos.X += bShake ? FMath::Sin(Now * 70.f) * 5.f * S : 0.f;
			RRUI::DrawRounded(Canvas, FBox2D(Pos - FVector2D(4.f * S), Pos + FVector2D(Height + 4.f * S)), Height * 0.3f, bShake ? RRUI::PinkDeep : RRUI::Gold);
		}
		RRUI::DrawKey(Canvas, Letters[Mine->GetClashKey(Seat, Index) & 3], Pos, Height, Index < Done ? 0.3f : 1.f);
	}
	Centered(Done >= Count ? TEXT("DONE! HANG ON FOR YOUR CREW") : TEXT("PRESS THE KEYS IN ORDER!"), FVector2D(Center.X, RowTop + Cap + 14.f * S), 11.f * S,
		Done >= Count ? RRUI::MintDeep : RRUI::InkSoft);
}
