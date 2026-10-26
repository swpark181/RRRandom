#include "RRRandomLava.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceTown.h"
#include "RRRandomEffects.h"
#include "RRRandomHUD.h"
#include "RRRandomUIStyle.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "NavAreas/NavArea_Null.h"

namespace
{
	const TCHAR* const CubeMesh = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* const LavaMaterial = TEXT("/Game/Effects/Lava/M_Lava.M_Lava");

	/** The fountain: blobs flung this far out at most, this many, landing this long after the burst. */
	constexpr int32 BlobCount = 7;
	constexpr float BlobReach = 420.f;
	constexpr float BlobFlight = 0.55f;
	/** Feet this far over the lava's top still burn (wading in it). */
	constexpr float WadeHeight = 25.f;
	/** Shares of a column's rise that count as lava for burns. */
	constexpr float BurnRise = 0.6f;
	constexpr float HopEvery = 0.6f;
	constexpr float HotEvery = 0.7f;
	/** Columns per light, and how bright they glow. */
	constexpr int32 ColumnsPerLight = 10;
	constexpr float LightIntensity = 9000.f;
	constexpr float LightRadius = 1100.f;
	const FLinearColor LightColor(1.f, 0.42f, 0.22f);

	uint32 Hash(int32 X, int32 Y, int32 Z)
	{
		uint32 H = uint32(X) * 73856093u ^ uint32(Y) * 19349663u ^ uint32(Z) * 83492791u;
		H ^= H >> 13;
		H *= 0x5bd1e995u;
		return H ^ (H >> 15);
	}
}

ARRRandomLava::ARRRandomLava()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
}

void ARRRandomLava::BeginPlay()
{
	Super::BeginPlay();
	Surface = NewObject<UInstancedStaticMeshComponent>(this);
	Surface->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CubeMesh));
	if (UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, LavaMaterial))
	{
		Surface->SetMaterial(0, Material);
	}
	Surface->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Surface->SetGenerateOverlapEvents(false);
	Surface->SetCanEverAffectNavigation(false);
	Surface->SetCastShadow(false);
	Surface->SetMobility(EComponentMobility::Movable);
	Surface->SetupAttachment(RootComponent);
	Surface->SetUsingAbsoluteLocation(true);
	Surface->SetUsingAbsoluteRotation(true);
	Surface->SetUsingAbsoluteScale(true);
	Surface->RegisterComponent();
	Surface->SetWorldTransform(FTransform::Identity);
	if (GetNetMode() != NM_DedicatedServer)
	{
		for (int32 Index = 0; Index < MaxLights; ++Index)
		{
			UPointLightComponent* Light = NewObject<UPointLightComponent>(this);
			Light->SetupAttachment(RootComponent);
			Light->SetUsingAbsoluteLocation(true);
			Light->SetLightColor(LightColor);
			Light->SetAttenuationRadius(LightRadius);
			Light->SetCastShadows(false);
			Light->SetIntensity(0.f);
			Light->SetVisibility(false);
			Light->RegisterComponent();
			Lights.Add(Light);
		}
	}
}

void ARRRandomLava::SetColumns(const TArray<FRRLavaColumn>& InColumns, float InTopZ, const TArray<FVector>& NewBreaches)
{
	const float Now = GetWorld()->GetTimeSeconds();
	TopZ = InTopZ;
	TSet<FIntPoint> Wanted;
	for (const FRRLavaColumn& In : InColumns)
	{
		Wanted.Add(In.Cell);
		FColumn* Column = Columns.Find(In.Cell);
		if (!Column)
		{
			// New: it rises with the breakthrough's glow and burst first
			Columns.Add(In.Cell, { In.Cell, In.Middle, In.FloorTop, Now, -1.f });
		}
		else
		{
			Column->FloorTop = In.FloorTop;
			if (Column->Gone >= 0.f)
			{
				// Flooding again while it sank: it rises from where it had sunk to
				const float Left = RiseOf(*Column, Now);
				Column->Gone = -1.f;
				Column->Born = Now - GlowTime - RiseTime * Left;
			}
		}
	}
	for (TPair<FIntPoint, FColumn>& Pair : Columns)
	{
		if (!Wanted.Contains(Pair.Key) && Pair.Value.Gone < 0.f)
		{
			Pair.Value.Gone = Now;
		}
	}
	for (const FVector& Breach : NewBreaches)
	{
		Fountains.Add({ Breach, Now });
		UE_LOG(LogTemp, Log, TEXT("RRRandom: lava breaks through at %s."), *Breach.ToCompactString());
	}
	bNavDirty = true;
	UE_LOG(LogTemp, Log, TEXT("RRRandom: lava in %d columns (%d drawn)."), InColumns.Num(), Columns.Num());
}

