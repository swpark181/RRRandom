#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RRRandomOre.h"
#include "RRRandomMeteor.generated.h"

class UMaterialInstanceDynamic;

/** A comic word popping up over a meteor (BOOM!, POP!, POOF!), drawn by the HUD in a starburst. */
struct FRRComicPop
{
	FString Text;
	FVector Where = FVector::ZeroVector;
	/** World time it popped (this machine's clock). */
	float Start = 0.f;
	FLinearColor Color = FLinearColor::White;
	/** 1 for an ordinary word. */
	float Size = 1.f;
	/** Degrees the burst behind it is turned. */
	float Tilt = 0.f;
};
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * A dice meteor of Meteor Rush, standing where it will land (its actor location: on the floor). WarnTime seconds ahead a
 * hologram ring marks the spot with a second ring closing in on it; then the meteor streaks down from the sky, knocks
 * everyone near aside (no damage) and, like a lottery machine, spins the dice in its glass globe and pops its ore out of
 * the top one piece at a time, the big crystal last. It lingers a while and shrinks away.
 * Every machine plays it from the replicated impact time on the server's clock; the server throws the ore.
 */
UCLASS()
class RRRANDOM_API ARRRandomMeteor : public AActor
{
	GENERATED_BODY()

public:
	ARRRandomMeteor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	/** Server, before FinishSpawning: lands at this server time; the golden meteor holds overtime's crystal alone. */
	void Setup(float InImpactTime, bool bInGolden);

	/** Seconds until it lands (negative after), on the server's clock. */
	float GetTimeToImpact() const;
	bool IsGolden() const { return bGolden; }
	/** Still to land or still throwing ore: worth heading for (server only: clients don't know what is left inside). */
	bool IsActive() const;
	/** Landed and spinning, flinging its ore (every machine). */
	bool IsBursting() const;

	/** The cartoon bits the HUD draws over it: comic words, swirling speed lines while it spins, dizzy stars after. */
	const TArray<FRRComicPop>& GetComicPops() const { return ComicPops; }
	bool IsSpinning() const { return bSpinning; }
	/** How fast it spins, 1 at full speed. */
	float GetSpinSpeedShare() const { return SpinSpeedShare; }
	/** 0 .. 1 while the dizzy stars circle over it. */
	float GetDizzyAlpha() const { return DizzyAlpha; }
	/** The top of the meteor as squashed and stretched now. */
	const FVector& GetTopLocation() const { return TopLocation; }
	/** The meteor's height standing (unsquashed). */
	static float GetHeight();

	/** Seconds of warning before it lands. */
	static constexpr float WarnTime = 4.f;
	/** Brawlers within this of where it lands are knocked aside. */
	static constexpr float ImpactRadius = 420.f;

protected:
	virtual void BeginPlay() override;

private:
	void UpdateLook(float DeltaSeconds);
	/** Server: the knock aside, then the ore one by one. */
	void UpdateServer();
	void ThrowNext();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Root;

	/** The hologram on the floor: a ring where it lands and one closing in on it. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Ring;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Closing;

	/** The falling meteor: its crust, the glass globe and the dice tumbling inside. */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> Body;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Crust;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Globe;

	UPROPERTY(VisibleAnywhere)
	TArray<TObjectPtr<UStaticMeshComponent>> Dice;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> Light;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RingMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ClosingMaterial;

	UPROPERTY(Replicated)
	float ImpactTime = 0.f;

	UPROPERTY(Replicated)
	bool bGolden = false;

	/** Server: what is still inside, thrown from the back of the list. */
	TArray<ERROreKind> Contents;
	float NextThrowTime = 0.f;
	/** Which way the last piece flew (degrees), so the next goes round from it. */
	float ThrowAngle = 0.f;
	bool bHasLanded = false;
	/** Each machine: the landing shown, the POOF shown, how many flings shown; the cartoon state for the HUD. */
	bool bLandingShown = false;
	bool bBurstShown = false;
	int32 ShownThrows = 0;
	TArray<FRRComicPop> ComicPops;
	bool bSpinning = false;
	float SpinSpeedShare = 0.f;
	float DizzyAlpha = 0.f;
	FVector TopLocation = FVector::ZeroVector;
	/** The dice's tumbling: each its own orbit in the globe. */
	TArray<FVector> DiceAxes;
	TArray<float> DicePhases;
	float Spin = 0.f;
	/** Each machine: where it falls from, off the spot (beyond it from its camera). */
	FVector FallFrom = FVector::ZeroVector;
	bool bFallFromSet = false;
	/** Each machine: the way the meteor faces (its camera). */
	FRotator Facing = FRotator::ZeroRotator;
	bool bFacingSet = false;
};
