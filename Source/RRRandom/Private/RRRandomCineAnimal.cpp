#include "RRRandomCineAnimal.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "MaterialShaderPrecompileMode.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "RRRandomFurComponent.h"
#include "RRRandomOverheadDieComponent.h"
#include "RRRandomSpecies.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	// The game's numbers (URRRandomGunComponent's GunLooks and bones, ARRRandomCharacter's mesh placement and shield)
	const FVector MeshFeet(0.f, 0.f, 0.f);
	const FRotator MeshTurn(0.f, -90.f, 0.f);
	const FName RightHandBone(TEXT("righthand"));
	const FName RightPalmBone(TEXT("righthandindex1"));
	const FName ElbowBone(TEXT("LeftForeArm"));
	const FName WristBone(TEXT("LeftHand"));
	const FName ChestBone(TEXT("Spine2"));
	constexpr float PistolLength = 34.f;
	const FVector PistolGrip(0.f, 0.f, 8.f);
	constexpr float HammerLength = 86.f;
	const FVector HammerGrip(0.f, 0.f, 0.f);
	constexpr float ShieldSize = 0.9f;
	constexpr float ShieldOnArm = 0.62f;
	constexpr float ShieldOffArm = 9.f;
	constexpr float ShieldCarryForward = 0.35f;

	UStaticMeshComponent* MakeProp(AActor* Owner, USceneComponent* Parent, const TCHAR* Name, const TCHAR* Path)
	{
		UStaticMeshComponent* Prop = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Prop->SetupAttachment(Parent);
		Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Prop->SetUsingAbsoluteLocation(true);
		Prop->SetUsingAbsoluteRotation(true);
		Prop->SetUsingAbsoluteScale(true);
		Prop->SetVisibility(false);
		ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(Path);
		if (Mesh.Succeeded())
		{
			Prop->SetStaticMesh(Mesh.Object);
		}
		return Prop;
	}
}

ARRRandomCineAnimal::ARRRandomCineAnimal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	Feet = CreateDefaultSubobject<USceneComponent>(TEXT("Feet"));
	RootComponent = Feet;
	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Feet);
	Body->SetRelativeLocationAndRotation(MeshFeet, MeshTurn);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	// Bones moved every frame, seen or not (the props are placed from them)
	Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	Fur = CreateDefaultSubobject<URRRandomFurComponent>(TEXT("Fur"));
	Fur->SetupAttachment(Body);
	// The expression's muzzle part (ExpressionParts), empty until an expression has one
	FacePart = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FacePart"));
	FacePart->SetupAttachment(Body);
	FacePart->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FacePart->SetGenerateOverlapEvents(false);
	FacePart->bUseBoundsFromLeaderPoseComponent = true;
	FacePart->SetVisibility(false);
	Pistol = MakeProp(this, Body, TEXT("Pistol"), TEXT("/Game/Props/GunsR90/SM_Pistol.SM_Pistol"));
	Hammer = MakeProp(this, Body, TEXT("Hammer"), TEXT("/Game/Props/Weapons98/SM_Hammer.SM_Hammer"));
	Shield = MakeProp(this, Body, TEXT("Shield"), TEXT("/Game/Props/Weapons98/SM_Shield.SM_Shield"));
	Shield->SetUsingAbsoluteLocation(false);
	Shield->SetUsingAbsoluteRotation(false);
	Shield->SetUsingAbsoluteScale(false);
}

void ARRRandomCineAnimal::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	AppliedSpecies = -1;
	ApplyLook();
	PlaceProps();
}

void ARRRandomCineAnimal::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AppliedSpecies != Species)
	{
		ApplyLook();
	}
	if (!bShadersReady && GetWorld() && GetWorld()->IsGameWorld())
	{
		bShadersReady = true;
#if WITH_EDITOR
		for (const TArray<TObjectPtr<UMaterialInterface>>* List : { &Expressions, &ExpressionFurs })
		{
			for (UMaterialInterface* Material : *List)
			{
				if (Material)
				{
					Material->CacheShaders(EMaterialShaderPrecompileMode::Synchronous);
				}
			}
		}
#endif
	}
	if (AppliedExpression != Expression)
	{
		ApplyExpression();
	}
	PlaceProps();
}