float ARRRandomLava::RiseOf(const FColumn& Column, float Now) const
{
	const float Up = FMath::Clamp((Now - Column.Born - GlowTime) / RiseTime, 0.f, 1.f);
	if (Column.Gone < 0.f)
	{
		return FMath::InterpEaseOut(0.f, 1.f, Up, 2.f);
	}
	const float Down = FMath::Clamp((Now - Column.Gone) / SinkTime, 0.f, 1.f);
	return Up * (1.f - Down);
}

bool ARRRandomLava::IsRisenAt(const FIntPoint& Cell) const
{
	const FColumn* Column = Columns.Find(Cell);
	return Column && Column->Gone < 0.f && RiseOf(*Column, GetWorld()->GetTimeSeconds()) >= BurnRise;
}

void ARRRandomLava::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	// Sunk away for good
	bool bRemoved = false;
	for (auto It = Columns.CreateIterator(); It; ++It)
	{
		if (It.Value().Gone >= 0.f && Now - It.Value().Gone >= SinkTime)
		{
			It.RemoveCurrent();
			bRemoved = true;
		}
	}
	bNavDirty |= bRemoved;
	if (bNavDirty && GetNetMode() != NM_Client)
	{
		UpdateNavBlockers();
	}
	bNavDirty = false;
	UpdateSurface(Now);
	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateLights(Now);
		UpdateEffects(DeltaSeconds, Now);
	}
	BurnPeople(Now);
	Embers.RemoveAll([Now](const FEmber& Ember) { return Now >= Ember.Until; });
	Fountains.RemoveAll([Now](const FFountain& Fountain) { return Now - Fountain.Start > GlowTime + RiseTime + 1.f; });
}

void ARRRandomLava::UpdateSurface(float Now)
{
	if (!Surface)
	{
		return;
	}
	// The same columns as drawn: just move them; otherwise lay them out again
	bool bSame = Drawn.Num() == Columns.Num();
	for (int32 Index = 0; bSame && Index < Drawn.Num(); ++Index)
	{
		bSame = Columns.Contains(Drawn[Index]);
	}
	if (!bSame)
	{
		Drawn.Reset();
		Columns.GetKeys(Drawn);
		Drawn.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.Y != B.Y ? A.Y < B.Y : A.X < B.X; });
		Surface->ClearInstances();
		TArray<FTransform> Hidden;
		Hidden.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), Drawn.Num());
		Surface->AddInstances(Hidden, false, true, false);
	}
	for (int32 Index = 0; Index < Drawn.Num(); ++Index)
	{
		const FColumn& Column = Columns[Drawn[Index]];
		// A box from the crater's floor up to as far as it has risen, its top a hair wobbly (a slow swell)
		const float Swell = 4.f * FMath::Sin(Now * 1.3f + Column.Cell.X * 0.7f + Column.Cell.Y * 1.1f);
		const float Height = FMath::Max(0.5f, (TopZ + Swell - Column.FloorTop) * RiseOf(Column, Now));
		const FVector Middle(Column.Middle.X, Column.Middle.Y, Column.FloorTop + Height * 0.5f);
		Surface->UpdateInstanceTransform(Index, FTransform(FQuat::Identity, Middle, FVector(1.002f, 1.002f, Height / 100.f)), true, false, false);
	}
	if (Drawn.Num() > 0 || !bSame)
	{
		Surface->MarkRenderStateDirty();
	}
}

