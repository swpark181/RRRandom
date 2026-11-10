#include "RRRandomOre.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceTown.h"
#include "RRRandomEffects.h"
#include "RRRandomGameState.h"
#include "RRRandomKit.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The meshes are 100 tall with the pivot at the bottom middle (char-pipeline/scripts/ue_import_meteor_rush.py)
	const TCHAR* const ShardPath = TEXT("/Game/MeteorRush/SM_MRShard.SM_MRShard");
	const TCHAR* const OrePath = TEXT("/Game/MeteorRush/SM_MROre.SM_MROre");
	const TCHAR* const CrystalPath = TEXT("/Game/MeteorRush/SM_MRBig.SM_MRBig");
	const TCHAR* const DiePath = TEXT("/Game/Props/Dice/SM_Dice.SM_Dice");
	const TCHAR* const DieMaterialPath = TEXT("/Game/MeteorRush/M_MRDie.M_MRDie");

	constexpr float Gravity = 1900.f;
	// Hovering this high over the floor (its bottom), bobbing this much
	constexpr float RestHeight = 14.f;
	constexpr float BobHeight = 6.f;
	// Not picked up this soon after being thrown
	constexpr float PickupDelay = 0.4f;
	constexpr float TouchRadius = 75.f;
	// Gone this far below where it was thrown
	constexpr float LostDepth = 2500.f;

	float GetHeight(ERROreKind Kind)
	{
		switch (Kind)
		{
		case ERROreKind::Shard: return 62.f;
		case ERROreKind::Ore: return 86.f;
		case ERROreKind::Big: return 115.f;
		case ERROreKind::Golden: return 135.f;
		}
		return 60.f;
	}

	/** A die's body color and pips, by slot name (SM_Dice: "Dice" the body, "Pip" the dots). */
	void PaintDie(UStaticMeshComponent* Die, UMaterialInterface* Body, UObject* Outer, const FLinearColor& Color, const FLinearColor& PipColor)
	{
		const TArray<FName> Slots = Die->GetMaterialSlotNames();
		for (int32 Slot = 0; Slot < Die->GetNumMaterials(); ++Slot)
		{
			const bool bPip = Slots.IsValidIndex(Slot) && Slots[Slot].ToString().Contains(TEXT("Pip"));
			if (bPip)
			{
				UMaterialInstanceDynamic* Pip = UMaterialInstanceDynamic::Create(Die->GetMaterial(Slot), Outer);
				Pip->SetVectorParameterValue(TEXT("PipColor"), PipColor);
				Die->SetMaterial(Slot, Pip);
			}
			else if (Body)
			{
				UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Body, Outer);
				Paint->SetVectorParameterValue(TEXT("Color"), Color);
				Die->SetMaterial(Slot, Paint);
			}
		}
	}
}

int32 RROre::GetPoints(ERROreKind Kind)
{
	switch (Kind)
	{
	case ERROreKind::Shard: return 1;
	case ERROreKind::Ore: return 3;
	case ERROreKind::Big:
	case ERROreKind::Golden: return 6;
	}
	return 0;
}

FLinearColor RROre::GetColor(ERROreKind Kind)
{
	switch (Kind)
	{
	case ERROreKind::Shard: return FLinearColor::FromSRGBColor(FColor(143, 221, 190));
	case ERROreKind::Ore: return FLinearColor::FromSRGBColor(FColor(247, 168, 196));
	case ERROreKind::Big: return FLinearColor::FromSRGBColor(FColor(240, 180, 70));
	case ERROreKind::Golden: return FLinearColor::FromSRGBColor(FColor(255, 214, 90));
	}
	return FLinearColor::White;
}

