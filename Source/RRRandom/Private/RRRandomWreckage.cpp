#include "RRRandomWreckage.h"
#include "RRRandomCharacter.h"
#include "RRRandomCover.h"
#include "RRRandomDiceLand.h"
#include "RRRandomDiceTown.h"
#include "RRRandomEffects.h"
#include "RRRandomStarRobot.h"
#include "RRRandomVehicle.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

namespace
{
	TAutoConsoleVariable<float> CVarRestoreSeconds(TEXT("rr.Wreck.RestoreSeconds"), 15.f,
		TEXT("Seconds a smashed structure stays smashed at least; then it mends as soon as nobody is in it."));

	/** Room kept round who's in a wreck before it may mend: animal friends (plus their size), cars, the robot. */
	constexpr float PeopleRoom = 120.f;
	constexpr float CarRoom = 320.f;
	constexpr float RobotRoom = 480.f;
	constexpr float CheckEvery = 0.25f;

	/** The rubble: the town's block, lit like it (custom data: body color, then the top's). */
	const TCHAR* const RubbleMesh = TEXT("/Game/Environment/DiceTown/SM_BTBlock.SM_BTBlock");
	constexpr int32 RubblePool = 420;
	constexpr int32 RubblePerBurst = 110;
	constexpr float RubbleGravity = 1900.f;
	constexpr float RubbleShrink = 0.35f;
	/** Seconds a piece takes to grow back into place, and sparkles where things grow back (at most). */
	constexpr float GrowSeconds = 0.4f;
	constexpr int32 GrowSparkles = 8;

	/** Dice Land's plaza (its pedestal under the spinning die) stays. */
	constexpr float LandKeepRadius = 400.f;

	const FLinearColor Soil(0.32f, 0.17f, 0.07f);
	const FLinearColor Grass(0.2f, 0.6f, 0.08f);
	const FLinearColor Candy[] = { FLinearColor(1.f, 0.45f, 0.62f), FLinearColor(1.f, 0.62f, 0.3f), FLinearColor(1.f, 0.88f, 0.3f),
		FLinearColor(0.35f, 0.9f, 0.6f), FLinearColor(0.35f, 0.75f, 1.f), FLinearColor(0.62f, 0.5f, 1.f) };

	FTransform Shrunk(const FTransform& Was)
	{
		return FTransform(Was.GetRotation(), Was.GetLocation(), FVector::ZeroVector);
	}

	FTransform Stowed(const FTransform& Was)
	{
		return FTransform(Was.GetRotation(), Was.GetLocation() - FVector(0.f, 0.f, 200000.f), Was.GetScale3D());
	}

	/** A wreck reaches into a box: the box's point nearest the line's point nearest the box's middle. */
	bool Touches(const FRRWreck& Wreck, const FBox& Box)
	{
		const FVector Middle = Box.GetCenter();
		const FVector Line = Wreck.To - Wreck.From;
		const float Alpha = Line.SizeSquared() > 1.f ? FMath::Clamp(FVector::DotProduct(Middle - Wreck.From, Line) / Line.SizeSquared(), 0.f, 1.f) : 0.f;
		FVector Near = Wreck.From + Line * Alpha;
		if (Wreck.bColumn)
		{
			Near.Z = Middle.Z;
		}
		return Wreck.Contains(Box.GetClosestPointTo(Near));
	}

	/** Bouncy ease: overshoots a little, then settles. */
	float BackOut(float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.f, 1.f) - 1.f;
		constexpr float Over = 1.9f;
		return 1.f + T * T * ((Over + 1.f) * T + Over);
	}
}

float FRRWreck::Distance(const FVector& Spot) const
{
	FVector A = From;
	FVector B = To;
	FVector P = Spot;
	if (bColumn)
	{
		A.Z = B.Z = P.Z = 0.f;
	}
	return FMath::PointDistToSegment(P, A, B) - Radius;
}

FBox FRRWreck::GetBounds(float Low, float High) const
{
	FBox Box(From.ComponentMin(To) - FVector(Radius), From.ComponentMax(To) + FVector(Radius));
	if (bColumn)
	{
		Box.Min.Z = Low;
		Box.Max.Z = High;
	}
	return Box;
}

