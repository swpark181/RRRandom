#pragma once

#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"

class AActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMesh;

/**
 * Round 100. Clears a view through Dice Town on this machine only: the drawn pieces of the town (blocks, roofs, trees,
 * giant dice: instances of the town's instanced meshes) that stand on the lines from the camera to what it should see
 * are put away far below and drawn as faint ghosts in their place, and put back when they are no longer in the way.
 * Used by the results camera (the user: "승리 화면에서 시야를 가리는 큰 장애물은 투명화") and by the top view for the animal
 * friend being watched ("탑뷰일 때 내가 보는 캐릭터가 건물에 들어가도 캐릭터가 보이게").
 *
 * Only what is drawn moves: the town's collision is its own hidden boxes, so nobody falls and shots still stop.
 * The town reads instance transforms back when a wreck takes or mends a piece; it asks GetTrueTransform, so a piece that
 * is put away here is still recorded where it really stands. A piece the town moved itself meanwhile (smashed, mended)
 * is the town's again: Update lets go of it.
 */
struct RRRANDOM_API FRRViewCut
{
	struct FParams
	{
		/** How far round each line a piece counts as in the way. */
		float Radius = 70.f;
		/** The lines stop this far short of what is seen (what stands right beside it stays). */
		float EndGap = 0.f;
		/** Only pieces whose top is at least this high. */
		float MinTop = -UE_BIG_NUMBER;
		/** Only pieces whose bottom is over MinBottom, or whose top is over TallTop (something taller than the one seen). */
		float MinBottom = -UE_BIG_NUMBER;
		float TallTop = UE_BIG_NUMBER;
		/** How much of a ghost shows. */
		float GhostOpacity = 0.1f;
	};

	~FRRViewCut() { Restore(); }

	/** One pass: what is in the way now is put away, what no longer is comes back. The ghosts belong to GhostOwner. */
	void Update(AActor* GhostOwner, TConstArrayView<FVector> Eyes, TConstArrayView<FVector> Seen, const FParams& Params);
	/** Everything back (the ghosts gone). */
	void Restore();
	int32 Num() const { return Held.Num(); }

	/** Where an instance really stands: where it was before a view cut put it away, or where it is. */
	static bool GetTrueTransform(const UInstancedStaticMeshComponent* Instances, int32 Index, FTransform& OutTransform);

private:
	struct FKey
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Instances;
		int32 Index = INDEX_NONE;
		bool operator==(const FKey& Other) const { return Instances == Other.Instances && Index == Other.Index; }
		friend uint32 GetTypeHash(const FKey& Key) { return HashCombine(GetTypeHash(Key.Instances), ::GetTypeHash(Key.Index)); }
	};
	/** What is put away, and where each really stands. */
	TMap<FKey, FTransform> Held;
	TMap<TWeakObjectPtr<UStaticMesh>, TWeakObjectPtr<UInstancedStaticMeshComponent>> Ghosts;
	TStrongObjectPtr<UMaterialInstanceDynamic> GhostMaterial;

	void RebuildGhosts(AActor* GhostOwner, float Opacity);
	/** Every view cut's pieces on this machine (GetTrueTransform). */
	static TMap<FKey, FTransform>& GetAllHeld();
};
