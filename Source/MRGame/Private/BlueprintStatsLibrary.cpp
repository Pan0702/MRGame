// Fill out your copyright notice in the Description page of Project Settings.

#include "BlueprintStatsLibrary.h"

#if WITH_EDITOR
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#endif

int32 UBlueprintStatsLibrary::CountAllBlueprintNodes()
{
#if WITH_EDITOR
	FAssetRegistryModule& ARM =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> Assets;
	// 第3引数 true で AnimBP / WidgetBP などのサブクラスも含める。
	ARM.Get().GetAssetsByClass(
		UBlueprint::StaticClass()->GetClassPathName(), Assets, /*bSearchSubClasses=*/true);

	int32 Total = 0;
	int32 LoadedBP = 0;
	for (const FAssetData& Data : Assets)
	{
		UBlueprint* BP = Cast<UBlueprint>(Data.GetAsset());
		if (!BP)
		{
			continue;
		}
		++LoadedBP;

		TArray<UEdGraph*> Graphs;
		BP->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			if (Graph)
			{
				Total += Graph->Nodes.Num();
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("Total BP nodes: %d (BP assets found: %d, loaded: %d)"),
		Total, Assets.Num(), LoadedBP);
	return Total;
#else
	UE_LOG(LogTemp, Warning, TEXT("CountAllBlueprintNodes is editor-only; returning 0 in non-editor build."));
	return 0;
#endif
}
