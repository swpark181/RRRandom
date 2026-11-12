// ARRRandomHUD, continued (round 97): the battle royale's bag and hands, the get-ready time, the team roll, the start spots on
// the map and the ultimate die's meteor.
#include "RRRandomHUD.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomGameState.h"
#include "RRRandomLoadout.h"
#include "RRRandomPlayerController.h"
#include "RRRandomUIStyle.h"
#include "RRRandomUltimateMeteor.h"
#include "RRRandomWeapon.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"

using RRUI::EWeight;

namespace
{
	// The hands, bottom middle
	constexpr float HandWidth = 156.f;
	constexpr float HandHeight = 46.f;
	constexpr float HandGap = 10.f;
	constexpr float HandBottom = 18.f;
	constexpr float HandLift = 7.f;
	// The bag's news over the hands lasts this long
	constexpr float NewsTime = 2.6f;
	// The bag's panel: cards in this many columns, this tall
	constexpr int32 BagColumns = 5;
	constexpr float BagCardHeight = 64.f;
	constexpr float BagGap = 10.f;
	constexpr float BagPad = 22.f;
	// The team roll: the dice tumble this long before the sums and places show
	constexpr float RollTumble = 2.2f;

	FLinearColor TierTint(ERRWeaponTier T)
	{
		return FRRWeapon::GetTierColor(T);
	}

	/** A tier's name color on white: the common grey would be too pale, so ink */
	FLinearColor TierText(ERRWeaponTier T)
	{
		return T == ERRWeaponTier::Common ? RRUI::Ink : RRUI::Readable(FRRWeapon::GetTierColor(T));
	}

