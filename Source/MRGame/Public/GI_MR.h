// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GI_MR.generated.h"

class UMotionControllerComponent;

/**
 * 
 */
UCLASS()
class MRGAME_API UGI_MR : public UGameInstance
{
	GENERATED_BODY()

public:
	//�|�����G�̐��̂��낢��//
	UFUNCTION(BlueprintCallable, Category = "Game")
	int32 GetScore() const;
	UFUNCTION(BlueprintCallable, Category = "Game")
	void SetScore(int32 NewScore);
	UFUNCTION(BlueprintCallable, Category = "Game")
	void AddScore(int32 NewAddScore);
	
	// 剣を持つ手。コンポーネントのポインタはレベル遷移でPawnごと破棄され無効になるため、
	// MotionSourceから正規化した "Left"/"Right" のFNameだけを保持する
	UFUNCTION(BlueprintCallable, Category = "Game")
	void SetHand(UMotionControllerComponent* NewHand);

	// 剣を持つ手("Left"/"Right")。未設定ならNAME_None
	UFUNCTION(BlueprintPure, Category = "Game")
	FName GetHandSource() const;
private:
	UPROPERTY(EditAnywhere, Category = "Game")
	int32 Score;

	// "Left" / "Right" / NAME_None(未設定)
	UPROPERTY(VisibleAnywhere, Category = "Game")
	FName HandSource;
};
