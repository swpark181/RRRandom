#include "RRRandomDefense.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomDiceTown.h"
#include "RRRandomEffects.h"
#include "RRRandomGameMode.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "RRRandomSpecies.h"
#include "RRRandomStarRobot.h"
#include "RRRandomUIStyle.h"
#include "RRRandomVehicle.h"
#include "RRRandomVillainAI.h"
#include "Algo/MinElement.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The town's blocks (char-pipeline/scripts/blender_block_town.py): 100 units, the pivot in the middle; the block material
	// reads custom floats 0 to 2 as the body's linear color, 3 to 5 as the top's
	constexpr float BlockSize = 100.f;
	UStaticMesh* LoadTownMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Environment/DiceTown/%s.%s"), Name, Name));
	}

	UInstancedStaticMeshComponent* MakeLooks(AActor* Owner, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* Instances = NewObject<UInstancedStaticMeshComponent>(Owner, NAME_None, RF_Transient);
		// Movable: placed at the origin afterwards (a static one ignores the move, and its instances would land twice as far out)
		Instances->SetMobility(EComponentMobility::Movable);
		Instances->SetStaticMesh(Mesh);
		Instances->NumCustomDataFloats = 6;
		Instances->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Instances->SetGenerateOverlapEvents(false);
		Instances->SetCanEverAffectNavigation(false);
		Instances->SetupAttachment(Owner->GetRootComponent());
		Instances->RegisterComponent();
		return Instances;
	}

	void AddLook(UInstancedStaticMeshComponent* Instances, const FTransform& Relative, const FColor& Body, const FColor& Accent, bool bWorldSpace = false)
	{
		const int32 Index = Instances->AddInstance(Relative, bWorldSpace);
		const FLinearColor A(Body);
		const FLinearColor B(Accent);
		const float Data[6] = { A.R, A.G, A.B, B.R, B.G, B.B };
		Instances->SetCustomData(Index, MakeArrayView(Data, 6));
	}

	FColor Shade(const FColor& Color, float Share)
	{
		return FColor(uint8(FMath::Clamp(Color.R * Share, 0.f, 255.f)), uint8(FMath::Clamp(Color.G * Share, 0.f, 255.f)), uint8(FMath::Clamp(Color.B * Share, 0.f, 255.f)));
	}

	/** The attacker behind a hit: a shot's or a bite's brawler. */
	const ARRRandomCharacter* GetAttacker(AController* EventInstigator, AActor* DamageCauser)
	{
		const ARRRandomCharacter* Attacker = DamageCauser ? Cast<ARRRandomCharacter>(DamageCauser->GetInstigator()) : nullptr;
		if (!Attacker && DamageCauser)
		{
			Attacker = Cast<ARRRandomCharacter>(DamageCauser);
		}
		if (!Attacker && EventInstigator)
		{
			Attacker = Cast<ARRRandomCharacter>(EventInstigator->GetPawn());
		}
		return Attacker;
	}

	// The tower: a pedestal of two tiers (10 and 8 cells across, a block high each), the die 6 blocks across floating over it
	constexpr int32 DieSize = 6;
	constexpr float DieFloat = 50.f;
	constexpr float DieBob = 18.f;
	constexpr float DieSpinRate = 9.f;
	const FColor TierLow(176, 132, 255);
	const FColor TierHigh(255, 118, 170);
	const FColor TowerDie(252, 250, 245);
	const FColor TowerPips(235, 62, 120);

	// A gate door: a row of planks per level of the gateway, this high each (filling it), this many blocks thick; mended
	// planks grow back in this long; pieces breaking off fall this fast and are gone after this long
	constexpr int32 DoorRows = RRDefense::GateLevels;
	constexpr float DoorRowHeight = 96.f;
	constexpr float DoorThickness = 1.2f;
	constexpr float WallGrowTime = 0.45f;
	constexpr float WallPieceGravity = 2200.f;
	constexpr float WallPieceLife = 1.8f;
	// An open gate's door sinks this far into the floor, taking this long to open or shut
	constexpr float DoorSink = DoorRows * DoorRowHeight + 15.f;
	constexpr float DoorOpenTime = 0.9f;

	// The villains: each wave's rats more, tougher, harder biting and bigger; their color goes round these (grey, brown,
	// green, blue, violet, crimson, gold); the stages tougher still. The cats: wave 4's and wave 7's boss.
	const FColor WaveTints[] = { FColor(150, 150, 165), FColor(165, 110, 70), FColor(90, 175, 90), FColor(70, 130, 220), FColor(150, 90, 220),
		FColor(220, 60, 70), FColor(240, 190, 50) };
	constexpr float StageToughness[] = { 1.f, 1.25f, 1.55f };
	constexpr int32 MaxAliveVillains = 30;
	constexpr float GateScatter = 160.f;
	// Drops: a rat now and then leaves a die; a boss three, one of them an ultimate die
	constexpr float RatDieChance = 0.18f;
	constexpr int32 BossDice = 3;
	constexpr float VillainClearDelay = 3.f;
	// Shutting for the next wave, a gate comes back by this share of its health on top of what it had (a broken one comes
	// back at this share), never past full
	constexpr float WallMendShare = 0.4f;
	constexpr float WallMendCap = 1.f;
	// The star team's cars for a boss wave: parked outside the ring on the tower's +X side
	constexpr float StarCarsOut = 2000.f;
	constexpr float StarCarGap = 500.f;

	// A die a villain dropped: thrown up and out, then pulled to the nearest animal friend this close, picked up this close
	constexpr float PickupGravity = 1800.f;
	constexpr float PickupPullRadius = 900.f;
	constexpr float PickupPullSpeed = 1500.f;
	constexpr float PickupTakeRadius = 90.f;
	constexpr float PickupLife = 25.f;
	constexpr float PickupSettle = 0.6f;
}

// ---------------------------------------------------------------------------------------------------------------------
// The tower

ARRRandomDiceTower::ARRRandomDiceTower()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	// One hidden box over the pedestal and the die stops everything (shots, brawlers, villains)
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
	Solid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Solid"));
	Solid->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Solid->SetStaticMesh(CubeMesh.Object);
	}
	Solid->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Solid->SetHiddenInGame(true);
	Solid->SetCanEverAffectNavigation(false);
	Solid->SetMobility(EComponentMobility::Movable);
}

ARRRandomDiceTower* ARRRandomDiceTower::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	TActorIterator<ARRRandomDiceTower> It(const_cast<UWorld*>(World));
	return World && It ? *It : nullptr;
}

float ARRRandomDiceTower::GetHalfWidth()
{
	return RRDefense::TowerHalfCells * BlockSize;
}

FVector ARRRandomDiceTower::GetTop() const
{
	return GetActorLocation() + FVector(0.f, 0.f, 2.f * BlockSize + DieFloat + DieSize * BlockSize);
}

void ARRRandomDiceTower::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomDiceTower, Health);
	DOREPLIFETIME(ARRRandomDiceTower, MaxHealth);
}

void ARRRandomDiceTower::BeginPlay()
{
	Super::BeginPlay();
	// The actor stands on the floor at the middle; the box covers the pedestal and the die (scaled from the 100 cube)
	const float Height = 2.f * BlockSize + DieFloat + DieSize * BlockSize;
	const float Width = 2.f * GetHalfWidth();
	Solid->SetRelativeScale3D(FVector(Width / BlockSize, Width / BlockSize, Height / BlockSize));
	Solid->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
	Build();
	LastHealth = Health;
}

