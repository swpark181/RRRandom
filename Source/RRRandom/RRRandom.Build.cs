using UnrealBuildTool;

public class RRRandom : ModuleRules
{
	public RRRandom(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Each .cpp keeps its own file-local helpers (ColorParameter, WithAlpha, ...); unity builds would merge and clash them
		bUseUnity = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"GameplayTasks",
			"Niagara",
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});
	}
}
