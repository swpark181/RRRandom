#include "RRRandomAcornRain.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Math/RandomStream.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Candy acorns: a warm brown nut, a darker cap (linear colors for M_DLCandy's custom data)
	const FLinearColor NutColor(0.62f, 0.27f, 0.08f);
	const FLinearColor CapColor(0.22f, 0.11f, 0.04f);
	// Instances in flight at once, at most (AcornsPerSecond x FallTime, with room)
	constexpr int32 MaxInFlight = 110;
	// Round 100 (the user: "다람쥐 궁극기는 오브젝트를 좀 더 이쁘게"): the nuts in three warm tones, now and then a bigger
	// golden one that sparkles; each a little bigger or smaller than the next
	const FLinearColor NutTones[] = { FLinearColor(0.62f, 0.27f, 0.08f), FLinearColor(0.8f, 0.43f, 0.12f), FLinearColor(0.5f, 0.2f, 0.07f) };
	const FLinearColor GoldenNut(1.f, 0.7f, 0.1f);
	const FLinearColor GoldenCap(0.62f, 0.36f, 0.05f);
	constexpr float GoldenShare = 0.12f;
}

ARRRandomAcornRain::ARRRandomAcornRain()
{
	// (round 100: twice as far across, the user's "범위를 두배로"; more acorns a second to go with it)
	AreaRadius = 1300.f;
	CursorHoloColor = FLinearColor(0.9f, 0.5f, 0.1f);
	TargetHoloColor = FLinearColor(0.75f, 0.32f, 0.05f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Nut(TEXT("/Game/Effects/Acorn/SM_AcornNut.SM_AcornNut"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cap(TEXT("/Game/Effects/Acorn/SM_AcornCap.SM_AcornCap"));
	auto MakePool = [this](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* Pool = CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Pool->SetupAttachment(Root);
		Pool->SetUsingAbsoluteLocation(true);
		Pool->SetUsingAbsoluteRotation(true);
		Pool->SetUsingAbsoluteScale(true);
		Pool->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pool->SetCastShadow(false);
		Pool->bAffectDistanceFieldLighting = false;
		Pool->bAffectDynamicIndirectLighting = false;
		Pool->NumCustomDataFloats = 3;
		if (Mesh)
		{
			Pool->SetStaticMesh(Mesh);
		}
		return Pool;
	};
	Nuts = MakePool(TEXT("Nuts"), Nut.Succeeded() ? Nut.Object : nullptr);
	Caps = MakePool(TEXT("Caps"), Cap.Succeeded() ? Cap.Object : nullptr);
}

FString ARRRandomAcornRain::GetStatusLine() const
{
	if (bRaining)
	{
		return FString::Printf(TEXT("%s  %.0fs"), *GetTitle(), FMath::Max(0.f, FMath::CeilToFloat(RainSeconds - RainTime)));
	}
	return FString::Printf(TEXT("%s  [CLICK ANYWHERE]  %ds"), *GetTitle(), FMath::RoundToInt(BaseSeconds) + Face);
}

void ARRRandomAcornRain::OnStarted()
{
	// The pools hold one instance per acorn in flight, parked out of sight until used
	for (UInstancedStaticMeshComponent* Pool : { Nuts.Get(), Caps.Get() })
	{
		const FLinearColor& Color = Pool == Nuts ? NutColor : CapColor;
		for (int32 Index = 0; Index < MaxInFlight; ++Index)
		{
			const int32 Instance = Pool->AddInstance(FTransform(FQuat::Identity, FVector::ZeroVector, FVector(KINDA_SMALL_NUMBER)), true);
			const float Data[] = { Color.R, Color.G, Color.B };
			Pool->SetCustomData(Instance, MakeArrayView(Data, 3));
		}
	}
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->AcornPop(StartFeet + FVector(0.f, 0.f, 150.f), 2.f);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s calls an acorn rain for %.0f s."), *Brawler->GetName(), BaseSeconds + Face);
}

void ARRRandomAcornRain::StartAction(const FVector& Target)
{
	Left = 0;
	MulticastRain(Target, FMath::Rand());
}

void ARRRandomAcornRain::MulticastRain_Implementation(FVector_NetQuantize Spot, int32 Seed)
{
	if (!HasStarted() || bRaining)
	{
		return;
	}
	bRaining = true;
	RainSpot = FindGround(Spot);
	RainSeed = Seed;
	RainTime = 0.f;
	RainSeconds = BaseSeconds + Face;
	AcornCount = FMath::RoundToInt(RainSeconds * AcornsPerSecond);
	NextToLand = 0;
	// A faint mark of the area the whole time it rains, under the acorns
	ShowTargetHolo(RainSpot, AreaRadius, 0.3f);
}

void ARRRandomAcornRain::GetAcorn(int32 Index, float& OutLandTime, FVector& OutWhere, float& OutSpin) const
{
	// Its own little stream, so every machine and the server agree on each acorn
	FRandomStream Stream(RainSeed * 7919 + Index * 104729);
	OutLandTime = FallTime + (Index + Stream.FRandRange(-0.4f, 0.4f)) / AcornsPerSecond;
	// Evenly over the disc
	const float Distance = AreaRadius * FMath::Sqrt(Stream.FRand());
	const float Angle = Stream.FRandRange(0.f, 2.f * PI);
	OutWhere = RainSpot + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, 0.f);
	OutSpin = Stream.FRandRange(-1.f, 1.f);
}

void ARRRandomAcornRain::TickUltimate(float DeltaSeconds)
{
	if (!bRaining)
	{
		return;
	}
	RainTime += DeltaSeconds;
	// The acorns in the air now: those landing within FallTime from now
	int32 Slot = 0;
	for (int32 Index = NextToLand; Index < AcornCount && Slot < MaxInFlight; ++Index)
	{
		float LandTime;
		FVector Where;
		float Spin;
		GetAcorn(Index, LandTime, Where, Spin);
		if (LandTime - RainTime > FallTime)
		{
			break;
		}
		if (LandTime <= RainTime)
		{
			// Down: a pop (and on the server, the hit); acorns land in order, near enough
			if (Index == NextToLand)
			{
				LandAcorn(Where, FRandomStream(RainSeed * 31 + Index * 131).FRand() < GoldenShare);
				++NextToLand;
			}
			continue;
		}
		// Falling faster and faster, tumbling a little
		const float Left01 = (LandTime - RainTime) / FallTime;
		const FVector At = Where + FVector(0.f, 0.f, 25.f + DropHeight * Left01 * Left01);
		const FRotator Turn(Spin * 40.f * Left01, Spin * 360.f * Left01, Spin * 30.f);
		// (its own look, the same on every machine)
		const FRandomStream Look(RainSeed * 31 + Index * 131);
		const bool bGolden = Look.FRand() < GoldenShare;
		const float Size = bGolden ? 1.5f : Look.FRandRange(0.8f, 1.25f);
		const FLinearColor Nut = bGolden ? GoldenNut : NutTones[Look.RandRange(0, UE_ARRAY_COUNT(NutTones) - 1)];
		const FLinearColor Cap = bGolden ? GoldenCap : CapColor;
		const FTransform Place(Turn, At, FVector(AcornScale * Size));
		Nuts->UpdateInstanceTransform(Slot, Place, true, false, true);
		Caps->UpdateInstanceTransform(Slot, Place, true, false, true);
		const float NutData[] = { Nut.R, Nut.G, Nut.B };
		const float CapData[] = { Cap.R, Cap.G, Cap.B };
		Nuts->SetCustomData(Slot, MakeArrayView(NutData, 3));
		Caps->SetCustomData(Slot, MakeArrayView(CapData, 3));
		// A warm streak behind it as it comes down (a golden one's sparkles)
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			const float Was01 = FMath::Min(1.f, Left01 + DeltaSeconds / FallTime);
			Effects->AcornFall(Where + FVector(0.f, 0.f, 25.f + DropHeight * Was01 * Was01), At, Size, bGolden, DeltaSeconds);
		}
		++Slot;
	}
	// The rest of the pool out of sight
	const FTransform Parked(FQuat::Identity, FVector::ZeroVector, FVector(KINDA_SMALL_NUMBER));
	for (; Slot < MaxInFlight; ++Slot)
	{
		Nuts->UpdateInstanceTransform(Slot, Parked, true, false, true);
		Caps->UpdateInstanceTransform(Slot, Parked, true, false, true);
	}
	Nuts->MarkRenderStateDirty();
	Caps->MarkRenderStateDirty();

	if (NextToLand >= AcornCount)
	{
		bRaining = false;
		ReleaseTargetHolo();
		if (HasAuthority())
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: the acorn rain at %s is over: %d acorns, %d hits."), *RainSpot.ToCompactString(), AcornCount, AcornHits);
			Finish();
		}
	}
}