void ARRRandomDiceTower::Build()
{
	UStaticMesh* BlockMesh = LoadTownMesh(TEXT("SM_BTBlock"));
	UStaticMesh* DieMesh = LoadTownMesh(TEXT("SM_BTDieBlock"));
	if (!BlockMesh || !DieMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: the dice tower's meshes are missing (/Game/Environment/DiceTown)."));
		return;
	}
	// Placed relative to the box (which is scaled): the looks go on the actor's own scale instead
	Blocks = MakeLooks(this, BlockMesh);
	Die = MakeLooks(this, DieMesh);
	for (UInstancedStaticMeshComponent* Looks : { Blocks.Get(), Die.Get() })
	{
		Looks->SetUsingAbsoluteScale(true);
		Looks->SetUsingAbsoluteLocation(true);
		Looks->SetUsingAbsoluteRotation(true);
		Looks->SetWorldTransform(FTransform::Identity);
	}
	const FVector Base = GetActorLocation();
	const int32 Half = RRDefense::TowerHalfCells;
	for (int32 Tier = 0; Tier < 2; ++Tier)
	{
		const int32 Reach = Half - Tier;
		const FColor Body = Tier == 0 ? TierLow : TierHigh;
		for (int32 X = -Reach; X < Reach; ++X)
		{
			for (int32 Y = -Reach; Y < Reach; ++Y)
			{
				// Only the rim shows (the inside is under the next tier or the die), a checker of two shades
				if (X > -Reach && X < Reach - 1 && Y > -Reach && Y < Reach - 1 && Tier == 0)
				{
					continue;
				}
				const FColor Top = ((X + Y) & 1) ? Body : Shade(Body, 1.12f);
				AddLook(Blocks, FTransform(Base + FVector((X + 0.5f) * BlockSize, (Y + 0.5f) * BlockSize, (Tier + 0.5f) * BlockSize)), Shade(Body, 0.85f), Top, true);
			}
		}
	}
	DieBase = Base + FVector(0.f, 0.f, 2.f * BlockSize + DieFloat + DieSize * BlockSize * 0.5f);
	AddLook(Die, FTransform(FRotator::ZeroRotator, DieBase, FVector(DieSize * 0.98f)), TowerDie, TowerPips, true);
	Blocks->MarkRenderStateDirty();
}

void ARRRandomDiceTower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Die || Die->GetInstanceCount() == 0)
	{
		return;
	}
	// Turning slowly and bobbing; a hit shakes it for a moment
	Spin += DieSpinRate * DeltaSeconds;
	HitShake = FMath::Max(0.f, HitShake - DeltaSeconds * 3.f);
	const float Time = GetWorld()->GetTimeSeconds();
	const FVector Shake = HitShake > 0.f ? FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f) * 14.f * HitShake : FVector::ZeroVector;
	const float Size = bBurst ? 0.f : DieSize * 0.98f * (1.f + 0.04f * HitShake);
	Die->UpdateInstanceTransform(0, FTransform(FRotator(0.f, Spin, 0.f), DieBase + FVector(0.f, 0.f, FMath::Sin(Time * 1.3f) * DieBob) + Shake, FVector(Size)), true, true);
}

float ARRRandomDiceTower::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Only the villains hurt it
	const ARRRandomCharacter* Attacker = GetAttacker(EventInstigator, DamageCauser);
	if (!HasAuthority() || IsDestroyed() || DamageAmount <= 0.f || !Attacker || !Attacker->IsVillain() || ARRRandomGameState::IsPlayLocked(this))
	{
		return 0.f;
	}
	Health = FMath::Max(0.f, Health - DamageAmount);
	MulticastHit(Attacker->GetActorLocation());
	OnRep_Health();
	if (IsDestroyed())
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: the dice tower falls (last bite from %s)."), *Attacker->GetName());
		MulticastBurst();
		if (ARRRandomDefenseDirector* Director = ARRRandomDefenseDirector::Get(this))
		{
			Director->OnTowerDestroyed();
		}
	}
	return DamageAmount;
}

void ARRRandomDiceTower::OnRep_Health()
{
	if (Health < LastHealth)
	{
		HitShake = 1.f;
	}
	LastHealth = Health;
}

void ARRRandomDiceTower::MulticastHit_Implementation(FVector_NetQuantize Where)
{
	HitShake = 1.f;
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		// Sparks on the pedestal's side toward the biter
		const FVector Toward = (FVector(Where) - GetActorLocation()).GetSafeNormal2D();
		Effects->HitBurst(GetActorLocation() + Toward * GetHalfWidth() + FVector(0.f, 0.f, 120.f), Toward, 0.6f, FLinearColor(1.f, 0.4f, 0.6f));
	}
}

void ARRRandomDiceTower::MulticastBurst_Implementation()
{
	bBurst = true;
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->UltimateDieBurst(DieBase, DieSize * BlockSize * 0.6f, GetActorLocation());
		Effects->FoxPoof(DieBase, 6.f);
	}
	ARRRandomHUD::PopComicWord(this, TEXT("CRASH!"), GetTop() + FVector(0.f, 0.f, 200.f), FLinearColor(1.f, 0.3f, 0.4f), 2.4f);
}

// ---------------------------------------------------------------------------------------------------------------------
// A wall

ARRRandomDefenseWall::ARRRandomDefenseWall()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	// The box that stops villains (only them): hidden, its own object type so shots, sight and the floor traces skip it
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
	Stopper = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stopper"));
	Stopper->SetupAttachment(RootComponent);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		Stopper->SetStaticMesh(CubeMesh.Object);
	}
	Stopper->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Stopper->SetCollisionObjectType(ECC_WorldDynamic);
	Stopper->SetCollisionResponseToAllChannels(ECR_Ignore);
	Stopper->SetCollisionResponseToChannel(RRDefense::VillainChannel, ECR_Block);
	// (and shots, and the animal friends and their rides: a shut gate stops everyone alike)
	Stopper->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Stopper->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	// (and a blast's reach: splash damage traces this channel, so a shut gate shields what is behind it)
	Stopper->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Stopper->SetGenerateOverlapEvents(false);
	Stopper->SetHiddenInGame(true);
	Stopper->SetCanEverAffectNavigation(false);
	Stopper->SetMobility(EComponentMobility::Movable);
}

void ARRRandomDefenseWall::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomDefenseWall, Health);
	DOREPLIFETIME(ARRRandomDefenseWall, MaxHealth);
	DOREPLIFETIME(ARRRandomDefenseWall, Length);
	DOREPLIFETIME(ARRRandomDefenseWall, Color);
	DOREPLIFETIME(ARRRandomDefenseWall, bOpen);
}

