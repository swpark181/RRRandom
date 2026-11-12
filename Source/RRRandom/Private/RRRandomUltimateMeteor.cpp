#include "RRRandomUltimateMeteor.h"
#include "RRRandomBattleRoyale.h"
#include "RRRandomCharacter.h"
#include "RRRandomDiceBuffComponent.h"
#include "RRRandomEffects.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The fall: a golden rock streaking down its last UltFallTime from UltFallRise up and UltFallSide across, sinking after it lands
	constexpr float UltRockHeight = 200.f;
	constexpr float UltFallTime = 1.f;
	constexpr float UltFallSide = 1400.f;
	constexpr float UltFallRise = 3200.f;
	constexpr float UltSinkTime = 0.8f;
	// Everyone this near is knocked aside (no harm) as it lands
	constexpr float UltKnockRadius = 450.f;
	constexpr float UltKnockSpeed = 550.f;
	constexpr float UltKnockLift = 380.f;
	// The golden die over the crater: this big, this high, bobbing and turning; the beam over it this tall and wide
	constexpr float UltDieSize = 90.f;
	constexpr float UltDieHeight = 130.f;
	constexpr float UltBeamHeight = 6000.f;
	constexpr float UltBeamWidth = 60.f;
	// Where it falls: inside the ring it is closing to, at most this share of its radius out
	constexpr float UltDropShare = 0.7f;

	float UltServerNow(const UWorld* World)
	{
		const AGameStateBase* State = World ? World->GetGameState() : nullptr;
		return State ? State->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.f);
	}

	float UltEaseIn(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * (0.35f + 0.65f * T);
	}
}

