// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GI_MR.generated.h"

/**
 * 
 */
UCLASS()
class MRGAME_API UGI_MR : public UGameInstance
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Game")
	int32 GetScore() const;
	UFUNCTION(BlueprintCallable, Category = "Game")
	void SetScore(int32 NewScore);
	UFUNCTION(BlueprintCallable, Category = "Game")
	void AddScore(int32 NewAddScore);

private:
	UPROPERTY(EditAnywhere, Category = "Game")
	int32 Score;
};