void ARRRandomLava::UpdateLights(float Now)
{
	// Spread over the lava: every so many columns of it in its drawn order, flickering; a breakthrough's glow first
	int32 Used = 0;
	for (const FFountain& Fountain : Fountains)
	{
		if (Used >= Lights.Num())
		{
			break;
		}
		const float Age = Now - Fountain.Start;
		const float Flare = Age < GlowTime ? Age / GlowTime : FMath::Max(0.f, 1.f - (Age - GlowTime) / (RiseTime + 1.f));
		if (Flare <= 0.f)
		{
			continue;
		}
		UPointLightComponent* Light = Lights[Used++];
		Light->SetWorldLocation(Fountain.Where + FVector(0.f, 0.f, 120.f));
		Light->SetIntensity(LightIntensity * 2.5f * Flare);
		Light->SetVisibility(true);
	}
	const int32 Count = FMath::Min(Lights.Num() - Used, FMath::DivideAndRoundUp(Drawn.Num(), ColumnsPerLight));
	for (int32 Pick = 0; Pick < Count; ++Pick)
	{
		const FColumn& Column = Columns[Drawn[(Pick * Drawn.Num()) / Count + Drawn.Num() / Count / 2]];
		const float Rise = RiseOf(Column, Now);
		const float Flicker = 0.82f + 0.12f * FMath::Sin(Now * 7.3f + Pick * 2.1f) + 0.06f * FMath::Sin(Now * 17.f + Pick);
		UPointLightComponent* Light = Lights[Used++];
		Light->SetWorldLocation(FVector(Column.Middle.X, Column.Middle.Y, TopZ + 140.f));
		Light->SetIntensity(LightIntensity * Rise * Flicker);
		Light->SetVisibility(Rise > 0.f);
	}
	for (; Used < Lights.Num(); ++Used)
	{
		Lights[Used]->SetVisibility(false);
	}
}

void ARRRandomLava::UpdateEffects(float DeltaSeconds, float Now)
{
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	if (!Effects)
	{
		return;
	}
	for (FFountain& Fountain : Fountains)
	{
		const float Age = Now - Fountain.Start;
		if (Age < GlowTime)
		{
			// The cracks glowing: fire licking up out of the floor
			Effects->GroundFire(Fountain.Where, 160.f, Age / GlowTime, DeltaSeconds);
			continue;
		}
		if (!Fountain.bBurst)
		{
			Fountain.bBurst = true;
			Effects->LavaFountain(Fountain.Where, 1.f);
			ARRRandomHUD::PopComicWord(this, TEXT("BLOOP!"), Fountain.Where + FVector(0.f, 0.f, 260.f), RRUI::PinkDeep, 1.5f);
			// The blobs land round it (the same spots on every machine: the server's embers burn)
			const FIntPoint Seed(FMath::RoundToInt(Fountain.Where.X), FMath::RoundToInt(Fountain.Where.Y));
			for (int32 Blob = 0; Blob < BlobCount; ++Blob)
			{
				const uint32 Roll = Hash(Seed.X, Seed.Y, Blob);
				const float Angle = (Blob + float(Roll % 1000) / 1000.f * 0.6f) * UE_TWO_PI / BlobCount;
				const float Out = BlobReach * (0.45f + 0.55f * float((Roll >> 10) % 1000) / 1000.f);
				// (On the floor round the crater: the fountain springs from the lava's top, LavaBelowFloor under it)
				const FVector Spot = Fountain.Where + FVector(FMath::Cos(Angle) * Out, FMath::Sin(Angle) * Out, ARRRandomDiceTown::LavaBelowFloor + 8.f);
				Embers.Add({ Spot, Now + BlobFlight, Now + BlobFlight + EmberSeconds });
			}
		}
	}
	for (const FEmber& Ember : Embers)
	{
		if (Now >= Ember.From)
		{
			Effects->GroundEmbers(Ember.Where, EmberRadius * 0.8f, FMath::Clamp((Ember.Until - Now) / EmberSeconds, 0.2f, 1.f), DeltaSeconds);
		}
	}
	// Bubbles popping and sparks rising off the risen lava, a few a second over all of it
	BubbleClock -= DeltaSeconds;
	if (BubbleClock <= 0.f && Drawn.Num() > 0)
	{
		BubbleClock = FMath::Max(0.03f, 0.5f / FMath::Sqrt(float(Drawn.Num())));
		const FColumn& Column = Columns[Drawn[FMath::RandRange(0, Drawn.Num() - 1)]];
		if (RiseOf(Column, Now) >= 0.9f)
		{
			const FVector2D Wobble = FMath::RandPointInCircle(35.f);
			Effects->LavaBubble(FVector(Column.Middle.X + Wobble.X, Column.Middle.Y + Wobble.Y, TopZ + 4.f), FMath::FRandRange(0.6f, 1.2f));
		}
	}
}

