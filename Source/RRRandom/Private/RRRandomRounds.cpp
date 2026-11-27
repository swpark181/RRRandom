#include "RRRandomRounds.h"
#include "RRRandomCharacter.h"
#include "RRRandomEffects.h"
#include "RRRandomGameState.h"
#include "RRRandomHUD.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

// ---------------------------------------------------------------------------------------------------------------------
// Rules, cards, respawn

float RRRounds::GetRespawnDelay(int32 KnockoutsBefore, float SinceLastKnockout, float& OutQuickPenalty)
{
	const float Stack = FMath::Min(RespawnStackMax, RespawnPerDeath * FMath::Max(0, KnockoutsBefore));
	OutQuickPenalty = SinceLastKnockout < QuickGap ? QuickPenaltyMax * (1.f - FMath::Max(0.f, SinceLastKnockout) / QuickGap) : 0.f;
	return FMath::Min(RespawnMax, RespawnBase + Stack + OutQuickPenalty);
}

ERRRoundRule RRRounds::GetRuleForFace(int32 Face)
{
	return Face <= 2 ? ERRRoundRule::Deathmatch : Face <= 4 ? ERRRoundRule::MostKills : ERRRoundRule::MeteorRush;
}

FString RRRounds::GetRuleName(ERRRoundRule Rule)
{
	switch (Rule)
	{
	case ERRRoundRule::Deathmatch: return TEXT("DEATHMATCH");
	case ERRRoundRule::MostKills: return TEXT("MOST KO");
	case ERRRoundRule::MeteorRush: return TEXT("METEOR RUSH");
	default: return FString();
	}
}

FString RRRounds::GetRuleLine(ERRRoundRule Rule)
{
	switch (Rule)
	{
	case ERRRoundRule::Deathmatch: return FString::Printf(TEXT("First team to %d KO takes the round"), KillTarget);
	case ERRRoundRule::MostKills: return TEXT("More KO when the clock runs out takes the round");
	case ERRRoundRule::MeteorRush: return FString::Printf(TEXT("Grab the meteors' ore, hold %d for %.0f seconds"), MeteorGoal, MeteorHold);
	default: return FString();
	}
}

FLinearColor RRRounds::GetRuleColor(ERRRoundRule Rule)
{
	switch (Rule)
	{
	case ERRRoundRule::Deathmatch: return FLinearColor::FromSRGBColor(FColor(242, 127, 166));
	case ERRRoundRule::MostKills: return FLinearColor::FromSRGBColor(FColor(154, 123, 224));
	case ERRRoundRule::MeteorRush: return FLinearColor::FromSRGBColor(FColor(233, 169, 63));
	default: return FLinearColor::White;
	}
}

FString RRRounds::GetCardName(ERRCard Card)
{
	switch (Card)
	{
	case ERRCard::Power: return TEXT("POWER");
	case ERRCard::Rapid: return TEXT("RAPID");
	case ERRCard::Swift: return TEXT("SWIFT");
	case ERRCard::Giant: return TEXT("GIANT");
	case ERRCard::Wings: return TEXT("WINGS");
	case ERRCard::Shield: return TEXT("SHIELD");
	case ERRCard::Cloak: return TEXT("CLOAK");
	case ERRCard::Heal: return TEXT("HEAL");
	default: return FString();
	}
}

FString RRRounds::GetCardLine(ERRCard Card, int32 Face)
{
	switch (Card)
	{
	case ERRCard::Power: return FString::Printf(TEXT("+%d%% damage %.0fs"), Face * 6, CardSeconds);
	case ERRCard::Rapid: return FString::Printf(TEXT("+%d%% attack speed %.0fs"), Face * 5, CardSeconds);
	case ERRCard::Swift: return FString::Printf(TEXT("+%d%% move speed %.0fs"), Face * 5, CardSeconds);
	case ERRCard::Giant: return FString::Printf(TEXT("giant %ds"), 3 + Face);
	case ERRCard::Wings: return FString::Printf(TEXT("fly %ds"), 4 + Face);
	case ERRCard::Shield: return FString::Printf(TEXT("%d shield %.0fs"), Face * 12, CardSeconds);
	case ERRCard::Cloak: return FString::Printf(TEXT("invisible %.1fs"), Face * 1.5f);
	case ERRCard::Heal: return FString::Printf(TEXT("+%d health"), 15 + Face * 10);
	default: return FString();
	}
}

