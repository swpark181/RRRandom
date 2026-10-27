#include "RRRandomPlaneStation.h"
#include "RRRandomDefense.h"
#include "RRRandomCannon.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceTown.h"
#include "RRRandomEffects.h"
#include "RRRandomHUD.h"
#include "RRRandomOverheadDieComponent.h"
#include "RRRandomUIStyle.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// Without the model: a flat engine cylinder (100 across, 100 high, pivot in its middle) for the deck
	const FVector FallbackDeckScale(5.f, 5.f, 0.3f);
	// SM_PlaneStation's round deck top across its middle (600 across; the booth, dice sign and windsock at one edge reach 364)
	constexpr float StationDeckTop = 103.f;
	// The die: how high its hop goes, how many turns it makes slowing to land, how long it stays and how long it shrinks
	constexpr float DieHop = 120.f;
	constexpr float DieTurns = 4.f;
	constexpr float DieHoldTime = 1.6f;
	constexpr float DieExitTime = 0.3f;
	// Who threw it gets the plane only while still this many reaches of the station away
	constexpr float BoardReachShare = 1.8f;
	// Dice Town's stations, from the town's middle: blue's beside its base (red's is this turned half way round)
	const FVector2D TownStation(-3500.f, 1000.f);
	constexpr float StationClear = 400.f;

	float ServerClock(const UWorld* World)
	{
		const AGameStateBase* State = World ? World->GetGameState() : nullptr;
		return State ? State->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.f);
	}
}

ARRRandomPlaneStation::ARRRandomPlaneStation()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> StationMesh(TEXT("/Game/Vehicles/SM_PlaneStation.SM_PlaneStation"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiceMesh(TEXT("/Game/Props/Dice/SM_Dice.SM_Dice"));

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	// Scenery: walked over and through (a plane pops in on it)
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);
	if (StationMesh.Succeeded())
	{
		Body->SetStaticMesh(StationMesh.Object);
	}
	else if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
		Body->SetRelativeLocation(FVector(0.f, 0.f, 50.f * FallbackDeckScale.Z));
		Body->SetRelativeScale3D(FallbackDeckScale);
	}

	Die = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Die"));
	Die->SetupAttachment(RootComponent);
	Die->SetUsingAbsoluteLocation(true);
	Die->SetUsingAbsoluteRotation(true);
	Die->SetUsingAbsoluteScale(true);
	Die->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Die->SetCanEverAffectNavigation(false);
	Die->CastShadow = false;
	Die->SetVisibleFlag(false);
	if (DiceMesh.Succeeded())
	{
		Die->SetStaticMesh(DiceMesh.Object);
	}
}

void ARRRandomPlaneStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomPlaneStation, Plane);
	DOREPLIFETIME(ARRRandomPlaneStation, ReadyAt);
	DOREPLIFETIME(ARRRandomPlaneStation, RollCount);
	DOREPLIFETIME(ARRRandomPlaneStation, RollFace);
	DOREPLIFETIME(ARRRandomPlaneStation, bRolling);
}

void ARRRandomPlaneStation::BeginPlay()
{
	Super::BeginPlay();
}

ERRVehicleKind ARRRandomPlaneStation::KindForFace(int32 Face)
{
	return Face <= 2 ? ERRVehicleKind::PlaneWood : (Face <= 4 ? ERRVehicleKind::PlaneFighter : ERRVehicleKind::PlaneSpaceship);
}

float ARRRandomPlaneStation::GetDeckHeight() const
{
	const UStaticMesh* Mesh = Body->GetStaticMesh();
	if (!Mesh)
	{
		return 0.f;
	}
	return Mesh->GetPathName().StartsWith(TEXT("/Engine")) ? 100.f * FallbackDeckScale.Z : StationDeckTop;
}

float ARRRandomPlaneStation::GetRechargeRemaining() const
{
	if (Plane)
	{
		return RechargeTime;
	}
	return FMath::Max(0.f, ReadyAt - ServerClock(GetWorld()));
}

bool ARRRandomPlaneStation::CanRoll(const ARRRandomCharacter* Who) const
{
	return Who && Who->CanBoardPlane() && !Plane && !bRolling && GetRechargeRemaining() <= 0.f
		&& FVector::Dist2D(Who->GetActorLocation(), GetActorLocation()) <= UseRange;
}

void ARRRandomPlaneStation::StartRoll(ARRRandomCharacter* Who)
{
	if (!HasAuthority() || !CanRoll(Who))
	{
		return;
	}
	Roller = Who;
	bRolling = true;
	RollFace = static_cast<uint8>(FMath::RandRange(1, 6));
	RollLandTime = GetWorld()->GetTimeSeconds() + RollTime;
	++RollCount;
	OnRep_Roll();
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s throws the plane die: %d (%s)."), *Who->GetName(), RollFace, ARRRandomVehicle::GetHandling(KindForFace(RollFace)).Name);
}

