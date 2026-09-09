// Copyright Convai Inc. All Rights Reserved.
using UnrealBuildTool;
public class ConvaiHTTPTransfer : ModuleRules
{
    public ConvaiHTTPTransfer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {"Core", "CoreUObject", "Engine", "Json", "CONVAIHTTP"});
    }
}