void ARRRandomDefenseWall::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ARRRandomDefenseWall::Build()
{
	UStaticMesh* BlockMesh = LoadTownMesh(TEXT("SM_BTBlock"));
	if (!BlockMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("RRRandom: the wall's block is missing (/Game/Environment/DiceTown)."));
		return;
	}
	// Placed in the world as they are (the component at the origin before any instance goes in)
	for (TObjectPtr<UInstancedStaticMeshComponent>* Looks : { &Blocks, &Rubble })
	{
		*Looks = MakeLooks(this, BlockMesh);
		(*Looks)->SetUsingAbsoluteLocation(true);
		(*Looks)->SetUsingAbsoluteRotation(true);
		(*Looks)->SetUsingAbsoluteScale(true);
		(*Looks)->SetWorldTransform(FTransform::Identity);
	}
	Rubble->CastShadow = false;

	// A gate door: rows of planks a cell wide each across the gateway (+Y), a little thicker than a block: dark and light
	// wood, an iron band through the middle row, its top row edged in its color (gold)
	const int32 Cells = FMath::Max(1, FMath::RoundToInt(Length / BlockSize));
	const float Step = Length / Cells;
	const FTransform Frame = GetActorTransform();
	const FColor WoodLight(205, 140, 82);
	const FColor WoodDeep(160, 98, 56);
	const FColor Iron(120, 116, 140);
	// The order they break off in: the top row, then the middle, then the bottom, each in a scrambled order (the same on
	// every machine: seeded by where the door stands)
	FRandomStream Shuffle(GetTypeHash(FIntPoint(FMath::RoundToInt(GetActorLocation().X), FMath::RoundToInt(GetActorLocation().Y))));
	TArray<int32> Tiers[DoorRows];
	for (int32 Row = 0; Row < DoorRows; ++Row)
	{
		for (int32 Cell = 0; Cell < Cells; ++Cell)
		{
			const FVector Local(0.f, (Cell + 0.5f) * Step - Length * 0.5f, (Row + 0.5f) * DoorRowHeight);
			Spots.Add(FTransform(Frame.GetRotation(), Frame.TransformPosition(Local), FVector(DoorThickness, Step / BlockSize * 0.98f, DoorRowHeight / BlockSize * 0.98f)));
			const FColor Plank = Row == DoorRows / 2 ? Iron : (Cell & 1) ? WoodLight : WoodDeep;
			Bodies.Add(Shade(Plank, 0.9f));
			Crests.Add(Row == DoorRows - 1 ? Color : Plank);
			Tiers[Row].Add(Spots.Num() - 1);
		}
	}
	for (int32 Row = DoorRows - 1; Row >= 0; --Row)
	{
		for (int32 Index = Tiers[Row].Num() - 1; Index > 0; --Index)
		{
			Tiers[Row].Swap(Index, Shuffle.RandRange(0, Index));
		}
		BreakOrder.Append(Tiers[Row]);
	}
	for (int32 Block = 0; Block < Spots.Num(); ++Block)
	{
		AddLook(Blocks, Spots[Block], Bodies[Block], Crests[Block], true);
	}
	// (the colors went in without telling the renderer: once for all of them)
	Blocks->MarkRenderStateDirty();
	Shown.Init(true, Spots.Num());
	Growth.Init(1.f, Spots.Num());
	bBuilt = true;
	Refresh(false);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s at %s, %d blocks, %.0f long."), *GetName(), *GetActorLocation().ToCompactString(), Blocks->GetInstanceCount(), Length);
}

void ARRRandomDefenseWall::Refresh(bool bShow)
{
	// The box: the actor stands on the floor at the door's middle, the door along its +Y; it stops everyone while the door is
	// shut and standing (open for the break, or broken, nothing)
	if (!IsBroken() && !bOpen)
	{
		const float Height = DoorRows * DoorRowHeight;
		Stopper->SetRelativeScale3D(FVector(DoorThickness, Length / BlockSize, Height / BlockSize));
		Stopper->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.5f));
		Stopper->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	else
	{
		Stopper->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (!bBuilt)
	{
		return;
	}

	// The blocks the health keeps: the first in the breaking order go first; down, none
	const int32 Total = BreakOrder.Num();
	const int32 Keep = IsBroken() ? 0 : FMath::Clamp(FMath::CeilToInt(Total * Health / FMath::Max(1.f, MaxHealth)), 1, Total);
	int32 Broke = 0;
	for (int32 Place = 0; Place < Total; ++Place)
	{
		const int32 Block = BreakOrder[Place];
		const bool bWanted = Place >= Total - Keep;
		if (bWanted == Shown[Block])
		{
			continue;
		}
		Shown[Block] = bWanted;
		if (bWanted)
		{
			// Mended: grows back up (at once when nobody is watching it happen)
			Growth[Block] = bShow ? 0.f : 1.f;
		}
		else if (bShow)
		{
			ThrowBlock(Block, IsBroken() ? 1.4f : 1.f);
			++Broke;
		}
		const FTransform& Spot = Spots[Block];
		Blocks->UpdateInstanceTransform(Block, GetBlockTransform(Block, bWanted ? FMath::Max(Growth[Block], 0.01f) : 0.001f), true, true);
	}
	SetActorTickEnabled(true);

	// Down: dust all along it and a word
	if (bShow && Broke > 0)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->DiceLandBurst(GetActorLocation() + FVector(0.f, 0.f, DoorRowHeight), FMath::Min(Length * 0.15f * Broke, Length * 0.6f));
			if (IsBroken())
			{
				for (const float Along : { -0.35f, 0.f, 0.35f })
				{
					Effects->FoxPoof(GetActorLocation() + GetActorRightVector() * Length * Along + FVector(0.f, 0.f, 60.f), 2.2f);
				}
			}
		}
		if (IsBroken())
		{
			ARRRandomHUD::PopComicWord(this, TEXT("CRUMBLE!"), GetActorLocation() + FVector(0.f, 0.f, 300.f), FLinearColor(1.f, 0.6f, 0.3f), 1.4f);
		}
	}
}

void ARRRandomDefenseWall::ThrowBlock(int32 Block, float Strength)
{
	// A copy of the block flies off in toward the tower (the wall's -X faces it), up and a little sideways, tumbling
	const FTransform& Spot = Spots[Block];
	FPiece Piece;
	Piece.Location = Spot.GetLocation();
	Piece.Rotation = Spot.GetRotation();
	Piece.Scale = Spot.GetScale3D() * 0.92f;
	const FVector Inward = -GetActorForwardVector();
	Piece.Velocity = (Inward * FMath::FRandRange(250.f, 550.f) + GetActorRightVector() * FMath::FRandRange(-200.f, 200.f)) * Strength
		+ FVector(0.f, 0.f, FMath::FRandRange(350.f, 650.f));
	Piece.Spin = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal() * FMath::FRandRange(4.f, 9.f);
	Piece.Age = 0.f;
	Piece.Instance = INDEX_NONE;
	const FLinearColor A(Bodies[Block]);
	const FLinearColor B(Crests[Block]);
	const float Data[6] = { A.R, A.G, A.B, B.R, B.G, B.B };
	// Reuse a finished piece's instance, or add one
	for (int32 Index = 0; Index < Rubble->GetInstanceCount() && Piece.Instance == INDEX_NONE; ++Index)
	{
		if (!Pieces.ContainsByPredicate([Index](const FPiece& Other) { return Other.Instance == Index; }))
		{
			Piece.Instance = Index;
		}
	}
	if (Piece.Instance == INDEX_NONE)
	{
		Piece.Instance = Rubble->AddInstance(FTransform(Piece.Rotation, Piece.Location, Piece.Scale), true);
	}
	Rubble->SetCustomData(Piece.Instance, MakeArrayView(Data, 6), true);
	Pieces.Add(Piece);
}

