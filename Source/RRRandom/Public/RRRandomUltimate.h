#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomUltimate.generated.h"

class ARRRandomCharacter;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * A brawler's ultimate, called by rolling an ultimate die (ARRRandomCharacter::OnUltimateDie); the die's face is Count
 * (how many strikes, clones, seconds...). Each species has its own (RRSpecies): the tiger's ARRRandomTigerSpirit, the
 * fox's ARRRandomFoxClones, the rabbit's ARRRandomMoonHop, the squirrel's ARRRandomAcornRain.
 *
 * What they share lives here. Spawned by the server and replicated (always relevant); every machine plays it from its own
 * clock once the brawler is known there (OnStarted), the server alone decides and deals damage.
 * - Picking spots (WantsSpots): the brawler's player looks through Overview, a camera high over the arena, with the cursor
 *   out; a hologram of the area (M_FXHolo on the engine cylinder, AreaRadius across) follows the cursor on that machine
 *   (ShowCursorSpot), and clicks become RequestSpot on the server. A spot asked for while the ultimate is busy waits for
 *   its turn; a player who leaves it too long gets GetDefaultSpot, a bot's brawler PickTarget (where most enemies are).
 *   Every machine sees a warning hologram where the next hit lands (ShowTargetHolo / ReleaseTargetHolo).
 * - The brawler may be held (can't move, shoot or be hurt: HoldsBrawler) and hidden (HidesBrawler) meanwhile.
 * - It ends by itself when Count runs out (Finish), or when its brawler is gone or knocked out or the match ends; Done
 *   gives the brawler back.
 */
UCLASS(Abstract)
class RRRANDOM_API ARRRandomUltimate : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomUltimate();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server, between SpawnActorDeferred and FinishSpawning. */
	void Setup(ARRRandomCharacter* InBrawler, int32 InCount);

	ARRRandomCharacter* GetBrawler() const { return Brawler; }
	/** What's left of the die's face (strikes, hops, or the clones still standing). */
	int32 GetLeft() const { return Left; }

	/** Clicks pick where it hits. */
	virtual bool WantsSpots() const { return false; }
	/** In the middle of something (getting ready, a hit under way): spots asked for now wait. */
	virtual bool IsBusy() const { return false; }
	/** Ready for the next spot right now. */
	bool IsReady() const { return WantsSpots() && bStarted && !bEnding && Left > 0 && !IsBusy(); }
	/** Still waiting for spots: its player's view is over the arena with the cursor out. */
	virtual bool IsPicking() const { return WantsSpots() && bStarted && !bEnding && Left > 0; }
	/**
	 * Its player's view stays over the arena: while picking, and to the end for one that holds the brawler (the last hop
	 * or strike is worth watching; the squirrel goes back to the fight once its rain is picked).
	 */
	bool KeepsOverview() const { return WantsSpots() && bStarted && !bDone && (IsPicking() || (HoldsBrawler() && !bEnding)); }
	/** Its brawler can't move, shoot, roll or be hurt meanwhile. */
	virtual bool HoldsBrawler() const { return false; }
	/** Its brawler's body is hidden meanwhile. */
	virtual bool HidesBrawler() const { return false; }
	/** Server: hit at this spot (anywhere: where the player clicked); waits if the ultimate is busy. */
	bool RequestSpot(const FVector& Target);

	/** For the HUD: the ultimate's name, and its line while it runs. */
	virtual FString GetTitle() const { return FString(); }
	virtual FString GetStatusLine() const;

	/** The floor's height where the brawler stood: a picked spot is found on the floor, not on what sticks up from it. */
	float GetGroundZ() const { return GroundZ; }
	/** The ground at a spot: the first walkable world surface going down from a little above it (or the spot itself). */
	FVector FindGround(const FVector& Spot) const;
	/** This machine's player is pointing at a spot: the area's hologram there (none with bShow false). */
	void ShowCursorSpot(bool bShow, const FVector& Spot);
	float GetAreaRadius() const { return AreaRadius; }

	/** Everyone of the other team inside the area. */
	UPROPERTY(EditAnywhere, Category = "Ultimate")
	float AreaRadius = 500.f;

	/** How tall the area's hologram wall stands. */
	UPROPERTY(EditAnywhere, Category = "Ultimate")
	float HoloHeight = 260.f;

	/** The holograms' colors: following the cursor, and where the next hit lands. */
	UPROPERTY(EditAnywhere, Category = "Ultimate")
	FLinearColor CursorHoloColor = FLinearColor(1.f, 0.42f, 0.03f);
	UPROPERTY(EditAnywhere, Category = "Ultimate")
	FLinearColor TargetHoloColor = FLinearColor(1.f, 0.08f, 0.02f);

	/** Seconds a player may take to pick the next spot before one is picked; a bot's brawler picks after BotDelay. */
	UPROPERTY(EditAnywhere, Category = "Ultimate")
	float PlayerDelay = 6.f;
	UPROPERTY(EditAnywhere, Category = "Ultimate")
	float BotDelay = 0.8f;

protected:
	virtual void BeginPlay() override;

	/** Once the brawler is known on this machine (every machine). */
	virtual void OnStarted() {}
	/** Every frame after OnStarted (every machine). */
	virtual void TickUltimate(float DeltaSeconds) {}
	/** Server: hit at this spot now (it was ready). Count down Left. */
	virtual void StartAction(const FVector& Target) {}
	/** Server: a player's brawler that didn't pick in time hits here. */
	virtual FVector GetDefaultSpot() const;
	/** Server: where a bot's brawler hits: the spot with the most enemies within reach (or the default). */
	virtual FVector PickTarget() const;
	/** The ending began (every machine): wind down, then call Done. */
	virtual void OnEnding() { Done(); }

	/** Server: no more to do; begins the ending everywhere. */
	void Finish();
	/** All over: the brawler is given back (server), the actor goes. */
	void Done();
	bool IsEnding() const { return bEnding; }
	bool HasStarted() const { return bStarted; }

	/** The warning where the next hit lands, on every machine: shown, then fading once it has landed. */
	void ShowTargetHolo(const FVector& Spot, float Radius, float Strength = 1.f);
	void ReleaseTargetHolo();
	/** Places a hologram on the floor at a spot, Radius across. */
	void PlaceHolo(UStaticMeshComponent* Holo, const FVector& Spot, float Radius) const;
	UMaterialInstanceDynamic* GetTargetHoloMaterial() const { return TargetHoloMaterial; }

	/** Every enemy of the brawler's within Radius of a spot (on the floor, flying over it or on something in it). */
	TArray<ARRRandomCharacter*> FindEnemiesNear(const FVector& Spot, float Radius, float Height) const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> Overview;

	UPROPERTY(Replicated)
	TObjectPtr<ARRRandomCharacter> Brawler;

	UPROPERTY(Replicated)
	int32 Left = 0;

	/** The die's face it was called with. */
	UPROPERTY(Replicated)
	int32 Face = 1;

	/** Where the brawler's feet were when it started. */
	FVector StartFeet = FVector::ZeroVector;

private:
	UFUNCTION()
	void OnRep_Ending();

	/** Begins once the brawler is known here (a joiner may get it a moment after this). */
	void TryStart();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CursorHolo;
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TargetHolo;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CursorHoloMaterial;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TargetHoloMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_Ending)
	bool bEnding = false;

	bool bStarted = false;
	bool bDone = false;
	float GroundZ = 0.f;
	/** The warning: seconds shown (-1 hidden), and how far it has faded after release (-1 not released). */
	float TargetHoloTime = -1.f;
	float TargetHoloFade = -1.f;
	float TargetHoloStrength = 1.f;
	/** Server: seconds ready and waiting for a spot, and a spot asked for while busy. */
	float ReadyTime = 0.f;
	FVector QueuedTarget = FVector::ZeroVector;
	bool bHasQueued = false;
};
