// ARRRandomHUD, continued (round 98): the main mode's screen: the rounds board, the round's intro (the rule die) and end,
// the cards a roll deals, the Q ability, who knocked me out, the emote wheel and the results' rounds and next match.
#include "RRRandomHUD.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomGameState.h"
#include "RRRandomKit.h"
#include "RRRandomMeteor.h"
#include "RRRandomPlayerController.h"
#include "RRRandomRounds.h"
#include "RRRandomSpecies.h"
#include "RRRandomUIStyle.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "EngineUtils.h"

using RRUI::EWeight;

namespace
{
	FLinearColor Faded(FLinearColor Color, float Alpha)
	{
		Color.A *= Alpha;
		return Color;
	}

	float Ease(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return 1.f - FMath::Square(1.f - T);
	}

	FVector2D Centered(UCanvas* Canvas, const FString& Text, float CenterX, float Y, float Size, EWeight Weight, const FLinearColor& Color, float Tracking = 0.f, bool bGlow = false)
	{
		const FVector2D Box = RRUI::MeasureText(Text, Size, Weight, Tracking);
		const FVector2D Pos(CenterX - Box.X * 0.5f, Y);
		return bGlow ? RRUI::DrawGlowText(Canvas, Text, Pos, Size, Weight, Color, Tracking) : RRUI::DrawText(Canvas, Text, Pos, Size, Weight, Color, Tracking);
	}

	FString Clock(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60);
	}

	// The cards: each this big on a 720 tall screen, this far apart, this high over the bottom
	constexpr float CardW = 128.f;
	constexpr float CardH = 92.f;
	constexpr float CardGap = 12.f;
	constexpr float CardsBottom = 150.f;
	// The emote wheel's size, and how far the mouse goes to pick a slice
	constexpr float WheelRadius = 120.f;
	constexpr float WheelPick = 30.f;
}

int32 ARRRandomHUD::GetEmotePick() const
{
	if (EmoteAim.Size() < WheelPick)
	{
		return 0;
	}
	// Slice 1 at the top, going round clockwise (screen Y down)
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(EmoteAim.X, -EmoteAim.Y));
	const float Slice = 360.f / ARRRandomCharacter::EmoteCount;
	return FMath::Clamp(FMath::FloorToInt(FMath::Fmod(Angle + 360.f + Slice * 0.5f, 360.f) / Slice), 0, ARRRandomCharacter::EmoteCount - 1) + 1;
}

float ARRRandomHUD::GetResultsAge() const
{
	return ResultsShownAt < 0.f ? -1.f : GetWorld()->GetRealTimeSeconds() - ResultsShownAt;
}

int32 ARRRandomHUD::GetResultsButtonValueAt(const FVector2D& ScreenPoint) const
{
	for (const FResultsButtonRect& Button : ResultsButtons)
	{
		if (Button.bEnabled && Button.Rect.IsInside(ScreenPoint))
		{
			return Button.Value;
		}
	}
	return INDEX_NONE;
}

