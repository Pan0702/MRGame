// Fill out your copyright notice in the Description page of Project Settings.


#include "TimerSubsystem.h"

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
}