void ARRRandomDefenseWall::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBuilt)
	{
		return;
	}
	bool bBusy = false;
	// Opening (sinking into the floor) or shutting (rising), eased; a puff of dust at its foot as it goes
	const float WantOpen = bOpen ? 1.f : 0.f;
	if (OpenShown != WantOpen)
	{
		OpenShown = FMath::FInterpConstantTo(OpenShown, WantOpen, DeltaSeconds, 1.f / DoorOpenTime);
		for (int32 Block = 0; Block < Spots.Num(); ++Block)
		{
			if (Shown[Block])
			{
				Blocks->UpdateInstanceTransform(Block, GetBlockTransform(Block, FMath::Max(Growth[Block], 0.01f)), true, Block == Spots.Num() - 1);
			}
		}
		Blocks->MarkRenderStateDirty();
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this); Effects && FMath::FRand() < DeltaSeconds * 6.f)
		{
			Effects->Footstep(GetActorLocation() + GetActorRightVector() * FMath::FRandRange(-0.5f, 0.5f) * Length, 1.4f);
		}
		bBusy = true;
	}
	// Mended blocks pop back up, a little past their size and back
	for (int32 Block = 0; Block < Spots.Num(); ++Block)
	{
		if (!Shown[Block] || Growth[Block] >= 1.f)
		{
			continue;
		}
		// The lower blocks first
		Growth[Block] = FMath::Min(1.f, Growth[Block] + DeltaSeconds / WallGrowTime);
		const float T = Growth[Block];
		const float Pop = T < 0.7f ? T / 0.7f * 1.15f : FMath::Lerp(1.15f, 1.f, (T - 0.7f) / 0.3f);
		Blocks->UpdateInstanceTransform(Block, GetBlockTransform(Block, FMath::Max(Pop, 0.01f)), true, true);
		bBusy = true;
	}
	// The pieces: falling, tumbling, a bounce on the floor, shrinking away at the end
	const float FloorZ = GetActorLocation().Z;
	for (int32 Index = Pieces.Num() - 1; Index >= 0; --Index)
	{
		FPiece& Piece = Pieces[Index];
		Piece.Age += DeltaSeconds;
		Piece.Velocity.Z -= WallPieceGravity * DeltaSeconds;
		Piece.Location += Piece.Velocity * DeltaSeconds;
		const float Half = Piece.Scale.Z * BlockSize * 0.5f;
		if (Piece.Location.Z < FloorZ + Half && Piece.Velocity.Z < 0.f)
		{
			Piece.Location.Z = FloorZ + Half;
			Piece.Velocity *= FVector(0.5f, 0.5f, -0.3f);
			Piece.Spin *= 0.5f;
		}
		Piece.Rotation = FQuat(Piece.Spin.GetSafeNormal(), Piece.Spin.Size() * DeltaSeconds) * Piece.Rotation;
		const float Fade = FMath::Clamp((WallPieceLife - Piece.Age) / 0.35f, 0.f, 1.f);
		const bool bGone = Piece.Age >= WallPieceLife;
		Rubble->UpdateInstanceTransform(Piece.Instance, FTransform(Piece.Rotation, Piece.Location, bGone ? FVector(0.001f) : Piece.Scale * Fade), true, true);
		if (bGone)
		{
			Pieces.RemoveAt(Index);
		}
	}
	bBusy |= Pieces.Num() > 0;
	if (!bBusy)
	{
		SetActorTickEnabled(false);
	}
}

float ARRRandomDefenseWall::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const ARRRandomCharacter* Attacker = GetAttacker(EventInstigator, DamageCauser);
	if (!HasAuthority() || IsBroken() || DamageAmount <= 0.f || !Attacker || !Attacker->IsVillain() || ARRRandomGameState::IsPlayLocked(this))
	{
		return 0.f;
	}
	Health = FMath::Max(0.f, Health - DamageAmount);
	MulticastBitten(Attacker->GetActorLocation());
	OnRep_Health();
	UE_CLOG(IsBroken(), LogTemp, Log, TEXT("RRRandom: %s broken by %s."), *GetName(), *Attacker->GetName());
	return DamageAmount;
}

void ARRRandomDefenseWall::MulticastBitten_Implementation(FVector_NetQuantize Where)
{
	// Sparks and grit off the wall's face toward the biter
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);
	if (!Effects)
	{
		return;
	}
	const FVector Out = GetActorForwardVector() * FMath::Sign(FVector::DotProduct(FVector(Where) - GetActorLocation(), GetActorForwardVector()));
	const float Along = FMath::Clamp(FVector::DotProduct(FVector(Where) - GetActorLocation(), GetActorRightVector()), -Length * 0.5f, Length * 0.5f);
	const FVector Face = GetActorLocation() + GetActorRightVector() * Along + Out * DoorThickness * BlockSize * 0.5f + FVector(0.f, 0.f, DoorRowHeight * 0.8f);
	Effects->HitBurst(Face, Out, 0.7f, FLinearColor(1.f, 0.85f, 0.6f));
	Effects->Footstep(Face - FVector(0.f, 0.f, DoorRowHeight * 0.6f), 1.2f);
}

bool ARRRandomDefenseWall::Mend(float Share, float Cap)
{
	if (!HasAuthority() || Health >= MaxHealth * Cap)
	{
		return false;
	}
	Health = FMath::Min(MaxHealth * Cap, Health + MaxHealth * Share);
	OnRep_Health();
	return true;
}

void ARRRandomDefenseWall::OnRep_Health()
{
	Refresh(true);
}

// ---------------------------------------------------------------------------------------------------------------------
// A jump ramp

ARRRandomJumpRamp::ARRRandomJumpRamp()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
	// What a ride drives up (the slope) and the block under its high end, both hidden boxes that stop everything
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	for (TObjectPtr<UStaticMeshComponent>* Box : { &Slope, &Support })
	{
		*Box = CreateDefaultSubobject<UStaticMeshComponent>(Box == &Slope ? TEXT("Slope") : TEXT("Support"));
		(*Box)->SetupAttachment(RootComponent);
		if (CubeMesh.Succeeded())
		{
			(*Box)->SetStaticMesh(CubeMesh.Object);
		}
		(*Box)->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		(*Box)->SetHiddenInGame(true);
		(*Box)->SetCanEverAffectNavigation(false);
		(*Box)->SetMobility(EComponentMobility::Movable);
	}
}

void ARRRandomJumpRamp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomJumpRamp, Length);
	DOREPLIFETIME(ARRRandomJumpRamp, LandDistance);
}

