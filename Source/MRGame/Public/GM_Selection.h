// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GM_DemoScene.h"
#include "GM_Selection.generated.h"

/**
 * SelectionLevel（ステージ選択画面）専用の軽量 GameMode。
 * 本編 GM_DemoScene の「敵ループ/壁スポーン/NavMesh生成」やオクルージョンは行わず、
 * MR表示に必要な パススルー だけを初期化する。
 * 選択UIや遷移は BP / レベル上のアクター側で持つ想定。
 */
UCLASS()
class MRGAME_API AGM_Selection : public AGM_DemoScene
{
	GENERATED_BODY()

public:
	AGM_Selection();
	virtual void BeginPlay() override;
};