void ARRRandomCineAnimal::ApplyExpression()
{
	const TArray<FRRSpeciesLook>& Looks = FRRSpeciesLook::GetAll();
	if (!Looks.IsValidIndex(Species))
	{
		return;
	}
	const FRRSpeciesLook& Look = Looks[Species];
	UMaterialInterface* Wanted = Expressions.IsValidIndex(Expression) && Expressions[Expression] ? Expressions[Expression].Get() : Look.Material.Get();
	if (!Wanted)
	{
		return;
	}
	USkeletalMesh* FurAsset = FurMesh ? FurMesh.Get() : Look.FurMesh.Get();
	const int32 FaceSlot = Body->GetMaterialIndex(TEXT("Face"));
	if (FaceSlot != INDEX_NONE)
	{
		// The face mask wears the expression, the rest of the body the game's material (and the fur over it)
		for (int32 Slot = 0; Slot < Body->GetNumMaterials(); ++Slot)
		{
			Body->SetMaterial(Slot, Slot == FaceSlot ? Wanted : Look.Material.Get());
		}
		Fur->SetFur(FurAsset, Body, Look.Material);
		ShowFacePart(nullptr, nullptr);
		AppliedExpression = Expression;
		return;
	}
	for (int32 Slot = 0; Slot < FMath::Max(1, Body->GetNumMaterials()); ++Slot)
	{
		Body->SetMaterial(Slot, Wanted);
	}
	// The expression's own muzzle part, shaped to its mouth, in place of the body's muzzle
	ShowFacePart(ExpressionParts.IsValidIndex(Expression) ? ExpressionParts[Expression].Get() : nullptr, Wanted);
	Fur->SetFur(FurAsset, Body, Wanted);
	// The fur over the face wears the same expression (its strands take the body's base color)
	UMaterialInterface* FurWanted = ExpressionFurs.IsValidIndex(Expression) ? ExpressionFurs[Expression].Get() : nullptr;
	if (FurWanted)
	{
		for (int32 Slot = 0; Slot < FMath::Max(1, Fur->GetNumMaterials()); ++Slot)
		{
			Fur->SetMaterial(Slot, FurWanted);
		}
	}
	else
	{
		Fur->EmptyOverrideMaterials();
	}
	AppliedExpression = Expression;
}

void ARRRandomCineAnimal::ShowFacePart(USkeletalMesh* Part, UMaterialInterface* Material)
{
	// (as ARRRandomCharacter::SetFacePart: the part on a leader-pose component, the body's "Muzzle" section hidden)
	const int32 MuzzleSlot = Body->GetSkeletalMeshAsset() ? Body->GetMaterialIndex(TEXT("Muzzle")) : INDEX_NONE;
	if (MuzzleSlot == INDEX_NONE)
	{
		Part = nullptr;
	}
	if (FacePart->GetSkeletalMeshAsset() != Part)
	{
		FacePart->SetSkeletalMeshAsset(Part);
		FacePart->SetLeaderPoseComponent(Part ? Body.Get() : nullptr);
	}
	if (Part)
	{
		for (int32 Slot = 0; Slot < FMath::Max(1, FacePart->GetNumMaterials()); ++Slot)
		{
			FacePart->SetMaterial(Slot, Material);
		}
		FacePart->SetRenderCustomDepth(Body->bRenderCustomDepth);
		FacePart->SetCustomDepthStencilValue(Body->CustomDepthStencilValue);
	}
	FacePart->SetVisibility(Part != nullptr);
	if (MuzzleSlot != INDEX_NONE)
	{
		for (int32 Lod = 0; Lod < Body->GetNumLODs(); ++Lod)
		{
			Body->ShowMaterialSection(MuzzleSlot, 0, Part == nullptr, Lod);
		}
	}
}

FRotator URRRandomCineLibrary::DieFaceRotation(int32 Face, FRotator Camera)
{
	return URRRandomOverheadDieComponent::FaceToward(Face, Camera.Quaternion()).Rotator();
}

void URRRandomCineLibrary::SetSectionEasing(UMovieSceneSection* Section, float EaseInSeconds, float EaseOutSeconds)
{
	if (!Section)
	{
		return;
	}
	const UMovieScene* Scene = Section->GetTypedOuter<UMovieScene>();
	const double TicksPerSecond = Scene ? Scene->GetTickResolution().AsDecimal() : 24000.0;
	Section->Modify();
	Section->Easing.bManualEaseIn = EaseInSeconds > 0.f;
	Section->Easing.ManualEaseInDuration = FMath::RoundToInt32(EaseInSeconds * TicksPerSecond);
	Section->Easing.bManualEaseOut = EaseOutSeconds > 0.f;
	Section->Easing.ManualEaseOutDuration = FMath::RoundToInt32(EaseOutSeconds * TicksPerSecond);
}

int32 URRRandomCineLibrary::ReplaceMaterialTexture(UMaterial* Material, UTexture* From, UTexture* To)
{
	int32 Count = 0;
#if WITH_EDITOR
	if (!Material || !From || !To)
	{
		return 0;
	}
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		UMaterialExpressionTextureBase* Sample = Cast<UMaterialExpressionTextureBase>(Expression);
		if (Sample && Sample->Texture == From)
		{
			Sample->Modify();
			Sample->Texture = To;
			++Count;
		}
	}
	if (Count > 0)
	{
		Material->PreEditChange(nullptr);
		Material->PostEditChange();
		Material->MarkPackageDirty();
	}
#endif
	return Count;
}

