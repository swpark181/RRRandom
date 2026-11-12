#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Math/RandomStream.h"
#include "RRRandomWeapon.h"
#include "RRRandomLoadout.generated.h"

class ARRRandomCharacter;

/** One weapon in the bag: its gun type (FRRWeapon::Make's) and tier. */
USTRUCT()
struct FRRBagItem
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 Type = 0;

	UPROPERTY()
	ERRWeaponTier Tier = ERRWeaponTier::Common;

	bool operator==(const FRRBagItem& Other) const { return Type == Other.Type && Tier == Other.Tier; }
};

/**
 * Round 97, the battle royale's bag (the user: "같은 무기가 3개 나오면 업글 가능. 인벤토리 같은 시스템도 필요" / two hands):
 * every weapon die puts a gun in the bag instead of in the hands (its type at random, its face a tier: a 6 rare, the rest
 * common); three of the same gun at the same tier combine into one a tier up (common, rare, epic, legendary); two of them
 * are held in the hands (keys 1 and 2 switch, the other hand's magazine kept), the rest wait in the bag. With both hands
 * empty the animal friend has the starting pistol.
 * Only in the battle royale (IsInUse); elsewhere a weapon die still swaps the gun in the hands. The server keeps it (the
 * bag is sorted by gun then tier, best first); the owner's machine sees it (the HUD's bag panel, Tab) and asks for changes
 * through the Server functions. Bots manage theirs with AutoManage and ChooseHandFor.
 */
UCLASS(ClassGroup = (RRRandom))
class RRRANDOM_API URRRandomLoadoutComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URRRandomLoadoutComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Guns the bag holds at most (hands included), how many hands, and how many of a kind combine. */
	static constexpr int32 Capacity = 20;
	static constexpr int32 HandCount = 2;
	static constexpr int32 CombineCount = 3;
	/** Seconds after switching hands before the new gun fires. */
	static constexpr float SwapTime = 0.35f;
	/** A weapon die's chance of the foam finger in the bag (rarer than the plain game's FoamFingerChance: thirty dice). */
	static constexpr float BagFingerChance = 0.03f;

	/** In use: the battle royale. */
	bool IsInUse() const;

	const TArray<FRRBagItem>& GetItems() const { return Items; }
	/** The bag index in a hand (INDEX_NONE for an empty one). */
	int32 GetHand(int32 Hand) const { return Hands.IsValidIndex(Hand) ? Hands[Hand] : INDEX_NONE; }
	int32 GetActiveHand() const { return ActiveHand; }
	/** Which hand holds a bag index (INDEX_NONE for neither). */
	int32 GetHandOf(int32 Item) const;
	int32 CountOf(const FRRBagItem& Item) const;
	/** Three or more of it, and a tier above it. */
	bool CanCombine(const FRRBagItem& Item) const;
	bool CanCombineAny() const;
	/** The gun a bag item is, for this owner. */
	FRRWeapon MakeWeapon(const FRRBagItem& Item) const;
	/** Goes up by one each time a gun comes into the bag (the HUD's "+ RIFLE"), and the last one that did. */
	int32 GetAddCount() const { return AddCount; }
	const FRRBagItem& GetLastAdded() const { return LastAdded; }
	/** Goes up when a gun was thrown away because the bag was full, or when a combine made one. */
	int32 GetFullCount() const { return FullCount; }
	int32 GetCombineCount() const { return CombineCountDone; }
	const FRRBagItem& GetLastCombined() const { return LastCombined; }

	// Server

	/** A weapon die landed: a gun of a random type, its tier by the face, into the bag (and an empty hand). */
	void AddRolled(int32 Face);
	/** Into the bag, and into an empty hand if there is one. False when the bag is full even after combining. */
	bool Add(const FRRBagItem& Item);
	/** Three of it into one a tier up (those in hands first given up last; a hand that held one holds the new one). */
	bool Combine(const FRRBagItem& Item);
	/** Every combine there is, lowest tiers first, till none is left. */
	void CombineAll();
	/** Puts a bag item in a hand (INDEX_NONE empties the hand); one item can't be in both. */
	void SetHand(int32 Hand, int32 Item);
	/** Switches to a hand (the gun in it, or the pistol). */
	void SelectHand(int32 Hand);
	void Discard(int32 Item);
	/** Empties the bag and hands (the pistol again). */
	void Clear();
	/** A bot's: combines all, then the best long gun in hand 1 and the best close one (or the next best) in hand 2. */
	void AutoManage();
	/** A bot's: the hand to fight with at this distance from its target (the close gun up close). */
	int32 ChooseHandFor(float Distance) const;

	UFUNCTION(Server, Reliable)
	void ServerSetHand(int32 Hand, int32 Item);
	UFUNCTION(Server, Reliable)
	void ServerSelectHand(int32 Hand);
	UFUNCTION(Server, Reliable)
	void ServerCombine(uint8 Type, ERRWeaponTier Tier);
	UFUNCTION(Server, Reliable)
	void ServerCombineAll();
	UFUNCTION(Server, Reliable)
	void ServerDiscard(int32 Item);

	/** A gun's worth to a bot (damage and tier), and whether it is for fighting up close (shotgun, sword, foam finger). */
	static float GetWorth(const FRRBagItem& Item);
	static bool IsCloseGun(int32 Type);

protected:
	virtual void BeginPlay() override;

private:
	ARRRandomCharacter* GetBrawler() const;
	/** Sorts the bag (gun, then tier, best first), keeping the hands on their items. */
	void Sort();
	void RemoveAt(int32 Index);
	/** The active hand's gun into the owner's hands, keeping each hand's magazine. bSwap: a hand switch (a short wait to fire). */
	void EquipActive(bool bSwap);

	UPROPERTY(Replicated)
	TArray<FRRBagItem> Items;

	UPROPERTY(Replicated)
	TArray<int32> Hands;

	UPROPERTY(Replicated)
	uint8 ActiveHand = 0;

	UPROPERTY(Replicated)
	int32 AddCount = 0;

	UPROPERTY(Replicated)
	FRRBagItem LastAdded;

	UPROPERTY(Replicated)
	int32 FullCount = 0;

	UPROPERTY(Replicated)
	int32 CombineCountDone = 0;

	UPROPERTY(Replicated)
	FRRBagItem LastCombined;

	/** Server: what each hand held when last let go of (its gun and rounds left), so switching back keeps them. */
	FRRBagItem HandItem[HandCount];
	int32 HandAmmo[HandCount] = { INDEX_NONE, INDEX_NONE };
	/** Server: the gun in the owner's hands came from this hand's item (INDEX_NONE: the pistol). */
	FRRBagItem Equipped;
	bool bEquippedFromBag = false;

	FRandomStream Stream;
};