void ARRRandomPlaneStation::OnRep_Roll()
{
	RollSeenTime = GetWorld()->GetTimeSeconds();
	bLandShown = false;
	// Mostly sideways, so it tumbles over rather than spinning like a top
	SpinAxis = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.4f, 0.4f)).GetSafeNormal();
	if (SpinAxis.IsNearlyZero())
	{
		SpinAxis = FVector::ForwardVector;
	}
}

void ARRRandomPlaneStation::OnPlaneGone()
{
	if (!HasAuthority())
	{
		return;
	}
	Plane = nullptr;
	ReadyAt = ServerClock(GetWorld()) + RechargeTime;
	ForceNetUpdate();
}

void ARRRandomPlaneStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateDie(DeltaSeconds);

	// The die has landed: the plane for its face, under the one who threw it if it is still here and free to board
	if (HasAuthority() && bRolling && GetWorld()->GetTimeSeconds() >= RollLandTime)
	{
		bRolling = false;
		ARRRandomCharacter* Who = Roller.Get();
		Roller = nullptr;
		if (Who && FVector::Dist2D(Who->GetActorLocation(), GetActorLocation()) <= UseRange * BoardReachShare)
		{
			Plane = SpawnPlaneFor(Who, RollFace, this);
		}
		ForceNetUpdate();
	}
}

void ARRRandomPlaneStation::UpdateDie(float DeltaSeconds)
{
	const float Age = GetWorld()->GetTimeSeconds() - RollSeenTime;
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Camera || Age > RollTime + DieHoldTime + DieExitTime || !Die->GetStaticMesh())
	{
		if (Die->IsVisible())
		{
			Die->SetVisibility(false);
		}
		return;
	}
	// Like the die over a head (URRRandomOverheadDieComponent), only bigger, over the deck: it pops up, tumbles along a hop
	// slowing to a stop on its face turned to this machine's camera, bounces, stays a while and shrinks away
	const FQuat View = Camera->GetCameraRotation().Quaternion();
	FVector Location = GetActorLocation() + FVector(0.f, 0.f, GetDeckHeight() + DieHeight);
	FQuat Rotation = URRRandomOverheadDieComponent::FaceToward(RollFace, View);
	float Scale = 1.f;
	if (Age < RollTime)
	{
		const float Alpha = Age / RollTime;
		Location.Z += DieHop * FMath::Sin(PI * Alpha);
		Rotation = FQuat(SpinAxis, DieTurns * FMath::Pow(1.f - Alpha, 3.f) * 2.f * PI) * Rotation;
		Scale = FMath::Min(1.f, Age / 0.15f);
	}
	else
	{
		const float Landed = Age - RollTime;
		Scale = 1.f + 0.2f * FMath::Sin(PI * FMath::Clamp(Landed / 0.2f, 0.f, 1.f));
		if (Landed > DieHoldTime)
		{
			Scale *= 1.f - FMath::Clamp((Landed - DieHoldTime) / DieExitTime, 0.f, 1.f);
		}
	}
	Die->SetWorldLocationAndRotation(Location, Rotation);
	Die->SetWorldScale3D(FVector(DieScale * FMath::Max(Scale, KINDA_SMALL_NUMBER)));
	if (!Die->IsVisible())
	{
		Die->SetVisibility(true);
	}

	// Glitter while it tumbles; landing, a burst and the plane's name over it
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		if (Age < RollTime)
		{
			Effects->DiceSpinSparkle(Location, 50.f * DieScale * Scale, DeltaSeconds);
		}
		else if (!bLandShown)
		{
			bLandShown = true;
			Effects->DiceLandBurst(Location, 50.f * DieScale);
			const ERRVehicleKind Kind = KindForFace(RollFace);
			const FLinearColor Color = Kind == ERRVehicleKind::PlaneWood ? RRUI::Gold : (Kind == ERRVehicleKind::PlaneFighter ? RRUI::SkyDeep : RRUI::LavenderDeep);
			ARRRandomHUD::PopComicWord(this, FString::Printf(TEXT("%d  %s!"), RollFace, ARRRandomVehicle::GetHandling(Kind).Name), Location + FVector(0.f, 0.f, 110.f), Color, 1.2f);
		}
	}
}

