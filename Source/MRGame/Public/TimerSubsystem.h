// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/Subsystem.h"
#include "TimerSubsystem.generated.h"
// UI更新用：残り秒数が変わるたびに通知
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeChanged, int32, RemainingSeconds);

// 時間切れ通知
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimeUp);

/**
 * 
 */
UCLASS()
class MRGAME_API UTimerSubsystem : public USubsystem
{
	GENERATED_BODY()
	// 制限時間を指定して開始（秒）
	UFUNCTION(BlueprintCallable, Category="GameTimer")
	void StartTimer(int32 InDurationSeconds);

	UFUNCTION(BlueprintCallable, Category="GameTimer")
	void PauseTimer();
	UFUNCTION(BlueprintCallable, Category="GameTimer")
	void Tick1Second();
	UFUNCTION(BlueprintCallable, Category="GameTimer")
	void ResumeTimer();

	UFUNCTION(BlueprintCallable, Category="GameTimer")
	void StopTimer();

	UFUNCTION(BlueprintPure, Category="GameTimer")
	int32 GetRemainingSeconds() const { return RemainingSeconds; }

	// 外部（GameMode/UI）が購読する
	UPROPERTY(BlueprintAssignable, Category="GameTimer")
	FOnTimeChanged OnTimeChanged;

	UPROPERTY(BlueprintAssignable, Category="GameTimer")
	FOnTimeUp OnTimeUp;

private:
	FTimerHandle CountHandle;
	int32 RemainingSeconds = 0;
};
