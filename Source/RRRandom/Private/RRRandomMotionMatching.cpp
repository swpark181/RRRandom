#include "RRRandomMotionMatching.h"

#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"

#if WITH_EDITOR
#include "AnimGraphNode_MotionMatching.h"
#include "AnimGraphNode_PoseSearchHistoryCollector.h"
#include "AnimGraphNode_Root.h"
#include "Animation/AnimBlueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Factories/AnimBlueprintFactory.h"
#include "FileHelpers.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchFeatureChannel_Pose.h"
#include "PoseSearch/PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/PoseSearchSchema.h"
#endif

URRRandomMotionMatchInstance::URRRandomMotionMatchInstance()
{
	// The clips carry root motion; it is taken out of the pose (the root stays put) and not handed to the movement
	RootMotionMode = ERootMotionMode::IgnoreRootMotion;
}

void URRRandomMotionMatchInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	if (DeltaSeconds <= 0.f || !TryGetPawnOwner())
	{
		return;
	}
	FTransformTrajectory World;
	UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectory(this, TrajectoryData, DeltaSeconds, WorldTrajectory, DesiredControllerYaw, World);
	if (World.Samples.IsEmpty())
	{
		return;
	}
	// Shrunk about where the animal friend is now (the sample nearest time zero), so speeds come down and facings stay
	int32 Now = 0;
	for (int32 Index = 1; Index < World.Samples.Num(); ++Index)
	{
		if (FMath::Abs(World.Samples[Index].TimeInSeconds) < FMath::Abs(World.Samples[Now].TimeInSeconds))
		{
			Now = Index;
		}
	}
	const FVector Origin = World.Samples[Now].Position;
	for (FTransformTrajectorySample& Sample : World.Samples)
	{
		Sample.Position = Origin + (Sample.Position - Origin) * DistanceScale;
	}
	Trajectory = MoveTemp(World);
}

#if WITH_EDITOR
namespace
{
	template <typename AssetType>
	AssetType* NewAsset(const FString& Folder, const FString& Name)
	{
		UPackage* Package = CreatePackage(*(Folder / Name));
		AssetType* Asset = NewObject<AssetType>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Asset);
		Package->MarkPackageDirty();
		return Asset;
	}

	bool SetNodeValue(UScriptStruct* Struct, void* Node, const TCHAR* Property, const FString& Text)
	{
		const FProperty* Found = Struct->FindPropertyByName(Property);
		return Found && Found->ImportText_InContainer(*Text, Node, nullptr, PPF_None) != nullptr;
	}

	/** A graph node keeps its runtime node in a private "Node" struct property. */
	void* RuntimeNode(UEdGraphNode* GraphNode)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(GraphNode->GetClass(), TEXT("Node"));
		return Property ? Property->ContainerPtrToValuePtr<void>(GraphNode) : nullptr;
	}

	UEdGraphPin* PosePin(UEdGraphNode* Node, EEdGraphPinDirection Direction, const TCHAR* Name = nullptr)
	{
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction == Direction && (Name ? Pin->PinName == Name : Direction == EGPD_Output))
			{
				return Pin;
			}
		}
		return nullptr;
	}
}
#endif

int32 URRRandomMotionMatchLibrary::AddClips(UPoseSearchDatabase* Database, const TArray<UAnimSequence*>& Clips)
{
#if WITH_EDITOR
	if (!Database || !Database->Schema || Clips.IsEmpty()) { return INDEX_NONE; }
	const UAnimationAsset* First = Cast<UAnimationAsset>(Database->GetNumAnimationAssets() ? Database->GetAnimationAsset(0) : nullptr);
	for (const UAnimSequence* Clip : Clips)
	{
		if (!Clip || !First || Clip->GetSkeleton() != First->GetSkeleton()) { return INDEX_NONE; }
	}
	TSet<UObject*> Existing;
	for (int32 I = 0; I < Database->GetNumAnimationAssets(); ++I) { Existing.Add(Database->GetAnimationAsset(I)); }
	for (UAnimSequence* Clip : Clips)
	{
		if (Existing.Contains(Clip)) { continue; }
		Database->Modify();
		FPoseSearchDatabaseAnimationAsset Entry;
		Entry.AnimAsset = Clip;
		Database->AddAnimationAsset(Entry);
		Existing.Add(Clip);
	}
	Database->PostEditChange();
	return UEditorLoadingAndSavingUtils::SavePackages({Database->GetPackage()}, false) ? Database->GetNumAnimationAssets() : INDEX_NONE;
#else
	return INDEX_NONE;
#endif
}

