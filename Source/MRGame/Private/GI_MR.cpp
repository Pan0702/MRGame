// Fill out your copyright notice in the Description page of Project Settings.


#include "GI_MR.h"

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
