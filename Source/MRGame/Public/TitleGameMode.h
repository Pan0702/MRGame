// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TitleGameMode.generated.h"

/**
 * タイトルレベル用のGameMode。
 * 特別なロジックは持たず、レベルのGameMode Override に設定できる状態にしておくだけ。
 * DefaultPawn / PlayerController などは BP 側または Project Settings で差し替える想定。
 */
UCLASS()
class MRGAME_API ATitleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATitleGameMode();
};
