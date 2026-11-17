#include "RRRandomViewCut.h"
#include "RRRandomDiceTown.h"
#include "RRRandomIsland.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// Put away: this far straight down (the town's own stowing uses the same drop)
	constexpr float AwayDrop = 200000.f;

	FTransform Away(const FTransform& Was)
	{
		return FTransform(Was.GetRotation(), Was.GetLocation() - FVector(0.f, 0.f, AwayDrop), Was.GetScale3D());
	}

	bool IsAway(const FTransform& Now, const FTransform& Was)
	{
		return FMath::IsNearlyEqual(Now.GetLocation().Z, Was.GetLocation().Z - AwayDrop, 10.) && FVector::DistSquared2D(Now.GetLocation(), Was.GetLocation()) < 100.;
	}
}

TMap<FRRViewCut::FKey, FTransform>& FRRViewCut::GetAllHeld()
{
	static TMap<FKey, FTransform> All;
	return All;
}

bool FRRViewCut::GetTrueTransform(const UInstancedStaticMeshComponent* Instances, int32 Index, FTransform& OutTransform)
{
	if (!Instances || !Instances->GetInstanceTransform(Index, OutTransform, true))
	{
		return false;
	}
	FKey Key;
	Key.Instances = const_cast<UInstancedStaticMeshComponent*>(Instances);
	Key.Index = Index;
	if (const FTransform* Was = GetAllHeld().Find(Key))
	{
		// (still where the view cut left it: not moved by the town since)
		if (IsAway(OutTransform, *Was))
		{
			OutTransform = *Was;
		}
	}
	return true;
}

void FRRViewCut::Update(AActor* GhostOwner, TConstArrayView<FVector> Eyes, TConstArrayView<FVector> Seen, const FParams& Params)
{
	UWorld* World = GhostOwner ? GhostOwner->GetWorld() : nullptr;
	if (!World || Eyes.Num() == 0 || Seen.Num() == 0)
	{
		Restore();
		return;
	}
	bool bChanged = false;
	// What the town took over meanwhile (smashed or mended: no longer where it was left) is let go
	for (auto It = Held.CreateIterator(); It; ++It)
	{
		UInstancedStaticMeshComponent* Instances = It.Key().Instances.Get();
		FTransform Now;
		if (!Instances || !Instances->GetInstanceTransform(It.Key().Index, Now, true) || !IsAway(Now, It.Value()))
		{
			GetAllHeld().Remove(It.Key());
			It.RemoveCurrent();
			bChanged = true;
		}
	}

	FBox Corridor(ForceInit);
	for (const FVector& Spot : Seen)
	{
		Corridor += Spot;
	}
	for (const FVector& Eye : Eyes)
	{
		Corridor += Eye;
	}
	Corridor = Corridor.ExpandBy(Params.Radius);
	auto IsInTheWay = [&](const FBox& Box)
	{
		if (Box.Max.Z < Params.MinTop || (Box.Min.Z <= Params.MinBottom && Box.Max.Z <= Params.TallTop) || !Box.Intersect(Corridor))
		{
			return false;
		}
		const FBox Wide = Box.ExpandBy(Params.Radius);
		for (const FVector& Eye : Eyes)
		{
			for (const FVector& Spot : Seen)
			{
				const FVector End = Spot + (Eye - Spot).GetSafeNormal() * Params.EndGap;
				if (FMath::LineBoxIntersection(Wide, Eye, End, End - Eye))
				{
					return true;
				}
			}
		}
		return false;
	};

	TSet<FKey> Wanted;
	// (Dice Town's blocks and models, and the top view's island with its mushroom houses, giant mushrooms and trees)
	for (TActorIterator<AActor> Map(World); Map; ++Map)
	{
		if (!Map->IsA<ARRRandomDiceTown>() && !Map->IsA<ARRRandomIsland>())
		{
			continue;
		}
		TInlineComponentArray<UInstancedStaticMeshComponent*> All(*Map);
		for (UInstancedStaticMeshComponent* Instances : All)
		{
			const UStaticMesh* Mesh = Instances->GetStaticMesh();
			// (the hidden collision boxes aren't drawn; a set none of whose instances is near is skipped whole. A set with
			// pieces put away has bounds reaching far down, which still cross the corridor)
			if (!Mesh || !Instances->IsVisible() || !Instances->Bounds.GetBox().Intersect(Corridor))
			{
				continue;
			}
			const FBox MeshBox = Mesh->GetBoundingBox();
			FKey Key;
			Key.Instances = Instances;
			for (int32 Index = 0; Index < Instances->GetInstanceCount(); ++Index)
			{
				Key.Index = Index;
				FTransform Where;
				if (const FTransform* Was = Held.Find(Key))
				{
					Where = *Was;
				}
				else
				{
					Instances->GetInstanceTransform(Index, Where, true);
					// Not what a wreck took (shrunk to nothing, or stowed far below), nor what another view cut holds
					if (Where.GetScale3D().GetAbsMax() < 0.01f || Where.GetLocation().Z < Corridor.Min.Z - 50000.f)
					{
						continue;
					}
				}
				if (IsInTheWay(MeshBox.TransformBy(Where)))
				{
					Wanted.Add(Key);
					if (!Held.Contains(Key))
					{
						Held.Add(Key, Where);
						GetAllHeld().Add(Key, Where);
						Instances->UpdateInstanceTransform(Index, Away(Where), true, false, true);
						bChanged = true;
					}
				}
			}
		}
	}
	// No longer in the way: back where it stands
	for (auto It = Held.CreateIterator(); It; ++It)
	{
		if (!Wanted.Contains(It.Key()))
		{
			if (UInstancedStaticMeshComponent* Instances = It.Key().Instances.Get())
			{
				Instances->UpdateInstanceTransform(It.Key().Index, It.Value(), true, false, true);
			}
			GetAllHeld().Remove(It.Key());
			It.RemoveCurrent();
			bChanged = true;
		}
	}
	if (bChanged)
	{
		RebuildGhosts(GhostOwner, Params.GhostOpacity);
	}
}