FString URRRandomMotionMatchLibrary::BuildSet(USkeletalMesh* Mesh, const TArray<UAnimSequence*>& Clips, const FString& Folder, const FString& Name)
{
#if WITH_EDITOR
	USkeleton* Skeleton = Mesh ? Mesh->GetSkeleton() : nullptr;
	if (!Skeleton || Clips.IsEmpty())
	{
		return TEXT("ERROR no skeleton or no clips");
	}

	// The schema: Unreal's default locomotion trajectory samples, and where the two feet are (not how fast they move: the
	// clips play RRMotionMatch::PlayRate times faster than they were made, which the feet's speeds would not match)
	UPoseSearchSchema* Schema = NewAsset<UPoseSearchSchema>(Folder, TEXT("PSS_") + Name);
	Schema->AddSkeleton(Skeleton);
	Schema->AddChannel(NewObject<UPoseSearchFeatureChannel_Trajectory>(Schema, NAME_None, RF_Transactional));
	UPoseSearchFeatureChannel_Pose* Feet = NewObject<UPoseSearchFeatureChannel_Pose>(Schema, NAME_None, RF_Transactional);
	Feet->SampledBones.Reset();
	for (const TCHAR* Bone : { TEXT("LeftFoot"), TEXT("RightFoot") })
	{
		FPoseSearchBone Sampled;
		Sampled.Reference.BoneName = Bone;
		Sampled.Flags = int32(EPoseSearchBoneFlags::Position);
		Feet->SampledBones.Add(Sampled);
	}
	Schema->AddChannel(Feet);
	FPropertyChangedEvent SchemaChanged(nullptr);
	Schema->PostEditChangeProperty(SchemaChanged);

	UPoseSearchDatabase* Database = NewAsset<UPoseSearchDatabase>(Folder, TEXT("PSD_") + Name);
	Database->Schema = Schema;
	int32 Added = 0;
	for (UAnimSequence* Clip : Clips)
	{
		if (Clip && Clip->GetSkeleton() == Skeleton)
		{
			FPoseSearchDatabaseAnimationAsset Entry;
			Entry.AnimAsset = Clip;
			Database->AddAnimationAsset(Entry);
			++Added;
		}
	}
	FPropertyChangedEvent DatabaseChanged(nullptr);
	Database->PostEditChangeProperty(DatabaseChanged);

	// The animation blueprint: Motion Matching -> Pose History (fed the instance's Trajectory) -> output
	const FString BlueprintName = TEXT("ABP_") + Name;
	UAnimBlueprintFactory* Factory = NewObject<UAnimBlueprintFactory>();
	Factory->TargetSkeleton = Skeleton;
	Factory->ParentClass = URRRandomMotionMatchInstance::StaticClass();
	Factory->PreviewSkeletalMesh = Mesh;
	UPackage* BlueprintPackage = CreatePackage(*(Folder / BlueprintName));
	UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(), BlueprintPackage, *BlueprintName,
		RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
	if (!Blueprint)
	{
		return TEXT("ERROR the animation blueprint could not be made");
	}
	FAssetRegistryModule::AssetCreated(Blueprint);
	UEdGraph* Graph = nullptr;
	for (UEdGraph* Candidate : Blueprint->FunctionGraphs)
	{
		if (Candidate && Candidate->GetFName() == UEdGraphSchema_K2::GN_AnimGraph)
		{
			Graph = Candidate;
		}
	}
	TArray<UAnimGraphNode_Root*> Roots;
	if (Graph)
	{
		Graph->GetNodesOfClass(Roots);
	}
	if (!Graph || Roots.IsEmpty())
	{
		return TEXT("ERROR the animation blueprint has no anim graph");
	}

	FGraphNodeCreator<UAnimGraphNode_MotionMatching> MatchCreator(*Graph);
	UAnimGraphNode_MotionMatching* Match = MatchCreator.CreateNode(false);
	Match->NodePosX = -700;
	void* MatchNode = RuntimeNode(Match);
	bool bSet = MatchNode && SetNodeValue(FAnimNode_MotionMatching::StaticStruct(), MatchNode, TEXT("Database"), Database->GetPathName());
	bSet = bSet && SetNodeValue(FAnimNode_MotionMatching::StaticStruct(), MatchNode, TEXT("PlayRateMultiplier"), FString::SanitizeFloat(RRMotionMatch::PlayRate));
	// (the search may also stretch a clip a little to the speed it is asked for)
	bSet = bSet && SetNodeValue(FAnimNode_MotionMatching::StaticStruct(), MatchNode, TEXT("PlayRate"), TEXT("(Min=0.800000,Max=1.250000)"));
	MatchCreator.Finalize();

	FGraphNodeCreator<UAnimGraphNode_PoseSearchHistoryCollector> HistoryCreator(*Graph);
	UAnimGraphNode_PoseSearchHistoryCollector* History = HistoryCreator.CreateNode(false);
	History->NodePosX = -350;
	HistoryCreator.Finalize();

	// (no montage slot here; single
	// clips such as the climbs are played on a second hidden body instead, ARRRandomCharacter::StartClimbMotion)
	FGraphNodeCreator<UK2Node_VariableGet> GetCreator(*Graph);
	UK2Node_VariableGet* Get = GetCreator.CreateNode(false);
	Get->NodePosX = -700;
	Get->NodePosY = 220;
	Get->VariableReference.SetSelfMember(GET_MEMBER_NAME_CHECKED(URRRandomMotionMatchInstance, Trajectory));
	GetCreator.Finalize();

	const UEdGraphSchema* GraphSchema = Graph->GetSchema();
	UEdGraphPin* MatchOut = PosePin(Match, EGPD_Output);
	UEdGraphPin* HistoryIn = PosePin(History, EGPD_Input, TEXT("Source"));
	UEdGraphPin* HistoryOut = PosePin(History, EGPD_Output);
	UEdGraphPin* TrajectoryIn = PosePin(History, EGPD_Input, TEXT("TransformTrajectory"));
	UEdGraphPin* Result = PosePin(Roots[0], EGPD_Input, TEXT("Result"));
	UEdGraphPin* DatabasePin = PosePin(Match, EGPD_Input, TEXT("Database"));
	if (DatabasePin)
	{
		DatabasePin->DefaultObject = Database;
	}
	if (!MatchOut || !HistoryIn || !HistoryOut || !TrajectoryIn || !Result || !Get->GetValuePin())
	{
		return FString::Printf(TEXT("ERROR pins missing: match out %d, history in %d out %d trajectory %d, result %d, variable %d"),
			MatchOut != nullptr, HistoryIn != nullptr, HistoryOut != nullptr, TrajectoryIn != nullptr, Result != nullptr, Get->GetValuePin() != nullptr);
	}
	const bool bLinked = GraphSchema->TryCreateConnection(MatchOut, HistoryIn) && GraphSchema->TryCreateConnection(HistoryOut, Result)
		&& GraphSchema->TryCreateConnection(Get->GetValuePin(), TrajectoryIn);

	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	const bool bCompiled = Blueprint->Status != BS_Error && Blueprint->GeneratedClass != nullptr;

	TArray<UPackage*> Packages = { Schema->GetPackage(), Database->GetPackage(), Blueprint->GetPackage() };
	const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
	return FString::Printf(TEXT("%s schema %s, database %s with %d of %d clips, blueprint %s: values set %d, linked %d, compiled %d (status %d), saved %d"),
		bSet && bLinked && bCompiled && bSaved ? TEXT("OK") : TEXT("ERROR"), *Schema->GetName(), *Database->GetName(), Added, Clips.Num(),
		*Blueprint->GetName(), bSet, bLinked, bCompiled, int32(Blueprint->Status), bSaved);
#else
	return TEXT("ERROR only in the editor");
#endif
}