void ARRRandomHUD::DrawRoundsBoard(const ARRRandomGameState& State)
{
	// "ROUND 2 · MOST KO" over "BLUE ●○  3 · 2  ○● RED" over the goal and the clock, on one soft plate
	const float S = UIScale;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Top = 12.f * S;
	const float RuleSize = 11.f * S;
	const float NumberSize = 30.f * S;
	const float TeamSize = 11.f * S;
	const float GoalSize = 10.f * S;
	const float Gap = 14.f * S;
	const ERRRoundRule Rule = State.GetRoundRule();
	const FString Heading = FString::Printf(TEXT("ROUND %d  ·  %s"), FMath::Max(1, State.GetRound()), *RRRounds::GetRuleName(Rule));
	const FString Blue = FString::FromInt(State.GetTeamScore(0));
	const FString Red = FString::FromInt(State.GetTeamScore(1));
	const float BlueWidth = RRUI::MeasureText(Blue, NumberSize, EWeight::Black).X;
	const float RedWidth = RRUI::MeasureText(Red, NumberSize, EWeight::Black).X;
	const float DotWidth = RRUI::MeasureText(TEXT("·"), NumberSize, EWeight::Black).X;
	const float Pip = 5.f * S;
	const float PipsWidth = RRRounds::RoundsToWin * (Pip * 2.f + 3.f * S);
	const float BlueLabel = RRUI::MeasureText(TEXT("BLUE"), TeamSize, EWeight::Black, TeamSize * 0.2f).X;
	const float RedLabel = RRUI::MeasureText(TEXT("RED"), TeamSize, EWeight::Black, TeamSize * 0.2f).X;
	const float NumberTop = Top + RuleSize * 1.4f;
	const float Baseline = NumberTop + NumberSize * 1.01f;
	const float GroupLeft = CenterX - DotWidth * 0.5f - Gap * 0.6f - BlueWidth - Gap - PipsWidth - 6.f * S - BlueLabel;
	const float GroupRight = CenterX + DotWidth * 0.5f + Gap * 0.6f + RedWidth + Gap + PipsWidth + 6.f * S + RedLabel;
	const float GoalTop = Baseline + 6.f * S;
	const float Bottom = GoalTop + GoalSize * 1.3f;
	RRUI::DrawPlate(Canvas, FBox2D(FVector2D(GroupLeft, Top), FVector2D(GroupRight, Bottom)), 18.f * S);

	Centered(Canvas, Heading, CenterX, Top, RuleSize, EWeight::Black, RRRounds::GetRuleColor(Rule), RuleSize * 0.2f);
	float X = CenterX - DotWidth * 0.5f;
	RRUI::DrawText(Canvas, TEXT("·"), FVector2D(X, NumberTop), NumberSize, EWeight::Black, Faded(RRUI::Ink, 0.4f));
	X -= Gap * 0.6f + BlueWidth;
	RRUI::DrawText(Canvas, Blue, FVector2D(X, NumberTop), NumberSize, EWeight::Black, RRUI::Ink);
	// The rounds each team has won, as dots by its name (filled: won)
	auto Pips = [&](int32 Team, float Left)
	{
		for (int32 Index = 0; Index < RRRounds::RoundsToWin; ++Index)
		{
			const FVector2D Center(Left + Pip + Index * (Pip * 2.f + 3.f * S), Baseline - TeamSize * 0.45f);
			RRUI::DrawRing(Canvas, Center, Pip, 1.5f * S, RRUI::TeamColorDeep(Team));
			if (Index < State.GetRoundWins(Team))
			{
				RRUI::DrawDisc(Canvas, Center, Pip - 1.f * S, RRUI::TeamColorDeep(Team));
			}
		}
	};
	X -= Gap + PipsWidth;
	Pips(0, X);
	X -= 6.f * S + BlueLabel;
	RRUI::DrawText(Canvas, TEXT("BLUE"), FVector2D(X, Baseline - TeamSize * 1.01f), TeamSize, EWeight::Black, RRUI::SkyDeep, TeamSize * 0.2f);
	X = CenterX + DotWidth * 0.5f + Gap * 0.6f;
	RRUI::DrawText(Canvas, Red, FVector2D(X, NumberTop), NumberSize, EWeight::Black, RRUI::Ink);
	X += RedWidth + Gap;
	Pips(1, X);
	X += PipsWidth + 6.f * S;
	RRUI::DrawText(Canvas, TEXT("RED"), FVector2D(X, Baseline - TeamSize * 1.01f), TeamSize, EWeight::Black, RRUI::PinkDeep, TeamSize * 0.2f);

	// What wins this round and the clock (sudden death in gold)
	const float Left = State.HasPlayStarted() ? State.GetMatchTimeRemaining() : RRRounds::RoundLength;
	FString Goal = Rule == ERRRoundRule::Deathmatch ? FString::Printf(TEXT("FIRST TO %d KO  ·  %s"), State.GetScoreToWin(), *Clock(Left))
		: Rule == ERRRoundRule::MostKills ? FString::Printf(TEXT("MOST KO  ·  %s"), *Clock(Left))
		: FString::Printf(TEXT("HOLD %d ORE  ·  %s"), State.GetScoreToWin(), *Clock(Left));
	if (State.IsOvertime())
	{
		Goal = Rule == ERRRoundRule::MeteorRush ? TEXT("SUDDEN DEATH  ·  GOLDEN CRYSTAL") : TEXT("SUDDEN DEATH  ·  NEXT KO WINS");
	}
	Centered(Canvas, Goal, CenterX, GoalTop, GoalSize, EWeight::ExtraBold, State.IsOvertime() ? RRUI::Gold : RRUI::InkSoft, GoalSize * 0.2f);

	// A meteor on its way (the meteor round): a pill under the board with the seconds to go
	if (State.IsMeteorRush())
	{
		for (TActorIterator<ARRRandomMeteor> It(GetWorld()); It; ++It)
		{
			const float Impact = It->GetTimeToImpact();
			if (Impact > 0.f)
			{
				const FString Warning = FString::Printf(TEXT("%s INCOMING  %d"), It->IsGolden() ? TEXT("GOLDEN METEOR") : TEXT("METEOR"), FMath::CeilToInt(Impact));
				const float WarnSize = 14.f * S;
				const FVector2D WarnBox = RRUI::MeasureText(Warning, WarnSize, EWeight::Black, WarnSize * 0.16f);
				const FLinearColor Color = It->IsGolden() ? RRUI::Gold : RRUI::PinkDeep;
				const FBox2D Pill(FVector2D(CenterX - WarnBox.X * 0.5f - 16.f * S, Bottom + 8.f * S), FVector2D(CenterX + WarnBox.X * 0.5f + 16.f * S, Bottom + 14.f * S + WarnBox.Y));
				RRUI::DrawPill(Canvas, Pill.ExpandBy(2.5f * S), Faded(Color, 0.35f + 0.65f * (0.5f + 0.5f * FMath::Cos(GetWorld()->GetTimeSeconds() * 12.f))));
				RRUI::DrawPill(Canvas, Pill, RRUI::White);
				RRUI::DrawText(Canvas, Warning, FVector2D(CenterX - WarnBox.X * 0.5f, Pill.Min.Y + 3.f * S), WarnSize, EWeight::Black, Color, WarnSize * 0.16f);
				break;
			}
		}
	}
}

