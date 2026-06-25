// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/Subsystem.h"
#include "TimerSubsystem.generated.h"

// 演出フェーズ。表示の切り替えはBP側がこの値を見て行う。
UENUM(BlueprintType)
enum class EGameTimerPhase : uint8
{
	None     UMETA(DisplayName="None"),     // 何もしていない
	Ready    UMETA(DisplayName="Ready"),    // 「Ready」表示中
	Go       UMETA(DisplayName="Go"),       // 「Go」表示中
	Playing  UMETA(DisplayName="Playing"),  // カウントダウン進行中
	Finished UMETA(DisplayName="Finished")  // 「Finish」表示中
};

// 終了直前カウントダウン表示用。WBPはReady/Go/Finishと同じようにSwitchで画像を切り替える。
UENUM(BlueprintType)
enum class EFinalCountdownPhase : uint8
{
	None UMETA(DisplayName="None"),
	Sec5 UMETA(DisplayName="Sec5"),
	Sec4 UMETA(DisplayName="Sec4"),
	Sec3 UMETA(DisplayName="Sec3"),
	Sec2 UMETA(DisplayName="Sec2"),
	Sec1 UMETA(DisplayName="Sec1")
};

// UI更新用：残り秒数が変わるたびに通知
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTimeChanged, int32, RemainingSeconds);

// 終了直前カウントダウン用：残り5〜1秒になった瞬間だけ通知
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFinalCountdownTick, int32, RemainingSeconds);

// 終了直前カウントダウン用：残り5〜1秒をenum化して通知
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFinalCountdownPhaseChanged, EFinalCountdownPhase, Phase);

// 時間切れ通知
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTimeUp);

// 演出フェーズが変わるたびに通知（BPはこれを購読して画像を切り替える）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged, EGameTimerPhase, Phase);

// Finish表示後、Resultへ遷移してよいタイミングを通知（BPはここでOpenLevel）
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSequenceFinished);

/**
 * 
 */
UCLASS()
class MRGAME_API UTimerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// === 演出シーケンス（Ready→Go→Playing→Finish）。これ1つで全フェーズをC++が進める ===
	// InDurationSeconds : 本編の制限時間（秒）
	// ReadySeconds      : 「Ready」を表示する時間（秒）
	// GoSeconds         : 「Go」を表示してからカウント開始までの時間（秒）
	// FinishSeconds     : 時間切れ後「Finish」を表示してからResult遷移を促すまでの時間（秒）
	UFUNCTION(BlueprintCallable, Category="GameTimer")
	void StartCountdownSequence(int32 InDurationSeconds, float ReadySeconds = 1.5f, float GoSeconds = 1.0f, float FinishSeconds = 2.0f);

	// 現在の演出フェーズ（BPバインド用）
	UFUNCTION(BlueprintPure, Category="GameTimer")
	EGameTimerPhase GetCurrentPhase() const { return CurrentPhase; }

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

	// 残り5〜1秒の大きなカウントダウン表示用。WBPはここを購読して画像を切り替える。
	UPROPERTY(BlueprintAssignable, Category="GameTimer")
	FOnFinalCountdownTick OnFinalCountdownTick;

	// 残り5〜1秒の大きなカウントダウン表示用。ApplyPhaseと同じようにenum Switchで扱える。
	UPROPERTY(BlueprintAssignable, Category="GameTimer")
	FOnFinalCountdownPhaseChanged OnFinalCountdownPhaseChanged;

	UPROPERTY(BlueprintAssignable, Category="GameTimer")
	FOnTimeUp OnTimeUp;

	// 演出フェーズの変化通知（BPで画像切り替え）
	UPROPERTY(BlueprintAssignable, Category="GameTimer")
	FOnPhaseChanged OnPhaseChanged;

	// Finish表示後、Result遷移OKの通知（BPでOpenLevel）
	UPROPERTY(BlueprintAssignable, Category="GameTimer")
	FOnSequenceFinished OnSequenceFinished;

private:
	EFinalCountdownPhase GetFinalCountdownPhase(int32 Seconds) const;

	// 残り秒数変更を通知し、必要なら終了直前カウントダウンも通知する。
	void BroadcastTimeChanged();

	// フェーズを変更して通知するヘルパー
	void SetPhase(EGameTimerPhase NewPhase);

	// シーケンス内部遷移
	void BeginGoPhase();    // Ready → Go
	void BeginPlayPhase();  // Go    → Playing（StartTimer）
	void BeginFinishPhase();// 時間切れ → Finished
	void FinishSequence();  // Finished → Result遷移を促す

	FTimerHandle CountHandle;
	int32 RemainingSeconds = 0;

	// 演出シーケンス用
	FTimerHandle SequenceHandle;
	EGameTimerPhase CurrentPhase = EGameTimerPhase::None;
	int32 PendingDuration = 0;
	float PendingGoSeconds = 1.0f;
	float PendingFinishSeconds = 2.0f;
};