ARRRandomWreckage::ARRRandomWreckage()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
}

float ARRRandomWreckage::GetRestoreSeconds()
{
	return FMath::Max(1.f, CVarRestoreSeconds.GetValueOnGameThread());
}

void ARRRandomWreckage::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomWreckage, Wrecks);
}

void ARRRandomWreckage::Wreck(const UObject* WorldContext, const FVector& From, const FVector& To, float Radius, bool bColumn)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World || !World->IsGameWorld() || World->GetNetMode() == NM_Client || Radius <= 0.f)
	{
		return;
	}
	ARRRandomWreckage* Wreckage = nullptr;
	for (TActorIterator<ARRRandomWreckage> It(World); It; ++It)
	{
		Wreckage = *It;
		break;
	}
	if (!Wreckage)
	{
		Wreckage = World->SpawnActor<ARRRandomWreckage>(ARRRandomWreckage::StaticClass(), FTransform::Identity);
		if (!Wreckage)
		{
			return;
		}
	}
	FRRWreck& Added = Wreckage->Wrecks.AddDefaulted_GetRef();
	Added.Id = Wreckage->NextId++;
	Added.From = From;
	Added.To = To;
	Added.Radius = Radius;
	Added.bColumn = bColumn;
	Wreckage->MadeAt.Add(Added.Id, World->GetTimeSeconds());
	UE_LOG(LogTemp, Log, TEXT("RRRandom: wreck %d, %s from %s to %s, radius %.0f."), Added.Id, bColumn ? TEXT("a column") : TEXT("a ball"),
		*From.ToCompactString(), *To.ToCompactString(), Radius);
	Wreckage->Apply();
	Wreckage->ForceNetUpdate();
}

void ARRRandomWreckage::BeginPlay()
{
	Super::BeginPlay();
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, RubbleMesh);
	if (!Mesh)
	{
		return;
	}
	Rubble = NewObject<UInstancedStaticMeshComponent>(this);
	Rubble->SetStaticMesh(Mesh);
	Rubble->NumCustomDataFloats = 6;
	Rubble->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Rubble->SetGenerateOverlapEvents(false);
	Rubble->SetCanEverAffectNavigation(false);
	Rubble->SetMobility(EComponentMobility::Movable);
	Rubble->SetupAttachment(RootComponent);
	Rubble->SetUsingAbsoluteLocation(true);
	Rubble->SetUsingAbsoluteRotation(true);
	Rubble->SetUsingAbsoluteScale(true);
	Rubble->RegisterComponent();
	Rubble->SetWorldTransform(FTransform::Identity);
	TArray<FTransform> Hidden;
	Hidden.Init(FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), RubblePool);
	Rubble->AddInstances(Hidden, false, true, false);
	for (int32 Slot = RubblePool - 1; Slot >= 0; --Slot)
	{
		FreeSlots.Add(Slot);
	}
}

void ARRRandomWreckage::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Chunks.Reset();
	Growing.Reset();
	Super::EndPlay(EndPlayReason);
}

void ARRRandomWreckage::OnRep_Wrecks()
{
	Apply();
}

