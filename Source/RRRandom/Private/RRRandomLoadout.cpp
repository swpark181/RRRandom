#include "RRRandomLoadout.h"
#include "RRRandomCharacter.h"
#include "RRRandomGameState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

URRRandomLoadoutComponent::URRRandomLoadoutComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	Hands.Init(INDEX_NONE, HandCount);
}

void URRRandomLoadoutComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Only its owner sees the bag (the gun in the hands everyone sees, on the brawler)
	DOREPLIFETIME_CONDITION(URRRandomLoadoutComponent, Items, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URRRandomLoadoutComponent, Hands, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URRRandomLoadoutComponent, ActiveHand, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URRRandomLoadoutComponent, AddCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URRRandomLoadoutComponent, LastAdded, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URRRandomLoadoutComponent, FullCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URRRandomLoadoutComponent, CombineCountDone, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(URRRandomLoadoutComponent, LastCombined, COND_OwnerOnly);
}

void URRRandomLoadoutComponent::BeginPlay()
{
	Super::BeginPlay();
	Stream.GenerateNewSeed();
}

ARRRandomCharacter* URRRandomLoadoutComponent::GetBrawler() const
{
	return Cast<ARRRandomCharacter>(GetOwner());
}

bool URRRandomLoadoutComponent::IsInUse() const
{
	const ARRRandomCharacter* Brawler = GetBrawler();
	return Brawler && !Brawler->IsVillain() && !Brawler->IsDecoy() && ARRRandomGameState::IsBattleRoyale(this);
}

int32 URRRandomLoadoutComponent::GetHandOf(int32 Item) const
{
	return Item == INDEX_NONE ? INDEX_NONE : Hands.IndexOfByKey(Item);
}

int32 URRRandomLoadoutComponent::CountOf(const FRRBagItem& Item) const
{
	int32 Count = 0;
	for (const FRRBagItem& Each : Items)
	{
		Count += Each == Item ? 1 : 0;
	}
	return Count;
}

bool URRRandomLoadoutComponent::CanCombine(const FRRBagItem& Item) const
{
	return Item.Tier != ERRWeaponTier::Legendary && Item.Type != FRRWeapon::GetFingerType() && CountOf(Item) >= CombineCount;
}

bool URRRandomLoadoutComponent::CanCombineAny() const
{
	for (const FRRBagItem& Each : Items)
	{
		if (CanCombine(Each))
		{
			return true;
		}
	}
	return false;
}

FRRWeapon URRRandomLoadoutComponent::MakeWeapon(const FRRBagItem& Item) const
{
	const ARRRandomCharacter* Brawler = GetBrawler();
	return FRRWeapon::MakeTiered(Item.Type, Item.Tier, Brawler ? Brawler->GetDefaultMagazineSize() : 15);
}

void URRRandomLoadoutComponent::AddRolled(int32 Face)
{
	FRRBagItem Item;
	Item.Type = static_cast<uint8>(FRRWeapon::RollType(Stream, BagFingerChance));
	Item.Tier = Item.Type == FRRWeapon::GetFingerType() ? ERRWeaponTier::Legendary : FRRWeapon::GetTierForFace(Face);
	Add(Item);
}

bool URRRandomLoadoutComponent::Add(const FRRBagItem& Item)
{
	if (!GetOwner()->HasAuthority())
	{
		return false;
	}
	if (Items.Num() >= Capacity)
	{
		CombineAll();
	}
	if (Items.Num() >= Capacity)
	{
		++FullCount;
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s's bag is full; a %s %s is left behind."), *GetNameSafe(GetOwner()), *FRRWeapon::GetTierName(Item.Tier), *FRRWeapon::GetTypeName(Item.Type));
		return false;
	}
	const int32 Index = Items.Add(Item);
	++AddCount;
	LastAdded = Item;
	// An empty hand takes it (the active one first)
	const int32 Empty = Hands[ActiveHand] == INDEX_NONE ? ActiveHand : Hands.IndexOfByKey(INDEX_NONE);
	if (Empty != INDEX_NONE)
	{
		Hands[Empty] = Index;
	}
	Sort();
	EquipActive(false);
	UE_LOG(LogTemp, Verbose, TEXT("RRRandom: %s puts a %s %s in the bag (%d)."), *GetNameSafe(GetOwner()), *FRRWeapon::GetTierName(Item.Tier), *FRRWeapon::GetTypeName(Item.Type), Items.Num());
	return true;
}

