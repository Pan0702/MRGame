// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class MRGame : ModuleRules
{
	public MRGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core", "CoreUObject", "Engine", "InputCore" ,"HeadMountedDisplay","XRBase","NavigationSystem","EnhancedInput","AIModule","OculusXRHMD","OculusXRPassthrough","OculusXRAnchors","MRUtilityKit","AndroidPermission","ProceduralMeshComponent","UMG"
        });

		PrivateDependencyModuleNames.AddRange(new string[] {  });

		// エディタビルド限定の依存。BP ノード数カウント(UBlueprintStatsLibrary)が使う
		// UnrealEd/AssetRegistry/BlueprintGraph はエディタモジュールなので、実機(Android)
		// パッケージには含めない（含めるとリンクできずビルドが壊れる）。
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"UnrealEd", "AssetRegistry", "BlueprintGraph"
			});
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