ARRRandomOre::ARRRandomOre()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);
	SetNetUpdateFrequency(20.f);

	Touch = CreateDefaultSubobject<USphereComponent>(TEXT("Touch"));
	Touch->InitSphereRadius(TouchRadius);
	Touch->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Touch->SetCollisionResponseToAllChannels(ECR_Ignore);
	Touch->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Touch->SetGenerateOverlapEvents(true);
	Touch->SetCanEverAffectNavigation(false);
	RootComponent = Touch;

	Float = CreateDefaultSubobject<USceneComponent>(TEXT("Float"));
	Float->SetupAttachment(Touch);

	Shell = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shell"));
	Shell->SetupAttachment(Float);
	Shell->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Shell->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DieMesh(DiePath);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		UStaticMeshComponent* Die = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Die%d"), Index));
		Die->SetupAttachment(Float);
		Die->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Die->SetCanEverAffectNavigation(false);
		Die->SetCastShadow(false);
		Die->SetVisibility(false);
		if (DieMesh.Succeeded())
		{
			Die->SetStaticMesh(DieMesh.Object);
		}
		Dice.Add(Die);
	}

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Float);
	Glow->SetCastShadows(false);
	Glow->SetVisibility(false);

	// A falling star's landing spot: a ring on the floor (the engine plane, 100 across, M_MRRing)
	Marker = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Marker"));
	Marker->SetupAttachment(Touch);
	Marker->SetUsingAbsoluteLocation(true);
	Marker->SetUsingAbsoluteRotation(true);
	Marker->SetUsingAbsoluteScale(true);
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCanEverAffectNavigation(false);
	Marker->SetCastShadow(false);
	Marker->SetVisibility(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (Plane.Succeeded())
	{
		Marker->SetStaticMesh(Plane.Object);
	}
}

void ARRRandomOre::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomOre, Kind);
	DOREPLIFETIME(ARRRandomOre, bResting);
	DOREPLIFETIME(ARRRandomOre, bFallingStar);
	DOREPLIFETIME(ARRRandomOre, StarTarget);
}

void ARRRandomOre::Setup(ERROreKind InKind, const FVector& InVelocity)
{
	Kind = InKind;
	Velocity = InVelocity;
}

void ARRRandomOre::SetupFallingStar(ERROreKind InKind, const FVector& Target)
{
	Kind = InKind;
	bFallingStar = true;
	StarTarget = Target;
}

void ARRRandomOre::BeginPlay()
{
	Super::BeginPlay();
	Phase = FMath::FRandRange(0.f, 2.f * PI);
	LostZ = (bFallingStar ? FVector(StarTarget).Z : GetActorLocation().Z) - LostDepth;
	ApplyLook();
	if (HasAuthority())
	{
		Touch->OnComponentBeginOverlap.AddDynamic(this, &ARRRandomOre::OnTouch);
		if (bFallingStar)
		{
			Velocity = (FVector(StarTarget) - GetActorLocation()).GetSafeNormal() * FallingStarSpeed;
		}
	}
	if (UMaterialInterface* Ring = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRRing.M_MRRing")))
	{
		MarkerMaterial = UMaterialInstanceDynamic::Create(Ring, this);
		MarkerMaterial->SetVectorParameterValue(TEXT("Color"), RROre::GetColor(Kind));
		MarkerMaterial->SetScalarParameterValue(TEXT("Thickness"), 0.14f);
		MarkerMaterial->SetScalarParameterValue(TEXT("Fill"), 0.2f);
		MarkerMaterial->SetScalarParameterValue(TEXT("Glow"), 2.6f);
		Marker->SetMaterial(0, MarkerMaterial);
	}
	// Thrown out (of a meteor or a brawler): a little pop where it sets off
	if (!bResting && !bFallingStar)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->DiceLandBurst(GetActorLocation() + FVector(0.f, 0.f, 30.f), GetHeight(Kind) * 0.35f);
		}
	}
}

void ARRRandomOre::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Picked up: a little pop of its color where it was (every machine sees it go)
	if (EndPlayReason == EEndPlayReason::Destroyed && GetActorLocation().Z > LostZ + 200.f)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->DiceLandBurst(Float->GetComponentLocation() + FVector(0.f, 0.f, GetHeight(Kind) * 0.5f), GetHeight(Kind) * 0.4f);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ARRRandomOre::OnRep_Kind()
{
	bLooked = false;
	ApplyLook();
}