bool ARRRandomWreckage::FindPieces()
{
	if (bFoundPieces)
	{
		return true;
	}
	UWorld* World = GetWorld();
	ARRRandomDiceLand* Land = nullptr;
	for (TActorIterator<ARRRandomDiceLand> It(World); It; ++It)
	{
		Land = *It;
		break;
	}
	if (Land && !Land->IsBuilt())
	{
		return false;
	}
	bFoundPieces = true;

	// The island's (and any map's) cover blocks: each goes whole
	for (TActorIterator<ARRRandomCover> It(World); It; ++It)
	{
		FPiece& Piece = Pieces.AddDefaulted_GetRef();
		Piece.Cover = *It;
	}

	// Dice Land: each hidden box and the dice, balls and trunks drawn over it (a tree's leaves over its trunk)
	UInstancedStaticMeshComponent* Solid = Land ? Land->GetWreckSolid() : nullptr;
	if (Solid)
	{
		const FVector Middle = Land->GetActorLocation();
		const int32 First = Pieces.Num();
		for (int32 Index = 0; Index < Solid->GetInstanceCount(); ++Index)
		{
			FTransform Box;
			Solid->GetInstanceTransform(Index, Box, true);
			if (FVector::Dist2D(Box.GetLocation(), Middle) < LandKeepRadius)
			{
				continue;
			}
			FPiece& Piece = Pieces.AddDefaulted_GetRef();
			Piece.Box = Solid;
			Piece.BoxIndex = Index;
			Piece.BoxTransform = Box;
		}
		TArray<UInstancedStaticMeshComponent*> LookSets;
		Land->GetWreckLooks(LookSets);
		int32 LookCount = 0;
		for (UInstancedStaticMeshComponent* Instances : LookSets)
		{
			for (int32 Index = 0; Instances && Index < Instances->GetInstanceCount(); ++Index)
			{
				FTransform Look;
				Instances->GetInstanceTransform(Index, Look, true);
				int32 Best = INDEX_NONE;
				float BestDistance = TNumericLimits<float>::Max();
				for (int32 PieceIndex = First; PieceIndex < Pieces.Num(); ++PieceIndex)
				{
					const FTransform& Box = Pieces[PieceIndex].BoxTransform;
					const FVector Half = Box.GetScale3D() * 50.f;
					const FVector Rel = Box.GetRotation().UnrotateVector(Look.GetLocation() - Box.GetLocation());
					const bool bInside = FMath::Abs(Rel.X) <= Half.X + 40.f && FMath::Abs(Rel.Y) <= Half.Y + 40.f && FMath::Abs(Rel.Z) <= Half.Z + 60.f;
					// (A trunk's leaves: over the top of a thin box, a little out to the sides)
					const bool bLeaves = FMath::Max(Half.X, Half.Y) < 80.f && FMath::Abs(Rel.X) <= Half.X + 180.f && FMath::Abs(Rel.Y) <= Half.Y + 180.f
						&& Rel.Z >= Half.Z - 20.f && Rel.Z <= Half.Z + 450.f;
					const float Distance = Rel.Size();
					if ((bInside || bLeaves) && Distance < BestDistance)
					{
						Best = PieceIndex;
						BestDistance = Distance;
					}
				}
				if (Best != INDEX_NONE)
				{
					Pieces[Best].Looks.Add({ Instances, Index, Look });
					++LookCount;
				}
			}
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: dice land wrecks: %d pieces with %d looks."), Pieces.Num() - First, LookCount);
	}
	return true;
}

void ARRRandomWreckage::Apply()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	ARRRandomDiceTown* Town = nullptr;
	for (TActorIterator<ARRRandomDiceTown> It(World); It; ++It)
	{
		Town = *It;
		break;
	}
	if ((Town && !Town->IsBuilt()) || (!Town && !FindPieces()))
	{
		bPending = true;
		return;
	}
	bPending = false;

	// Whatever is still growing back is done at once (what goes again now goes from where it stands)
	TSet<UInstancedStaticMeshComponent*> Dirty;
	for (const FGrowing& Grow : Growing)
	{
		if (UInstancedStaticMeshComponent* Instances = Grow.Regrow.Instances.Get())
		{
			Instances->UpdateInstanceTransform(Grow.Regrow.Index, Grow.Regrow.Target, true, false, false);
			Dirty.Add(Instances);
		}
	}
	Growing.Reset();

	TArray<FRRBrokenBit> BrokenBits;
	TArray<FRRRegrow> Regrow;
	if (Town)
	{
		Town->ApplyWrecks(Wrecks, BrokenBits, Regrow);
	}
	int32 Changed = 0;
	for (FPiece& Piece : Pieces)
	{
		FBox Bounds(ForceInit);
		if (ARRRandomCover* Cover = Piece.Cover.Get())
		{
			Bounds = Cover->GetComponentsBoundingBox(true);
		}
		else if (Piece.Box.IsValid())
		{
			Bounds = FBox(FVector(-50.f), FVector(50.f)).TransformBy(Piece.BoxTransform);
		}
		else
		{
			continue;
		}
		bool bHit = false;
		for (const FRRWreck& Wreck : Wrecks)
		{
			if (Touches(Wreck, Bounds))
			{
				bHit = true;
				break;
			}
		}
		if (bHit == Piece.bBroken)
		{
			continue;
		}
		Piece.bBroken = bHit;
		++Changed;
		if (ARRRandomCover* Cover = Piece.Cover.Get())
		{
			Cover->SetActorHiddenInGame(bHit);
			Cover->SetActorEnableCollision(!bHit);
			if (bHit)
			{
				// Soil blocks with grass on top, from all over it
				const FVector Extent = Bounds.GetExtent();
				const int32 Count = FMath::Clamp(FMath::RoundToInt(Extent.X * Extent.Y * Extent.Z / 120000.f), 4, 24);
				for (int32 Bit = 0; Bit < Count; ++Bit)
				{
					BrokenBits.Add({ FMath::RandPointInBox(Bounds), Soil, Grass, 1.1f });
				}
			}
			else if (GetNetMode() != NM_DedicatedServer)
			{
				if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
				{
					Effects->DiceLandBurst(Bounds.GetCenter(), FMath::Min(Bounds.GetExtent().GetMax() * 0.3f, 50.f));
				}
			}
			continue;
		}
		UInstancedStaticMeshComponent* Box = Piece.Box.Get();
		Box->UpdateInstanceTransform(Piece.BoxIndex, bHit ? Stowed(Piece.BoxTransform) : Piece.BoxTransform, true, false, true);
		for (const FPiece::FLook& Look : Piece.Looks)
		{
			UInstancedStaticMeshComponent* Instances = Look.Instances.Get();
			if (!Instances)
			{
				continue;
			}
			Instances->UpdateInstanceTransform(Look.Index, bHit ? Shrunk(Look.Transform) : Look.Transform, true, false, false);
			Dirty.Add(Instances);
			if (!bHit)
			{
				Regrow.Add({ Instances, Look.Index, Look.Transform });
				continue;
			}
			// The rubble takes the look's color (its first three custom floats)
			FLinearColor Body = Candy[Look.Index % UE_ARRAY_COUNT(Candy)];
			const int32 Floats = Instances->NumCustomDataFloats;
			if (Floats >= 3 && Instances->PerInstanceSMCustomData.IsValidIndex(Look.Index * Floats + 2))
			{
				const float* Data = &Instances->PerInstanceSMCustomData[Look.Index * Floats];
				Body = FLinearColor(Data[0], Data[1], Data[2]);
			}
			const float Size = FMath::Clamp(Look.Transform.GetScale3D().GetMax(), 0.6f, 2.2f);
			for (int32 Bit = 0; Bit < 3; ++Bit)
			{
				BrokenBits.Add({ Look.Transform.GetLocation() + FMath::VRand() * 30.f * Size, Body, FMath::Lerp(Body, FLinearColor::White, 0.5f), Size * 0.8f });
			}
		}
	}
	if (Changed > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: wrecks: %d in force, %d pieces changed."), Wrecks.Num(), Changed);
	}

	// Rubble for what broke; what mends grows back with a sparkle (on screens only)
	if (GetNetMode() != NM_DedicatedServer)
	{
		ThrowRubble(BrokenBits);
		ARRRandomEffects* Effects = Regrow.Num() > 0 ? ARRRandomEffects::Get(this) : nullptr;
		for (int32 Sparkle = 0; Effects && Sparkle < FMath::Min(GrowSparkles, Regrow.Num()); ++Sparkle)
		{
			const FRRRegrow& Pick = Regrow[FMath::RandRange(0, Regrow.Num() - 1)];
			Effects->DiceLandBurst(Pick.Target.GetLocation(), 28.f);
		}
		for (const FRRRegrow& Grow : Regrow)
		{
			if (UInstancedStaticMeshComponent* Instances = Grow.Instances.Get())
			{
				Instances->UpdateInstanceTransform(Grow.Index, Shrunk(Grow.Target), true, false, false);
				Dirty.Add(Instances);
				Growing.Add({ Grow, 0.f });
			}
		}
	}
	for (UInstancedStaticMeshComponent* Instances : Dirty)
	{
		Instances->MarkRenderStateDirty();
	}
}

