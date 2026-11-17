#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RRRandomRounds.h"
#include "RRRandomKit.generated.h"

class ARRRandomCharacter;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMeshComponent;

/**
 * What makes a species play its own way besides its ultimate: one always-on passive and one die of its own. The giant
 * and flight dice belong to the tiger and the dragon; the fox, rabbit and squirrel each get their own die in their place
 * (the same share of rolls, so the game keeps its pace).
 */
struct FRRKit
{
	// The passive
	float HealthScale = 1.f;
	float MoveSpeedScale = 1.f;
	/** Every push the brawler takes (meteors, ultimates, being run over), as a factor. */
	float KnockbackScale = 1.f;
	/** How far it reaches to pick up ore, as a factor. */
	float OrePickupScale = 1.f;
	/** Jumps before landing (the rabbit's second one). */
	int32 JumpCount = 1;
	/** Holding jump while falling glides down (the dragon). */
	bool bGlide = false;
	/** Flies and glides on its own wings (the dragon: its wing bones flap and spread, URRRandomBodyAnimInstance) instead of
	 * the glowing ones (URRRandomWingsComponent) and their sparkles. */
	bool bOwnWings = false;
	/** Shots from behind, and the first shot out of invisibility, hit harder (the fox). */
	bool bBackstab = false;
	/** Keeps part of its ore and its gun when knocked out (the squirrel). */
	bool bCheekPouch = false;

	// Its dice
	float GiantDieChance = 0.f;
	float FlightDieChance = 0.f;
	float OwnDieChance = 0.f;

	// Words for the brawler pick and the HUD
	const TCHAR* PassiveName = TEXT("");
	const TCHAR* PassiveLine = TEXT("");
	const TCHAR* DieName = TEXT("");
	const TCHAR* DieLine = TEXT("");
	/** The roll's word over the head ("5 HOP"); empty when its own die is the giant or flight die. */
	const TCHAR* DieWord = TEXT("");
	FLinearColor DieColor = FLinearColor::White;

	/**
	 * Round 98, the main mode's role (the user's table in Docs/TPSOverhaulQuestions.md): its name and line, the weapon it
	 * always holds there (a FRRWeapon type name: no weapon dice there), and its Q ability (AbilityPower: seconds, or hops)
	 * with its cooldown.
	 */
	const TCHAR* RoleName = TEXT("");
	const TCHAR* RoleLine = TEXT("");
	const TCHAR* RoleWeapon = TEXT("");
	ERRAbility Ability = ERRAbility::None;
	float AbilityPower = 0.f;
	float AbilityCooldown = 10.f;
	const TCHAR* AbilityLine = TEXT("");
};

namespace RRKit
{
	const FRRKit& Get(int32 Species);

	/** Seconds of invisibility per pip, super hops per pip, shield per pip (and seconds it lasts), ore magnet seconds per pip. */
	constexpr float CloakSecondsPerPip = 2.f;
	constexpr float ShieldPerPip = 10.f;
	constexpr float ShieldSeconds = 20.f;
	constexpr float MagnetSecondsPerPip = 2.f;
	constexpr int32 MaxSuperHops = 9;
	/** Damage from behind (and out of invisibility) as a factor; "behind" is within this many degrees of straight behind. */
	constexpr float BackstabMultiplier = 1.25f;
	constexpr float BackstabHalfAngle = 60.f;
}

/**
 * A brawler's kit (FRRKit, by its species): its passive's numbers and the state of its own die. The server runs it and
 * replicates the state; every machine draws it. The fox's invisibility swaps the body and gun to a see-through shimmer
 * (M_FXCloak, fainter on opponents' screens), the squirrel's shield is a glass bubble round it.
 */