ARRRandomUltimateMeteor::ARRRandomUltimateMeteor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CrustMesh(TEXT("/Game/MeteorRush/SM_MRMeteor.SM_MRMeteor"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DiceMesh(TEXT("/Game/Props/Dice/SM_Dice.SM_Dice"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Gold(TEXT("/Game/Props/Dice/M_DiceGold.M_DiceGold"));
	GoldMaterial = Gold.Succeeded() ? Gold.Object : nullptr;
	auto MakeMesh = [this](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UStaticMeshComponent* Part = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Part->SetupAttachment(Root);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCanEverAffectNavigation(false);
		Part->SetCastShadow(false);
		Part->SetStaticMesh(Mesh);
		Part->SetUsingAbsoluteLocation(true);
		Part->SetUsingAbsoluteRotation(true);
		Part->SetUsingAbsoluteScale(true);
		return Part;
	};
	Ring = MakeMesh(TEXT("Ring"), Plane.Object);
	Rock = MakeMesh(TEXT("Rock"), CrustMesh.Object);
	Die = MakeMesh(TEXT("Die"), DiceMesh.Object);
	Die->SetCastShadow(true);
	Beam = MakeMesh(TEXT("Beam"), Cylinder.Object);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetUsingAbsoluteLocation(true);
	Light->SetCastShadows(false);
	Light->SetAttenuationRadius(900.f);
	Light->SetLightColor(GetGold());
}

void ARRRandomUltimateMeteor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomUltimateMeteor, ImpactTime);
	DOREPLIFETIME(ARRRandomUltimateMeteor, bTaken);
}

ARRRandomUltimateMeteor* ARRRandomUltimateMeteor::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ARRRandomUltimateMeteor> It(World); It; ++It)
	{
		if (!It->IsTaken() && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

bool ARRRandomUltimateMeteor::CanTake(const ARRRandomCharacter* Who)
{
	const URRRandomDiceBuffComponent* Dice = Who ? Who->GetDiceBuffs() : nullptr;
	return Dice && Who->IsAlive() && !Who->IsDecoy() && !Who->IsVillain() && !Who->GetVehicle() && !Who->IsUsingUltimate()
		&& Dice->UltimateDieChance > 0.f && Dice->GetUltimateCharges() == 0;
}

float ARRRandomUltimateMeteor::GetTimeToImpact() const
{
	return ImpactTime - UltServerNow(GetWorld());
}

void ARRRandomUltimateMeteor::UpdateSky(UWorld* World, float& NextDropTime)
{
	const ARRRandomGameState* State = World ? World->GetGameState<ARRRandomGameState>() : nullptr;
	if (!State || !State->IsBattleRoyale() || !State->HasPlayStarted() || State->IsMatchOver())
	{
		return;
	}
	const float Now = State->GetServerWorldTimeSeconds();
	if (NextDropTime <= 0.f)
	{
		// (-RRUltimateFirst=N: the first one N seconds in, for tests)
		float First = FirstDrop;
		FParse::Value(FCommandLine::Get(), TEXT("RRUltimateFirst="), First);
		NextDropTime = Now + First;
		return;
	}
	// One at a time: the next is planned once this one is taken
	if (Get(World))
	{
		NextDropTime = FMath::Max(NextDropTime, Now + NextDrop);
		return;
	}
	if (Now < NextDropTime)
	{
		return;
	}
	const ARRRandomBattleZone* Zone = ARRRandomBattleZone::Get(World);
	const ARRRandomBattleMap* Battle = ARRRandomBattleMap::Get(World);
	if (!Zone || !Battle)
	{
		return;
	}
	// Somewhere inside the ring it is closing to, on open ground
	const FVector2D Center = Zone->GetNextCenter();
	const float Radius = FMath::Min(Zone->GetNextRadius(), Zone->GetRadius()) * UltDropShare;
	FVector Spot;
	bool bFound = false;
	for (int32 Try = 0; Try < 12 && !bFound; ++Try)
	{
		const float Angle = FMath::FRandRange(0.f, UE_TWO_PI);
		const FVector2D Want = Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius * FMath::Sqrt(FMath::FRand());
		bFound = Battle->FindOpenGround(Want, 250.f, 1500.f, Spot) && Zone->IsInside(Spot);
	}
	if (!bFound)
	{
		NextDropTime = Now + 2.f;
		return;
	}
	const FTransform Where(Spot);
	if (ARRRandomUltimateMeteor* Meteor = World->SpawnActorDeferred<ARRRandomUltimateMeteor>(ARRRandomUltimateMeteor::StaticClass(), Where))
	{
		Meteor->Setup(Now + Lead);
		Meteor->FinishSpawning(Where);
		UE_LOG(LogTemp, Log, TEXT("RRRandom: an ultimate die meteor falls at %s in %.0f s."), *Spot.ToCompactString(), Lead);
	}
	NextDropTime = Now + NextDrop;
}

void ARRRandomUltimateMeteor::BeginPlay()
{
	Super::BeginPlay();
	// The landing pad: the Meteor Rush hologram in gold; the beam the glass, gold and glowing
	if (UMaterialInterface* Marker = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRRing.M_MRRing")))
	{
		RingMaterial = UMaterialInstanceDynamic::Create(Marker, this);
		RingMaterial->SetVectorParameterValue(TEXT("Color"), GetGold());
		RingMaterial->SetScalarParameterValue(TEXT("Glow"), 3.f);
		RingMaterial->SetScalarParameterValue(TEXT("Thickness"), 0.08f);
		RingMaterial->SetScalarParameterValue(TEXT("Fill"), 0.25f);
		Ring->SetMaterial(0, RingMaterial);
	}
	if (UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRGlass.M_MRGlass")))
	{
		BeamMaterial = UMaterialInstanceDynamic::Create(Glass, this);
		BeamMaterial->SetVectorParameterValue(TEXT("Tint"), GetGold());
		BeamMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.25f);
		BeamMaterial->SetScalarParameterValue(TEXT("EdgeOpacity"), 0.5f);
		BeamMaterial->SetScalarParameterValue(TEXT("Glow"), 4.f);
		Beam->SetMaterial(0, BeamMaterial);
	}
	if (UMaterialInterface* CrustMaterial = Rock->GetMaterial(0))
	{
		UMaterialInstanceDynamic* Paint = UMaterialInstanceDynamic::Create(CrustMaterial, this);
		Paint->SetScalarParameterValue(TEXT("Glow"), 3.f);
		Rock->SetMaterial(0, Paint);
	}
	// The die: gold all over, its pips deep red (as the tiger's ultimate die over a head)
	for (int32 Slot = 0; Slot < Die->GetNumMaterials(); ++Slot)
	{
		const bool bPips = Die->GetMaterialSlotNames().IsValidIndex(Slot) && Die->GetMaterialSlotNames()[Slot].ToString().Contains(TEXT("Pip"));
		if (bPips)
		{
			UMaterialInstanceDynamic* Pips = UMaterialInstanceDynamic::Create(Die->GetMaterial(Slot), this);
			Pips->SetVectorParameterValue(TEXT("PipColor"), FLinearColor(0.45f, 0.02f, 0.01f));
			Die->SetMaterial(Slot, Pips);
		}
		else if (GoldMaterial)
		{
			Die->SetMaterial(Slot, GoldMaterial);
		}
	}
	const float Yaw = float(GetActorLocation().X * 0.011f + GetActorLocation().Y * 0.017f);
	FallFrom = FVector(UltFallSide, 0.f, 0.f).RotateAngleAxis(FMath::Fmod(Yaw * 57.3f, 360.f), FVector::UpVector) + FVector(0.f, 0.f, UltFallRise);
	UpdateLook(0.f);
}

void ARRRandomUltimateMeteor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLook(DeltaSeconds);
	if (!HasAuthority())
	{
		return;
	}
	if (HasLanded() && !bLanded)
	{
		bLanded = true;
		Land();
	}
	if (bLanded && !bTaken)
	{
		TryPickUp();
	}
	// Gone a moment after it is taken (the pop plays first)
	if (bTaken && GetWorld()->GetTimeSeconds() - TakenAt > 1.f)
	{
		Destroy();
	}
}

void ARRRandomUltimateMeteor::TryPickUp()
{
	const FVector Here = GetActorLocation();
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Who = *It;
		const FVector Feet = Who->GetActorLocation() - FVector(0.f, 0.f, Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (FVector::Dist2D(Feet, Here) > PickupRadius + Who->GetCapsuleComponent()->GetScaledCapsuleRadius() || FMath::Abs(Feet.Z - Here.Z) > 250.f || !CanTake(Who))
		{
			continue;
		}
		Who->GetDiceBuffs()->AddCharge(true);
		bTaken = true;
		TakenAt = GetWorld()->GetTimeSeconds();
		ShowTaken();
		UE_LOG(LogTemp, Log, TEXT("RRRandom: %s (team %d) takes the ultimate die."), *Who->GetName(), Who->GetTeam());
		return;
	}
}

void ARRRandomUltimateMeteor::OnRep_Taken()
{
	if (bTaken)
	{
		ShowTaken();
	}
}

void ARRRandomUltimateMeteor::ShowTaken()
{
	if (bTakenShown)
	{
		return;
	}
	bTakenShown = true;
	const FVector Where = GetActorLocation() + FVector(0.f, 0.f, UltDieHeight);
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->StarImpact(Where, FVector::UpVector, 1.6f, GetGold());
	}
	ARRRandomHUD::PopComicWord(this, TEXT("ULTIMATE!"), Where + FVector(0.f, 0.f, 120.f), GetGold(), 1.1f);
}

void ARRRandomUltimateMeteor::Land()
{
	const FVector Target = GetActorLocation();
	// No harm: everyone close knocked aside
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Friend = *It;
		const FVector Offset = Friend->GetActorLocation() - Target;
		if (!Friend->IsAlive() || Friend->IsDecoy() || Friend->GetVehicle() || Offset.Size2D() > UltKnockRadius || FMath::Abs(Offset.Z) > 400.f)
		{
			continue;
		}
		const FVector Away = Offset.Size2D() > 10.f ? Offset.GetSafeNormal2D() : FVector(1.f, 0.f, 0.f);
		Friend->LaunchCharacter(Away * UltKnockSpeed + FVector(0.f, 0.f, UltKnockLift), true, true);
	}
	UE_LOG(LogTemp, Log, TEXT("RRRandom: the ultimate die meteor lands at %s."), *Target.ToCompactString());
}