void ARRRandomHUD::DrawRoundIntro(const ARRRandomGameState& State, const ARRRandomCharacter* Viewer)
{
	const float Since = State.GetSinceRoundIntro();
	if (State.HasPlayStarted() || State.IsRoundOver() || Since < 0.f)
	{
		return;
	}
	const float S = UIScale;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.2f;
	const float In = Ease(Since / 0.35f);
	// ROUND n (MATCH POINT when a team is one round off)
	const FString Title = FString::Printf(TEXT("ROUND %d"), FMath::Max(1, State.GetRound()));
	const float TitleSize = 46.f * S;
	const FLinearColor TitleStops[] = { RRUI::LavenderDeep, RRUI::PinkDeep };
	const FVector2D TitleBox = RRUI::MeasureText(Title, TitleSize, EWeight::Black, TitleSize * 0.08f);
	RRUI::DrawGradientText(Canvas, Title, FVector2D(CenterX - TitleBox.X * 0.5f, Y), TitleSize, EWeight::Black, TitleStops, In, TitleSize * 0.08f);
	const bool bMatchPoint = State.GetRoundWins(0) == RRRounds::RoundsToWin - 1 || State.GetRoundWins(1) == RRRounds::RoundsToWin - 1;
	if (bMatchPoint)
	{
		Centered(Canvas, State.GetRoundWins(0) == State.GetRoundWins(1) ? TEXT("FINAL ROUND") : TEXT("MATCH POINT"), CenterX, Y - 16.f * S, 12.f * S, EWeight::Black, Faded(RRUI::Gold, In), 12.f * S * 0.3f, true);
	}

	// The rule die: tumbling through faces, slowing, then landing on its face with a pop
	const float DieSize = 64.f * S;
	const FVector2D DieCenter(CenterX, Y + TitleBox.Y + 14.f * S + DieSize * 0.5f);
	const float RollEnd = RRRounds::RuleRollTime * 0.62f;
	int32 Face = State.GetRuleFace();
	float Pop = 1.f;
	if (Since < RollEnd)
	{
		// Faces flicking, slower and slower (the same on every screen, from the clock)
		const float Phase = FMath::Pow(Since / RollEnd, 0.6f) * 16.f;
		Face = 1 + (FMath::FloorToInt(Phase) * 5 + State.GetRound()) % 6;
		const float Wobble = FMath::Sin(Since * 30.f) * 6.f * S * (1.f - Since / RollEnd);
		RRUI::DrawDisc(Canvas, DieCenter + FVector2D(0.f, 4.f * S), DieSize * 0.62f, Faded(RRUI::Shadow, 0.5f));
		DrawDieFace(DieCenter + FVector2D(Wobble, -FMath::Abs(Wobble)), DieSize, Face, In);
	}
	else
	{
		Pop = 1.f + 0.35f * FMath::Max(0.f, 1.f - (Since - RollEnd) / 0.25f);
		RRUI::DrawDisc(Canvas, DieCenter, DieSize * 0.75f * Pop, Faded(RRRounds::GetRuleColor(State.GetRoundRule()), 0.35f));
		DrawDieFace(DieCenter, DieSize * Pop, Face, 1.f);
		// The rule and its line
		const float RuleIn = Ease((Since - RollEnd) / 0.3f);
		const float RuleSize = 28.f * S;
		const float RuleTop = DieCenter.Y + DieSize * 0.5f + 12.f * S;
		Centered(Canvas, RRRounds::GetRuleName(State.GetRoundRule()), CenterX, RuleTop, RuleSize, EWeight::Black, Faded(RRRounds::GetRuleColor(State.GetRoundRule()), RuleIn), RuleSize * 0.1f, true);
		Centered(Canvas, RRRounds::GetRuleLine(State.GetRoundRule()), CenterX, RuleTop + RuleSize * 1.25f, 13.f * S, EWeight::ExtraBold, Faded(RRUI::InkSoft, RuleIn), 0.f, true);
	}
}