UCLASS()
class RRRANDOM_API URRRandomKitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URRRandomKitComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	const FRRKit& GetKit() const;

	/** Server: its own die landed (the fox's invisibility, the rabbit's super hops, the squirrel's shield and ore magnet). */
	void ApplyOwnDie(int32 Face);
	/** Server: knocked out; every power of its own die ends. */
	void ClearPowers();

	// Round 98: the Q ability (FRRKit::Ability), the cards' powers, the red fox's disguise
	/** Server: uses the Q ability if it is ready; false if not (cooling down, knocked out, play locked, nothing to do). */
	bool UseAbility();
	/** Seconds until Q is ready again (0: ready), and its full cooldown. */
	float GetAbilityCooldown() const { return AbilityCooldown; }
	float GetAbilityCooldownMax() const { return GetKit().AbilityCooldown; }
	/** Server: Q ready at once (a new round). */
	void ResetAbility() { AbilityCooldown = 0.f; }
	/** Server: the cards' powers (any species). */
	void GrantCloak(float Seconds);
	void GrantShield(float Amount, float Seconds);
	bool IsDisguised() const { return DisguiseRemaining > 0.f; }
	float GetDisguiseRemaining() const { return DisguiseRemaining; }
	/** Within this distance an opponent sees through a disguise. */
	static constexpr float DisguiseNoticeRange = 200.f;

	// The fox's invisibility
	bool IsCloaked() const { return CloakRemaining > 0.f; }
	float GetCloakRemaining() const { return CloakRemaining; }
	/**
	 * Server or any machine: whether a team can't make this brawler out right now: invisible, not just hit, and none of
	 * that team standing within CloakNoticeRange of it. Bots and the minimap go by this.
	 */
	bool IsHiddenFrom(int32 Team) const;
	/** This machine's screen shows it as an opponent's faint shimmer (no HUD over it, no die over its head). */
	bool IsCloakedOnThisScreen() const { return bCloakForEnemy; }
	/** Server: it shot. That ends invisibility, and the shot gets the ambush bonus. */
	void OnShot();
	/** Server: whether a hit now still has the ambush bonus from leaving invisibility (used up by it). */
	bool ConsumeAmbush();
	/** Server: it was hit (an invisible brawler flickers into view for a moment). */
	void OnHit();

	// The rabbit's super hops
	int32 GetSuperHops() const { return SuperHops; }
	/** Server: spends one super hop; false when there is none. */
	bool SpendSuperHop();

	// The squirrel's shield and ore magnet
	float GetShield() const { return Shield; }
	float GetShieldRemaining() const { return Shield > 0.f ? ShieldRemaining : 0.f; }
	float GetMagnetRemaining() const { return MagnetRemaining; }
	/** Server: takes what it can of a hit; returns what gets through to health. */
	float AbsorbDamage(float Damage);

	/** Within this distance an opponent notices an invisible brawler anyway. */
	static constexpr float CloakNoticeRange = 250.f;
	/** The ore magnet's reach. */
	static constexpr float MagnetRadius = 600.f;

protected:
	virtual void BeginPlay() override;

private:
	ARRRandomCharacter* GetBrawler() const;
	/** Every machine: the invisible look on or off, and how see-through for this screen. */
	void UpdateCloakLook(float DeltaTime);
	void SetCloakLook(bool bOn, bool bForEnemy);
	/** Every machine: the shield bubble. */
	void UpdateShieldLook(float DeltaTime);
	/** Server: pulls resting ore near the squirrel toward it. */
	void UpdateMagnet();

	UPROPERTY(Replicated)
	float CloakRemaining = 0.f;

	UPROPERTY(Replicated)
	int32 SuperHops = 0;

	UPROPERTY(Replicated)
	float Shield = 0.f;

	UPROPERTY(Replicated)
	float ShieldMax = 0.f;

	UPROPERTY(Replicated)
	float ShieldRemaining = 0.f;

	UPROPERTY(Replicated)
	float MagnetRemaining = 0.f;

	UPROPERTY(Replicated)
	float AbilityCooldown = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_Disguise)
	float DisguiseRemaining = 0.f;

	UFUNCTION()
	void OnRep_Disguise();
	/** Every machine: the body's team tint follows the disguise (a puff as it changes). */
	bool bDisguiseShown = false;
	void UpdateDisguiseLook();

	/** Server clock: the ambush bonus lasts until this; an invisible brawler shows after a hit until this. */
	float AmbushUntil = -1.f;
	float RevealUntil = -1.f;
	float NextMagnetScan = 0.f;

	/** This machine: the cloak materials in place, the ones they replaced, and how faint they are. */
	bool bCloakShown = false;
	bool bCloakForEnemy = false;
	float CloakFadeIn = 0.f;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SavedBodyMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> SavedGunMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CloakMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Bubble;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BubbleMaterial;

	float LastShield = 0.f;
	float BubbleFlash = 0.f;
};