bool ARRRandomWreckage::IsOccupied(const FRRWreck& Wreck) const
{
	UWorld* World = GetWorld();
	for (TActorIterator<ARRRandomCharacter> It(World); It; ++It)
	{
		const UCapsuleComponent* Capsule = It->GetCapsuleComponent();
		const float Room = PeopleRoom + (Capsule ? (Wreck.bColumn ? Capsule->GetScaledCapsuleRadius() : Capsule->GetScaledCapsuleHalfHeight()) : 100.f);
		if (Wreck.Contains(It->GetActorLocation(), Room))
		{
			return true;
		}
	}
	// A car with someone in it is bigger than its rider (a parked one doesn't hold a wreck: it isn't anybody)
	for (TActorIterator<ARRRandomVehicle> It(World); It; ++It)
	{
		if (!It->IsCombined() && It->GetRider() && Wreck.Contains(It->GetActorLocation(), CarRoom))
		{
			return true;
		}
	}
	// The robot too, while it's coming together or anyone is aboard
	for (TActorIterator<ARRRandomStarRobot> It(World); It; ++It)
	{
		if ((It->CountCrew() > 0 || (!It->IsAssembled() && It->GetCombiner())) && Wreck.Contains(It->GetActorLocation(), RobotRoom))
		{
			return true;
		}
	}
	return false;
}