void ARRRandomHUD::DrawRoundOver(const ARRRandomGameState& State, const ARRRandomCharacter* Viewer)
{
	if (!State.IsRoundOver())
	{
		return;
	}
	const float Since = State.GetSinceRoundOver();
	const float S = UIScale;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY * 0.24f;
	const float In = Ease(Since / 0.3f);
	const int32 Winner = State.GetRoundWinner();
	const bool bMine = Viewer && Viewer->GetTeam() == Winner;
	const FString Headline = Viewer ? (bMine ? TEXT("ROUND WON!") : TEXT("ROUND LOST")) : FString::Printf(TEXT("%s TAKES IT"), Winner == 0 ? TEXT("BLUE") : TEXT("RED"));
	const float HeadSize = 58.f * S * (1.f + 0.25f * (1.f - In));
	const FLinearColor WinStops[] = { RRUI::LavenderDeep, RRUI::PinkDeep, RRUI::Gold };
	const FLinearColor LossStops[] = { RRUI::SkyDeep, RRUI::LavenderDeep };
	const FVector2D HeadBox = RRUI::MeasureText(Headline, HeadSize, EWeight::Black, HeadSize * 0.1f);
	RRUI::DrawGradientText(Canvas, Headline, FVector2D(CenterX - HeadBox.X * 0.5f, Y), HeadSize, EWeight::Black,
		bMine || !Viewer ? TArrayView<const FLinearColor>(WinStops) : TArrayView<const FLinearColor>(LossStops), In, HeadSize * 0.1f);
	float Line = Y + HeadBox.Y + 4.f * S;
	Centered(Canvas, FString::Printf(TEXT("%s TAKES ROUND %d  ·  %d : %d"), Winner == 0 ? TEXT("BLUE") : TEXT("RED"), State.GetRound(), State.GetTeamScore(0), State.GetTeamScore(1)),
		CenterX, Line, 14.f * S, EWeight::Black, Faded(RRUI::TeamColorDeep(Winner), In), 14.f * S * 0.2f, true);
	Line += 24.f * S;
	// A sudden death settled by the dice: both teams' faces
	if (State.GetTiebreakDie(0) > 0)
	{
		const float Die = 40.f * S;
		DrawDieFace(FVector2D(CenterX - Die, Line + Die * 0.5f), Die, State.GetTiebreakDie(0), In);
		DrawDieFace(FVector2D(CenterX + Die, Line + Die * 0.5f), Die, State.GetTiebreakDie(1), In);
		Centered(Canvas, TEXT("VS"), CenterX, Line + Die * 0.25f, 12.f * S, EWeight::Black, Faded(RRUI::InkSoft, In), 0.f, true);
		Line += Die + 8.f * S;
	}
	// The rounds so far: BLUE ● ●  ·  ○ ● RED, big
	const float Pip = 11.f * S;
	for (int32 Team = 0; Team < 2; ++Team)
	{
		for (int32 Index = 0; Index < RRRounds::RoundsToWin; ++Index)
		{
			const float Offset = (Index + 0.5f) * (Pip * 2.f + 8.f * S) + 18.f * S;
			const FVector2D Center(CenterX + (Team == 0 ? -Offset : Offset), Line + Pip + 4.f * S);
			RRUI::DrawRing(Canvas, Center, Pip, 2.5f * S, Faded(RRUI::TeamColorDeep(Team), In));
			if (Index < State.GetRoundWins(Team))
			{
				// The one just won pops in
				const bool bNew = Team == Winner && Index == State.GetRoundWins(Team) - 1;
				const float Grow = bNew ? Ease((Since - 0.4f) / 0.3f) : 1.f;
				RRUI::DrawDisc(Canvas, Center, (Pip - 2.f * S) * Grow, Faded(RRUI::TeamColorDeep(Team), In));
			}
		}
	}
	// What comes next
	const bool bDecided = State.GetRoundWins(Winner) >= RRRounds::RoundsToWin;
	const FString Next = bDecided ? FString::Printf(TEXT("%s WINS THE MATCH"), Winner == 0 ? TEXT("BLUE") : TEXT("RED"))
		: FString::Printf(TEXT("ROUND %d IN %d"), State.GetRound() + 1, FMath::Max(1, FMath::CeilToInt(RRRounds::RoundOverTime - Since)));
	Centered(Canvas, Next, CenterX, Line + Pip * 2.f + 16.f * S, 13.f * S, EWeight::ExtraBold, Faded(bDecided ? RRUI::Gold : RRUI::InkSoft, In), 13.f * S * 0.2f, true);
}

