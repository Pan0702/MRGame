// Fill out your copyright notice in the Description page of Project Settings.


#include "TimerSubsystem.h"

void UTimerSubsystem::StartCountdownSequence(int32 InDurationSeconds, float ReadySeconds, float GoSeconds, float FinishSeconds)
{
	// 後段フェーズで使う値を保持
	PendingDuration = InDurationSeconds;
	PendingGoSeconds = GoSeconds;
	PendingFinishSeconds = FinishSeconds;

	// 残り秒数を初期表示用にセット（タイマーはまだ動かさない）
	RemainingSeconds = InDurationSeconds;
	OnTimeChanged.Broadcast(RemainingSeconds);

	// Ready フェーズ開始
	SetPhase(EGameTimerPhase::Ready);

	// ReadySeconds 後に Go へ
	GetWorld()->GetTimerManager().SetTimer(
		SequenceHandle, this, &UTimerSubsystem::BeginGoPhase, FMath::Max(0.01f, ReadySeconds), /*bLoop=*/false);
}

void UTimerSubsystem::BeginGoPhase()
{
	SetPhase(EGameTimerPhase::Go);

	// GoSeconds 後に本編（カウント開始）へ
	GetWorld()->GetTimerManager().SetTimer(
		SequenceHandle, this, &UTimerSubsystem::BeginPlayPhase, FMath::Max(0.01f, PendingGoSeconds), /*bLoop=*/false);
}

void UTimerSubsystem::BeginPlayPhase()
{
	SetPhase(EGameTimerPhase::Playing);
	StartTimer(PendingDuration); // ここで初めてカウントダウン開始
}

void UTimerSubsystem::BeginFinishPhase()
{
	SetPhase(EGameTimerPhase::Finished);

	// FinishSeconds 後に Result 遷移を促す
	GetWorld()->GetTimerManager().SetTimer(
		SequenceHandle, this, &UTimerSubsystem::FinishSequence, FMath::Max(0.01f, PendingFinishSeconds), /*bLoop=*/false);
}

void UTimerSubsystem::FinishSequence()
{
	OnSequenceFinished.Broadcast(); // BP側でここから OpenLevel(Result)
}

void UTimerSubsystem::SetPhase(EGameTimerPhase NewPhase)
{
	if (CurrentPhase == NewPhase)
	{
		return;
	}
	CurrentPhase = NewPhase;
	OnPhaseChanged.Broadcast(CurrentPhase);
}

void UTimerSubsystem::StartTimer(int32 InDurationSeconds)
{
	RemainingSeconds = InDurationSeconds;
	OnTimeChanged.Broadcast(RemainingSeconds); // 初期値を即通知

	GetWorld()->GetTimerManager().SetTimer(
		CountHandle, this, &UTimerSubsystem::Tick1Second, 1.0f, /*bLoop=*/true);
}

void UTimerSubsystem::Tick1Second()
{
	RemainingSeconds = FMath::Max(0, RemainingSeconds - 1);
	OnTimeChanged.Broadcast(RemainingSeconds);

	if (RemainingSeconds <= 0)
	{
		GetWorld()->GetTimerManager().ClearTimer(CountHandle);
		OnTimeUp.Broadcast();

		// 演出シーケンス中なら Finish フェーズへ進める
		if (CurrentPhase == EGameTimerPhase::Playing)
		{
			BeginFinishPhase();
		}
	}
}

void UTimerSubsystem::PauseTimer()
{
	GetWorld()->GetTimerManager().PauseTimer(CountHandle);
}

void UTimerSubsystem::ResumeTimer()
{
	GetWorld()->GetTimerManager().UnPauseTimer(CountHandle);
}

void UTimerSubsystem::StopTimer()
{
	GetWorld()->GetTimerManager().ClearTimer(CountHandle);
	GetWorld()->GetTimerManager().ClearTimer(SequenceHandle); // 演出シーケンスも止める
	SetPhase(EGameTimerPhase::None);
}