bool URRRandomLoadoutComponent::Combine(const FRRBagItem& Item)
{
	if (!GetOwner()->HasAuthority() || !CanCombine(Item))
	{
		return false;
	}
	// Three of them: those in no hand first
	TArray<int32> Taken;
	for (int32 Pass = 0; Pass < 2 && Taken.Num() < CombineCount; ++Pass)
	{
		for (int32 Index = 0; Index < Items.Num() && Taken.Num() < CombineCount; ++Index)
		{
			const bool bInHand = GetHandOf(Index) != INDEX_NONE;
			if (Items[Index] == Item && bInHand == (Pass == 1))
			{
				Taken.Add(Index);
			}
		}
	}
	bool bHandGets[HandCount] = { false, false };
	for (const int32 Index : Taken)
	{
		const int32 Hand = GetHandOf(Index);
		if (Hand != INDEX_NONE)
		{
			bHandGets[Hand] = true;
		}
	}
	Taken.Sort([](int32 A, int32 B) { return A > B; });
	for (const int32 Index : Taken)
	{
		RemoveAt(Index);
	}
	FRRBagItem Better = Item;
	Better.Tier = static_cast<ERRWeaponTier>(static_cast<int32>(Item.Tier) + 1);
	const int32 New = Items.Add(Better);
	for (int32 Hand = 0; Hand < HandCount; ++Hand)
	{
		if (bHandGets[Hand] && Hands.IndexOfByKey(New) == INDEX_NONE)
		{
			Hands[Hand] = New;
		}
	}
	++CombineCountDone;
	LastCombined = Better;
	Sort();
	EquipActive(false);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s combines three %s %s into a %s one."), *GetNameSafe(GetOwner()), *FRRWeapon::GetTierName(Item.Tier),
		*FRRWeapon::GetTypeName(Item.Type), *FRRWeapon::GetTierName(Better.Tier));
	return true;
}

void URRRandomLoadoutComponent::CombineAll()
{
	// Lowest tiers first, so a fresh rare can go on to make an epic
	for (int32 Tier = 0; Tier < static_cast<int32>(ERRWeaponTier::Legendary); ++Tier)
	{
		bool bAgain = true;
		while (bAgain)
		{
			bAgain = false;
			for (const FRRBagItem& Each : Items)
			{
				if (static_cast<int32>(Each.Tier) == Tier && CanCombine(Each))
				{
					const FRRBagItem Copy = Each;
					Combine(Copy);
					bAgain = true;
					break;
				}
			}
		}
	}
}

void URRRandomLoadoutComponent::SetHand(int32 Hand, int32 Item)
{
	if (!GetOwner()->HasAuthority() || !Hands.IsValidIndex(Hand) || (Item != INDEX_NONE && !Items.IsValidIndex(Item)))
	{
		return;
	}
	// Already in the other hand: the two swap
	const int32 Other = GetHandOf(Item);
	if (Other != INDEX_NONE && Other != Hand)
	{
		Hands[Other] = Hands[Hand];
	}
	Hands[Hand] = Item;
	EquipActive(false);
}

void URRRandomLoadoutComponent::SelectHand(int32 Hand)
{
	if (!GetOwner()->HasAuthority() || !Hands.IsValidIndex(Hand) || Hand == ActiveHand)
	{
		return;
	}
	ActiveHand = static_cast<uint8>(Hand);
	EquipActive(true);
}

void URRRandomLoadoutComponent::Discard(int32 Item)
{
	if (!GetOwner()->HasAuthority() || !Items.IsValidIndex(Item))
	{
		return;
	}
	RemoveAt(Item);
	EquipActive(false);
}

void URRRandomLoadoutComponent::Clear()
{
	Items.Reset();
	Hands.Init(INDEX_NONE, HandCount);
	ActiveHand = 0;
	for (int32 Hand = 0; Hand < HandCount; ++Hand)
	{
		HandAmmo[Hand] = INDEX_NONE;
	}
	if (GetOwner()->HasAuthority())
	{
		EquipActive(false);
	}
}

void URRRandomLoadoutComponent::RemoveAt(int32 Index)
{
	Items.RemoveAt(Index);
	for (int32& Hand : Hands)
	{
		Hand = Hand == Index ? INDEX_NONE : (Hand > Index ? Hand - 1 : Hand);
	}
}

void URRRandomLoadoutComponent::Sort()
{
	TArray<int32> Order;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		Order.Add(Index);
	}
	Order.StableSort([this](int32 A, int32 B)
	{
		return Items[A].Type != Items[B].Type ? Items[A].Type < Items[B].Type : Items[A].Tier > Items[B].Tier;
	});
	TArray<FRRBagItem> Sorted;
	TArray<int32> NewIndex;
	NewIndex.Init(INDEX_NONE, Items.Num());
	for (int32 Rank = 0; Rank < Order.Num(); ++Rank)
	{
		Sorted.Add(Items[Order[Rank]]);
		NewIndex[Order[Rank]] = Rank;
	}
	Items = MoveTemp(Sorted);
	for (int32& Hand : Hands)
	{
		Hand = NewIndex.IsValidIndex(Hand) ? NewIndex[Hand] : INDEX_NONE;
	}
}