void ARRRandomCineAnimal::ApplyLook()
{
	const TArray<FRRSpeciesLook>& Looks = FRRSpeciesLook::GetAll();
	if (!Looks.IsValidIndex(Species) || !Looks[Species].Mesh)
	{
		return;
	}
	const FRRSpeciesLook& Look = Looks[Species];
	USkeletalMesh* Wanted = BodyMesh ? BodyMesh.Get() : Look.Mesh.Get();
	if (Body->GetSkeletalMeshAsset() != Wanted)
	{
		Body->SetSkeletalMeshAsset(Wanted);
	}
	Body->SetRelativeScale3D(FVector(Look.MeshScale));
	// The game's tintable body material on every slot (ARRRandomCharacter::BeginPlay), and the toon outline's stencil
	if (Look.Material)
	{
		for (int32 Slot = 0; Slot < FMath::Max(1, Body->GetNumMaterials()); ++Slot)
		{
			Body->SetMaterial(Slot, Look.Material);
		}
	}
	Body->SetRenderCustomDepth(true);
	Body->SetCustomDepthStencilValue(1);
	for (UStaticMeshComponent* Prop : { Pistol.Get(), Hammer.Get(), Shield.Get() })
	{
		Prop->SetRenderCustomDepth(true);
		Prop->SetCustomDepthStencilValue(1);
	}
	Fur->SetFur(FurMesh ? FurMesh.Get() : Look.FurMesh.Get(), Body, Look.Material);
	AppliedSpecies = Species;
	ApplyExpression();
}

void ARRRandomCineAnimal::PlaceProps()
{
	const bool bHands = Body->GetBoneIndex(RightHandBone) != INDEX_NONE && Body->GetBoneIndex(RightPalmBone) != INDEX_NONE;
	Pistol->SetVisibility(bHoldPistol && bHands);
	Hammer->SetVisibility(bHoldHammer && bHands);
	if (bHoldPistol && bHands)
	{
		PlaceInFist(Pistol, PistolLength, PistolGrip);
	}
	if (bHoldHammer && bHands)
	{
		PlaceInFist(Hammer, HammerLength, HammerGrip);
	}
	const bool bArm = Body->GetBoneIndex(ElbowBone) != INDEX_NONE && Body->GetBoneIndex(WristBone) != INDEX_NONE && Body->GetBoneIndex(ChestBone) != INDEX_NONE;
	Shield->SetVisibility(bHoldShield && bArm);
	if (bHoldShield && bArm)
	{
		PlaceShield();
	}
}

void ARRRandomCineAnimal::PlaceInFist(UStaticMeshComponent* Prop, float Length, const FVector& Grip) const
{
	const TArray<FRRSpeciesLook>& Looks = FRRSpeciesLook::GetAll();
	const FVector FixOffset = Looks.IsValidIndex(Species) ? Looks[Species].GunHandOffset : FVector::ZeroVector;
	const FRotator FixTurn = Looks.IsValidIndex(Species) ? Looks[Species].GunHandTurn : FRotator::ZeroRotator;
	const float Size = GetActorScale3D().Z;
	const FVector RightHand = Body->GetBoneLocation(RightHandBone);
	const FVector Palm = (RightHand + Body->GetBoneLocation(RightPalmBone)) * 0.5f;
	const FQuat Fist = Body->GetBoneQuaternion(RightHandBone) * FixTurn.Quaternion();
	const FQuat HandRotation = Fist;
	const FVector HandLocation = Palm + Fist.RotateVector(FixOffset) * Size - HandRotation.RotateVector(Grip) * Length / 100.f * Size;
	Prop->SetWorldLocationAndRotation(HandLocation, HandRotation);
	Prop->SetWorldScale3D(FVector(Length / 100.f * Size));
}

void ARRRandomCineAnimal::PlaceShield() const
{
	// ARRRandomCharacter::UpdateShield, carried (not guarding): in the body's own space
	const FVector Elbow = Body->GetSocketTransform(ElbowBone, RTS_Component).GetLocation();
	const FVector Wrist = Body->GetSocketTransform(WristBone, RTS_Component).GetLocation();
	const FVector Chest = Body->GetSocketTransform(ChestBone, RTS_Component).GetLocation();
	const FVector Along = (Wrist - Elbow).GetSafeNormal();
	const FVector Middle = FMath::Lerp(Elbow, Wrist, ShieldOnArm);
	const FTransform ToBody = Body->GetComponentTransform().Inverse();
	const FVector Forward = ToBody.TransformVectorNoScale(GetActorForwardVector()).GetSafeNormal();
	const FVector Up = ToBody.TransformVectorNoScale(FVector::UpVector).GetSafeNormal();
	const FVector Out = ((Middle - Chest) - Up * FVector::DotProduct(Middle - Chest, Up) * 0.7f).GetSafeNormal();
	FVector Face = FMath::Lerp(Out, Forward, ShieldCarryForward).GetSafeNormal();
	Face = (Face - Along * FVector::DotProduct(Face, Along) * 0.85f).GetSafeNormal();
	if (Face.IsNearlyZero())
	{
		Face = Forward;
	}
	const float BodyScale = FMath::Max(Body->GetComponentScale().Z, 0.01f);
	Shield->SetRelativeLocationAndRotation(Middle + Face * (ShieldOffArm / BodyScale), FRotationMatrix::MakeFromXZ(Face, Up).ToQuat());
	Shield->SetRelativeScale3D(FVector(ShieldSize / BodyScale));
}
