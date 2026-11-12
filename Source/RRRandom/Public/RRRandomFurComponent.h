#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "RRRandomFurComponent.generated.h"

/**
 * Round 95: shell fur over an animal friend's body (SK_<Name>Fur, char-pipeline/scripts/blender_fur_shells_skel.py and
 * ue_import_fur_shells.py), in the way of "Sky: Children of the Light"'s fur (GDC 2025): the material blows it about
 * with a scrolling wind noise, and this pushes it with the body's motion — air resistance against its velocity, inertia
 * against its acceleration, through a spring that overshoots, so the fur lies back on a run, swings on a sudden stop,
 * curls up while falling and bounces on landing. The push goes to the material as custom primitive data 0-2 ("FurPush",
 * world space, in fur lengths); the material bends each shell by it times the square of its height in the fur.
 *
 * Follows the body's pose (leader pose) and shows only while the body does and wears its own material (not while
 * invisible or in another effect's material). Cosmetic: every machine runs its own, none on a dedicated server.
 */
UCLASS()
class URRRandomFurComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	URRRandomFurComponent();

	/** The fur for this body (null: none), and the body material it shows with (its tinted copies count). */
	void SetFur(USkeletalMesh* FurMesh, USkeletalMeshComponent* Body, UMaterialInterface* BodyMaterial);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** How far the fur leans per body height per second of speed, against the motion (in fur lengths). */
	UPROPERTY(EditAnywhere, Category = "Fur")
	float Drag = 0.35f;

	/** How far it leans per body height per second squared of acceleration, against it. */
	UPROPERTY(EditAnywhere, Category = "Fur")
	float Inertia = 0.06f;

	/** The spring's natural frequency (Hz) and damping ratio: below 1 it overshoots and jiggles. */
	UPROPERTY(EditAnywhere, Category = "Fur")
	float SpringHz = 2.6f;

	UPROPERTY(EditAnywhere, Category = "Fur")
	float SpringDamping = 0.22f;

	/** The furthest it bends, in fur lengths (the material caps the sum with the wind at the same). */
	UPROPERTY(EditAnywhere, Category = "Fur")
	float MaxBend = 1.3f;

	/** The body's height (cm) the speeds are measured in. */
	UPROPERTY(EditAnywhere, Category = "Fur")
	float BodyHeight = 180.f;

private:
	bool ShouldShow() const;

	TWeakObjectPtr<USkeletalMeshComponent> BodyMesh;
	TWeakObjectPtr<UMaterialInterface> BodyBase;
	FVector LastVelocity = FVector::ZeroVector;
	FVector Bend = FVector::ZeroVector;
	FVector BendSpeed = FVector::ZeroVector;
	bool bHasLast = false;
};
