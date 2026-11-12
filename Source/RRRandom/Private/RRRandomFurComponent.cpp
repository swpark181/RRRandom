#include "RRRandomFurComponent.h"

#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInterface.h"

namespace
{
	// For measuring what the fur costs (and a switch if it's ever too much): rr.Fur 0 hides every animal friend's fur
	TAutoConsoleVariable<int32> CVarFur(TEXT("rr.Fur"), 1, TEXT("Shell fur on the animal friends that have it (1 on, 0 off)."));
}

URRRandomFurComponent::URRRandomFurComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the body has moved and posed
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	// The body's own shadow and silhouette line are enough; the shells would only add their cost and a noisy outline
	CastShadow = false;
	bRenderCustomDepth = false;
	bReceivesDecals = false;
	bUseBoundsFromLeaderPoseComponent = true;
	SetHiddenInGame(true);
}

void URRRandomFurComponent::SetFur(USkeletalMesh* FurMesh, USkeletalMeshComponent* Body, UMaterialInterface* BodyMaterial)
{
	BodyMesh = Body;
	BodyBase = BodyMaterial;
	SetSkeletalMeshAsset(FurMesh);
	SetLeaderPoseComponent(FurMesh ? Body : nullptr);
	Bend = BendSpeed = FVector::ZeroVector;
	bHasLast = false;
	SetHiddenInGame(!ShouldShow());
	SetComponentTickEnabled(FurMesh != nullptr && GetNetMode() != NM_DedicatedServer);
}

bool URRRandomFurComponent::ShouldShow() const
{
	const USkeletalMeshComponent* Body = BodyMesh.Get();
	if (CVarFur.GetValueOnGameThread() == 0 || !GetSkeletalMeshAsset() || !Body || !Body->IsVisible() || Body->bHiddenInGame || (GetOwner() && GetOwner()->IsHidden()))
	{
		return false;
	}
	// Its own material or a tinted copy of it; anything else (invisibility, a ghost or effect look) hides the fur
	UMaterialInterface* Worn = Body->GetMaterial(0);
	UMaterialInterface* Base = BodyBase.Get();
	return Worn && Base && Worn->GetBaseMaterial() == Base->GetBaseMaterial();
}

void URRRandomFurComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const bool bShow = ShouldShow();
	if (bHiddenInGame == bShow)
	{
		SetHiddenInGame(!bShow);
	}
	if (!bShow || DeltaTime <= 0.f || !GetOwner())
	{
		bHasLast = false;
		return;
	}
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	const FVector Velocity = GetOwner()->GetVelocity() / BodyHeight;
	FVector Acceleration = bHasLast ? (Velocity - LastVelocity) / Dt : FVector::ZeroVector;
	// A teleport or respawn isn't a shove
	Acceleration = Acceleration.GetClampedToMaxSize(60.f);
	LastVelocity = Velocity;
	bHasLast = true;
	const FVector Target = -Velocity * Drag - Acceleration * Inertia;
	const float K = FMath::Square(UE_TWO_PI * SpringHz);
	const float C = 2.f * SpringDamping * FMath::Sqrt(K);
	constexpr int32 Steps = 4;
	const float Step = Dt / Steps;
	for (int32 Index = 0; Index < Steps; ++Index)
	{
		BendSpeed += (K * (Target - Bend) - C * BendSpeed) * Step;
		Bend += BendSpeed * Step;
	}
	Bend = Bend.GetClampedToMaxSize(MaxBend);
	SetCustomPrimitiveDataVector3(0, Bend);
}