void ARRRandomHUD::DrawCards(const ARRRandomCharacter& Viewer)
{
	const URRRandomDiceBuffComponent* Dice = Viewer.GetDiceBuffs();
	if (!Dice || !Dice->HasCards())
	{
		return;
	}
	const TArray<ERRCard>& Cards = Dice->GetCards();
	const int32 Face = Dice->GetCardFace();
	const float S = UIScale;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Width = Cards.Num() * CardW * S + (Cards.Num() - 1) * CardGap * S;
	const float Top = Canvas->ClipY - (CardsBottom + CardH) * S;
	const float Bob = FMath::Sin(GetWorld()->GetRealTimeSeconds() * 3.f) * 2.f * S;
	Centered(Canvas, FString::Printf(TEXT("PICK A CARD  ·  DICE %d"), Face), CenterX, Top - 22.f * S, 12.f * S, EWeight::Black, RRUI::Ink, 12.f * S * 0.24f, true);
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		const ERRCard Card = Cards[Index];
		const FLinearColor Color = RRRounds::GetCardColor(Card);
		const float Left = CenterX - Width * 0.5f + Index * (CardW + CardGap) * S;
		const FBox2D Box(FVector2D(Left, Top + (Index == 1 ? Bob : -Bob)), FVector2D(Left + CardW * S, Top + CardH * S + (Index == 1 ? Bob : -Bob)));
		RRUI::DrawSoftShadow(Canvas, Box, 12.f * S, 10.f * S, 4.f * S, RRUI::Shadow);
		RRUI::DrawRounded(Canvas, Box, 12.f * S, RRUI::White);
		RRUI::DrawRounded(Canvas, FBox2D(Box.Min, FVector2D(Box.Max.X, Box.Min.Y + 30.f * S)), 12.f * S, RRUI::Pastel(Color, 0.15f));
		// The key, the name, the die's strength, what it does
		RRUI::DrawKey(Canvas, FString::FromInt(Index + 1), FVector2D(Box.Min.X + 8.f * S, Box.Min.Y + 6.f * S), 18.f * S);
		RRUI::DrawText(Canvas, RRRounds::GetCardName(Card), FVector2D(Box.Min.X + 32.f * S, Box.Min.Y + 7.f * S), 15.f * S, EWeight::Black, RRUI::Ink, 15.f * S * 0.1f);
		const FString Line = RRRounds::GetCardLine(Card, Face);
		Centered(Canvas, Line, Box.GetCenter().X, Box.Min.Y + 44.f * S, 11.f * S, EWeight::ExtraBold, RRUI::InkSoft);
		for (int32 Pip = 0; Pip < 6; ++Pip)
		{
			RRUI::DrawDisc(Canvas, FVector2D(Box.GetCenter().X + (Pip - 2.5f) * 10.f * S, Box.Max.Y - 14.f * S), 3.f * S, Pip < Face ? RRUI::Readable(Color) : Faded(RRUI::Ink, 0.12f));
		}
	}
}