FLinearColor RRRounds::GetCardColor(ERRCard Card)
{
	switch (Card)
	{
	case ERRCard::Power: return FLinearColor::FromSRGBColor(FColor(255, 150, 110));
	case ERRCard::Rapid: return FLinearColor::FromSRGBColor(FColor(200, 150, 255));
	case ERRCard::Swift: return FLinearColor::FromSRGBColor(FColor(120, 220, 150));
	case ERRCard::Giant: return FLinearColor::FromSRGBColor(FColor(110, 215, 245));
	case ERRCard::Wings: return FLinearColor::FromSRGBColor(FColor(247, 168, 196));
	case ERRCard::Shield: return FLinearColor::FromSRGBColor(FColor(143, 221, 190));
	case ERRCard::Cloak: return FLinearColor::FromSRGBColor(FColor(189, 166, 240));
	case ERRCard::Heal: return FLinearColor::FromSRGBColor(FColor(247, 216, 138));
	default: return FLinearColor::White;
	}
}

FString RRRounds::GetAbilityName(ERRAbility Ability)
{
	switch (Ability)
	{
	case ERRAbility::Cloak: return TEXT("STEALTH");
	case ERRAbility::SuperHop: return TEXT("SUPER HOP");
	case ERRAbility::AcornTrap: return TEXT("ACORN TRAP");
	case ERRAbility::Giant: return TEXT("GIANT");
	case ERRAbility::Flight: return TEXT("TAKE OFF");
	case ERRAbility::Disguise: return TEXT("DISGUISE");
	default: return FString();
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The arena fence

namespace
{
	// A panel every PanelSpacing along each side, PanelHeight tall (the glass), a post between panels
	constexpr float PanelSpacing = 400.f;
	constexpr float PanelHeight = 260.f;
	constexpr float PanelThickness = 6.f;
	constexpr float PostSize = 22.f;
	constexpr float PostHeight = 300.f;
	// The hidden walls are this thick, just outside the glass
	constexpr float WallThickness = 80.f;
}

ARRRandomArenaFence::ARRRandomArenaFence()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ARRRandomArenaFence::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomArenaFence, HalfExtent);
	DOREPLIFETIME(ARRRandomArenaFence, FloorZ);
}

void ARRRandomArenaFence::Setup(float InHalfExtent, float InFloorZ)
{
	HalfExtent = InHalfExtent;
	FloorZ = InFloorZ;
}

ARRRandomArenaFence* ARRRandomArenaFence::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? *TActorIterator<ARRRandomArenaFence>(const_cast<UWorld*>(World)) : nullptr;
}

bool ARRRandomArenaFence::IsInside(const FVector& Where, float Margin) const
{
	const FVector Offset = Where - GetActorLocation();
	return HalfExtent > 0.f && FMath::Abs(Offset.X) <= HalfExtent - Margin && FMath::Abs(Offset.Y) <= HalfExtent - Margin;
}

void ARRRandomArenaFence::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ARRRandomArenaFence::OnRep_Shape()
{
	Build();
}

