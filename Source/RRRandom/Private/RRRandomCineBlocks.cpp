#include "RRRandomCineBlocks.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// A flying block: thrown this fast (cm/s) outward and up, spun up to this fast (degrees/s); the floor bounces it back
	// with this share of its speed down and keeps this share along; after this long it lies where it is
	constexpr float ThrowOut = 240.f;
	constexpr float ThrowUp = 380.f;
	constexpr float ThrowSpread = 0.45f;
	constexpr float SpinMax = 720.f;
	constexpr float Gravity = 980.f;
	constexpr float Bounce = 0.32f;
	constexpr float Slide = 0.6f;
	constexpr float SettleSeconds = 3.f;
	constexpr float BlockSize = 100.f;

	UInstancedStaticMeshComponent* MakeBlocks(AActor* Owner, const TCHAR* Name, const TCHAR* Path)
	{
		UInstancedStaticMeshComponent* Instances = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Instances->SetupAttachment(Owner->GetRootComponent());
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(Path);
		if (Mesh.Succeeded())
		{
			Instances->SetStaticMesh(Mesh.Object);
		}
		Instances->NumCustomDataFloats = 6;
		Instances->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Instances->SetUsingAbsoluteLocation(true);
		Instances->SetUsingAbsoluteRotation(true);
		Instances->SetUsingAbsoluteScale(true);
		// Moved every frame while blocks fly
		Instances->SetMobility(EComponentMobility::Movable);
		return Instances;
	}
}

ARRRandomCineBlocks::ARRRandomCineBlocks()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Plain = MakeBlocks(this, TEXT("Plain"), TEXT("/Game/Environment/DiceTown/SM_BTBlock.SM_BTBlock"));
	Dice = MakeBlocks(this, TEXT("Dice"), TEXT("/Game/Environment/DiceTown/SM_BTDieBlock.SM_BTDieBlock"));
}

void ARRRandomCineBlocks::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Plain->ClearInstances();
	Dice->ClearInstances();
	Plain->SetWorldTransform(FTransform::Identity);
	Dice->SetWorldTransform(FTransform::Identity);
	for (const FRRCineBlock& Block : Blocks)
	{
		UInstancedStaticMeshComponent* Instances = Block.bDie ? Dice.Get() : Plain.Get();
		// (a die block a hair smaller, like the town's giant dice, so neighbors don't flicker into each other)
		const float Size = Block.Scale * (Block.bDie ? 0.98f : 1.f);
		const int32 Index = Instances->AddInstance(FTransform(FRotator(0.f, Block.Yaw, 0.f), Block.Location, FVector(Size)), true);
		const FLinearColor A(Block.Body);
		const FLinearColor B(Block.Accent);
		const float Data[6] = { A.R, A.G, A.B, B.R, B.G, B.B };
		Instances->SetCustomData(Index, MakeArrayView(Data, 6));
	}
}

void ARRRandomCineBlocks::BeginPlay()
{
	Super::BeginPlay();
	int32 Plains = 0;
	int32 Dies = 0;
	InstanceOf.SetNum(Blocks.Num());
	Broken.Init(false, Blocks.Num());
	for (int32 Index = 0; Index < Blocks.Num(); ++Index)
	{
		InstanceOf[Index] = Blocks[Index].bDie ? Dies++ : Plains++;
	}
}

void ARRRandomCineBlocks::SmashAt(const FVector& Center, float Radius, const FVector& From)
{
	if (!GetWorld() || !GetWorld()->IsGameWorld() || InstanceOf.Num() != Blocks.Num())
	{
		return;
	}
	int32 Count = 0;
	for (int32 Index = 0; Index < Blocks.Num(); ++Index)
	{
		const FRRCineBlock& Block = Blocks[Index];
		if (Broken[Index] || FVector::Dist(Block.Location, Center) > Radius)
		{
			continue;
		}
		Broken[Index] = true;
		FFlying& Piece = Flying.AddDefaulted_GetRef();
		Piece.Block = Index;
		Piece.Location = Block.Location;
		Piece.Rotation = FRotator(0.f, Block.Yaw, 0.f).Quaternion();
		// Away from the blow (flat), spread sideways, and up; smaller pieces fly a little faster
		FVector Away = Block.Location - From;
		Away.Z = 0.f;
		Away = Away.GetSafeNormal();
		if (Away.IsNearlyZero())
		{
			Away = FVector::ForwardVector;
		}
		const FVector Side = FVector::CrossProduct(FVector::UpVector, Away);
		const float Light = 1.f;
		Piece.Velocity = (Away * ThrowOut * Stream.FRandRange(0.7f, 1.3f) + Side * ThrowOut * Stream.FRandRange(-ThrowSpread, ThrowSpread)) * Light
			+ FVector::UpVector * ThrowUp * Stream.FRandRange(0.7f, 1.3f) * Light
			// (the higher in the pile, the higher it goes)
			+ FVector::UpVector * FMath::Max(0.f, Block.Location.Z - Center.Z) * 2.f;
		Piece.Spin = FVector(Stream.FRandRange(-SpinMax, SpinMax), Stream.FRandRange(-SpinMax, SpinMax), Stream.FRandRange(-SpinMax, SpinMax));
		++Count;
	}
	if (Count > 0)
	{
		SetActorTickEnabled(true);
	}
	UE_LOG(LogTemp, Log, TEXT("RRCine: %s smashed %d blocks at %s"), *GetName(), Count, *Center.ToString());
}

void ARRRandomCineBlocks::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	bool bMoving = false;
	for (FFlying& Piece : Flying)
	{
		if (Piece.bResting)
		{
			continue;
		}
		const FRRCineBlock& Block = Blocks[Piece.Block];
		const float Half = BlockSize * 0.5f * Block.Scale;
		Piece.Age += DeltaSeconds;
		Piece.Velocity.Z -= Gravity * DeltaSeconds;
		Piece.Location += Piece.Velocity * DeltaSeconds;
		Piece.Rotation = FQuat::MakeFromEuler(Piece.Spin * DeltaSeconds) * Piece.Rotation;
		// The floor (Z 0): bounce, losing most of the fall and some of the slide and spin
		if (Piece.Location.Z < Half && Piece.Velocity.Z < 0.f)
		{
			Piece.Location.Z = Half;
			Piece.Velocity.Z = -Piece.Velocity.Z * Bounce;
			Piece.Velocity.X *= Slide;
			Piece.Velocity.Y *= Slide;
			Piece.Spin *= Slide;
			if (Piece.Velocity.Z < 60.f)
			{
				Piece.Velocity.Z = 0.f;
			}
		}
		if (Piece.Age > SettleSeconds || (Piece.Location.Z <= Half + 0.5f && Piece.Velocity.Size() < 20.f))
		{
			// Lie flat on a face, turned as it came to rest
			Piece.bResting = true;
			Piece.Location.Z = Half;
			Piece.Rotation = FRotator(0.f, Piece.Rotation.Rotator().Yaw, 0.f).Quaternion();
		}
		const float Size = Block.Scale * (Block.bDie ? 0.98f : 1.f);
		UInstancedStaticMeshComponent* Instances = Block.bDie ? Dice.Get() : Plain.Get();
		Instances->UpdateInstanceTransform(InstanceOf[Piece.Block], FTransform(Piece.Rotation, Piece.Location, FVector(Size)), true, false, true);
		bMoving |= !Piece.bResting;
	}
	Plain->MarkRenderStateDirty();
	Dice->MarkRenderStateDirty();
	if (!bMoving)
	{
		SetActorTickEnabled(false);
	}
}