void ARRRandomHUD::DrawAbility(const ARRRandomCharacter& Viewer)
{
	const URRRandomKitComponent* Kit = Viewer.GetKit();
	const FRRKit& Own = RRKit::Get(Viewer.GetSpecies());
	if (!Kit || Own.Ability == ERRAbility::None)
	{
		return;
	}
	// A round button left of the ammo panel: Q, the cooldown sweeping round it, the name under it
	const float S = UIScale;
	const float Radius = 26.f * S;
	const FVector2D Center(Canvas->ClipX - 300.f * S, Canvas->ClipY - 62.f * S);
	const float Left = Kit->GetAbilityCooldown();
	const float Max = FMath::Max(0.1f, Kit->GetAbilityCooldownMax());
	const bool bReady = Left <= 0.f;
	RRUI::DrawDisc(Canvas, Center, Radius + 3.f * S, RRUI::White);
	RRUI::DrawDisc(Canvas, Center, Radius, bReady ? RRUI::Pastel(Own.DieColor, 0.2f) : Faded(RRUI::Ink, 0.12f));
	RRUI::DrawDashedRing(Canvas, Center, Radius + 1.f * S, 4.f * S, 24, 1.f - Left / Max, bReady ? RRUI::Gold : RRUI::Readable(Own.DieColor));
	const FString Key = bReady ? TEXT("Q") : FString::FromInt(FMath::CeilToInt(Left));
	const FVector2D KeyBox = RRUI::MeasureText(Key, 22.f * S, EWeight::Black);
	RRUI::DrawText(Canvas, Key, Center - KeyBox * 0.5f, 22.f * S, EWeight::Black, bReady ? RRUI::Ink : RRUI::InkSoft);
	Centered(Canvas, RRRounds::GetAbilityName(Own.Ability), Center.X, Center.Y + Radius + 5.f * S, 9.f * S, EWeight::Black, bReady ? RRUI::Ink : RRUI::InkSoft, 9.f * S * 0.2f, true);
	// The red fox disguised: a chip over it
	if (Kit->IsDisguised())
	{
		Centered(Canvas, FString::Printf(TEXT("DISGUISED %ds"), FMath::CeilToInt(Kit->GetDisguiseRemaining())), Center.X, Center.Y - Radius - 22.f * S, 10.f * S, EWeight::Black, RRUI::PinkDeep, 10.f * S * 0.2f, true);
	}
}