void ARRRandomUltimateMeteor::UpdateLook(float DeltaSeconds)
{
	const float T = -GetTimeToImpact();
	const FVector Target = GetActorLocation();
	const float Now = GetWorld()->GetTimeSeconds();

	// The pad: glowing gold round the spot while it comes, pulsing quicker as it nears
	const bool bPad = T < 0.f;
	Ring->SetVisibility(bPad);
	if (bPad)
	{
		const float Left = FMath::Clamp(-T / Lead, 0.f, 1.f);
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * (6.f + 10.f * (1.f - Left)));
		Ring->SetWorldLocationAndRotation(Target + FVector(0.f, 0.f, 8.f), FRotator::ZeroRotator);
		Ring->SetWorldScale3D(FVector(UltKnockRadius / 50.f, UltKnockRadius / 50.f, 1.f));
		if (RingMaterial)
		{
			RingMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.5f + 0.5f * Pulse);
		}
	}

	// The rock: streaking down its last UltFallTime with a golden tail, then sinking into the crater
	const bool bFalling = T >= -UltFallTime && T < 0.f;
	FVector Where = Target;
	float Size = 1.f;
	if (bFalling)
	{
		Where = FMath::Lerp(Target + FallFrom, Target, UltEaseIn((T + UltFallTime) / UltFallTime));
	}
	else if (T >= 0.f)
	{
		Size = FMath::Clamp(1.f - T / UltSinkTime, 0.f, 1.f);
		Where = Target - FVector(0.f, 0.f, UltRockHeight * 0.5f * (1.f - Size));
	}
	const bool bRock = (bFalling || T >= 0.f) && Size > 0.02f;
	Rock->SetVisibility(bRock);
	if (bRock)
	{
		Rock->SetWorldLocationAndRotation(Where, FRotator(T * 220.f, T * 300.f, 0.f));
		Rock->SetWorldScale3D(FVector(UltRockHeight / 100.f * Size));
	}
	if (bFalling && DeltaSeconds > 0.f)
	{
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			const FVector Before = FMath::Lerp(Target + FallFrom, Target, UltEaseIn((T - DeltaSeconds + UltFallTime) / UltFallTime));
			Effects->StarTail(Before, Where, UltRockHeight * 0.5f, 2200.f, GetGold(), DeltaSeconds);
		}
	}
	if (T >= 0.f && !bLandingShown)
	{
		bLandingShown = true;
		if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
		{
			Effects->StarImpact(Target, -FallFrom, 2.f, GetGold());
			Effects->FoxPoof(Target + FVector(0.f, 0.f, 30.f), 1.2f);
		}
		ARRRandomHUD::PopComicWord(this, TEXT("CLINK!"), Target + FVector(0.f, 0.f, UltRockHeight * 1.2f), GetGold(), 1.1f);
	}

	// The golden die over the crater (once the rock has sunk a little), bobbing and turning under its beam of light, till taken
	const float Grow = FMath::Clamp((T - 0.3f) / 0.5f, 0.f, 1.f);
	const bool bDie = Grow > 0.f && !bTaken;
	Die->SetVisibility(bDie);
	Beam->SetVisibility(bDie || bPad);
	Light->SetVisibility(bDie || bFalling);
	if (bDie)
	{
		const float Bob = FMath::Sin(Now * 2.2f) * 18.f;
		const float Pop = Grow < 1.f ? FMath::Sin(Grow * PI * 0.5f) * (1.f + 0.25f * FMath::Sin(Grow * PI)) : 1.f;
		Die->SetWorldLocationAndRotation(Target + FVector(0.f, 0.f, UltDieHeight + Bob), FRotator(20.f, Now * 70.f, 15.f));
		const float Mesh = Die->GetStaticMesh() ? FMath::Max(1.f, Die->GetStaticMesh()->GetBounds().BoxExtent.GetMax() * 2.f) : 100.f;
		Die->SetWorldScale3D(FVector(UltDieSize / Mesh * Pop));
		Light->SetWorldLocation(Target + FVector(0.f, 0.f, UltDieHeight));
		Light->SetIntensity(9000.f);
	}
	else if (bFalling)
	{
		Light->SetWorldLocation(Where);
		Light->SetIntensity(14000.f);
	}
	if (Beam->IsVisible())
	{
		// (the engine cylinder is 100 across and 100 tall, round its middle)
		const float Width = bDie ? UltBeamWidth : UltBeamWidth * 0.5f;
		Beam->SetWorldLocationAndRotation(Target + FVector(0.f, 0.f, UltBeamHeight * 0.5f), FRotator::ZeroRotator);
		Beam->SetWorldScale3D(FVector(Width / 100.f, Width / 100.f, UltBeamHeight / 100.f));
		if (BeamMaterial)
		{
			BeamMaterial->SetScalarParameterValue(TEXT("Opacity"), (bDie ? 0.22f : 0.12f) + 0.06f * FMath::Sin(Now * 3.f));
		}
	}
}