ARRRandomPlaneStation* ARRRandomPlaneStation::FindNear(const ARRRandomCharacter* Who)
{
	if (!Who)
	{
		return nullptr;
	}
	ARRRandomPlaneStation* Nearest = nullptr;
	float NearestDistance = UseRange;
	for (TActorIterator<ARRRandomPlaneStation> It(Who->GetWorld()); It; ++It)
	{
		const float Distance = FVector::Dist2D(It->GetActorLocation(), Who->GetActorLocation());
		if (Distance <= NearestDistance && FMath::Abs(It->GetActorLocation().Z - Who->GetActorLocation().Z) < 300.f)
		{
			Nearest = *It;
			NearestDistance = Distance;
		}
	}
	return Nearest;
}

ARRRandomPlaneStation* ARRRandomPlaneStation::FindInReach(const ARRRandomCharacter* Who)
{
	ARRRandomPlaneStation* Near = FindNear(Who);
	return Near && Near->CanRoll(Who) ? Near : nullptr;
}

ARRRandomVehicle* ARRRandomPlaneStation::SpawnPlaneFor(ARRRandomCharacter* Who, int32 Face, ARRRandomPlaneStation* Station)
{
	if (!Who || !Who->HasAuthority() || !Who->CanBoardPlane())
	{
		return nullptr;
	}
	UWorld* World = Who->GetWorld();
	// Under its feet, facing its way (the plane's pivot is the middle of its bottom)
	const FVector Feet = Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	const FTransform Transform(FRotator(0.f, Who->GetActorRotation().Yaw, 0.f), Feet);
	ARRRandomVehicle* NewPlane = World->SpawnActorDeferred<ARRRandomVehicle>(ARRRandomVehicle::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!NewPlane)
	{
		return nullptr;
	}
	NewPlane->SetKind(KindForFace(Face));
	NewPlane->SetStation(Station);
	NewPlane->FinishSpawning(Transform);
	if (!Who->BoardPlane(NewPlane))
	{
		NewPlane->SetStation(nullptr);
		NewPlane->Destroy();
		return nullptr;
	}
	return NewPlane;
}

void ARRRandomPlaneStation::SpawnDiceTownStations(UWorld* World, const ARRRandomDiceTown* Town)
{
	if (!World || !Town || TActorIterator<ARRRandomPlaneStation>(World))
	{
		return;
	}
	const FVector2D Middle(Town->GetActorLocation().X, Town->GetActorLocation().Y);
	// Blue's, then red's looked for from blue's turned half way round the middle (so they match where they can)
	FVector2D Want = Middle + TownStation;
	for (const float Turn : { 1.f, -1.f })
	{
		FVector Spot;
		if (Turn < 0.f)
		{
			Want = Middle * 2.f - Want;
		}
		if (!ARRRandomCannon::FindOpenGround(World, Town, Want, StationClear, Spot))
		{
			UE_LOG(LogTemp, Warning, TEXT("RRRandom: no open ground for a plane station near %s."), *(TownStation * Turn).ToString());
			continue;
		}
		// Facing the middle of the town, where the planes are headed
		const FTransform Transform(FRotator(0.f, Turn > 0.f ? 0.f : 180.f, 0.f), Spot);
		World->SpawnActor<ARRRandomPlaneStation>(ARRRandomPlaneStation::StaticClass(), Transform);
		Want = FVector2D(Spot);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: plane station at %s."), *(FVector2D(Spot) - Middle).ToString());
	}
}

void ARRRandomPlaneStation::SpawnDefenseStations(UWorld* World, const ARRRandomDiceTown* Town, const FVector& Center)
{
	if (!World || !Town || TActorIterator<ARRRandomPlaneStation>(World))
	{
		return;
	}
	// A defense stage (round 90): one inside the castle south of the tower (the north is the inner jump ramp's), the planes
	// flying out over the wall, and one outside on the south side (room kept for it, mirroring the outer jump ramp)
	const FVector2D Middle(Center.X, Center.Y);
	const FVector2D Wants[] = { FVector2D(0.f, -Town->GetDefenseRingRadius() * 0.7f), FVector2D(RRDefense::OuterRampX, -RRDefense::OuterRampFootY) };
	for (const FVector2D& Want : Wants)
	{
		const float Side = -1.f;
		FVector Spot;
		if (!ARRRandomCannon::FindOpenGround(World, Town, Middle + Want, StationClear, Spot, [](const FVector&) { return true; }, 500.f))
		{
			UE_LOG(LogTemp, Warning, TEXT("RRRandom: no open ground for a defense plane station (side %.0f)."), Side);
			continue;
		}
		World->SpawnActor<ARRRandomPlaneStation>(ARRRandomPlaneStation::StaticClass(), FTransform(FRotator(0.f, Side > 0.f ? 90.f : -90.f, 0.f), Spot));
		UE_LOG(LogTemp, Log, TEXT("RRRandom: defense plane station at %s."), *(FVector2D(Spot) - Middle).ToString());
	}
}