void ARRRandomLava::BurnPeople(float Now)
{
	const ARRRandomDiceTown* Town = Cast<ARRRandomDiceTown>(GetOwner());
	if (!Town || (Columns.Num() == 0 && Embers.Num() == 0 && LastInLava.Num() == 0))
	{
		return;
	}
	const bool bServer = HasAuthority() && GetNetMode() != NM_Client;
	BurnClock += GetWorld()->GetDeltaSeconds();
	const bool bTick = BurnClock >= BurnTick;
	if (bTick)
	{
		BurnClock -= BurnTick;
	}
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Who = *It;
		if (!Who->IsAlive())
		{
			LastInLava.Remove(Who);
			continue;
		}
		const float Half = Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const FVector Feet = Who->GetActorLocation() - FVector(0.f, 0.f, Half);
		const bool bInLava = Feet.Z < TopZ + WadeHeight && IsRisenAt(Town->GetCellAt(Feet));
		// Every machine: HOT! over who is in it (now and then)
		if (bInLava && GetNetMode() != NM_DedicatedServer)
		{
			float& Said = LastHot.FindOrAdd(Who, -100.f);
			if (Now - Said >= HotEvery)
			{
				Said = Now;
				ARRRandomHUD::PopComicWord(this, TEXT("HOT!"), Feet + FVector(0.f, 0.f, Half * 2.f + 70.f), RRUI::PinkDeep, 1.1f);
			}
		}
		if (!bServer)
		{
			continue;
		}
		if (bInLava)
		{
			LastInLava.Add(Who, Now);
			float& Hopped = LastHop.FindOrAdd(Who, -100.f);
			if (Now - Hopped >= HopEvery)
			{
				Hopped = Now;
				Who->LaunchCharacter(FVector(0.f, 0.f, HopSpeed), false, true);
			}
		}
		if (!bTick)
		{
			continue;
		}
		float Damage = 0.f;
		if (bInLava)
		{
			Damage += BurnPerSecond * BurnTick;
		}
		else if (const float* Was = LastInLava.Find(Who))
		{
			if (Now - *Was <= AfterBurnSeconds)
			{
				Damage += AfterBurnPerSecond * BurnTick;
			}
			else
			{
				LastInLava.Remove(Who);
			}
		}
		for (const FEmber& Ember : Embers)
		{
			if (Now >= Ember.From && FVector::Dist2D(Feet, Ember.Where) <= EmberRadius && FMath::Abs(Feet.Z - Ember.Where.Z) < 150.f)
			{
				Damage += EmberDamage;
				break;
			}
		}
		if (Damage > 0.f)
		{
			UGameplayStatics::ApplyDamage(Who, Damage, nullptr, this, UDamageType::StaticClass());
		}
	}
}

void ARRRandomLava::UpdateNavBlockers()
{
	for (UBoxComponent* Box : NavBlockers)
	{
		if (Box)
		{
			Box->DestroyComponent();
		}
	}
	NavBlockers.Reset();
	// Rows of columns side by side along X, each a box the navmesh leaves out (bots go round)
	TArray<FIntPoint> Cells;
	for (const TPair<FIntPoint, FColumn>& Pair : Columns)
	{
		if (Pair.Value.Gone < 0.f)
		{
			Cells.Add(Pair.Key);
		}
	}
	Cells.Sort([](const FIntPoint& A, const FIntPoint& B) { return A.Y != B.Y ? A.Y < B.Y : A.X < B.X; });
	for (int32 Start = 0; Start < Cells.Num(); )
	{
		int32 End = Start;
		while (End + 1 < Cells.Num() && Cells[End + 1].Y == Cells[Start].Y && Cells[End + 1].X == Cells[End].X + 1)
		{
			++End;
		}
		const FColumn& First = Columns[Cells[Start]];
		const FColumn& Last = Columns[Cells[End]];
		const FVector2D Middle = (First.Middle + Last.Middle) * 0.5f;
		UBoxComponent* Box = NewObject<UBoxComponent>(this);
		Box->SetupAttachment(RootComponent);
		Box->SetUsingAbsoluteLocation(true);
		Box->SetUsingAbsoluteRotation(true);
		Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Box->SetGenerateOverlapEvents(false);
		Box->SetBoxExtent(FVector(50.f * (End - Start + 1), 50.f, 250.f));
		Box->bDynamicObstacle = true;
		Box->SetAreaClassOverride(UNavArea_Null::StaticClass());
		Box->SetCanEverAffectNavigation(true);
		Box->SetWorldLocation(FVector(Middle.X, Middle.Y, TopZ));
		Box->RegisterComponent();
		NavBlockers.Add(Box);
		Start = End + 1;
	}
}