void ARRRandomOre::ApplyLook()
{
	if (bLooked)
	{
		return;
	}
	bLooked = true;
	const float Height = GetHeight(Kind);
	const TCHAR* MeshPath = Kind == ERROreKind::Shard ? ShardPath : Kind == ERROreKind::Ore ? OrePath : CrystalPath;
	if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
	{
		Shell->SetStaticMesh(Mesh);
	}
	Shell->SetRelativeScale3D(FVector(Height / 100.f));
	// Hover with the bottom RestHeight over the floor; the actor stands on the floor
	Float->SetRelativeLocation(FVector(0.f, 0.f, RestHeight));

	const FLinearColor Color = RROre::GetColor(Kind);
	if (UMaterialInterface* Material = Shell->GetMaterial(0))
	{
		UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(Material, this);
		// The glass takes the kind's tint; the golden crystal is the big one gilded and brighter
		Paint->SetVectorParameterValue(TEXT("Tint"), Kind == ERROreKind::Golden ? FLinearColor(1.35f, 1.15f, 0.55f) : Kind == ERROreKind::Big ? FLinearColor::White : Color);
		Paint->SetScalarParameterValue(TEXT("Glow"), Kind == ERROreKind::Golden ? 0.9f : Kind == ERROreKind::Big ? 0.45f : 0.6f);
		// The glass a little thicker than the material's default, so it reads on the grass from above
		Paint->SetScalarParameterValue(TEXT("Opacity"), 0.4f);
		for (int32 Slot = 0; Slot < Shell->GetNumMaterials(); ++Slot)
		{
			Shell->SetMaterial(Slot, Paint);
		}
	}

	// Real dice inside the see-through ones: a white die in the mint chip, three pink ones in the pink lump
	UMaterialInterface* DieBody = LoadObject<UMaterialInterface>(nullptr, DieMaterialPath);
	struct FDieSpot
	{
		FVector Where;
		float Size;
	};
	TArray<FDieSpot> Spots;
	FLinearColor DieColor = FLinearColor::White;
	FLinearColor PipColor = FLinearColor::White;
	if (Kind == ERROreKind::Shard)
	{
		Spots = { { FVector(0.f, 0.f, 0.5f), 0.46f } };
		DieColor = FLinearColor(0.95f, 0.98f, 0.96f);
		PipColor = FLinearColor::FromSRGBColor(FColor(60, 170, 130));
	}
	else if (Kind == ERROreKind::Ore)
	{
		Spots = { { FVector(-0.14f, -0.1f, 0.34f), 0.3f }, { FVector(0.15f, 0.08f, 0.5f), 0.3f }, { FVector(-0.02f, 0.16f, 0.66f), 0.27f } };
		DieColor = FLinearColor::FromSRGBColor(FColor(242, 110, 160));
		PipColor = FLinearColor::White;
	}
	for (int32 Index = 0; Index < Dice.Num(); ++Index)
	{
		UStaticMeshComponent* Die = Dice[Index];
		const bool bUsed = Spots.IsValidIndex(Index) && Die->GetStaticMesh();
		Die->SetVisibility(bUsed);
		if (!bUsed)
		{
			continue;
		}
		const FVector Extent = Die->GetStaticMesh()->GetBounds().BoxExtent;
		const float Side = FMath::Max(1.f, Extent.GetMax() * 2.f);
		Die->SetRelativeScale3D(FVector(Spots[Index].Size * Height / Side));
		// SM_Dice's pivot is its center
		Die->SetRelativeLocation(Spots[Index].Where * Height);
		Die->SetRelativeRotation(FRotator(FMath::FRandRange(-40.f, 40.f), FMath::FRandRange(0.f, 360.f), FMath::FRandRange(-40.f, 40.f)));
		PaintDie(Die, DieBody, this, DieColor, PipColor);
		DieSpin.Add(FRotator(FMath::FRandRange(-30.f, 30.f), FMath::FRandRange(25.f, 60.f) * (FMath::RandBool() ? 1.f : -1.f), FMath::FRandRange(-30.f, 30.f)));
	}

	const bool bCrystal = RROre::IsCrystal(Kind);
	Glow->SetVisibility(true);
	Glow->SetLightColor(Color);
	Glow->SetIntensity(bCrystal ? (Kind == ERROreKind::Golden ? 9000.f : 5000.f) : 2200.f);
	Glow->SetAttenuationRadius(bCrystal ? 420.f : 240.f);
	Glow->SetRelativeLocation(FVector(0.f, 0.f, Height * 0.55f));
}

