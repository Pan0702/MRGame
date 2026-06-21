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
	//ì|ÇµÇΩìGÇÃêîÇÃÇ¢ÇÎÇ¢ÇÎ//
	UFUNCTION(BlueprintCallable, Category = "Game")
	int32 GetScore() const;
	UFUNCTION(BlueprintCallable, Category = "Game")
	void SetScore(int32 NewScore);
	UFUNCTION(BlueprintCallable, Category = "Game")
	void AddScore(int32 NewAddScore);
	
	//éùÇ¡ÇƒÇÈòrÇ¢ÇÎÇ¢ÇÎ//
	UFUNCTION(BlueprintCallable, Category = "Game")
	void SetHand(UMotionControllerComponent* NewHand);
	UFUNCTION(BlueprintCallable, Category = "Game")
	UMotionControllerComponent* GetHand() const;
private:
	UPROPERTY(EditAnywhere, Category = "Game")
	int32 Score;
	
	UPROPERTY(EditAnywhere, Category = "Game")
	UMotionControllerComponent* Hand;
};
