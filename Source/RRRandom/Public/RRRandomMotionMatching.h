#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/TrajectoryTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "RRRandomMotionMatching.generated.h"

class UAnimSequence;
class USkeletalMesh;
class UPoseSearchDatabase;

/**
 * Motion matching test (2026-11-20). An animal friend with a motion matching set (/Game/Characters/<Name>MM: a copy of
 * its mesh with a root bone, clips retargeted from Unreal's Game Animation Sample, a Pose Search database and the
 * animation blueprint ABP_<Name>MM) gets a hidden "driver" body that runs Unreal's Motion Matching node. The body the
 * player sees (URRRandomBodyAnimInstance) takes the driver's legs, hips, spine and head as its base pose and keeps its
 * own arms (they hold the gun) and every layer it already had on top. On by default since 2026-11-20 (the user: "게임속 여우에도 적용해줘"); rr.MotionMatch 0 or -RRNoMotionMatch turns it off.
 */
namespace RRMotionMatch
{
	/**
	 * The animal friends run much faster for their leg length than the people the clips were captured from: a fox walks
	 * 300 units a second on legs a fifth as long. So the clips play this much faster, and the trajectory the search sees
	 * is slowed down by the same amount (and by the mesh's scale), so a walking fox matches the sample's run.
	 */
	constexpr float PlayRate = 1.5f;
}

/**
 * The driver's animation instance: every frame, where the animal friend has been and where it is about to go
 * (Unreal's character trajectory prediction), brought down to the clips' scale; ABP_<Name>MM feeds it to the pose
 * history node. Root motion is taken out of the pose and dropped: the capsule does the moving.
 */
UCLASS(Blueprintable, Transient)
class RRRANDOM_API URRRandomMotionMatchInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	URRRandomMotionMatchInstance();

	/** The trajectory for the search, in the clips' units and speed. */
	UPROPERTY(BlueprintReadOnly, Category = "Motion Matching")
	FTransformTrajectory Trajectory;

	/** World distances times this are the clips' distances: 1 / (mesh scale x RRMotionMatch::PlayRate). Set by the character. */
	float DistanceScale = 1.f;

protected:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	UPROPERTY(Transient)
	FPoseSearchTrajectoryData TrajectoryData;
	/** The same trajectory in world units, kept from frame to frame (its history). */
	UPROPERTY(Transient)
	FTransformTrajectory WorldTrajectory;
	float DesiredControllerYaw = 0.f;
};

/** Editor tool, called from Python (Tools/char-pipeline/scripts/mm_rr_build_set.py). */
UCLASS()
class RRRANDOM_API URRRandomMotionMatchLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Makes, in Folder, the Pose Search schema PSS_<Name> (trajectory + the two feet), the database PSD_<Name> holding
	 * Clips, and the animation blueprint ABP_<Name> (Motion Matching -> Pose History -> output, parent
	 * URRRandomMotionMatchInstance) for Mesh's skeleton. Assets of those names must not exist yet. Returns what it did, or
	 * a line starting with "ERROR".
	 */
	UFUNCTION(BlueprintCallable, Category = "RRRandom|Motion Matching")
	static FString BuildSet(USkeletalMesh* Mesh, const TArray<UAnimSequence*>& Clips, const FString& Folder, const FString& Name);
	/** Extend a saved set without replacing its schema, blueprint, or existing entries. */
	UFUNCTION(BlueprintCallable, Category = "RRRandom|Motion Matching")
	static int32 AddClips(UPoseSearchDatabase* Database, const TArray<UAnimSequence*>& Clips);
};