void ARRRandomWreckage::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPending)
	{
		Apply();
	}
	TickRubble(DeltaSeconds);
	TickGrowing(DeltaSeconds);

	// Server: mend each wreck that has stood long enough once nobody is in it
	if (!HasAuthority())
	{
		return;
	}
	CheckTimer -= DeltaSeconds;
	if (CheckTimer > 0.f || Wrecks.Num() == 0)
	{
		return;
	}
	CheckTimer = CheckEvery;
	const float Now = GetWorld()->GetTimeSeconds();
	const float Wait = GetRestoreSeconds();
	int32 Mended = 0;
	for (int32 Index = Wrecks.Num() - 1; Index >= 0; --Index)
	{
		const FRRWreck& Wreck = Wrecks[Index];
		const float* Made = MadeAt.Find(Wreck.Id);
		if (Made && Now - *Made < Wait)
		{
			continue;
		}
		if (IsOccupied(Wreck))
		{
			continue;
		}
		MadeAt.Remove(Wreck.Id);
		Wrecks.RemoveAt(Index);
		++Mended;
	}
	if (Mended > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %d wrecks mend, nobody in them (%d left)."), Mended, Wrecks.Num());
		Apply();
		ForceNetUpdate();
	}
}

void ARRRandomWreckage::ThrowRubble(const TArray<FRRBrokenBit>& Bits)
{
	if (!Rubble || Bits.Num() == 0)
	{
		return;
	}
	// The floor they land on: the bottom of the lowest piece that broke
	float FloorZ = TNumericLimits<float>::Max();
	FVector Middle = FVector::ZeroVector;
	for (const FRRBrokenBit& Bit : Bits)
	{
		FloorZ = FMath::Min(FloorZ, Bit.Where.Z - 50.f * Bit.Size);
		Middle += Bit.Where;
	}
	Middle /= Bits.Num();
	const int32 Count = FMath::Min(RubblePerBurst, Bits.Num() * 2);
	for (int32 Index = 0; Index < Count && FreeSlots.Num() > 0; ++Index)
	{
		const FRRBrokenBit& Bit = Bits[Bits.Num() <= Count ? Index % Bits.Num() : FMath::RandRange(0, Bits.Num() - 1)];
		FChunk Chunk;
		Chunk.Slot = FreeSlots.Pop(EAllowShrinking::No);
		Chunk.Location = Bit.Where + FMath::VRand() * 25.f * Bit.Size;
		// Out from the middle of what broke, and up
		const FVector Out = ((Bit.Where - Middle) * FVector(1.f, 1.f, 0.f)).GetSafeNormal();
		const FVector Way = (Out.IsNearlyZero() ? FMath::VRand() * FVector(1.f, 1.f, 0.f) : Out + FMath::VRand() * FVector(0.6f, 0.6f, 0.f)).GetSafeNormal();
		Chunk.Velocity = Way * FMath::FRandRange(180.f, 620.f) + FVector(0.f, 0.f, FMath::FRandRange(250.f, 820.f));
		Chunk.Turn = FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f));
		Chunk.Spin = FRotator(FMath::FRandRange(-540.f, 540.f), FMath::FRandRange(-540.f, 540.f), FMath::FRandRange(-540.f, 540.f));
		Chunk.Size = Bit.Size * FMath::FRandRange(0.26f, 0.5f);
		Chunk.Age = 0.f;
		Chunk.Life = FMath::FRandRange(1.3f, 2.2f);
		Chunk.FloorZ = FloorZ;
		const float Shade = FMath::FRandRange(0.88f, 1.08f);
		const float Data[6] = { Bit.Body.R * Shade, Bit.Body.G * Shade, Bit.Body.B * Shade, Bit.Accent.R * Shade, Bit.Accent.G * Shade, Bit.Accent.B * Shade };
		Rubble->SetCustomData(Chunk.Slot, MakeArrayView(Data, 6));
		Chunks.Add(Chunk);
	}
}