void ARRRandomHUD::DrawKnockedOutBy(const ARRRandomCharacter& Viewer)
{
	const float S = UIScale;
	const float CenterX = Canvas->ClipX * 0.5f;
	float Y = Canvas->ClipY * 0.28f + 80.f * S;
	if (const ARRRandomCharacter* Killer = Viewer.GetLastKiller())
	{
		const FString By = FString::Printf(TEXT("KO BY %s  ·  %s"), *CharacterLabel(*Killer, &Viewer), *RRSpecies::GetDisplayName(Killer->GetSpecies()));
		Centered(Canvas, By, CenterX, Y, 15.f * S, EWeight::Black, RRUI::TeamColorDeep(Killer->GetTeam()), 15.f * S * 0.16f, true);
		Y += 22.f * S;
	}
	if (Viewer.GetRespawnQuick() > 0.05f)
	{
		Centered(Canvas, FString::Printf(TEXT("+%.1fs  QUICK KO"), Viewer.GetRespawnQuick()), CenterX, Y, 11.f * S, EWeight::ExtraBold, RRUI::PinkDeep, 11.f * S * 0.2f, true);
		Y += 16.f * S;
	}
	// The wait as a bar
	const float Total = FMath::Max(0.1f, Viewer.GetRespawnTotal());
	const FBox2D Bar(FVector2D(CenterX - 90.f * S, Y + 4.f * S), FVector2D(CenterX + 90.f * S, Y + 9.f * S));
	RRUI::DrawBar(Canvas, Bar, 1.f - Viewer.GetRespawnRemaining() / Total, Faded(RRUI::White, 0.8f), RRUI::Lavender);
}

void ARRRandomHUD::DrawEmoteWheel(const ARRRandomCharacter& Viewer)
{
	if (!bEmoteWheel)
	{
		return;
	}
	const float S = UIScale;
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const int32 Pick = GetEmotePick();
	RRUI::DrawDisc(Canvas, Center, WheelRadius * S + 34.f * S, Faded(RRUI::White, 0.55f));
	for (int32 Index = 1; Index <= ARRRandomCharacter::EmoteCount; ++Index)
	{
		const float Angle = FMath::DegreesToRadians((Index - 1) * 360.f / ARRRandomCharacter::EmoteCount);
		const FVector2D Spot = Center + FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * WheelRadius * S;
		const bool bHas = Viewer.HasEmote(Index);
		const bool bPicked = Index == Pick;
		RRUI::DrawDisc(Canvas, Spot, (bPicked ? 34.f : 28.f) * S, bPicked ? RRUI::Pink : RRUI::White);
		const FString Name = ARRRandomCharacter::GetEmoteName(Index);
		const FVector2D Box = RRUI::MeasureText(Name, 11.f * S, EWeight::Black, 11.f * S * 0.1f);
		RRUI::DrawText(Canvas, Name, Spot - Box * 0.5f, 11.f * S, EWeight::Black, bHas ? RRUI::Ink : Faded(RRUI::Ink, 0.3f), 11.f * S * 0.1f);
	}
	Centered(Canvas, TEXT("EMOTE"), Center.X, Center.Y - 8.f * S, 12.f * S, EWeight::Black, RRUI::InkSoft, 12.f * S * 0.3f);
	// Where the mouse points
	RRUI::DrawDisc(Canvas, Center + EmoteAim * 0.5f * S, 5.f * S, RRUI::PinkDeep);
}