void ARRRandomArenaFence::Build()
{
	if (bBuilt || HalfExtent <= 0.f || !HasActorBegunPlay())
	{
		return;
	}
	bBuilt = true;
	const FVector Center(GetActorLocation().X, GetActorLocation().Y, FloorZ);

	// The hidden walls: brawlers (pawns) only, so shots and the camera pass
	for (int32 Side = 0; Side < 4; ++Side)
	{
		UBoxComponent* Wall = NewObject<UBoxComponent>(this);
		const bool bAlongY = Side < 2;
		const float Sign = Side % 2 == 0 ? 1.f : -1.f;
		const float Out = HalfExtent + WallThickness * 0.5f;
		Wall->SetBoxExtent(bAlongY ? FVector(WallThickness * 0.5f, HalfExtent + WallThickness, RRRounds::FenceHeight * 0.5f)
			: FVector(HalfExtent + WallThickness, WallThickness * 0.5f, RRRounds::FenceHeight * 0.5f));
		Wall->SetCollisionProfileName(TEXT("NoCollision"));
		Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Wall->SetCollisionObjectType(ECC_WorldStatic);
		Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
		Wall->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		Wall->SetCanEverAffectNavigation(false);
		Wall->SetupAttachment(RootComponent);
		Wall->SetUsingAbsoluteLocation(true);
		Wall->RegisterComponent();
		Wall->SetWorldLocation(Center + (bAlongY ? FVector(Sign * Out, 0.f, 0.f) : FVector(0.f, Sign * Out, 0.f)) + FVector(0.f, 0.f, RRRounds::FenceHeight * 0.5f - 200.f));
		Walls.Add(Wall);
	}

	// The look: candy glass panels and glowing posts (the battle royale's cloud wall material)
	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Glass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/DiceFX/M_FXCloak.M_FXCloak"));
	if (!Cube || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	auto MakeInstances = [this, Cube](TObjectPtr<UInstancedStaticMeshComponent>& Out)
	{
		Out = NewObject<UInstancedStaticMeshComponent>(this);
		Out->SetStaticMesh(Cube);
		Out->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Out->SetCastShadow(false);
		Out->SetCanEverAffectNavigation(false);
		Out->SetupAttachment(RootComponent);
		Out->SetUsingAbsoluteLocation(true);
		Out->SetUsingAbsoluteRotation(true);
		Out->SetUsingAbsoluteScale(true);
		Out->RegisterComponent();
		Out->SetWorldTransform(FTransform::Identity);
	};
	MakeInstances(Panels);
	MakeInstances(Posts);
	if (Glass)
	{
		PanelMaterial = UMaterialInstanceDynamic::Create(Glass, this);
		PanelMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(FColor(247, 168, 196)));
		PanelMaterial->SetScalarParameterValue(TEXT("Glow"), 1.4f);
		PanelMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.35f);
		PanelMaterial->SetScalarParameterValue(TEXT("Fill"), 0.6f);
		Panels->SetMaterial(0, PanelMaterial);
		PostMaterial = UMaterialInstanceDynamic::Create(Glass, this);
		PostMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(FColor(189, 166, 240)));
		PostMaterial->SetScalarParameterValue(TEXT("Glow"), 3.f);
		PostMaterial->SetScalarParameterValue(TEXT("Opacity"), 0.9f);
		PostMaterial->SetScalarParameterValue(TEXT("Fill"), 1.2f);
		Posts->SetMaterial(0, PostMaterial);
	}
	// The engine cube is 100 across
	const int32 Count = FMath::Max(1, FMath::RoundToInt(2.f * HalfExtent / PanelSpacing));
	const float Step = 2.f * HalfExtent / Count;
	TArray<FTransform> PanelTransforms;
	TArray<FTransform> PostTransforms;
	for (int32 Side = 0; Side < 4; ++Side)
	{
		const bool bAlongY = Side < 2;
		const float Sign = Side % 2 == 0 ? 1.f : -1.f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const float Along = -HalfExtent + Step * (Index + 0.5f);
			const FVector Mid = Center + (bAlongY ? FVector(Sign * HalfExtent, Along, 0.f) : FVector(Along, Sign * HalfExtent, 0.f));
			const FVector PanelScale = bAlongY ? FVector(PanelThickness, Step - PostSize, PanelHeight) / 100.f : FVector(Step - PostSize, PanelThickness, PanelHeight) / 100.f;
			PanelTransforms.Add(FTransform(FQuat::Identity, Mid + FVector(0.f, 0.f, PanelHeight * 0.5f), PanelScale));
			const FVector Corner = Center + (bAlongY ? FVector(Sign * HalfExtent, Along - Step * 0.5f, 0.f) : FVector(Along - Step * 0.5f, Sign * HalfExtent, 0.f));
			PostTransforms.Add(FTransform(FQuat::Identity, Corner + FVector(0.f, 0.f, PostHeight * 0.5f), FVector(PostSize, PostSize, PostHeight) / 100.f));
		}
		// The rail along the top
		const FVector RailMid = Center + (bAlongY ? FVector(Sign * HalfExtent, 0.f, PostHeight) : FVector(0.f, Sign * HalfExtent, PostHeight));
		PostTransforms.Add(FTransform(FQuat::Identity, RailMid, bAlongY ? FVector(PostSize * 0.6f, 2.f * HalfExtent, PostSize * 0.6f) / 100.f
			: FVector(2.f * HalfExtent, PostSize * 0.6f, PostSize * 0.6f) / 100.f));
	}
	Panels->AddInstances(PanelTransforms, false, true);
	Posts->AddInstances(PostTransforms, false, true);
}

// ---------------------------------------------------------------------------------------------------------------------
// Round 102: the dice spots

namespace
{
	// The pad: the meteor's floor mark (a glowing band round a faintly filled disc) a little over the ground, the number
	// lying on it
	constexpr float PadLift = 4.f;
	constexpr float PadGlow = 3.f;
	constexpr float PadBand = 0.1f;
	constexpr float PadFill = 0.f;
	constexpr float NumberSize = 300.f;
	const FColor PadColor(189, 166, 240);
	const FColor NumberColor(58, 28, 140);
}

ARRRandomSpawnZones::ARRRandomSpawnZones()
{
	bReplicates = true;
	bAlwaysRelevant = true;
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	for (uint8& Die : TeamDice)
	{
		Die = 0;
	}
}