void FRRViewCut::RebuildGhosts(AActor* GhostOwner, float Opacity)
{
	if (!GhostMaterial.IsValid())
	{
		if (UMaterialInterface* Ghost = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Effects/DiceFX/M_FXGhost.M_FXGhost")))
		{
			GhostMaterial.Reset(UMaterialInstanceDynamic::Create(Ghost, GetTransientPackage()));
			GhostMaterial->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.75f, 0.85f, 1.f));
			GhostMaterial->SetScalarParameterValue(TEXT("Glow"), 0.25f);
		}
	}
	if (!GhostMaterial.IsValid())
	{
		return;
	}
	GhostMaterial->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	for (const TPair<TWeakObjectPtr<UStaticMesh>, TWeakObjectPtr<UInstancedStaticMeshComponent>>& Ghost : Ghosts)
	{
		if (Ghost.Value.IsValid())
		{
			Ghost.Value->ClearInstances();
		}
	}
	for (const TPair<FKey, FTransform>& Piece : Held)
	{
		UInstancedStaticMeshComponent* Instances = Piece.Key.Instances.Get();
		UStaticMesh* Mesh = Instances ? Instances->GetStaticMesh().Get() : nullptr;
		if (!Mesh)
		{
			continue;
		}
		TWeakObjectPtr<UInstancedStaticMeshComponent>& GhostSet = Ghosts.FindOrAdd(Mesh);
		if (!GhostSet.IsValid())
		{
			UInstancedStaticMeshComponent* Made = NewObject<UInstancedStaticMeshComponent>(GhostOwner);
			Made->SetStaticMesh(Mesh);
			Made->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Made->SetCastShadow(false);
			Made->bAffectDistanceFieldLighting = false;
			Made->bAffectDynamicIndirectLighting = false;
			Made->SetUsingAbsoluteLocation(true);
			Made->SetUsingAbsoluteRotation(true);
			Made->SetUsingAbsoluteScale(true);
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				Made->SetMaterial(Slot, GhostMaterial.Get());
			}
			Made->SetupAttachment(GhostOwner->GetRootComponent());
			Made->RegisterComponent();
			GhostSet = Made;
		}
		GhostSet->AddInstance(Piece.Value, true);
	}
}

void FRRViewCut::Restore()
{
	for (const TPair<FKey, FTransform>& Piece : Held)
	{
		UInstancedStaticMeshComponent* Instances = Piece.Key.Instances.Get();
		FTransform Now;
		// (only if it is still where it was put: the town may have taken it since)
		if (Instances && Instances->GetInstanceTransform(Piece.Key.Index, Now, true) && IsAway(Now, Piece.Value))
		{
			Instances->UpdateInstanceTransform(Piece.Key.Index, Piece.Value, true, false, true);
		}
		GetAllHeld().Remove(Piece.Key);
	}
	Held.Reset();
	for (const TPair<TWeakObjectPtr<UStaticMesh>, TWeakObjectPtr<UInstancedStaticMeshComponent>>& Ghost : Ghosts)
	{
		if (Ghost.Value.IsValid())
		{
			Ghost.Value->DestroyComponent();
		}
	}
	Ghosts.Reset();
}