	FString GunLabel(const FRRBagItem& Item)
	{
		return FRRWeapon::GetTypeName(Item.Type) == TEXT("FINGER") ? FString(TEXT("FOAM FINGER")) : FRRWeapon::GetTypeName(Item.Type);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The hands

void ARRRandomHUD::DrawHands(const ARRRandomCharacter& Viewer)
{
	const URRRandomLoadoutComponent* Bag = Viewer.GetLoadout();
	if (!Bag || !Bag->IsInUse())
	{
		return;
	}
	const float S = UIScale;
	const float Now = GetWorld()->GetTimeSeconds();
	const TArray<FRRBagItem>& Items = Bag->GetItems();

	// The bag's news: a gun in, a combine, a full bag
	if (Bag->GetAddCount() != SeenBagAdds)
	{
		SeenBagAdds = Bag->GetAddCount();
		BagNews = FString::Printf(TEXT("+ %s %s"), *FRRWeapon::GetTierName(Bag->GetLastAdded().Tier), *GunLabel(Bag->GetLastAdded()));
		BagNewsColor = TierText(Bag->GetLastAdded().Tier);
		BagNewsTime = Now;
	}
	if (Bag->GetCombineCount() != SeenBagCombines)
	{
		SeenBagCombines = Bag->GetCombineCount();
		BagNews = FString::Printf(TEXT("UPGRADE! %s %s"), *FRRWeapon::GetTierName(Bag->GetLastCombined().Tier), *GunLabel(Bag->GetLastCombined()));
		BagNewsColor = TierText(Bag->GetLastCombined().Tier);
		BagNewsTime = Now;
	}
	if (Bag->GetFullCount() != SeenBagFull)
	{
		SeenBagFull = Bag->GetFullCount();
		BagNews = TEXT("BAG FULL");
		BagNewsColor = RRUI::PinkDeep;
		BagNewsTime = Now;
	}

	const float Width = HandWidth * S;
	const float Height = HandHeight * S;
	const float Left = Canvas->ClipX * 0.5f - Width - HandGap * S * 0.5f;
	const float Bottom = Canvas->ClipY - HandBottom * S;
	for (int32 Hand = 0; Hand < URRRandomLoadoutComponent::HandCount; ++Hand)
	{
		const bool bActive = Bag->GetActiveHand() == Hand;
		const int32 Item = Bag->GetHand(Hand);
		const bool bHas = Items.IsValidIndex(Item);
		const float Lift = bActive ? HandLift * S : 0.f;
		const FBox2D Card(FVector2D(Left + Hand * (Width + HandGap * S), Bottom - Height - Lift), FVector2D(Left + Hand * (Width + HandGap * S) + Width, Bottom - Lift));
		RRUI::DrawPanel(Canvas, Card, 14.f * S, S, RRUI::WithAlpha(RRUI::White, bActive ? 0.95f : 0.62f));
		if (bActive)
		{
			RRUI::DrawPill(Canvas, FBox2D(FVector2D(Card.Min.X + 12.f * S, Card.Max.Y - 4.f * S), FVector2D(Card.Max.X - 12.f * S, Card.Max.Y - 1.5f * S)),
				bHas ? TierTint(Items[Item].Tier) : RRUI::Mint);
		}
		const float KeyWidth = RRUI::DrawKey(Canvas, FString::FromInt(Hand + 1), FVector2D(Card.Min.X + 10.f * S, Card.GetCenter().Y - 9.f * S), 18.f * S, bActive ? 1.f : 0.7f);
		const float TextX = Card.Min.X + 16.f * S + KeyWidth;
		const FString Name = bHas ? GunLabel(Items[Item]) : FString(TEXT("PISTOL"));
		const float NameSize = 12.f * S;
		RRUI::DrawText(Canvas, Name, FVector2D(TextX, Card.Min.Y + 8.f * S), NameSize, EWeight::Black, bHas ? TierText(Items[Item].Tier) : RRUI::InkSoft, NameSize * 0.04f);
		// Its tier: a dot each, and the name
		const ERRWeaponTier ItemTier = bHas ? Items[Item].Tier : ERRWeaponTier::Common;
		const float DotY = Card.Min.Y + 30.f * S;
		for (int32 Dot = 0; Dot < FRRWeapon::GetTierStars(ItemTier); ++Dot)
		{
			RRUI::DrawDisc(Canvas, FVector2D(TextX + 4.f * S + Dot * 10.f * S, DotY + 2.f * S), 3.6f * S, bHas ? TierTint(ItemTier) : RRUI::WithAlpha(RRUI::InkSoft, 0.4f));
		}
		if (bHas)
		{
			RRUI::DrawText(Canvas, FRRWeapon::GetTierName(ItemTier), FVector2D(TextX + 8.f * S + FRRWeapon::GetTierStars(ItemTier) * 10.f * S, DotY - 4.f * S), 8.f * S, EWeight::ExtraBold,
				RRUI::InkSoft, 8.f * S * 0.1f);
		}
	}
	// Over them: the bag's key and count, and its news
	const FString BagLine = FString::Printf(TEXT("BAG %d/%d"), Items.Num(), URRRandomLoadoutComponent::Capacity);
	const float LineSize = 9.f * S;
	const float LineY = Bottom - Height - HandLift * S - 20.f * S;
	const float KeyWidth = RRUI::DrawKey(Canvas, TEXT("TAB"), FVector2D(Canvas->ClipX * 0.5f - 46.f * S, LineY), 15.f * S, 0.9f);
	RRUI::DrawGlowText(Canvas, BagLine, FVector2D(Canvas->ClipX * 0.5f - 40.f * S + KeyWidth, LineY + 2.f * S), LineSize, EWeight::Black, RRUI::Ink, LineSize * 0.1f);
	if (Bag->CanCombineAny())
	{
		RRUI::DrawGlowText(Canvas, TEXT("UPGRADE READY!"), FVector2D(Canvas->ClipX * 0.5f + 60.f * S, LineY + 2.f * S), LineSize, EWeight::Black, RRUI::Readable(RRUI::Gold), LineSize * 0.1f);
	}
	const float Since = Now - BagNewsTime;
	if (Since >= 0.f && Since < NewsTime)
	{
		const float Alpha = 1.f - FMath::SmoothStep(NewsTime - 0.6f, NewsTime, Since);
		const float Pop = 1.f + 0.3f * FMath::Max(0.f, 1.f - Since / 0.15f);
		const float Size = 15.f * S * Pop;
		const FVector2D Box = RRUI::MeasureText(BagNews, Size, EWeight::Black, Size * 0.06f);
		RRUI::DrawGlowText(Canvas, BagNews, FVector2D(Canvas->ClipX * 0.5f - Box.X * 0.5f, LineY - 10.f * S - Box.Y - Since * 10.f * S), Size, EWeight::Black,
			RRUI::WithAlpha(BagNewsColor, Alpha), Size * 0.06f);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The bag's panel

void ARRRandomHUD::DrawBag(const ARRRandomCharacter& Viewer)
{
	BagHits.Reset();
	const URRRandomLoadoutComponent* Bag = Viewer.GetLoadout();
	if (!Bag || !Bag->IsInUse())
	{
		bBagOpen = false;
		return;
	}
	const float S = UIScale;
	float MouseX = -1.f;
	float MouseY = -1.f;
	if (PlayerOwner)
	{
		PlayerOwner->GetMousePosition(MouseX, MouseY);
	}
	const FVector2D Mouse(MouseX, MouseY);
	const TArray<FRRBagItem>& Items = Bag->GetItems();

	// A veil, and the panel in the middle
	RRUI::DrawBox(Canvas, FBox2D(FVector2D::ZeroVector, FVector2D(Canvas->ClipX, Canvas->ClipY)), FLinearColor::FromSRGBColor(FColor(244, 240, 252, 120)));
	const float Width = FMath::Min(Canvas->ClipX - 40.f * S, 800.f * S);
	const int32 Rows = FMath::Max(1, FMath::DivideAndRoundUp(URRRandomLoadoutComponent::Capacity, BagColumns));
	const float CardW = (Width - BagPad * 2.f * S - BagGap * S * (BagColumns - 1)) / BagColumns;
	const float CardH = BagCardHeight * S;
	const float HandsH = 64.f * S;
	const float Height = BagPad * S + 34.f * S + HandsH + 24.f * S + Rows * CardH + (Rows - 1) * BagGap * S + 60.f * S;
	const FVector2D Min((Canvas->ClipX - Width) * 0.5f, FMath::Max(20.f * S, (Canvas->ClipY - Height) * 0.5f));
	const FBox2D Panel(Min, Min + FVector2D(Width, Height));
	RRUI::DrawPanel(Canvas, Panel, 24.f * S, S, RRUI::WithAlpha(RRUI::White, 0.96f));

	// Title, count, how it works
	float Y = Panel.Min.Y + BagPad * S;
	const float TitleSize = 20.f * S;
	const FVector2D Title = RRUI::DrawText(Canvas, TEXT("BAG"), FVector2D(Panel.Min.X + BagPad * S, Y), TitleSize, EWeight::Black, RRUI::Ink, TitleSize * 0.08f);
	RRUI::DrawText(Canvas, FString::Printf(TEXT("%d / %d"), Items.Num(), URRRandomLoadoutComponent::Capacity), FVector2D(Panel.Min.X + BagPad * S + Title.X + 12.f * S, Y + 6.f * S),
		11.f * S, EWeight::ExtraBold, RRUI::InkSoft);
	const FString Help = TEXT("LEFT CLICK: HAND 1   RIGHT CLICK: HAND 2   3 OF A KIND: UPGRADE");
	const float HelpSize = 9.f * S;
	const FVector2D HelpBox = RRUI::MeasureText(Help, HelpSize, EWeight::ExtraBold, HelpSize * 0.08f);
	RRUI::DrawText(Canvas, Help, FVector2D(Panel.Max.X - BagPad * S - HelpBox.X - 30.f * S, Y + 8.f * S), HelpSize, EWeight::ExtraBold, RRUI::InkSoft, HelpSize * 0.08f);
	// The close button
	const FBox2D Close(FVector2D(Panel.Max.X - BagPad * S - 20.f * S, Y + 2.f * S), FVector2D(Panel.Max.X - BagPad * S, Y + 22.f * S));
	RRUI::DrawDisc(Canvas, Close.GetCenter(), 10.f * S, Close.IsInside(Mouse) ? RRUI::Pink : RRUI::WithAlpha(RRUI::InkSoft, 0.25f));
	RRUI::DrawText(Canvas, TEXT("X"), Close.GetCenter() - FVector2D(3.5f, 6.f) * S, 10.f * S, EWeight::Black, RRUI::White);
	BagHits.Add({ Close, EBagHit::Close, INDEX_NONE });
	Y += 34.f * S;

	// The two hands: click one to use it
	const float HandW = (Width - BagPad * 2.f * S - BagGap * S) * 0.5f;
	for (int32 Hand = 0; Hand < URRRandomLoadoutComponent::HandCount; ++Hand)
	{
		const FBox2D Box(FVector2D(Panel.Min.X + BagPad * S + Hand * (HandW + BagGap * S), Y), FVector2D(Panel.Min.X + BagPad * S + Hand * (HandW + BagGap * S) + HandW, Y + HandsH));
		const int32 Item = Bag->GetHand(Hand);
		const bool bHas = Items.IsValidIndex(Item);
		const bool bActive = Bag->GetActiveHand() == Hand;
		const FLinearColor Mark = bHas ? TierTint(Items[Item].Tier) : RRUI::Mint;
		RRUI::DrawRounded(Canvas, Box, 14.f * S, RRUI::WithAlpha(RRUI::Pastel(Mark, 0.82f), Box.IsInside(Mouse) ? 1.f : 0.85f));
		if (bActive)
		{
			RRUI::DrawRing(Canvas, FVector2D(Box.Min.X + 18.f * S, Box.GetCenter().Y), 11.f * S, 2.5f * S, RRUI::Readable(Mark));
		}
		RRUI::DrawText(Canvas, FString::FromInt(Hand + 1), FVector2D(Box.Min.X + 13.5f * S, Box.GetCenter().Y - 8.f * S), 13.f * S, EWeight::Black, RRUI::Ink);
		const FString Name = bHas ? FString::Printf(TEXT("%s  %s"), *GunLabel(Items[Item]), *FRRWeapon::GetTierName(Items[Item].Tier)) : FString(TEXT("EMPTY (PISTOL)"));
		RRUI::DrawText(Canvas, Name, FVector2D(Box.Min.X + 38.f * S, Box.Min.Y + 12.f * S), 14.f * S, EWeight::Black, bHas ? TierText(Items[Item].Tier) : RRUI::InkSoft, 14.f * S * 0.04f);
		RRUI::DrawText(Canvas, bActive ? TEXT("IN USE") : TEXT("CLICK TO USE"), FVector2D(Box.Min.X + 38.f * S, Box.Min.Y + 36.f * S), 8.5f * S, EWeight::ExtraBold, RRUI::InkSoft, 8.5f * S * 0.1f);
		BagHits.Add({ Box, EBagHit::Hand, Hand });
	}
	Y += HandsH + 24.f * S;

	// The cards: every gun in the bag (sorted by gun then tier); UPGRADE on the first of three alike
	TSet<int32> Offered;
	for (int32 Index = 0; Index < URRRandomLoadoutComponent::Capacity; ++Index)
	{
		const int32 Column = Index % BagColumns;
		const int32 Row = Index / BagColumns;
		const FBox2D Card(FVector2D(Panel.Min.X + BagPad * S + Column * (CardW + BagGap * S), Y + Row * (CardH + BagGap * S)),
			FVector2D(Panel.Min.X + BagPad * S + Column * (CardW + BagGap * S) + CardW, Y + Row * (CardH + BagGap * S) + CardH));
		if (!Items.IsValidIndex(Index))
		{
			RRUI::DrawRounded(Canvas, Card, 12.f * S, RRUI::WithAlpha(RRUI::Lavender, 0.18f));
			continue;
		}
		const FRRBagItem& Item = Items[Index];
		const FLinearColor Mark = TierTint(Item.Tier);
		const bool bOver = Card.IsInside(Mouse);
		RRUI::DrawSoftShadow(Canvas, Card, 12.f * S, 6.f * S, 2.f * S, RRUI::WithAlpha(RRUI::Shadow, 0.5f));
		RRUI::DrawRounded(Canvas, Card, 12.f * S, bOver ? RRUI::Pastel(Mark, 0.7f) : RRUI::White);
		RRUI::DrawRounded(Canvas, FBox2D(Card.Min, FVector2D(Card.Min.X + 6.f * S, Card.Max.Y)), 3.f * S, Mark);
		RRUI::DrawText(Canvas, GunLabel(Item), FVector2D(Card.Min.X + 14.f * S, Card.Min.Y + 9.f * S), 11.f * S, EWeight::Black, TierText(Item.Tier), 11.f * S * 0.04f);
		for (int32 Dot = 0; Dot < FRRWeapon::GetTierStars(Item.Tier); ++Dot)
		{
			RRUI::DrawDisc(Canvas, FVector2D(Card.Min.X + 18.f * S + Dot * 9.f * S, Card.Min.Y + 31.f * S), 3.2f * S, Mark);
		}
		RRUI::DrawText(Canvas, FRRWeapon::GetTierName(Item.Tier), FVector2D(Card.Min.X + 18.f * S + FRRWeapon::GetTierStars(Item.Tier) * 9.f * S, Card.Min.Y + 25.f * S), 8.f * S,
			EWeight::ExtraBold, RRUI::InkSoft, 8.f * S * 0.1f);
		// In a hand: its number
		const int32 Hand = Bag->GetHandOf(Index);
		if (Hand != INDEX_NONE)
		{
			const FVector2D Badge(Card.Max.X - 14.f * S, Card.Min.Y + 14.f * S);
			RRUI::DrawDisc(Canvas, Badge, 9.f * S, RRUI::Ink);
			RRUI::DrawText(Canvas, FString::FromInt(Hand + 1), Badge - FVector2D(3.f, 6.f) * S, 10.f * S, EWeight::Black, RRUI::White);
		}
		else
		{
			// Throw it away (only with the mouse over the card)
			const FBox2D Discard(FVector2D(Card.Max.X - 22.f * S, Card.Min.Y + 4.f * S), FVector2D(Card.Max.X - 6.f * S, Card.Min.Y + 20.f * S));
			if (bOver)
			{
				RRUI::DrawDisc(Canvas, Discard.GetCenter(), 8.f * S, Discard.IsInside(Mouse) ? RRUI::PinkDeep : RRUI::WithAlpha(RRUI::InkSoft, 0.35f));
				RRUI::DrawText(Canvas, TEXT("x"), Discard.GetCenter() - FVector2D(3.f, 6.5f) * S, 10.f * S, EWeight::Black, RRUI::White);
			}
			BagHits.Add({ Card, EBagHit::Card, Index });
			BagHits.Add({ Discard, EBagHit::Discard, Index });
		}
		if (Hand != INDEX_NONE)
		{
			BagHits.Add({ Card, EBagHit::Card, Index });
		}
		// Three alike: UPGRADE on the first of them
		const int32 Key = Item.Type * 8 + static_cast<int32>(Item.Tier);
		if (Bag->CanCombine(Item) && !Offered.Contains(Key))
		{
			Offered.Add(Key);
			const FString Label = FString::Printf(TEXT("UPGRADE x%d"), URRRandomLoadoutComponent::CombineCount);
			const float Size = 8.5f * S;
			const FVector2D LabelBox = RRUI::MeasureText(Label, Size, EWeight::Black, Size * 0.08f);
			const FBox2D Button(FVector2D(Card.Max.X - LabelBox.X - 18.f * S, Card.Max.Y - LabelBox.Y - 12.f * S), FVector2D(Card.Max.X - 6.f * S, Card.Max.Y - 5.f * S));
			const FLinearColor Next = TierTint(static_cast<ERRWeaponTier>(static_cast<int32>(Item.Tier) + 1));
			RRUI::DrawPill(Canvas, Button, Button.IsInside(Mouse) ? Next : RRUI::Pastel(Next, 0.3f));
			RRUI::DrawText(Canvas, Label, FVector2D(Button.Min.X + 6.f * S, Button.Min.Y + 3.f * S), Size, EWeight::Black, RRUI::White, Size * 0.08f);
			BagHits.Add({ Button, EBagHit::Combine, Index });
		}
	}

	// Every upgrade there is, at once
	if (Bag->CanCombineAny())
	{
		const FString Label = TEXT("UPGRADE ALL");
		const float Size = 12.f * S;
		const FVector2D LabelBox = RRUI::MeasureText(Label, Size, EWeight::Black, Size * 0.08f);
		const FBox2D Button(FVector2D(Panel.GetCenter().X - LabelBox.X * 0.5f - 22.f * S, Panel.Max.Y - 46.f * S), FVector2D(Panel.GetCenter().X + LabelBox.X * 0.5f + 22.f * S, Panel.Max.Y - 16.f * S));
		RRUI::DrawPill(Canvas, Button, Button.IsInside(Mouse) ? RRUI::Gold : RRUI::Pastel(RRUI::Gold, 0.25f));
		RRUI::DrawText(Canvas, Label, FVector2D(Button.GetCenter().X - LabelBox.X * 0.5f, Button.GetCenter().Y - LabelBox.Y * 0.5f), Size, EWeight::Black, RRUI::White, Size * 0.08f);
		BagHits.Add({ Button, EBagHit::CombineAll, INDEX_NONE });
	}
}

bool ARRRandomHUD::HandleBagClick(const FVector2D& ScreenPoint, bool bRight)
{
	const ARRRandomCharacter* Mine = PlayerOwner ? Cast<ARRRandomCharacter>(PlayerOwner->GetPawn()) : nullptr;
	URRRandomLoadoutComponent* Bag = Mine ? Mine->GetLoadout() : nullptr;
	if (!Bag)
	{
		return false;
	}
	// The last drawn first (buttons over cards)
	for (int32 Hit = BagHits.Num() - 1; Hit >= 0; --Hit)
	{
		const FBagHit& Each = BagHits[Hit];
		if (!Each.Rect.IsInside(ScreenPoint))
		{
			continue;
		}
		const TArray<FRRBagItem>& Items = Bag->GetItems();
		switch (Each.Kind)
		{
		case EBagHit::Close:
			bBagOpen = false;
			break;
		case EBagHit::Hand:
			Bag->ServerSelectHand(Each.Index);
			break;
		case EBagHit::CombineAll:
			Bag->ServerCombineAll();
			break;
		case EBagHit::Combine:
			if (Items.IsValidIndex(Each.Index))
			{
				Bag->ServerCombine(Items[Each.Index].Type, Items[Each.Index].Tier);
			}
			break;
		case EBagHit::Discard:
			Bag->ServerDiscard(Each.Index);
			break;
		case EBagHit::Card:
			Bag->ServerSetHand(bRight ? 1 : 0, Each.Index);
			break;
		}
		return true;
	}
	return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// The get-ready time and the team roll

void ARRRandomHUD::DrawPrep(const ARRRandomGameState& State, const ARRRandomCharacter* Viewer)
{
	const float S = UIScale;
	const float CenterX = Canvas->ClipX * 0.5f;
	if (State.IsPrep())
	{
		// The banner: GET READY and the time left (bigger in the last ten seconds), what to do
		const float Left = FMath::Max(0.f, State.GetPrepRemaining());
		const int32 Seconds = FMath::CeilToInt(Left);
		const bool bLast = Left <= 10.f;
		const FString Time = FString::Printf(TEXT("%d:%02d"), Seconds / 60, Seconds % 60);
		const float Top = 74.f * S;
		const float TitleSize = 13.f * S;
		const FString Title = TEXT("GET READY  ·  NO FIGHTING");
		const FVector2D TitleBox = RRUI::MeasureText(Title, TitleSize, EWeight::Black, TitleSize * 0.14f);
		RRUI::DrawGlowText(Canvas, Title, FVector2D(CenterX - TitleBox.X * 0.5f, Top), TitleSize, EWeight::Black, RRUI::Readable(RRUI::Mint), TitleSize * 0.14f);
		const float Pop = bLast ? 1.f + 0.35f * FMath::Pow(Left - FMath::FloorToFloat(Left), 4.f) : 1.f;
		const float TimeSize = (bLast ? 46.f : 30.f) * S * Pop;
		const FVector2D TimeBox = RRUI::MeasureText(Time, TimeSize, EWeight::Black);
		RRUI::DrawGlowText(Canvas, Time, FVector2D(CenterX - TimeBox.X * 0.5f, Top + TitleBox.Y + 2.f * S), TimeSize, EWeight::Black, bLast ? RRUI::Readable(RRUI::Pink) : RRUI::Ink);
		// What to do: roll the dice, the bag, where to start
		const int32 Dice = Viewer ? Viewer->GetDiceBuffs()->GetCharges() : 0;
		const float LineSize = 10.f * S;
		const float LineY = Top + TitleBox.Y + TimeBox.Y + 10.f * S;
		const FString Roll = Dice > 0 ? FString::Printf(TEXT("ROLL FOR WEAPONS · %d DICE LEFT"), Dice) : FString(TEXT("ALL DICE ROLLED"));
		const FString Rest = TEXT("BAG");
		const FString Pick = TEXT("PICK WHERE TO START");
		float X = CenterX - 210.f * S;
		X += RRUI::DrawKey(Canvas, TEXT("E"), FVector2D(X, LineY - 2.f * S), 16.f * S) + 6.f * S;
		X += RRUI::DrawGlowText(Canvas, Roll, FVector2D(X, LineY), LineSize, EWeight::Black, Dice > 0 ? RRUI::Readable(RRUI::Gold) : RRUI::InkSoft, LineSize * 0.1f).X + 16.f * S;
		X += RRUI::DrawKey(Canvas, TEXT("TAB"), FVector2D(X, LineY - 2.f * S), 16.f * S) + 6.f * S;
		X += RRUI::DrawGlowText(Canvas, Rest, FVector2D(X, LineY), LineSize, EWeight::Black, RRUI::Ink, LineSize * 0.1f).X + 16.f * S;
		X += RRUI::DrawKey(Canvas, TEXT("M"), FVector2D(X, LineY - 2.f * S), 16.f * S) + 6.f * S;
		RRUI::DrawGlowText(Canvas, Pick, FVector2D(X, LineY), LineSize, EWeight::Black, RRUI::Ink, LineSize * 0.1f);
		return;
	}
	if (!State.IsTeamRoll())
	{
		return;
	}

	// The team roll: every team's three dice tumbling, then by place, highest sum first (it gets its pick first)
	const float Since = State.GetServerWorldTimeSeconds() - State.GetPrepEndTime();
	const bool bShown = Since >= RollTumble && State.GetTeamOrder(0) > 0;
	const int32 Teams = State.GetBattleTeamCount();
	const float RowH = 30.f * S;
	const float Width = 420.f * S;
	const float Height = 64.f * S + Teams * RowH + 20.f * S;
	const FBox2D Panel(FVector2D(CenterX - Width * 0.5f, 70.f * S), FVector2D(CenterX + Width * 0.5f, 70.f * S + Height));
	RRUI::DrawPanel(Canvas, Panel, 22.f * S, S, RRUI::WithAlpha(RRUI::White, 0.92f));
	const FString Title = bShown ? TEXT("TEAM ROLL · HIGHEST PICKS FIRST") : TEXT("TEAM ROLL!");
	const float TitleSize = 15.f * S;
	const FVector2D TitleBox = RRUI::MeasureText(Title, TitleSize, EWeight::Black, TitleSize * 0.1f);
	RRUI::DrawText(Canvas, Title, FVector2D(CenterX - TitleBox.X * 0.5f, Panel.Min.Y + 18.f * S), TitleSize, EWeight::Black, RRUI::Ink, TitleSize * 0.1f);
	TArray<int32> Rows;
	for (int32 Team = 0; Team < Teams; ++Team)
	{
		Rows.Add(Team);
	}
	if (bShown)
	{
		Rows.Sort([&State](int32 A, int32 B) { return State.GetTeamOrder(A) < State.GetTeamOrder(B); });
	}
	const int32 MyTeam = Viewer ? Viewer->GetTeam() : INDEX_NONE;
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 Row = 0; Row < Rows.Num(); ++Row)
	{
		const int32 Team = Rows[Row];
		const float RowTop = Panel.Min.Y + 52.f * S + Row * RowH;
		const bool bMine = Team == MyTeam;
		if (bMine)
		{
			RRUI::DrawRounded(Canvas, FBox2D(FVector2D(Panel.Min.X + 12.f * S, RowTop), FVector2D(Panel.Max.X - 12.f * S, RowTop + RowH - 4.f * S)), 10.f * S,
				RRUI::WithAlpha(RRUI::Pastel(RRUI::TeamColor(Team), 0.75f), 1.f));
		}
		const float MidY = RowTop + (RowH - 4.f * S) * 0.5f;
		if (bShown)
		{
			RRUI::DrawText(Canvas, FString::Printf(TEXT("#%d"), State.GetTeamOrder(Team)), FVector2D(Panel.Min.X + 22.f * S, MidY - 8.f * S), 13.f * S, EWeight::Black, RRUI::Ink);
		}
		RRUI::DrawDisc(Canvas, FVector2D(Panel.Min.X + 70.f * S, MidY), 7.f * S, RRUI::TeamColor(Team));
		RRUI::DrawText(Canvas, bMine ? FString(TEXT("YOUR TEAM")) : FString::Printf(TEXT("TEAM %d"), Team + 1), FVector2D(Panel.Min.X + 84.f * S, MidY - 7.f * S), 11.f * S,
			EWeight::Black, bMine ? RRUI::Ink : RRUI::InkSoft, 11.f * S * 0.06f);
		int32 Sum = 0;
		for (int32 Die = 0; Die < 3; ++Die)
		{
			const int32 Face = bShown ? State.GetTeamDie(Team, Die) : 1 + (int32(Now * 14.f) + Team * 3 + Die * 5) % 6;
			Sum += Face;
			DrawDieFace(FVector2D(Panel.Min.X + 230.f * S + Die * 30.f * S, MidY), 22.f * S, Face, 1.f);
		}
		if (bShown)
		{
			RRUI::DrawText(Canvas, FString::Printf(TEXT("= %d"), Sum), FVector2D(Panel.Min.X + 330.f * S, MidY - 8.f * S), 13.f * S, EWeight::Black, RRUI::Ink);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The start spots on the map

void ARRRandomHUD::DrawStartSpots(const FBox2D& Rect, TFunctionRef<FVector2D(const FVector&)> ToScreen, const ARRRandomCharacter* Viewer)
{
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (!State || !(State->IsPrep() || State->IsTeamRoll()))
	{
		return;
	}
	const float S = UIScale;
	const float Scale = Rect.GetSize().X / MinimapSize;
	const int32 MyTeam = Viewer ? Viewer->GetTeam() : INDEX_NONE;
	const bool bRolled = State->GetTeamSpot(0) != INDEX_NONE;
	const TArray<FVector_NetQuantize>& Spots = State->GetStartSpots();
	if (bMapExpanded)
	{
		StartSpotHits.Reset();
	}
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.f);
	for (int32 Spot = 0; Spot < Spots.Num(); ++Spot)
	{
		const FVector2D Point = ToScreen(Spots[Spot]);
		if (!Rect.ExpandBy(4.f * S).IsInside(Point))
		{
			continue;
		}
		// Who wants it (before the roll) or has it (after)
		int32 Mine = INDEX_NONE;
		TArray<int32, TInlineAllocator<4>> Others;
		for (int32 Team = 0; Team < State->GetBattleTeamCount(); ++Team)
		{
			const int32 Taken = bRolled ? State->GetTeamSpot(Team) : State->GetTeamPick(Team);
			if (Taken == Spot)
			{
				(Team == MyTeam ? Mine : Others.AddDefaulted_GetRef()) = Team;
			}
		}
		const float Radius = (bMapExpanded ? 5.f : 2.5f) * Scale;
		RRUI::DrawDisc(Canvas, Point, Radius + 1.5f * Scale, RRUI::Ink);
		RRUI::DrawDisc(Canvas, Point, Radius, RRUI::White);
		for (int32 Other = 0; Other < Others.Num(); ++Other)
		{
			const FVector2D Dot = Point + FVector2D(Radius + 3.f * Scale, 0.f).GetRotated(Other * 70.f - 30.f);
			RRUI::DrawDisc(Canvas, Dot, 2.4f * Scale, RRUI::TeamColor(Others[Other]));
		}
		if (Mine != INDEX_NONE)
		{
			const FLinearColor Color = RRUI::TeamColor(MyTeam);
			RRUI::DrawRing(Canvas, Point, Radius + (4.f + 2.f * Pulse) * Scale, 2.f * Scale, Color);
			RRUI::DrawDisc(Canvas, Point, Radius, Color);
			if (bMapExpanded)
			{
				const FString Label = bRolled ? FString::Printf(TEXT("START #%d"), State->GetTeamOrder(MyTeam)) : FString(TEXT("MY PICK"));
				const float Size = 10.f * S;
				const FVector2D Box = RRUI::MeasureText(Label, Size, EWeight::Black, Size * 0.1f);
				RRUI::DrawGlowText(Canvas, Label, FVector2D(Point.X - Box.X * 0.5f, Point.Y - Radius - 10.f * Scale - Box.Y), Size, EWeight::Black, RRUI::Readable(Color), Size * 0.1f);
			}
		}
		if (bMapExpanded)
		{
			StartSpotHits.Add(TPair<int32, FVector2D>(Spot, Point));
		}
	}
	if (bMapExpanded && State->IsPrep())
	{
		const FString Hint = TEXT("CLICK A SPOT TO START THERE · TEAMS ROLL FOR WHO PICKS FIRST");
		const float Size = 11.f * S;
		const FVector2D Box = RRUI::MeasureText(Hint, Size, EWeight::Black, Size * 0.1f);
		RRUI::DrawGlowText(Canvas, Hint, FVector2D(Rect.GetCenter().X - Box.X * 0.5f, Rect.Max.Y + 10.f * S), Size, EWeight::Black, RRUI::Readable(RRUI::Mint), Size * 0.1f);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The ultimate die's meteor

void ARRRandomHUD::DrawUltimateOnMap(TFunctionRef<FVector2D(const FVector&)> ToScreen, float Scale)
{
	const ARRRandomUltimateMeteor* Meteor = ARRRandomUltimateMeteor::Get(this);
	if (!Meteor)
	{
		return;
	}
	const FVector2D Point = ToScreen(Meteor->GetActorLocation());
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f);
	RRUI::DrawRing(Canvas, Point, (7.f + 3.f * Pulse) * Scale, 1.6f * Scale, RRUI::WithAlpha(ARRRandomUltimateMeteor::GetGold(), 0.8f));
	RRUI::DrawDisc(Canvas, Point, 5.f * Scale, RRUI::White);
	RRUI::DrawDisc(Canvas, Point, 3.6f * Scale, ARRRandomUltimateMeteor::GetGold());
}

void ARRRandomHUD::DrawUltimateMeteor(const ARRRandomCharacter* Viewer)
{
	const ARRRandomUltimateMeteor* Meteor = ARRRandomUltimateMeteor::Get(this);
	if (!Meteor)
	{
		return;
	}
	const float S = UIScale;
	const FLinearColor Gold = ARRRandomUltimateMeteor::GetGold();
	const FVector2D InsetMin(70.f * S, 150.f * S);
	const FVector2D InsetMax(70.f * S, 130.f * S);
	const FVector Projected = Project(Meteor->GetActorLocation() + FVector(0.f, 0.f, 300.f));
	FVector2D Point(Projected.X, Projected.Y);
	const bool bBehind = Projected.Z <= 0.f;
	const FBox2D Allowed(InsetMin, FVector2D(Canvas->ClipX, Canvas->ClipY) - InsetMax);
	FVector2D Out(0.f, 1.f);
	if (bBehind || !Allowed.IsInside(Point))
	{
		const FVector2D Middle = Allowed.GetCenter();
		Out = (Point - Middle) * (bBehind ? -1.f : 1.f);
		Out = Out.IsNearlyZero() ? FVector2D(0.f, 1.f) : Out.GetSafeNormal();
		const FVector2D Half = Allowed.GetExtent();
		const float Reach = FMath::Min(Out.X != 0.f ? Half.X / FMath::Abs(Out.X) : UE_BIG_NUMBER, Out.Y != 0.f ? Half.Y / FMath::Abs(Out.Y) : UE_BIG_NUMBER);
		Point = Middle + Out * Reach;
	}
	const float Pulse = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 5.f);
	const float Radius = (15.f + 2.f * Pulse) * S;
	const FVector2D Tip = Point + Out * (Radius + 10.f * S);
	const FVector2D Side(-Out.Y, Out.X);
	RRUI::DrawTriangle(Canvas, Tip, Point + Out * Radius * 0.6f + Side * Radius * 0.55f, Point + Out * Radius * 0.6f - Side * Radius * 0.55f, Gold);
	RRUI::DrawDisc(Canvas, Point, Radius + 2.5f * S, RRUI::White);
	RRUI::DrawDisc(Canvas, Point, Radius, Gold);
	DrawDieFace(Point, Radius * 1.1f, 6, 1.f);
	const float Left = Meteor->GetTimeToImpact();
	FString Label = Left > 0.f ? FString::Printf(TEXT("ULTIMATE DIE IN %d"), FMath::CeilToInt(Left)) : FString(TEXT("ULTIMATE DIE"));
	if (Viewer && Left <= 0.f)
	{
		Label += FString::Printf(TEXT(" · %dm"), FMath::RoundToInt(FVector::Dist2D(Viewer->GetActorLocation(), Meteor->GetActorLocation()) / 100.f));
	}
	const float Size = 10.f * S;
	const FVector2D Box = RRUI::MeasureText(Label, Size, EWeight::Black, Size * 0.12f);
	RRUI::DrawGlowText(Canvas, Label, FVector2D(Point.X - Box.X * 0.5f, Point.Y - Radius - 6.f * S - Box.Y), Size, EWeight::Black, RRUI::Readable(Gold), Size * 0.12f);
}

// ---------------------------------------------------------------------------------------------------------------------
// The light sword's guard

void ARRRandomHUD::DrawDeflectFocus(const ARRRandomCharacter& Viewer)
{
	if (!Viewer.GetWeapon().bSword || !Viewer.IsAlive())
	{
		return;
	}
	const float S = UIScale;
	const float Share = FMath::Clamp(Viewer.GetDeflectFocus() / ARRRandomCharacter::DeflectFocusMax, 0.f, 1.f);
	const bool bOn = Viewer.IsDeflecting();
	const bool bReady = Viewer.GetDeflectFocus() >= ARRRandomCharacter::DeflectMinStart;
	const FLinearColor Blue(0.45f, 0.85f, 1.f);
	// Over the hands (or where they would be): a thin bar of focus, and the key
	const float Width = 120.f * S;
	const float CenterX = Canvas->ClipX * 0.5f;
	const float Y = Canvas->ClipY - (HandBottom + HandHeight + HandLift + 44.f) * S;
	const FBox2D Bar(FVector2D(CenterX - Width * 0.5f, Y), FVector2D(CenterX + Width * 0.5f, Y + 5.f * S));
	RRUI::DrawBar(Canvas, Bar, Share, FLinearColor(1.f, 1.f, 1.f, 0.6f), bOn ? Blue : (bReady ? RRUI::Pastel(Blue, 0.3f) : RRUI::WithAlpha(RRUI::InkSoft, 0.5f)));
	const FString Label = bOn ? TEXT("GUARD!") : (bReady ? TEXT("RMB  GUARD") : TEXT("FOCUS..."));
	const float Size = (bOn ? 11.f : 9.f) * S;
	const FVector2D Box = RRUI::MeasureText(Label, Size, EWeight::Black, Size * 0.12f);
	RRUI::DrawGlowText(Canvas, Label, FVector2D(CenterX - Box.X * 0.5f, Y - Box.Y - 3.f * S), Size, EWeight::Black, bOn ? RRUI::Readable(Blue) : RRUI::Ink, Size * 0.12f);
}

// ---------------------------------------------------------------------------------------------------------------------
// The sniper's scope

void ARRRandomHUD::DrawScope(const ARRRandomCharacter& Viewer)
{
	const float Blend = Viewer.GetScopeBlend();
	if (Blend <= 0.02f || Viewer.GetScopeZoom() <= 0.f)
	{
		return;
	}
	const float S = UIScale;
	const FVector2D Center(Canvas->ClipX * 0.5f, Canvas->ClipY * 0.5f);
	const float Radius = Canvas->ClipY * 0.44f * FMath::Lerp(1.6f, 1.f, Blend);
	const float Far = FVector2D(Canvas->ClipX, Canvas->ClipY).Size() * 0.6f + Radius;
	const FLinearColor Shade(0.13f, 0.1f, 0.2f, 0.88f * Blend);
	// Outside the lens a deep lilac shade; its rim white, a soft blue tint in the glass
	RRUI::DrawRing(Canvas, Center, (Radius + Far) * 0.5f, Far - Radius, Shade);
	RRUI::DrawRing(Canvas, Center, Radius, 4.f * S, RRUI::WithAlpha(RRUI::White, 0.9f * Blend));
	RRUI::DrawRing(Canvas, Center, Radius - 9.f * S, 10.f * S, FLinearColor(0.55f, 0.75f, 1.f, 0.12f * Blend));
	// The reticle: thin lines from the rim with a gap in the middle, a dot
	const FLinearColor Line = RRUI::WithAlpha(RRUI::Pink, 0.9f * Blend);
	const float Gap = 14.f * S;
	const float Thin = 1.5f * S;
	RRUI::DrawBox(Canvas, FBox2D(FVector2D(Center.X - Radius, Center.Y - Thin), FVector2D(Center.X - Gap, Center.Y + Thin)), Line);
	RRUI::DrawBox(Canvas, FBox2D(FVector2D(Center.X + Gap, Center.Y - Thin), FVector2D(Center.X + Radius, Center.Y + Thin)), Line);
	RRUI::DrawBox(Canvas, FBox2D(FVector2D(Center.X - Thin, Center.Y + Gap), FVector2D(Center.X + Thin, Center.Y + Radius)), Line);
	RRUI::DrawBox(Canvas, FBox2D(FVector2D(Center.X - Thin, Center.Y - Radius), FVector2D(Center.X + Thin, Center.Y - Gap)), Line);
	RRUI::DrawDisc(Canvas, Center, 2.5f * S, Line);
	// How close, by the tier: "x1.5 SCOPE"
	const FString Label = FString::Printf(TEXT("x%.1f SCOPE"), Viewer.GetScopeZoom());
	const float Size = 12.f * S;
	const FVector2D Box = RRUI::MeasureText(Label, Size, EWeight::Black, Size * 0.14f);
	RRUI::DrawText(Canvas, Label, FVector2D(Center.X - Box.X * 0.5f, Center.Y + Radius - Box.Y - 18.f * S), Size, EWeight::Black,
		RRUI::WithAlpha(Viewer.GetWeapon().Tier == ERRWeaponTier::Common ? RRUI::White : RRUI::Pastel(FRRWeapon::GetTierColor(Viewer.GetWeapon().Tier), 0.4f), Blend), Size * 0.14f);
}