void ARRRandomJumpRamp::BeginPlay()
{
	Super::BeginPlay();
	// The slope: a slab from the foot (the actor, on the floor) up Rise to the lip, Length along +X; its top surface is the ramp
	constexpr float Thickness = 40.f;
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, Length));
	const float Slant = FMath::Sqrt(FMath::Square(Length) + FMath::Square(Rise));
	const FVector Down = FRotator(Pitch, 0.f, 0.f).RotateVector(FVector::UpVector) * (-Thickness * 0.5f);
	Slope->SetRelativeLocationAndRotation(FVector(Length * 0.5f, 0.f, Rise * 0.5f) + Down, FRotator(Pitch, 0.f, 0.f));
	Slope->SetRelativeScale3D(FVector(Slant / BlockSize, Width / BlockSize, Thickness / BlockSize));
	// (under the slab at its high end, its top below the slab's underside there)
	const float SupportTop = Rise * (Length - 120.f) / Length - Thickness * 1.2f;
	Support->SetRelativeLocation(FVector(Length - 60.f, 0.f, SupportTop * 0.5f));
	Support->SetRelativeScale3D(FVector(1.2f, Width / BlockSize, FMath::Max(SupportTop, 10.f) / BlockSize));
	// (only what is under the slab: a ride's body never catches on it)
	Support->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// The looks (round 90, a speed booster): a deep violet wedge of the town's blocks with lavender sides, and on it in
	// glowing blocks chevrons pointing at the lip, neon rails along both edges, a gold lip, and more chevrons on the floor
	// leading up to it; a pulse of light runs up them toward the lip (Tick)
	UStaticMesh* BlockMesh = LoadTownMesh(TEXT("SM_BTBlock"));
	if (!BlockMesh)
	{
		return;
	}
	for (TObjectPtr<UInstancedStaticMeshComponent>* Set : { &Looks, &Glows })
	{
		*Set = MakeLooks(this, BlockMesh);
		(*Set)->SetUsingAbsoluteLocation(true);
		(*Set)->SetUsingAbsoluteRotation(true);
		(*Set)->SetUsingAbsoluteScale(true);
		(*Set)->SetWorldTransform(FTransform::Identity);
	}
	// Lit from within (the block material's Glow, as the town's stars)
	Glows->CastShadow = false;
	for (int32 Slot = 0; Slot < Glows->GetNumMaterials(); ++Slot)
	{
		if (UMaterialInstanceDynamic* Lit = UMaterialInstanceDynamic::Create(Glows->GetMaterial(Slot), this))
		{
			Lit->SetScalarParameterValue(TEXT("Glow"), RampGlow);
			Glows->SetMaterial(Slot, Lit);
		}
	}
	const FTransform Frame = GetActorTransform();
	const FQuat Tilt = FRotator(Pitch, 0.f, 0.f).Quaternion();
	const FVector SlopeUp = Tilt.RotateVector(FVector::UpVector);
	constexpr int32 Stripes = 6;
	const FColor Deep(70, 48, 150);
	const FColor Violet(104, 72, 200);
	const FColor Lavender(176, 132, 255);
	for (int32 Stripe = 0; Stripe < Stripes; ++Stripe)
	{
		// A slanted plate on top, and a column under it down to the floor
		const float Along = (Stripe + 0.5f) / Stripes;
		const float Top = Rise * Along;
		const FVector Plate = FVector(Length * Along, 0.f, Top) + Down;
		AddLook(Looks, FTransform(Frame.GetRotation() * Tilt, Frame.TransformPosition(Plate),
			FVector(Slant / Stripes / BlockSize, Width / BlockSize, Thickness / BlockSize)), Deep, (Stripe & 1) ? Deep : Violet, true);
		const float Column = FMath::Max(Top - Thickness * 0.5f, 6.f);
		AddLook(Looks, FTransform(Frame.GetRotation(), Frame.TransformPosition(FVector(Length * Along, 0.f, Column * 0.5f)),
			FVector(Length / Stripes / BlockSize, Width / BlockSize * 0.96f, Column / BlockSize)), Shade(Lavender, 0.8f), Lavender, true);
	}

	// A glowing piece: where on the run (local), turned (in the slope's plane or flat), sized, colored
	auto AddGlow = [&](const FVector& Local, const FQuat& Turn, const FVector& Size, const FColor& Color, float Place)
	{
		AddLook(Glows, FTransform(Frame.GetRotation() * Turn, Frame.TransformPosition(Local), Size), Color, Color, true);
		GlowColors.Add(Color);
		GlowPlaces.Add(Place);
	};
	// A chevron: two arms meeting at its point (X along the run), in the slope's plane or flat on the floor
	auto AddChevron = [&](float PointX, bool bOnSlope, const FColor& Color)
	{
		constexpr float Back = 60.f;
		constexpr float Spread = 85.f;
		for (const float Side : { -1.f, 1.f })
		{
			const FVector2D Middle(PointX - Back * 0.5f, Side * Spread * 0.5f);
			const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Side * Spread, Back));
			const float ArmLength = FMath::Sqrt(FMath::Square(Back) + FMath::Square(Spread));
			const FQuat Turn = (bOnSlope ? Tilt : FQuat::Identity) * FRotator(0.f, Yaw, 0.f).Quaternion();
			const FVector Local = bOnSlope ? FVector(Middle.X, Middle.Y, Rise * Middle.X / Length) + SlopeUp * 4.f : FVector(Middle.X, Middle.Y, 3.f);
			AddGlow(Local, Turn, FVector(ArmLength / BlockSize, 0.2f, 0.07f), Color, PointX / Length);
		}
	};
	const FColor Cyan(80, 230, 255);
	const FColor Hot(255, 90, 190);
	const FColor Gold(255, 205, 60);
	for (int32 Chevron = 0; Chevron < 4; ++Chevron)
	{
		AddChevron(Length * (0.25f + 0.22f * Chevron), true, Chevron & 1 ? Hot : Cyan);
	}
	for (int32 Chevron = 1; Chevron <= 3; ++Chevron)
	{
		AddChevron(-Chevron * 130.f + 40.f, false, Gold);
	}
	// The rails: a strip along each top edge, in pieces, cyan and pink by turns
	constexpr int32 RailPieces = 6;
	for (const float Side : { -1.f, 1.f })
	{
		for (int32 Piece = 0; Piece < RailPieces; ++Piece)
		{
			const float X = Length * (Piece + 0.5f) / RailPieces;
			AddGlow(FVector(X, Side * (Width * 0.5f - 14.f), Rise * X / Length) + SlopeUp * 7.f, Tilt,
				FVector(Slant / RailPieces / BlockSize * 0.86f, 0.22f, 0.14f), (Piece & 1) ? Hot : Cyan, X / Length);
		}
	}
	// The lip: a gold bar right across, and two glowing posts at the foot
	AddGlow(FVector(Length - 10.f, 0.f, Rise) + SlopeUp * 6.f, Tilt, FVector(0.2f, Width / BlockSize, 0.14f), Gold, 1.f);
	for (const float Side : { -1.f, 1.f })
	{
		AddGlow(FVector(-40.f, Side * (Width * 0.5f + 30.f), 70.f), FQuat::Identity, FVector(0.35f, 0.35f, 1.4f), Hot, 0.f);
	}
	Looks->MarkRenderStateDirty();
	Glows->MarkRenderStateDirty();
}

void ARRRandomJumpRamp::Animate(float DeltaSeconds)
{
	if (!Glows || GlowColors.Num() == 0)
	{
		return;
	}
	// A pulse of light running up the run toward the lip, over and over: each piece brightest as it passes
	const float Time = GetWorld()->GetTimeSeconds();
	for (int32 Index = 0; Index < GlowColors.Num(); ++Index)
	{
		const float Wave = FMath::Max(0.f, FMath::Cos(UE_TWO_PI * (GlowPlaces[Index] * 0.8f - Time * PulseRate)));
		const float Bright = 0.35f + 0.65f * FMath::Pow(Wave, 6.f);
		const FLinearColor Color = FLinearColor(GlowColors[Index]) * Bright;
		const float Data[6] = { Color.R, Color.G, Color.B, Color.R, Color.G, Color.B };
		Glows->SetCustomData(Index, MakeArrayView(Data, 6), Index == GlowColors.Num() - 1);
	}
	// A glint now and then over the lip (kept to a minimum: the user asked for few stars)
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->DiceSpinSparkle(GetActorTransform().TransformPosition(FVector(Length, 0.f, Rise + 40.f)), Width * 0.3f, DeltaSeconds * 0.1f);
	}
}

void ARRRandomJumpRamp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Animate(DeltaSeconds);
	const float Now = GetWorld()->GetTimeSeconds();
	const FTransform Frame = GetActorTransform();
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Rider = *It;
		// A driver (the one behind is carried along), on its own machine or the server
		if (!Rider->IsAlive() || !Rider->IsDriving() || Rider->IsPiloting() || !(HasAuthority() || Rider->IsLocallyControlled()))
		{
			continue;
		}
		// At the lip, going up it fast enough
		const FVector Local = Frame.InverseTransformPosition(Rider->GetActorLocation());
		const FVector Velocity = Frame.InverseTransformVector(Rider->GetVelocity());
		if (Local.X < Length * 0.35f || Local.X > Length + 60.f || FMath::Abs(Local.Y) > Width * 0.5f + 30.f || Local.Z > Rise + 260.f
			|| Velocity.X < MinSpeed)
		{
			continue;
		}
		if (const float* Last = LastLeap.Find(Rider); Last && Now - *Last < 2.5f)
		{
			continue;
		}
		LastLeap.Add(Rider, Now);
		// Round 91: a heavier fall while it flies, so the arc is low and quick and the ride goes faster than it drives
		Rider->StartRampLeap(LeapGravity);
		// Up Apex over the lip, coming down LandDistance on at the floor (Rise under the lip)
		const float Gravity = FMath::Max(1.f, FMath::Abs(Rider->GetCharacterMovement()->GetGravityZ()));
		const float Up = FMath::Sqrt(2.f * Gravity * Apex);
		const float Flight = (Up + FMath::Sqrt(FMath::Square(Up) + 2.f * Gravity * Rise)) / Gravity;
		const FVector Leap = GetActorForwardVector() * (LandDistance / Flight) + FVector(0.f, 0.f, Up);
		// (the pushes' own launch, without a species' knockback share)
		Rider->ACharacter::LaunchCharacter(Leap, true, true);
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->SonicBoom(Rider->GetActorLocation(), GetActorForwardVector(), FLinearColor(1.f, 0.85f, 0.3f));
		}
		ARRRandomHUD::PopComicWord(this, TEXT("JUMP!"), Rider->GetActorLocation() + FVector(0.f, 0.f, 220.f), RRUI::Gold, 1.3f);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s leaps off the jump ramp at %s (%.0f on, %.0f up)."), *Rider->GetName(), *GetActorLocation().ToCompactString(), LandDistance / Flight, Up);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// A dropped die

