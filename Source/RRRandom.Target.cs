using UnrealBuildTool;
using System.Collections.Generic;

public class RRRandomTarget : TargetRules
{
	public RRRandomTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RRRandom");
	}
}