void ARRRandomSpawnZones::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARRRandomSpawnZones, Zones);
	DOREPLIFETIME(ARRRandomSpawnZones, TeamDice);
}

void ARRRandomSpawnZones::SetZones(const TArray<FVector>& InZones)
{
	Zones.Reset();
	for (const FVector& Zone : InZones)
	{
		Zones.Add(Zone);
	}
}

void ARRRandomSpawnZones::SetTeamDice(int32 Team, int32 DieA, int32 DieB)
{
	if (Team == 0 || Team == 1)
	{
		TeamDice[Team * 2] = uint8(FMath::Clamp(DieA, 0, 6));
		TeamDice[Team * 2 + 1] = uint8(FMath::Clamp(DieB, 0, 6));
		ForceNetUpdate();
	}
}

bool ARRRandomSpawnZones::GetZone(int32 Number, FVector& OutFeet) const
{
	const int32 Index = Number - FirstNumber;
	if (!Zones.IsValidIndex(Index))
	{
		return false;
	}
	OutFeet = Zones[Index];
	return true;
}

int32 ARRRandomSpawnZones::GetTeamDie(int32 Team, int32 Which) const
{
	return (Team == 0 || Team == 1) && (Which == 0 || Which == 1) ? TeamDice[Team * 2 + Which] : 0;
}

ARRRandomSpawnZones* ARRRandomSpawnZones::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	TActorIterator<ARRRandomSpawnZones> It(const_cast<UWorld*>(World));
	return It ? *It : nullptr;
}

void ARRRandomSpawnZones::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

void ARRRandomSpawnZones::OnRep_Zones()
{
	Build();
}

void ARRRandomSpawnZones::Build()
{
	if (bBuilt || Zones.Num() == 0 || !HasActorBegunPlay() || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	bBuilt = true;
	// (the meteor's floor mark: a band at the edge of a flat plane, the inside faintly filled)
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* Marker = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/MeteorRush/M_MRRing.M_MRRing"));
	if (!Plane)
	{
		return;
	}
	if (Marker)
	{
		PadMaterial = UMaterialInstanceDynamic::Create(Marker, this);
		PadMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor::FromSRGBColor(PadColor));
		PadMaterial->SetScalarParameterValue(TEXT("Glow"), PadGlow);
		PadMaterial->SetScalarParameterValue(TEXT("Thickness"), PadBand);
		PadMaterial->SetScalarParameterValue(TEXT("Fill"), PadFill);
	}
	for (int32 Index = 0; Index < Zones.Num(); ++Index)
	{
		const FVector Feet = Zones[Index];
		// A mesh each (the ring's material isn't made for instances); the engine's plane is 100 across, flat, facing up
		UStaticMeshComponent* Pad = NewObject<UStaticMeshComponent>(this);
		Pad->SetStaticMesh(Plane);
		Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pad->SetCastShadow(false);
		Pad->SetCanEverAffectNavigation(false);
		Pad->SetupAttachment(RootComponent);
		Pad->SetUsingAbsoluteLocation(true);
		Pad->SetUsingAbsoluteRotation(true);
		Pad->SetUsingAbsoluteScale(true);
		if (PadMaterial)
		{
			Pad->SetMaterial(0, PadMaterial);
		}
		Pad->RegisterComponent();
		Pad->SetWorldTransform(FTransform(FQuat::Identity, Feet + FVector(0.f, 0.f, PadLift), FVector(PadRadius / 50.f, PadRadius / 50.f, 1.f)));
		Pads.Add(Pad);
		UTextRenderComponent* Number = NewObject<UTextRenderComponent>(this);
		Number->SetupAttachment(RootComponent);
		Number->SetUsingAbsoluteLocation(true);
		Number->SetUsingAbsoluteRotation(true);
		Number->SetCastShadow(false);
		Number->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Number->SetHorizontalAlignment(EHTA_Center);
		Number->SetVerticalAlignment(EVRTA_TextCenter);
		Number->SetWorldSize(NumberSize);
		Number->SetTextRenderColor(NumberColor);
		Number->SetText(FText::AsNumber(Index + FirstNumber));
		Number->RegisterComponent();
		Number->SetWorldLocationAndRotation(Feet + FVector(0.f, 0.f, PadLift + 3.f), FRotator(90.f, 180.f, 0.f));
		Numbers.Add(Number);
	}
}