void ARRRandomWreckage::TickRubble(float DeltaSeconds)
{
	if (!Rubble || Chunks.Num() == 0)
	{
		return;
	}
	for (int32 Index = Chunks.Num() - 1; Index >= 0; --Index)
	{
		FChunk& Chunk = Chunks[Index];
		Chunk.Age += DeltaSeconds;
		if (Chunk.Age >= Chunk.Life)
		{
			Rubble->UpdateInstanceTransform(Chunk.Slot, FTransform(FQuat::Identity, FVector::ZeroVector, FVector::ZeroVector), true, false, false);
			FreeSlots.Add(Chunk.Slot);
			Chunks.RemoveAtSwap(Index);
			continue;
		}
		Chunk.Velocity.Z -= RubbleGravity * DeltaSeconds;
		Chunk.Location += Chunk.Velocity * DeltaSeconds;
		Chunk.Turn += Chunk.Spin * DeltaSeconds;
		// Bounces on the floor, slower each time
		const float Half = Chunk.Size * 50.f;
		if (Chunk.Location.Z < Chunk.FloorZ + Half && Chunk.Velocity.Z < 0.f)
		{
			Chunk.Location.Z = Chunk.FloorZ + Half;
			Chunk.Velocity = FVector(Chunk.Velocity.X * 0.55f, Chunk.Velocity.Y * 0.55f, -Chunk.Velocity.Z * 0.3f);
			Chunk.Spin *= 0.5f;
		}
		const float Left = Chunk.Life - Chunk.Age;
		const float Scale = Chunk.Size * (Left < RubbleShrink ? Left / RubbleShrink : 1.f);
		Rubble->UpdateInstanceTransform(Chunk.Slot, FTransform(Chunk.Turn, Chunk.Location, FVector(Scale)), true, false, false);
	}
	Rubble->MarkRenderStateDirty();
}

void ARRRandomWreckage::TickGrowing(float DeltaSeconds)
{
	if (Growing.Num() == 0)
	{
		return;
	}
	TSet<UInstancedStaticMeshComponent*> Dirty;
	for (int32 Index = Growing.Num() - 1; Index >= 0; --Index)
	{
		FGrowing& Grow = Growing[Index];
		UInstancedStaticMeshComponent* Instances = Grow.Regrow.Instances.Get();
		if (!Instances)
		{
			Growing.RemoveAtSwap(Index);
			continue;
		}
		Grow.Age += DeltaSeconds;
		const float Alpha = Grow.Age / GrowSeconds;
		FTransform Now = Grow.Regrow.Target;
		if (Alpha < 1.f)
		{
			Now.SetScale3D(Grow.Regrow.Target.GetScale3D() * FMath::Max(0.f, BackOut(Alpha)));
		}
		Instances->UpdateInstanceTransform(Grow.Regrow.Index, Now, true, false, false);
		Dirty.Add(Instances);
		if (Alpha >= 1.f)
		{
			Growing.RemoveAtSwap(Index);
		}
	}
	for (UInstancedStaticMeshComponent* Instances : Dirty)
	{
		Instances->MarkRenderStateDirty();
	}
}