void ARRRandomHUD::DrawRoundsResults(const ARRRandomGameState& State, const ARRRandomCharacter* Viewer, float Fade)
{
	const float S = UIScale;
	const float ScreenW = Canvas->ClipX;
	const float ScreenH = Canvas->ClipY;
	const float CenterX = ScreenW * 0.5f;
	// The next match by itself, and CHANGE FRIEND over the buttons
	const bool bHost = GetWorld()->GetAuthGameMode() != nullptr;
	const float Age = GetResultsAge();
	const float ButtonW = 170.f * S;
	const float ButtonH = 36.f * S;
	const float Top = ScreenH - 60.f * S - 46.f * S - ButtonH - 14.f * S;
	ResultsButtons.Add({ ERRResultsButton::ChangeFriend, FBox2D(FVector2D(CenterX - ButtonW * 0.5f, Top), FVector2D(CenterX + ButtonW * 0.5f, Top + ButtonH)), true });
	DrawMenuButton(ResultsButtons.Last(), bFriendPick ? TEXT("CLOSE") : TEXT("CHANGE FRIEND"), false, Fade, S);
	if (bHost && !bFriendPick && Age >= 0.f)
	{
		const int32 Left = FMath::Max(0, FMath::CeilToInt(RRRounds::NextMatchDelay - Age));
		Centered(Canvas, FString::Printf(TEXT("NEXT MATCH IN %d"), Left), CenterX, Top - 22.f * S, 13.f * S, EWeight::Black, Faded(RRUI::LavenderDeep, Fade), 13.f * S * 0.2f, true);
	}
	if (!bFriendPick)
	{
		return;
	}
	// The row of animal friends (the one played now ringed in gold): a click picks for the next match
	const float Card = 74.f * S;
	const float Gap = 10.f * S;
	const float RowW = RRSpecies::Count * Card + (RRSpecies::Count - 1) * Gap;
	const float RowTop = Top - Card - 40.f * S;
	RRUI::DrawPanel(Canvas, FBox2D(FVector2D(CenterX - RowW * 0.5f - 14.f * S, RowTop - 26.f * S), FVector2D(CenterX + RowW * 0.5f + 14.f * S, RowTop + Card + 30.f * S)), 18.f * S, S, Faded(RRUI::Panel, Fade));
	Centered(Canvas, TEXT("PICK YOUR ANIMAL FRIEND"), CenterX, RowTop - 20.f * S, 11.f * S, EWeight::Black, Faded(RRUI::Ink, Fade), 11.f * S * 0.24f);
	float MouseX = -1.f;
	float MouseY = -1.f;
	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	const TArray<FRRSpeciesLook>& Looks = FRRSpeciesLook::GetAll();
	for (int32 Species = 0; Species < RRSpecies::Count; ++Species)
	{
		const float Left = CenterX - RowW * 0.5f + Species * (Card + Gap);
		const FBox2D Box(FVector2D(Left, RowTop), FVector2D(Left + Card, RowTop + Card));
		const bool bHover = Box.IsInside(FVector2D(MouseX, MouseY));
		const bool bCurrent = Viewer && Viewer->GetSpecies() == Species;
		RRUI::DrawRounded(Canvas, Box.ExpandBy(3.f * S), 14.f * S, Faded(bCurrent ? RRUI::Gold : bHover ? RRUI::Pink : RRUI::White, Fade));
		if (Looks.IsValidIndex(Species) && Looks[Species].Portrait)
		{
			RRUI::DrawTextureRounded(Canvas, Looks[Species].Portrait, Box, 12.f * S, Faded(FLinearColor::White, Fade));
		}
		Centered(Canvas, RRSpecies::GetDisplayName(Species), Box.GetCenter().X, Box.Max.Y + 4.f * S, 9.f * S, EWeight::Black, Faded(RRUI::Ink, Fade), 9.f * S * 0.1f);
		FResultsButtonRect& Button = ResultsButtons.AddDefaulted_GetRef();
		Button.Button = ERRResultsButton::Friend;
		Button.Rect = Box;
		Button.Value = Species;
	}
}