void ARRRandomSpawnZones::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// The numbers lie on the ground turned to read from this machine's camera (the top away from it)
	if (Numbers.Num() == 0)
	{
		return;
	}
	const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!Camera)
	{
		return;
	}
	const FRotator Facing(90.f, Camera->GetCameraRotation().Yaw + 180.f, 0.f);
	for (UTextRenderComponent* Number : Numbers)
	{
		if (Number)
		{
			Number->SetWorldRotation(Facing);
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The squirrel's acorn trap

ARRRandomAcornTrap::ARRRandomAcornTrap()
{
	bReplicates = true;
	SetReplicateMovement(true);
	PrimaryActorTick.bCanEverTick = true;
	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	RootComponent = Visual;
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Nut(TEXT("/Game/Effects/Acorn/SM_AcornNut.SM_AcornNut"));
	if (Nut.Succeeded())
	{
		Visual->SetStaticMesh(Nut.Object);
	}
}

int32 ARRRandomAcornTrap::GetTeam() const
{
	const ARRRandomCharacter* Setter = Cast<ARRRandomCharacter>(GetOwner());
	return Setter ? Setter->GetTeam() : INDEX_NONE;
}

void ARRRandomAcornTrap::BeginPlay()
{
	Super::BeginPlay();
	// The acorn models are about 100 tall: a little nut half sunk in the ground
	SetActorScale3D(FVector(0.55f));
	if (HasAuthority())
	{
		SetLifeSpan(Lifetime + 1.f);
	}
}

void ARRRandomAcornTrap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	// A slow wobble once armed, so it reads as live
	if (Age > ArmTime && !bBurst)
	{
		const float Wobble = FMath::Sin(Age * 6.f) * 8.f;
		Visual->SetRelativeRotation(FRotator(Wobble, Age * 40.f, 0.f));
	}
	if (!HasAuthority() || bBurst || Age < ArmTime || ARRRandomGameState::IsPlayLocked(this))
	{
		return;
	}
	if (Age > Lifetime)
	{
		Destroy();
		return;
	}
	const int32 Team = GetTeam();
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		if (It->IsAlive() && It->GetTeam() != Team && !It->IsDecoy() && FVector::Dist(It->GetActorLocation(), GetActorLocation()) < TriggerRadius + It->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			Burst();
			return;
		}
	}
}

void ARRRandomAcornTrap::Burst()
{
	bBurst = true;
	ARRRandomCharacter* Setter = Cast<ARRRandomCharacter>(GetOwner());
	const int32 Team = GetTeam();
	const FVector Where = GetActorLocation();
	for (TActorIterator<ARRRandomCharacter> It(GetWorld()); It; ++It)
	{
		ARRRandomCharacter* Victim = *It;
		const FVector Offset = Victim->GetActorLocation() - Where;
		if (!Victim->IsAlive() || Victim->GetTeam() == Team || Offset.Size() > BlastRadius + Victim->GetCapsuleComponent()->GetScaledCapsuleRadius())
		{
			continue;
		}
		const float Share = 1.f - 0.5f * FMath::Clamp(Offset.Size() / BlastRadius, 0.f, 1.f);
		FVector Away = Offset.GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = FVector::ForwardVector;
		}
		// Thrown up and away, spinning if it goes down (the user's "날아가는 장면 과장")
		Victim->SetBlastImpact(Away * 650.f + FVector(0.f, 0.f, 300.f));
		FHitResult Hit;
		Hit.ImpactPoint = Victim->GetActorLocation();
		Hit.Location = Hit.ImpactPoint;
		UGameplayStatics::ApplyPointDamage(Victim, BlastDamage * Share, Away, Hit, Setter ? Setter->GetController() : nullptr, Setter ? static_cast<AActor*>(Setter) : this, UDamageType::StaticClass());
		if (Victim->IsAlive())
		{
			Victim->LaunchCharacter(Away * 450.f + FVector(0.f, 0.f, 520.f), true, true);
		}
	}
	MulticastBurst(Where);
	SetLifeSpan(0.3f);
	SetActorHiddenInGame(true);
	UE_LOG(LogTemp, Log, TEXT("RRRandom: %s's acorn trap bursts."), Setter ? *Setter->GetName() : TEXT("someone"));
}

void ARRRandomAcornTrap::MulticastBurst_Implementation(FVector_NetQuantize Where)
{
	if (ARRRandomEffects* Effects = ARRRandomEffects::Get(this))
	{
		Effects->AcornPop(Where, 1.6f);
		Effects->RocketBlast(Where, FVector::UpVector, 0.8f);
	}
	ARRRandomHUD::PopComicWord(this, TEXT("CRUNCH!"), Where + FVector(0.f, 0.f, 120.f), FLinearColor::FromSRGBColor(FColor(233, 169, 63)), 1.1f);
}