ARRRandomDiePickup::ARRRandomDiePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	// One instance of the town's die block (its material takes its colors per instance)
	Mesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DieMesh(TEXT("/Game/Environment/DiceTown/SM_BTDieBlock.SM_BTDieBlock"));
	if (DieMesh.Succeeded())
	{
		Mesh->SetStaticMesh(DieMesh.Object);
	}
	Mesh->NumCustomDataFloats = 6;
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Mesh->SetCanEverAffectNavigation(false);
	SetLifeSpan(PickupLife);
}

void ARRRandomDiePickup::BeginPlay()
{
	Super::BeginPlay();
	// White with pink pips, or gold for an ultimate die
	AddLook(Mesh, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.4f)), bUltimate ? FColor(255, 196, 40) : FColor(252, 250, 245), FColor(235, 62, 120));
}

void ARRRandomDiePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomDiePickup, bUltimate);
}

void ARRRandomDiePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// Turning on every machine
	AddActorLocalRotation(FRotator(0.f, 160.f * DeltaSeconds, 0.f));
	if (!HasAuthority())
	{
		return;
	}

	// Popping out, falling back to the floor
	if (Age < PickupSettle || !Taker.IsValid())
	{
		Velocity.Z -= PickupGravity * DeltaSeconds;
		FVector Next = GetActorLocation() + Velocity * DeltaSeconds;
		FHitResult Floor;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomDiePickup), false, this);
		if (Velocity.Z < 0.f && GetWorld()->LineTraceSingleByObjectType(Floor, GetActorLocation(), Next - FVector(0.f, 0.f, 25.f), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Next = Floor.ImpactPoint + FVector(0.f, 0.f, 25.f);
			Velocity = FVector::ZeroVector;
		}
		SetActorLocation(Next);
	}
	if (Age < PickupSettle)
	{
		if (Velocity.IsNearlyZero() && Age < DeltaSeconds * 2.f)
		{
			Velocity = FVector(FMath::FRandRange(-250.f, 250.f), FMath::FRandRange(-250.f, 250.f), 650.f);
		}
		return;
	}
	// The nearest animal friend close by pulls it in
	if (!Taker.IsValid() || !Taker->IsAlive())
	{
		Taker = nullptr;
		float Best = PickupPullRadius;
		for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
		{
			const float Distance = FVector::Dist(It->GetActorLocation(), GetActorLocation());
			if (It->GetTeam() == 0 && It->IsAlive() && !It->IsDecoy() && !It->IsVillain() && Distance < Best)
			{
				Best = Distance;
				Taker = *It;
			}
		}
	}
	if (ARRRandomCharacter* Friend = Taker.Get())
	{
		const FVector To = Friend->GetActorLocation() - GetActorLocation();
		if (To.Size() <= PickupTakeRadius)
		{
			Friend->GetDiceBuffs()->AddCharge(bUltimate);
			ARRRandomHUD::PopComicWord(this, bUltimate ? TEXT("+ULTIMATE DIE!") : TEXT("+1 DIE"), Friend->GetActorLocation() + FVector(0.f, 0.f, 180.f), bUltimate ? URRRandomDiceBuffComponent::GetUltimateColor() : RRUI::Gold, 0.9f);
			Destroy();
			return;
		}
		SetActorLocation(GetActorLocation() + To.GetSafeNormal() * FMath::Min(To.Size(), PickupPullSpeed * DeltaSeconds));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The waves

ARRRandomDefenseDirector::ARRRandomDefenseDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	// The host's only: everyone sees the waves through the game state and the villains themselves
	bReplicates = false;
}

ARRRandomDefenseDirector* ARRRandomDefenseDirector::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ARRRandomDefenseDirector> It(World);
	return It ? *It : nullptr;
}

void ARRRandomDefenseDirector::Begin(int32 InStage, ARRRandomDiceTown* InTown, ARRRandomDiceTower* InTower, const TArray<ARRRandomDefenseWall*>& InWalls)
{
	Stage = FMath::Clamp(InStage, 1, 3);
	Town = InTown;
	Tower = InTower;
	for (ARRRandomDefenseWall* Wall : InWalls)
	{
		Walls.Add(Wall);
	}
	if (InTown)
	{
		InTown->GetDefenseGates(AllGates);
	}
	Stream.Initialize(9000 + Stage);
	ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	const float PlayStart = State ? State->GetServerWorldTimeSeconds() + State->GetCountdownRemaining() : GetWorld()->GetTimeSeconds();
	NextWaveAt = PlayStart + FirstWaveDelay;
	bBreak = true;
	bBegun = true;
	if (State)
	{
		State->SetWaveBreak(NextWaveAt);
	}
	OpenGatesFor(1);
	// Before the first wave the gates stand open too (a break like the others)
	SetGatesOpen(true);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: defense stage %d, %d gates (%d open for wave 1), %d walls, tower %.0f."), Stage, AllGates.Num(), OpenGates.Num(), Walls.Num(),
		InTower ? InTower->GetMaxHealth() : 0.f);
}

int32 ARRRandomDefenseDirector::CountPlayers() const
{
	const ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	return State ? FMath::Max(1, State->PlayerArray.Num()) : 1;
}

void ARRRandomDefenseDirector::OpenGatesFor(int32 ForWave)
{
	// The four compass roads (round 90: the rush comes from all four sides and makes for the castle's two gates), the diagonals
	// too once there are more players than that (always more ways in than players)
	const int32 Count = FMath::Clamp(FMath::Max(4, CountPlayers() + 1), 1, FMath::Max(1, AllGates.Num()));
	OpenGates.Reset();
	for (int32 Index = 0; Index < Count && Index < AllGates.Num(); ++Index)
	{
		OpenGates.Add(AllGates[Index]);
	}
}

void ARRRandomDefenseDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	if (!bBegun || bOver || !State || State->IsMatchOver())
	{
		return;
	}
	const float Now = State->GetServerWorldTimeSeconds();
	if (bBreak)
	{
		if (Now >= NextWaveAt && State->HasPlayStarted())
		{
			StartWave(Wave + 1);
		}
		return;
	}

	Alive.RemoveAll([](const TWeakObjectPtr<ARRRandomCharacter>& Villain) { return !Villain.IsValid() || !Villain->IsAlive(); });
	SpawnTimer -= DeltaSeconds;
	if (Queue.Num() > 0 && SpawnTimer <= 0.f && Alive.Num() < MaxAliveVillains && OpenGates.Num() > 0)
	{
		SpawnNext();
		// A villain at each open gate about every 1.2 seconds
		SpawnTimer = FMath::Max(0.25f, 1.2f / OpenGates.Num());
	}
	State->SetVillainsLeft(Queue.Num() + Alive.Num());
	// Every 10 seconds of a wave, how it stands (for tests: the closest villain to the tower, the walls down)
	StatusTimer -= DeltaSeconds;
	if (StatusTimer <= 0.f)
	{
		StatusTimer = 10.f;
		float Closest = UE_BIG_NUMBER;
		FVector ClosestAt = FVector::ZeroVector;
		const FVector Center = Tower.IsValid() ? Tower->GetActorLocation() : GetActorLocation();
		for (const TWeakObjectPtr<ARRRandomCharacter>& Villain : Alive)
		{
			if (Villain.IsValid() && FVector::Dist2D(Villain->GetActorLocation(), Center) < Closest)
			{
				Closest = FVector::Dist2D(Villain->GetActorLocation(), Center);
				ClosestAt = Villain->GetActorLocation() - Center;
			}
		}
		int32 Broken = 0;
		float Weakest = 1.f;
		for (const TWeakObjectPtr<ARRRandomDefenseWall>& Wall : Walls)
		{
			Broken += Wall.IsValid() && Wall->IsBroken() ? 1 : 0;
			Weakest = Wall.IsValid() ? FMath::Min(Weakest, Wall->GetHealth() / FMath::Max(1.f, Wall->GetMaxHealth())) : Weakest;
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: defense wave %d: %d to come, %d out (closest %.0f from the tower at %s), tower %.0f, %d walls down (weakest %.0f%%)."), Wave, Queue.Num(), Alive.Num(),
			Alive.Num() > 0 ? Closest : 0.f, *ClosestAt.ToCompactString(), Tower.IsValid() ? Tower->GetHealth() : 0.f, Broken, Weakest * 100.f);
	}
	if (Queue.Num() == 0 && Alive.Num() == 0)
	{
		EndWave();
	}
}

