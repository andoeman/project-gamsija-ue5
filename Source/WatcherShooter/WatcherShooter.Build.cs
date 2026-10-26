using UnrealBuildTool;

public class WatcherShooter : ModuleRules
{
    public WatcherShooter(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // SightTarget's public header exposes AIModule types to consumers.
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "AIModule"
        });
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "InputCore", "NavigationSystem", "MotionWarping", "EnhancedInput", "Niagara"
        });
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[] { "AnimGraph", "BlueprintGraph" });
        }
    }
}