void ARRRandomOre::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (HasAuthority())
	{
		if (!bResting)
		{
			UpdateFlight(DeltaSeconds);
		}
		// Into Dice Town's lava: it melts away (round 89; every machine sees it pop as it goes)
		TActorIterator<ARRRandomDiceTown> Town(GetWorld());
		float LavaTop = 0.f;
		if (Town && Town->GetLavaAt(GetActorLocation(), LavaTop) && GetActorLocation().Z < LavaTop + GetHeight(Kind) * 0.5f)
		{
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s melts in the lava."), *GetName());
			Destroy();
			return;
		}
		if (Age >= PickupDelay)
		{
			TryPickUp();
		}
	}

	// Every machine: a slow turn and a bob once it rests (thrown, it tumbles); the dice inside roll about
	const float Time = GetWorld()->GetTimeSeconds() + Phase;
	if (bResting)
	{
		Float->SetRelativeLocation(FVector(0.f, 0.f, RestHeight + BobHeight * (0.5f + 0.5f * FMath::Sin(Time * 2.2f))));
		Float->AddLocalRotation(FRotator(0.f, (RROre::IsCrystal(Kind) ? 40.f : 25.f) * DeltaSeconds, 0.f));
		const FRotator Upright = Float->GetRelativeRotation();
		Float->SetRelativeRotation(FMath::RInterpTo(Upright, FRotator(0.f, Upright.Yaw, 0.f), DeltaSeconds, 6.f));
	}
	else
	{
		Float->AddLocalRotation(FRotator(260.f, 190.f, 0.f) * DeltaSeconds);
	}
	for (int32 Index = 0; Index < DieSpin.Num() && Index < Dice.Num(); ++Index)
	{
		Dice[Index]->AddLocalRotation(DieSpin[Index] * DeltaSeconds);
	}
	if (RROre::IsCrystal(Kind))
	{
		Glow->SetIntensity((Kind == ERROreKind::Golden ? 9000.f : 5000.f) * (0.8f + 0.2f * FMath::Sin(Time * 3.f)));
	}
	UpdateEffects(DeltaSeconds);
}

void ARRRandomOre::UpdateEffects(float DeltaSeconds)
{
	const FVector Where = GetActorLocation();
	const float Height = GetHeight(Kind);
	const FLinearColor Color = RROre::GetColor(Kind);
	ARRRandomEffects* Effects = ARRRandomEffects::Get(this);

	// In the air: a shooting star's tail laid along the way it came (a falling star's longer and wider), from where it was
	// last frame (clients see it move in steps: the tail fills the gaps)
	if (Effects && (!bResting || !bLandingShown) && bHasLastSeen && DeltaSeconds > 0.f)
	{
		const FVector Moved = Where - LastSeen;
		if (Moved.SizeSquared() > FMath::Square(50.f * DeltaSeconds))
		{
			TrailWay = Moved.GetSafeNormal();
			const FVector Lift(0.f, 0.f, RestHeight + Height * 0.5f);
			Effects->StarTail(LastSeen + Lift, Where + Lift, Height * (bFallingStar ? 0.7f : 0.5f), Height * (bFallingStar ? 18.f : 9.f), Color, DeltaSeconds);
		}
	}
	LastSeen = Where;
	bHasLastSeen = true;

	// A falling star's landing spot, pulsing faster as it nears
	const bool bMark = bFallingStar && !bResting;
	Marker->SetVisibility(bMark);
	if (bMark)
	{
		const float Left = FMath::Clamp(FVector::Dist(Where, StarTarget) / 3000.f, 0.f, 1.f);
		const float Radius = 90.f + 60.f * Left;
		Marker->SetWorldLocationAndRotation(FVector(StarTarget) + FVector(0.f, 0.f, 6.f), FRotator::ZeroRotator);
		Marker->SetWorldScale3D(FVector(Radius / 50.f, Radius / 50.f, 1.f));
		if (MarkerMaterial)
		{
			MarkerMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.55f + 0.45f * FMath::Sin(GetWorld()->GetTimeSeconds() * (10.f + 12.f * (1.f - Left))));
		}
	}

	// Down: a shooting star's splash of its color on the floor and a pop round it (bigger for a falling star)
	if (bResting && !bLandingShown)
	{
		bLandingShown = true;
		if (Effects && Age < 30.f)
		{
			Effects->StarImpact(Where, TrailWay, Height / 62.f * (bFallingStar ? 1.5f : 0.75f), Color);
			Effects->DiceLandBurst(Where + FVector(0.f, 0.f, RestHeight + Height * 0.4f), Height * (bFallingStar ? 0.9f : 0.45f));
		}
	}
}

