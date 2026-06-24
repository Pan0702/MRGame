// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "BlueprintStatsLibrary.generated.h"

/**
 * Blueprint アセットの統計を取るエディタ専用ユーティリティ。
 * AssetRegistry/UBlueprint/UEdGraph はエディタモジュール由来のため、関数本体は WITH_EDITOR 限定。
 * 実機(Android)パッケージにはカウント機能は含まれない（呼ぶと 0 を返す）。
 */
UCLASS()
class MRGAME_API UBlueprintStatsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * プロジェクト内の全 Blueprint（AnimBP/WidgetBP などサブクラス含む）の
	 * 全グラフのノード総数を数えてログ出力し、その合計を返す。
	 * エディタでのみ動作する（CallInEditor でディテールのボタンから実行可）。
	 * @return 全 BP のノード総数（非エディタビルドでは 0）。
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Stats")
	static int32 CountAllBlueprintNodes();
};
