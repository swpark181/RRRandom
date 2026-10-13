#pragma once

#include "CoreMinimal.h"
#include "RRRandomAimAnimations.generated.h"

class UAnimSequence;

/**
 * The fox's aiming animations (Mixamo rifle set, in place; char-pipeline/scripts/ue_import_fox_anim.py), which
 * URRRandomBodyAnimInstance blends while the brawler shoots: the standing aim and fire, and the aimed run in four
 * directions, mixed by which way the brawler moves relative to where it faces.
 */
USTRUCT()
struct FRRAimAnimations
{
	GENERATED_BODY()

	/** Standing, rifle up. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	TObjectPtr<UAnimSequence> Idle;

	/** Standing, firing (restarted on every shot). */
	UPROPERTY(EditAnywhere, Category = "Aim")
	TObjectPtr<UAnimSequence> Fire;

	/** Aimed run forward, backward, left, right. */
	UPROPERTY(EditAnywhere, Category = "Aim")
	TObjectPtr<UAnimSequence> Move[4];

	/** Ground speed of each run at the brawler's size (the planted foot's slide, measured by blender_anim_speed.py). */
	UPROPERTY(EditAnywhere, Category = "Aim")
	float MoveSpeed[4] = { 0.f, 0.f, 0.f, 0.f };

	bool IsComplete() const
	{
		return Idle && Fire && Move[0] && Move[1] && Move[2] && Move[3];
	}
};