void ARRRandomOre::UpdateFlight(float DeltaSeconds)
{
	// Pulled by an ore magnet: an arc that homes in on the puller's feet over PullTime, through anything in the way; there
	// it is picked up (TryPickUp). If the puller is gone or down, it drops from wherever it got to.
	if (Puller.IsValid())
	{
		constexpr float PullTime = 0.45f;
		constexpr float PullHop = 140.f;
		const ARRRandomCharacter* Target = Puller.Get();
		if (!Target->IsAlive())
		{
			Puller = nullptr;
			Velocity = FVector::ZeroVector;
			return;
		}
		PullAge += DeltaSeconds;
		const float Alpha = FMath::Clamp(PullAge / PullTime, 0.f, 1.f);
		const FVector Feet = Target->GetActorLocation() - FVector(0.f, 0.f, Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		SetActorLocation(FMath::Lerp(PullFrom, Feet, Alpha * Alpha) + FVector(0.f, 0.f, PullHop * FMath::Sin(Alpha * PI)));
		if (Alpha >= 1.f && PullAge > PullTime + 0.5f)
		{
			// Arrived but not taken (an ultimate holds the puller): let it fall where it is
			Puller = nullptr;
			Velocity = FVector::ZeroVector;
		}
		return;
	}

	// A falling star: straight down its line to the spot
	if (bFallingStar)
	{
		const FVector Here = GetActorLocation();
		const FVector ToTarget = FVector(StarTarget) - Here;
		const float Step = FallingStarSpeed * DeltaSeconds;
		if (ToTarget.Size() <= Step)
		{
			SetActorLocation(StarTarget);
			Velocity = FVector::ZeroVector;
			bResting = true;
			return;
		}
		SetActorLocation(Here + ToTarget.GetSafeNormal() * Step);
		return;
	}
	const FVector From = GetActorLocation();
	Velocity.Z -= Gravity * DeltaSeconds;
	FVector To = From + Velocity * DeltaSeconds;

	FHitResult Hit;
	const FCollisionObjectQueryParams Solid(ECC_WorldStatic);
	if (GetWorld()->SweepSingleByObjectType(Hit, From + FVector(0.f, 0.f, 20.f), To + FVector(0.f, 0.f, 20.f), FQuat::Identity, Solid, FCollisionShape::MakeSphere(18.f)))
	{
		if (Hit.ImpactNormal.Z > 0.6f && Velocity.Z <= 0.f)
		{
			// Down on the floor: rest here
			SetActorLocation(FVector(Hit.Location.X, Hit.Location.Y, Hit.ImpactPoint.Z));
			Velocity = FVector::ZeroVector;
			bResting = true;
			return;
		}
		// A wall: bounce off it, losing most of the way it was going
		To = Hit.Location - FVector(0.f, 0.f, 20.f);
		const FVector Normal = FVector(Hit.ImpactNormal.X, Hit.ImpactNormal.Y, 0.f).GetSafeNormal();
		Velocity = Velocity - 2.f * FVector::DotProduct(Velocity, Normal) * Normal;
		Velocity.X *= 0.35f;
		Velocity.Y *= 0.35f;
	}
	SetActorLocation(To);
	if (To.Z < LostZ)
	{
		Destroy();
	}
}

void ARRRandomOre::PullToward(const ARRRandomCharacter* InPuller)
{
	if (!HasAuthority() || !bResting || !InPuller)
	{
		return;
	}
	// No longer a falling star (it would head back to its spot); ready to be picked up the moment it arrives
	Puller = InPuller;
	PullFrom = GetActorLocation();
	PullAge = 0.f;
	bResting = false;
	bFallingStar = false;
	Age = FMath::Max(Age, PickupDelay);
}

void ARRRandomOre::OnTouch(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Age >= PickupDelay)
	{
		TryPickUp();
	}
}