void ARRRandomAcornRain::LandAcorn(const FVector& Where, bool bGolden)
{
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->AcornPop(Where + FVector(0.f, 0.f, 20.f), bGolden ? 1.7f : 1.f);
		if (bGolden)
		{
			Effects->StarImpact(Where + FVector(0.f, 0.f, 20.f), FVector::UpVector, 1.2f, GoldenNut);
		}
	}
	if (!HasAuthority() || !Brawler)
	{
		return;
	}
	for (ARRRandomCharacter* Victim : FindEnemiesNear(Where, HitRadius, DropHeight))
	{
		// Bonked and sent skidding off the way the acorn bounced
		const FVector Out = ((Victim->GetActorLocation() - Where) * FVector(1.f, 1.f, 0.f)).GetSafeNormal();
		const FVector Way = Out.IsNearlyZero() ? FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f).Vector() : Out;
		FHitResult Hit(Victim, Victim->GetCapsuleComponent(), Victim->GetActorLocation(), -Way);
		UGameplayStatics::ApplyPointDamage(Victim, AcornDamage, Way, Hit, Brawler->GetController(), this, UDamageType::StaticClass());
		if (Victim->IsAlive())
		{
			Victim->LaunchCharacter(Way * SkidSpeed + FVector(0.f, 0.f, 160.f), true, true);
		}
		UE_LOG(LogTemp, Verbose, TEXT("RRRandom: acorn hits %s, health %.0f."), *Victim->GetName(), Victim->GetHealth());
		++AcornHits;
	}
}
