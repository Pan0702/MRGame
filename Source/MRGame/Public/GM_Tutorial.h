// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GM_DemoScene.h"
#include "GM_Tutorial.generated.h"

/**
 * チュートリアル専用の軽量 GameMode。
 * 本編 GM_DemoScene の「敵ループ/壁スポーン/NavMesh生成」などは行わず、
 * MR表示に必要な パススルー（と任意でMRUKオクルージョン）だけを初期化する。
 * 敵の出現は TutorialDirector が担当する。
 */
UCLASS()
class MRGAME_API AGM_Tutorial : public AGM_DemoScene
{
	GENERATED_BODY()

public:
	AGM_Tutorial();
	virtual void BeginPlay() override;
};
