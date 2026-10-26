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
			// The third person maps' navmesh: the bots' routes (RRRandomAIController)
			"NavigationSystem",
			"Niagara",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			// Whether the game's window is the one in use (third person gives the mouse back otherwise)
			"ApplicationCore",
			// GWhiteTexture for the HUD's canvas triangles
			"RenderCore",
			// The HUD's runtime Nunito font: measured and rasterized by Slate's font services (RRRandomUIStyle)
			"Slate",
			"SlateCore"
		});
	}
}