void ARRRandomDefenseDirector::StartWave(int32 NewWave)
{
	Wave = NewWave;
	bBreak = false;
	SpawnTimer = 0.f;
	OpenGatesFor(Wave);
	// The gates shut, each mended by the wave's share on top of what it had left
	int32 Mended = 0;
	for (const TWeakObjectPtr<ARRRandomDefenseWall>& Gate : Walls)
	{
		Mended += Gate.IsValid() && Wave > 1 && Gate->Mend(WallMendShare, WallMendCap) ? 1 : 0;
	}
	SetGatesOpen(false);
	UE_CLOG(Mended > 0, LogTemp, Log, TEXT("RRRandom: %d gates mended as they shut."), Mended);
	// More rats every wave and on later stages; a cat boss a third of the way into waves 4 and 7
	const int32 Rats = FMath::RoundToInt((6 + 4 * (Wave - 1)) * (1.f + 0.25f * (Stage - 1)));
	Queue.Reset();
	for (int32 Index = 0; Index < Rats; ++Index)
	{
		Queue.Add({ RRSpecies::Rat, false });
	}
	const bool bBossWave = Wave == 4 || Wave == ARRRandomGameState::WaveCount;
	if (bBossWave)
	{
		Queue.Insert({ RRSpecies::Cat, true }, Rats / 3);
		SpawnStarCars();
	}
	if (ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>())
	{
		State->SetWave(Wave, Queue.Num());
		State->SetBossOut(false);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: defense wave %d: %d rats%s through %d gates."), Wave, Rats, bBossWave ? TEXT(" and a cat boss") : TEXT(""), OpenGates.Num());
}

void ARRRandomDefenseDirector::EndWave()
{
	UE_LOG(LogTemp, Log, TEXT("RRRandom: defense wave %d beaten (round 90)."), Wave);
	if (Wave == 4 || Wave == ARRRandomGameState::WaveCount)
	{
		RemoveStarCars();
	}
	if (Wave >= ARRRandomGameState::WaveCount)
	{
		bOver = true;
		if (ARRRandomGameMode* Mode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>())
		{
			Mode->EndDefense(true);
		}
		return;
	}
	// A short break: the gates open (everyone can go in and out) till the next wave shuts and mends them
	bBreak = true;
	ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	NextWaveAt = (State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + BreakSeconds;
	SetGatesOpen(true);
	if (State)
	{
		State->SetWaveBreak(NextWaveAt);
		State->SetVillainsLeft(0);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: gates open; wave %d in %.0f seconds."), Wave + 1, BreakSeconds);
}

void ARRRandomDefenseDirector::SpawnNext()
{
	const FQueued What = Queue[0];
	Queue.RemoveAt(0);
	const FVector Gate = OpenGates[NextGate++ % OpenGates.Num()];
	if (ARRRandomCharacter* Villain = SpawnVillain(What, Gate))
	{
		Alive.Add(Villain);
		if (What.bBoss)
		{
			if (ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>())
			{
				State->SetBossOut(true);
			}
			ARRRandomHUD::PopComicWord(this, TEXT("BOSS!"), Villain->GetActorLocation() + FVector(0.f, 0.f, 400.f), FLinearColor(0.75f, 0.35f, 1.f), 2.f);
		}
	}
}

ARRRandomCharacter* ARRRandomDefenseDirector::SpawnVillain(const FQueued& What, const FVector& Gate)
{
	UWorld* World = GetWorld();
	const float Toughness = StageToughness[Stage - 1];
	const int32 Step = Wave - 1;
	float Health;
	float Damage;
	float Size;
	FLinearColor Tint;
	if (What.bBoss)
	{
		// Wave 4's cat a mid boss, wave 7's the real one: bigger, tougher, harder hitting
		const bool bFinal = Wave >= ARRRandomGameState::WaveCount;
		Health = (bFinal ? 25.f : 12.f) * Toughness;
		Damage = (bFinal ? 30.f : 22.f) * Toughness;
		Size = bFinal ? 2.f : 1.5f;
		Tint = FLinearColor::FromSRGBColor(bFinal ? FColor(120, 40, 160) : FColor(70, 60, 90));
	}
	else
	{
		Health = (0.6f + 0.15f * Step) * Toughness;
		Damage = (6.f + 1.5f * Step) * Toughness;
		Size = 0.8f + 0.05f * Step;
		Tint = FLinearColor::FromSRGBColor(WaveTints[FMath::Clamp(Step, 0, UE_ARRAY_COUNT(WaveTints) - 1)]);
	}

	// Scattered round the gate, on the floor there, facing the tower
	const ARRRandomCharacter* Defaults = GetDefault<ARRRandomCharacter>();
	const float HalfHeight = Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() * Size;
	const FVector Center = Tower.IsValid() ? Tower->GetActorLocation() : GetActorLocation();
	FVector Spot = Gate + FVector(Stream.FRandRange(-GateScatter, GateScatter), Stream.FRandRange(-GateScatter, GateScatter), 0.f);
	FHitResult Floor;
	if (World->LineTraceSingleByObjectType(Floor, Spot + FVector(0.f, 0.f, 800.f), Spot - FVector(0.f, 0.f, 1500.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		Spot = Floor.ImpactPoint;
	}
	const FTransform Where(FRotator(0.f, (Center - Spot).Rotation().Yaw, 0.f), Spot + FVector(0.f, 0.f, HalfHeight + 5.f), FVector(Size));
	ARRRandomCharacter* Villain = World->SpawnActorDeferred<ARRRandomCharacter>(ARRRandomCharacter::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Villain)
	{
		return nullptr;
	}
	Villain->SetTeam(1);
	Villain->SetSpecies(What.Species);
	Villain->SetupVillain(Health, Damage, Size, Tint, What.bBoss);
	Villain->AIControllerClass = ARRRandomVillainAI::StaticClass();
	Villain->FinishSpawning(Where);
	Villain->SpawnDefaultController();
	++SpawnSerial;
	UE_LOG(LogTemp, Verbose, TEXT("RRRandom: %s comes in (%s, health %.0f, bite %.0f, size %.2f)."), *Villain->GetName(), RRSpecies::GetAssetName(What.Species),
		Villain->GetMaxHealth(), Damage, Size);
	return Villain;
}

void ARRRandomDefenseDirector::OnVillainDown(ARRRandomCharacter* Villain, ARRRandomCharacter* Attacker)
{
	if (!Villain)
	{
		return;
	}
	Alive.Remove(Villain);
	if (Attacker && Attacker->GetTeam() == 0)
	{
		Attacker->AddKill();
	}
	// Its dice for the animal friends (a boss's are three, one an ultimate die), then it is cleared away
	const int32 Drops = Villain->IsVillainBoss() ? BossDice : Stream.FRand() < RatDieChance ? 1 : 0;
	for (int32 Index = 0; Index < Drops; ++Index)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FTransform Where(Villain->GetActorLocation() + FVector(0.f, 0.f, 60.f));
		if (ARRRandomDiePickup* Pickup = GetWorld()->SpawnActorDeferred<ARRRandomDiePickup>(ARRRandomDiePickup::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Pickup->SetUltimate(Villain->IsVillainBoss() && Index == 0);
			Pickup->FinishSpawning(Where);
		}
	}
	if (Villain->IsVillainBoss())
	{
		if (ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>())
		{
			State->SetBossOut(false);
		}
		ARRRandomHUD::PopComicWord(this, TEXT("BOSS DOWN!"), Villain->GetActorLocation() + FVector(0.f, 0.f, 400.f), RRUI::Gold, 2.f);
	}
	Villain->SetLifeSpan(VillainClearDelay);
}

void ARRRandomDefenseDirector::OnTowerDestroyed()
{
	if (bOver)
	{
		return;
	}
	bOver = true;
	if (ARRRandomGameMode* Mode = GetWorld()->GetAuthGameMode<ARRRandomGameMode>())
	{
		Mode->EndDefense(false);
	}
}

void ARRRandomDefenseDirector::SpawnStarCars()
{
	RemoveStarCars();
	const FVector Center = Tower.IsValid() ? Tower->GetActorLocation() : GetActorLocation();
	const ERRVehicleKind Kinds[] = { ERRVehicleKind::StarChest, ERRVehicleKind::StarArms, ERRVehicleKind::StarLegs };
	const float Offsets[] = { -StarCarGap, StarCarGap, 0.f };
	for (int32 Part = 0; Part < 3; ++Part)
	{
		FVector Spot = Center + FVector(StarCarsOut + (Part == 2 ? StarCarGap : 0.f), Offsets[Part], 0.f);
		FHitResult Floor;
		if (GetWorld()->LineTraceSingleByObjectType(Floor, Spot + FVector(0.f, 0.f, 800.f), Spot - FVector(0.f, 0.f, 1500.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
		{
			Spot = Floor.ImpactPoint;
		}
		const FTransform Where(FRotator::ZeroRotator, Spot);
		if (ARRRandomVehicle* Car = GetWorld()->SpawnActorDeferred<ARRRandomVehicle>(ARRRandomVehicle::StaticClass(), Where, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Car->SetKind(Kinds[Part]);
			Car->SetStarTeam(0);
			Car->FinishSpawning(Where);
			StarCars.Add(Car);
			ARRRandomHUD::PopComicWord(this, TEXT("ROBOT CARS!"), Spot + FVector(0.f, 0.f, 300.f), RRUI::Gold, 1.2f);
		}
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the star team's cars are parked for the boss (%d)."), StarCars.Num());
}

void ARRRandomDefenseDirector::RemoveStarCars()
{
	// The robot (if they combined it) comes apart first, its crew back on their feet
	if (ARRRandomStarRobot* Robot = ARRRandomStarRobot::Get(this, 0))
	{
		Robot->CheatBreak();
	}
	for (const TWeakObjectPtr<AActor>& Car : StarCars)
	{
		if (AActor* Gone = Car.Get())
		{
			if (ARRRandomVehicle* Vehicle = Cast<ARRRandomVehicle>(Gone); Vehicle && Vehicle->GetRider())
			{
				Vehicle->GetRider()->LeaveVehicle();
			}
			Gone->SetLifeSpan(0.5f);
		}
	}
	StarCars.Reset();
}

void ARRRandomDefenseDirector::CheatSkipToWave(int32 InWave)
{
	for (const TWeakObjectPtr<ARRRandomCharacter>& Villain : Alive)
	{
		if (Villain.IsValid())
		{
			Villain->Destroy();
		}
	}
	Alive.Reset();
	Queue.Reset();
	Wave = FMath::Clamp(InWave, 1, ARRRandomGameState::WaveCount) - 1;
	bBreak = true;
	SetGatesOpen(true);
	ARRRandomGameState* State = GetWorld()->GetGameState<ARRRandomGameState>();
	NextWaveAt = (State ? State->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds()) + 1.f;
	if (State)
	{
		State->SetWaveBreak(NextWaveAt);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: cheat, wave %d next."), Wave + 1);
}

void ARRRandomDefenseDirector::CheatClearWave()
{
	Queue.Reset();
	for (const TWeakObjectPtr<ARRRandomCharacter>& Villain : Alive)
	{
		if (ARRRandomCharacter* Down = Villain.Get())
		{
			UGameplayStatics::ApplyDamage(Down, 100000.f, nullptr, nullptr, nullptr);
		}
	}
}

void ARRRandomDefenseDirector::ReturnToGate(ARRRandomCharacter* Villain)
{
	if (!Villain || !Villain->IsAlive() || OpenGates.Num() == 0)
	{
		return;
	}
	const FVector From = Villain->GetActorLocation();
	const FVector* Nearest = Algo::MinElementBy(OpenGates, [&From](const FVector& Gate) { return FVector::DistSquared2D(Gate, From); });
	FVector Spot = *Nearest;
	FHitResult Floor;
	if (GetWorld()->LineTraceSingleByObjectType(Floor, Spot + FVector(0.f, 0.f, 800.f), Spot - FVector(0.f, 0.f, 1500.f), FCollisionObjectQueryParams(ECC_WorldStatic)))
	{
		Spot = Floor.ImpactPoint;
	}
	Spot.Z += Villain->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.f;
	Villain->TeleportTo(Spot, Villain->GetActorRotation());
	Villain->GetCharacterMovement()->StopMovementImmediately();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s was stuck at %s; back to the gate at %s."), *Villain->GetName(), *From.ToCompactString(), *Spot.ToCompactString());
}

FTransform ARRRandomDefenseWall::GetBlockTransform(int32 Block, float Scale) const
{
	const FTransform& Spot = Spots[Block];
	const float Sink = DoorSink * FMath::SmoothStep(0.f, 1.f, OpenShown);
	return FTransform(Spot.GetRotation(), Spot.GetLocation() - FVector(0.f, 0.f, Sink), Spot.GetScale3D() * Scale);
}

void ARRRandomDefenseWall::SetOpen(bool bInOpen)
{
	if (!HasAuthority() || bOpen == bInOpen)
	{
		return;
	}
	bOpen = bInOpen;
	OnRep_Open();
}

void ARRRandomDefenseWall::OnRep_Open()
{
	// The box goes with it (Refresh), the door sinks or rises in Tick
	Refresh(false);
	SetActorTickEnabled(true);
	ARRRandomHUD::PopComicWord(this, bOpen ? TEXT("GATE OPEN") : TEXT("GATE SHUT!"), GetActorLocation() + FVector(0.f, 0.f, 330.f),
		bOpen ? FLinearColor(0.4f, 0.9f, 0.7f) : FLinearColor(1.f, 0.75f, 0.3f), 1.1f);
}

void ARRRandomDefenseDirector::SetGatesOpen(bool bOpen)
{
	for (const TWeakObjectPtr<ARRRandomDefenseWall>& Gate : Walls)
	{
		if (Gate.IsValid())
		{
			Gate->SetOpen(bOpen);
		}
	}
}