void URRRandomLoadoutComponent::EquipActive(bool bSwap)
{
	ARRRandomCharacter* Brawler = GetBrawler();
	if (!Brawler || !GetOwner()->HasAuthority())
	{
		return;
	}
	const int32 Item = Hands[ActiveHand];
	const bool bFromBag = Items.IsValidIndex(Item);
	const FRRBagItem Wanted = bFromBag ? Items[Item] : FRRBagItem();
	// The same gun still in the hand (the bag only moved round it): nothing changes
	if (!bSwap && bFromBag == bEquippedFromBag && (!bFromBag || Wanted == Equipped))
	{
		return;
	}
	// The hand let go of keeps its rounds, for when it comes back to that gun
	if (bEquippedFromBag || bSwap)
	{
		const int32 Left = bSwap ? 1 - ActiveHand : ActiveHand;
		HandItem[Left] = Equipped;
		HandAmmo[Left] = bEquippedFromBag ? Brawler->GetAmmo() : INDEX_NONE;
	}
	const FRRWeapon Weapon = bFromBag ? MakeWeapon(Wanted) : FRRWeapon();
	const int32 Ammo = bFromBag && HandItem[ActiveHand] == Wanted ? HandAmmo[ActiveHand] : INDEX_NONE;
	Brawler->EquipWeaponWithAmmo(Weapon, Ammo);
	if (bSwap)
	{
		Brawler->DelayFire(SwapTime);
	}
	Equipped = Wanted;
	bEquippedFromBag = bFromBag;
}

void URRRandomLoadoutComponent::AutoManage()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	CombineAll();
	int32 BestLong = INDEX_NONE;
	int32 BestClose = INDEX_NONE;
	int32 Second = INDEX_NONE;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		const bool bClose = IsCloseGun(Items[Index].Type);
		int32& Best = bClose ? BestClose : BestLong;
		if (Best == INDEX_NONE || GetWorth(Items[Index]) > GetWorth(Items[Best]))
		{
			Best = Index;
		}
	}
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (Index != BestLong && Index != BestClose && (Second == INDEX_NONE || GetWorth(Items[Index]) > GetWorth(Items[Second])))
		{
			Second = Index;
		}
	}
	const int32 First = BestLong != INDEX_NONE ? BestLong : (BestClose != INDEX_NONE ? BestClose : Second);
	const int32 Other = BestLong != INDEX_NONE && BestClose != INDEX_NONE ? BestClose : Second;
	Hands[0] = First;
	Hands[1] = Other != First ? Other : INDEX_NONE;
	EquipActive(false);
}

int32 URRRandomLoadoutComponent::ChooseHandFor(float Distance) const
{
	const bool bFirstClose = Items.IsValidIndex(Hands[0]) && IsCloseGun(Items[Hands[0]].Type);
	const bool bSecondClose = Items.IsValidIndex(Hands[1]) && IsCloseGun(Items[Hands[1]].Type);
	if (bFirstClose == bSecondClose)
	{
		return Items.IsValidIndex(Hands[ActiveHand]) ? ActiveHand : (Items.IsValidIndex(Hands[0]) ? 0 : 1);
	}
	// Up close the close one, further the other
	const bool bWantClose = Distance < 650.f;
	return bWantClose == bFirstClose ? 0 : 1;
}

float URRRandomLoadoutComponent::GetWorth(const FRRBagItem& Item)
{
	if (Item.Type == FRRWeapon::GetFingerType())
	{
		return 0.5f;
	}
	// Each gun about as good as the others at its tier (they trade damage for pace): the tier counts most
	const FRRWeapon Weapon = FRRWeapon::MakeTiered(Item.Type, Item.Tier, 15);
	const float PerSecond = Weapon.DamageMultiplier * Weapon.Pellets / FMath::Max(0.1f, Weapon.FireIntervalMultiplier);
	return FRRWeapon::GetTierDamageScale(Item.Tier) * 10.f + PerSecond;
}

bool URRRandomLoadoutComponent::IsCloseGun(int32 Type)
{
	const FString Name = FRRWeapon::GetTypeName(Type);
	return Type == FRRWeapon::GetFingerType() || Name == TEXT("SHOTGUN") || Name == TEXT("LIGHT SWORD");
}

void URRRandomLoadoutComponent::ServerSetHand_Implementation(int32 Hand, int32 Item)
{
	SetHand(Hand, Item);
}

void URRRandomLoadoutComponent::ServerSelectHand_Implementation(int32 Hand)
{
	SelectHand(Hand);
}

void URRRandomLoadoutComponent::ServerCombine_Implementation(uint8 Type, ERRWeaponTier Tier)
{
	FRRBagItem Item;
	Item.Type = Type;
	Item.Tier = Tier;
	Combine(Item);
}

void URRRandomLoadoutComponent::ServerCombineAll_Implementation()
{
	CombineAll();
}

void URRRandomLoadoutComponent::ServerDiscard_Implementation(int32 Item)
{
	Discard(Item);
}