void ARRRandomOre::TryPickUp()
{
	if (IsActorBeingDestroyed() || ARRRandomGameState::IsPlayLocked(this))
	{
		return;
	}
	// Whoever's capsule is within the touch sphere, which reaches further for a brawler whose kit picks up from further
	// (the rabbit's)
	const FVector Center = Touch->GetComponentLocation();
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Brawler = *It;
		URRRandomOreCarryComponent* Carry = Brawler->GetOreCarry();
		if (!Carry || !Brawler->IsAlive() || Brawler->IsDecoy() || Brawler->IsUsingUltimate())
		{
			continue;
		}
		const float Reach = TouchRadius * Brawler->GetKit()->GetKit().OrePickupScale;
		const UCapsuleComponent* Capsule = Brawler->GetCapsuleComponent();
		const FVector Offset = Brawler->GetActorLocation() - Center;
		if (Offset.Size2D() <= Reach + Capsule->GetScaledCapsuleRadius() && FMath::Abs(Offset.Z) <= Reach + Capsule->GetScaledCapsuleHalfHeight())
		{
			Carry->Add(Kind);
			UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) picks up a %s; carries %d."), *Brawler->GetName(), Brawler->GetTeam(),
				*StaticEnum<ERROreKind>()->GetNameStringByValue(int64(Kind)), Carry->GetPoints());
			Destroy();
			return;
		}
	}
}

URRRandomOreCarryComponent::URRRandomOreCarryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void URRRandomOreCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URRRandomOreCarryComponent, Points);
	DOREPLIFETIME(URRRandomOreCarryComponent, Crystals);
	DOREPLIFETIME(URRRandomOreCarryComponent, Shards);
	DOREPLIFETIME(URRRandomOreCarryComponent, Ores);
	DOREPLIFETIME(URRRandomOreCarryComponent, bGolden);
	DOREPLIFETIME(URRRandomOreCarryComponent, Collected);
}

void URRRandomOreCarryComponent::Add(ERROreKind Kind)
{
	const int32 Gain = RROre::GetPoints(Kind);
	Points += Gain;
	Collected += Gain;
	if (Kind == ERROreKind::Shard)
	{
		++Shards;
	}
	else if (Kind == ERROreKind::Ore)
	{
		++Ores;
	}
	else if (Kind == ERROreKind::Big)
	{
		++Crystals;
	}
	else if (Kind == ERROreKind::Golden)
	{
		bGolden = true;
	}
}

void URRRandomOreCarryComponent::DropAll(bool bLost)
{
	const ARRRandomCharacter* Owner = Cast<ARRRandomCharacter>(GetOwner());
	if (!Owner || !Owner->HasAuthority() || Points <= 0)
	{
		Points = 0;
		Crystals = 0;
		Shards = 0;
		Ores = 0;
		bGolden = false;
		return;
	}
	// The squirrel's cheek pouch keeps a third of the points (rounded up), out of the shards and lumps: crystals always drop
	const int32 CrystalPoints = RROre::GetPoints(ERROreKind::Big) * (Crystals + (bGolden ? 1 : 0));
	const int32 Kept = Owner->GetKit()->GetKit().bCheekPouch ? FMath::Min(FMath::DivideAndRoundUp(Points, 3), Points - CrystalPoints) : 0;
	if (!bLost)
	{
		// From the waist: crystals whole and close by, the rest broken into one-point shards flying all round (three-point
		// lumps when there would be too many pieces)
		TArray<ERROreKind> Pieces;
		if (bGolden)
		{
			Pieces.Add(ERROreKind::Golden);
		}
		for (int32 Index = 0; Index < Crystals; ++Index)
		{
			Pieces.Add(ERROreKind::Big);
		}
		int32 Rest = Points - CrystalPoints - Kept;
		constexpr int32 MaxPieces = 16;
		while (Rest > 0)
		{
			const bool bLump = Rest >= 3 && Pieces.Num() + Rest > MaxPieces;
			Pieces.Add(bLump ? ERROreKind::Ore : ERROreKind::Shard);
			Rest -= bLump ? 3 : 1;
		}
		const FVector From = Owner->GetActorLocation();
		const float FeetZ = From.Z - Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		const float Start = FMath::FRandRange(0.f, 360.f);
		for (int32 Index = 0; Index < Pieces.Num(); ++Index)
		{
			const bool bCrystal = RROre::IsCrystal(Pieces[Index]);
			const FVector Out = FVector(1.f, 0.f, 0.f).RotateAngleAxis(Start + Index * 360.f / Pieces.Num() + FMath::FRandRange(-15.f, 15.f), FVector::UpVector);
			const FVector Throw = Out * (bCrystal ? FMath::FRandRange(120.f, 220.f) : FMath::FRandRange(260.f, 520.f)) + FVector(0.f, 0.f, FMath::FRandRange(520.f, 720.f));
			const FTransform Spawn(FVector(From.X, From.Y, FeetZ + 40.f));
			if (ARRRandomOre* Piece = GetWorld()->SpawnActorDeferred<ARRRandomOre>(ARRRandomOre::StaticClass(), Spawn, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
			{
				Piece->Setup(Pieces[Index], Throw);
				Piece->FinishSpawning(Spawn);
			}
		}
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s drops %d ore points in %d pieces, keeps %d."), *Owner->GetName(), Points - Kept, Pieces.Num(), Kept);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s falls off with %d ore points; %d are lost, %d kept."), *Owner->GetName(), Points, Points - Kept, Kept);
	}
	// What the pouch kept, as lumps and shards
	Points = Kept;
	Crystals = 0;
	Ores = Kept / RROre::GetPoints(ERROreKind::Ore);
	Shards = Kept % RROre::GetPoints(ERROreKind::Ore);
	bGolden = false;
}

void URRRandomOreCarryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// A crystal carried circles the waist in its glow, so everyone can see who has it
	const ARRRandomCharacter* Owner = Cast<ARRRandomCharacter>(GetOwner());
	const bool bShow = Owner && Owner->IsAlive() && HasCrystal() && GetNetMode() != NM_DedicatedServer;
	if (bShow && !Orbiter)
	{
		Orbiter = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("CarriedCrystal"));
		Orbiter->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, CrystalPath));
		Orbiter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Orbiter->SetUsingAbsoluteScale(true);
		Orbiter->SetupAttachment(this);
		Orbiter->RegisterComponent();
		if (UMaterialInterface* Material = Orbiter->GetMaterial(0))
		{
			OrbiterMaterial = UMaterialInstanceDynamic::Create(Material, this);
			OrbiterMaterial->SetScalarParameterValue(TEXT("Glow"), 0.6f);
			for (int32 Slot = 0; Slot < Orbiter->GetNumMaterials(); ++Slot)
			{
				Orbiter->SetMaterial(Slot, OrbiterMaterial);
			}
		}
		Light = NewObject<UPointLightComponent>(GetOwner(), TEXT("CarriedGlow"));
		Light->SetCastShadows(false);
		Light->SetAttenuationRadius(480.f);
		Light->SetupAttachment(this);
		Light->RegisterComponent();
	}
	if (!Orbiter)
	{
		return;
	}
	Orbiter->SetVisibility(bShow);
	Light->SetVisibility(bShow);
	if (!bShow)
	{
		return;
	}
	const FLinearColor Color = RROre::GetColor(bGolden ? ERROreKind::Golden : ERROreKind::Big);
	if (OrbiterMaterial)
	{
		OrbiterMaterial->SetVectorParameterValue(TEXT("Tint"), bGolden ? FLinearColor(1.35f, 1.15f, 0.55f) : FLinearColor::White);
	}
	OrbitAngle = FMath::Fmod(OrbitAngle + DeltaTime * 150.f, 360.f);
	const float Scale = Owner->GetActorScale3D().Z;
	const float HalfHeight = Owner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Around = FVector(70.f * Scale, 0.f, 0.f).RotateAngleAxis(OrbitAngle, FVector::UpVector);
	Orbiter->SetWorldLocation(Owner->GetActorLocation() + Around + FVector(0.f, 0.f, -HalfHeight * 0.1f + 10.f * FMath::Sin(OrbitAngle * PI / 90.f)));
	Orbiter->SetWorldRotation(FRotator(0.f, OrbitAngle * 2.f, 0.f));
	Orbiter->SetWorldScale3D(FVector((bGolden ? 0.5f : 0.42f) * Scale));
	Light->SetWorldLocation(Owner->GetActorLocation() + FVector(0.f, 0.f, HalfHeight * 0.3f));
	Light->SetLightColor(Color);
	Light->SetIntensity((bGolden ? 14000.f : 8000.f) * (0.8f + 0.2f * FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f)));
}
