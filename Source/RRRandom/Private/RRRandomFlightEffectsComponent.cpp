#include "RRRandomFlightEffectsComponent.h"
#include "RRRandomCharacter.h"
#include "RRRandomKit.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// How far down to look for the ground, and how deep the shadow and dust decals reach around it (so they still land
	// on a step or a slope)
	constexpr float GroundSearch = 4000.f;
	constexpr float DecalDepth = 90.f;
	// Rings at once (take-offs and landings rarely overlap more than this)
	constexpr int32 DustRings = 3;
	// The engine plane is 100 across
	constexpr float PlaneSize = 100.f;
	// Streaks appear around the body: this far to the sides and up or down from its middle
	constexpr float StreakSpread = 45.f;
	constexpr float StreakRise = 55.f;
	constexpr float StreakBrightness = 0.85f;
}

URRRandomFlightEffectsComponent::URRRandomFlightEffectsComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the movement has moved the brawler this frame
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(false);

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Blob(TEXT("/Game/Effects/Flight/M_BlobShadow.M_BlobShadow"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Dust(TEXT("/Game/Effects/Flight/M_DustRing.M_DustRing"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Streak(TEXT("/Game/Effects/Flight/M_WindStreak.M_WindStreak"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	ShadowMaterialAsset = Blob.Succeeded() ? Blob.Object : nullptr;
	DustMaterialAsset = Dust.Succeeded() ? Dust.Object : nullptr;
	StreakMaterialAsset = Streak.Succeeded() ? Streak.Object : nullptr;
	PlaneMesh = Plane.Succeeded() ? Plane.Object : nullptr;
}

void URRRandomFlightEffectsComponent::OnRegister()
{
	Super::OnRegister();
	AActor* Owner = GetOwner();
	if (!Owner || Shadow || !GetWorld() || !GetWorld()->IsGameWorld())
	{
		return;
	}

	auto MakeDecal = [this, Owner](UMaterialInterface* Material, UMaterialInstanceDynamic*& OutMaterial)
	{
		UDecalComponent* Decal = NewObject<UDecalComponent>(Owner);
		Decal->SetupAttachment(this);
		// Placed on the ground in world space, pointing straight down
		Decal->SetUsingAbsoluteLocation(true);
		Decal->SetUsingAbsoluteRotation(true);
		Decal->SetUsingAbsoluteScale(true);
		Decal->SetWorldRotation(FRotator(-90.f, 0.f, 0.f));
		OutMaterial = Material ? UMaterialInstanceDynamic::Create(Material, this) : nullptr;
		Decal->SetDecalMaterial(OutMaterial);
		Decal->SetVisibility(false);
		Decal->RegisterComponent();
		return Decal;
	};
	UMaterialInstanceDynamic* Made = nullptr;
	Shadow = MakeDecal(ShadowMaterialAsset, Made);
	ShadowMaterial = Made;
	for (int32 Ring = 0; Ring < DustRings; ++Ring)
	{
		DustDecals.Add(MakeDecal(DustMaterialAsset, Made));
		DustMaterials.Add(Made);
		DustAge.Add(-1.f);
		DustStrength.Add(0.f);
	}

	if (PlaneMesh && StreakMaterialAsset)
	{
		Streaks = NewObject<UInstancedStaticMeshComponent>(Owner);
		Streaks->SetStaticMesh(PlaneMesh);
		Streaks->SetMaterial(0, StreakMaterialAsset);
		Streaks->NumCustomDataFloats = 1;
		Streaks->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Streaks->SetGenerateOverlapEvents(false);
		Streaks->SetCanEverAffectNavigation(false);
		Streaks->CastShadow = false;
		Streaks->SetupAttachment(this);
		Streaks->SetUsingAbsoluteLocation(true);
		Streaks->SetUsingAbsoluteRotation(true);
		Streaks->SetUsingAbsoluteScale(true);
		Streaks->RegisterComponent();
		Streaks->SetWorldTransform(FTransform::Identity);
	}
}

void URRRandomFlightEffectsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const ARRRandomCharacter* Brawler = Cast<ARRRandomCharacter>(GetOwner());
	if (!Brawler || !Shadow)
	{
		return;
	}
	const UCapsuleComponent* Capsule = Brawler->GetCapsuleComponent();
	const float Size = Brawler->GetActorScale3D().Z;
	const FVector Feet = Brawler->GetActorLocation() - FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight());

	// The ground under the brawler: only what stays put (floor, hills, cover, roads, bridges), not other brawlers
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RRRandomFlightShadow), false, Brawler);
	bHasGround = GetWorld()->LineTraceSingleByObjectType(Hit, Feet + FVector(0.f, 0.f, 10.f), Feet - FVector(0.f, 0.f, GroundSearch),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params);
	GroundPoint = bHasGround ? Hit.ImpactPoint : Feet;
	HeightOffGround = bHasGround ? FMath::Max(0.f, Feet.Z - GroundPoint.Z) : GroundSearch;

	const UCharacterMovementComponent* Movement = Brawler->GetCharacterMovement();
	const bool bInAir = Movement && Movement->IsFalling();
	// (none under an invisible opponent: it would give it away)
	const bool bShow = bHasGround && Brawler->IsAlive() && !Brawler->GetVehicle() && !Brawler->GetKit()->IsCloakedOnThisScreen();
	UpdateShadow(Feet, bShow, Size);
	UpdateDust(Feet, bInAir && Brawler->IsAlive(), DeltaTime, Size);

	const float Speed = Brawler->GetVelocity().Size2D();
	UpdateStreaks(bInAir && Brawler->IsAlive() && HeightOffGround > StreakMinHeight * Size && Speed > StreakMinSpeed, Speed, DeltaTime, Size);
}

void URRRandomFlightEffectsComponent::UpdateShadow(const FVector& Feet, bool bShow, float Size)
{
	if (Shadow->IsVisible() != bShow)
	{
		Shadow->SetVisibility(bShow);
	}
	if (!bShow)
	{
		return;
	}
	// Higher up: smaller and fainter
	const float Up = FMath::Clamp(HeightOffGround / FMath::Max(ShadowFadeHeight * Size, 1.f), 0.f, 1.f);
	const float Radius = ShadowRadius * Size * FMath::Lerp(1.f, ShadowSmallest, Up);
	Shadow->DecalSize = FVector(DecalDepth, Radius, Radius);
	Shadow->SetWorldLocationAndRotation(GroundPoint, FRotator(-90.f, 0.f, 0.f));
	if (ShadowMaterial)
	{
		ShadowMaterial->SetScalarParameterValue(TEXT("Opacity"), FMath::Lerp(ShadowOpacity.X, ShadowOpacity.Y, Up));
	}
	Shadow->MarkRenderStateDirty();
}

void URRRandomFlightEffectsComponent::StartDust(const FVector& Ground, float Strength, float Size)
{
	// The oldest ring makes way
	int32 Pick = 0;
	for (int32 Ring = 0; Ring < DustAge.Num(); ++Ring)
	{
		if (DustAge[Ring] < 0.f)
		{
			Pick = Ring;
			break;
		}
		if (DustAge[Ring] > DustAge[Pick])
		{
			Pick = Ring;
		}
	}
	if (!DustDecals.IsValidIndex(Pick))
	{
		return;
	}
	DustAge[Pick] = 0.f;
	DustStrength[Pick] = Strength * Size;
	DustDecals[Pick]->SetWorldLocationAndRotation(Ground, FRotator(-90.f, 0.f, 0.f));
	DustDecals[Pick]->SetVisibility(true);
}

void URRRandomFlightEffectsComponent::UpdateDust(const FVector& Feet, bool bInAir, float DeltaTime, float Size)
{
	// Leaving the ground fast (a jump, a flight lift) or landing after a drop throws a ring of dust
	if (bInAir)
	{
		const AActor* Owner = GetOwner();
		if (!bWasInAir)
		{
			AirPeakZ = Feet.Z;
			if (Owner->GetVelocity().Z > TakeOffSpeed && bHasGround && HeightOffGround < 60.f)
			{
				StartDust(GroundPoint, 0.6f, Size);
			}
		}
		AirPeakZ = FMath::Max(AirPeakZ, Feet.Z);
	}
	else if (bWasInAir)
	{
		const float Drop = AirPeakZ - Feet.Z;
		if (Drop > DustMinDrop * Size && bHasGround)
		{
			StartDust(GroundPoint, FMath::GetMappedRangeValueClamped(FVector2D(DustMinDrop, DustFullDrop) * Size, FVector2D(0.5f, 1.f), Drop), Size);
		}
	}
	bWasInAir = bInAir;

	for (int32 Ring = 0; Ring < DustAge.Num(); ++Ring)
	{
		if (DustAge[Ring] < 0.f)
		{
			continue;
		}
		DustAge[Ring] += DeltaTime;
		const float Life = DustAge[Ring] / FMath::Max(DustTime, KINDA_SMALL_NUMBER);
		if (Life >= 1.f)
		{
			DustAge[Ring] = -1.f;
			DustDecals[Ring]->SetVisibility(false);
			continue;
		}
		// Puffs out fast and slows, fading as it goes
		const float Grow = 1.f - FMath::Square(1.f - Life);
		const float Radius = DustRadius * DustStrength[Ring] * FMath::Lerp(0.45f, 1.f, Grow);
		DustDecals[Ring]->DecalSize = FVector(DecalDepth, Radius, Radius);
		if (UMaterialInstanceDynamic* Material = DustMaterials[Ring])
		{
			Material->SetScalarParameterValue(TEXT("Ring"), FMath::Lerp(0.35f, 1.f, Grow));
			Material->SetScalarParameterValue(TEXT("Opacity"), 0.75f * (1.f - Life));
		}
		DustDecals[Ring]->MarkRenderStateDirty();
	}
}

void URRRandomFlightEffectsComponent::UpdateStreaks(bool bFlyingFast, float Speed, float DeltaTime, float Size)
{
	if (!Streaks)
	{
		return;
	}
	const AActor* Owner = GetOwner();
	// New streaks around the body, lying along the way it goes, more the faster it flies
	if (bFlyingFast)
	{
		const FVector Velocity = Owner->GetVelocity();
		const FRotator Heading(0.f, FVector(Velocity.X, Velocity.Y, 0.f).Rotation().Yaw, 0.f);
		StreakDebt += DeltaTime * StreakRate * FMath::Clamp((Speed - StreakMinSpeed) / FMath::Max(StreakFullSpeed - StreakMinSpeed, 1.f), 0.2f, 1.f);
		while (StreakDebt >= 1.f)
		{
			StreakDebt -= 1.f;
			if (LiveStreaks.Num() >= MaxStreaks)
			{
				LiveStreaks.RemoveAt(0);
			}
			const FVector Middle = Owner->GetActorLocation() + Heading.RotateVector(FVector(FMath::FRandRange(-30.f, 20.f),
				FMath::FRandRange(-StreakSpread, StreakSpread), FMath::FRandRange(-StreakRise, StreakRise))) * Size;
			const float Length = FMath::FRandRange(StreakLength.X, StreakLength.Y) * Size;
			FStreak& Streak = LiveStreaks.AddDefaulted_GetRef();
			// Its head at the spot, its tail trailing back the way the brawler came
			Streak.Transform = FTransform(Heading, Middle - Heading.Vector() * Length * 0.5f, FVector(Length / PlaneSize, StreakWidth * Size / PlaneSize, 1.f));
		}
	}
	else
	{
		StreakDebt = 0.f;
	}

	for (FStreak& Streak : LiveStreaks)
	{
		Streak.Age += DeltaTime;
	}
	LiveStreaks.RemoveAll([this](const FStreak& Streak) { return Streak.Age >= StreakLife; });
	if (LiveStreaks.Num() == 0 && Streaks->GetInstanceCount() == 0)
	{
		return;
	}
	Streaks->ClearInstances();
	for (const FStreak& Streak : LiveStreaks)
	{
		const int32 Index = Streaks->AddInstance(Streak.Transform, true);
		Streaks->SetCustomDataValue(Index, 0, StreakBrightness * (1.f - Streak.Age / FMath::Max(StreakLife, KINDA_SMALL_NUMBER)));
	}
}
