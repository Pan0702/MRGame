// Fill out your copyright notice in the Description page of Project Settings.


#include "GI_MR.h"

#include "MotionControllerComponent.h"

int32 UGI_MR::GetScore() const
{
	return Score;
}

void UGI_MR::SetScore(int32 NewScore)
{
	Score = NewScore;
}

void UGI_MR::AddScore(int32 NewAddScore)
{
	Score += NewAddScore;
}

void UGI_MR::SetHand(UMotionControllerComponent* NewHand)
{
	if (!NewHand)
	{
		return;
	}

	// MotionSourceはPawnによって "Left"/"LeftGrip"/"LeftAim" 等と揺れるため、
	// 部分一致で "Left"/"Right" に正規化して保存する
	const FString Src = NewHand->MotionSource.ToString();
	if (Src.Contains(TEXT("Left")))
	{
		HandSource = FName("Left");
	}
	else if (Src.Contains(TEXT("Right")))
	{
		HandSource = FName("Right");
	}

	UE_LOG(LogTemp, Log, TEXT("GI_MR: SetHand %s (MotionSource=%s)"), *HandSource.ToString(), *Src);
}

FName UGI_MR::GetHandSource() const
{
	return HandSource;
}
