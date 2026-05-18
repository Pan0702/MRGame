// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"

#include "Kismet/GameplayStatics.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	TargetPawn = UGameplayStatics::GetPlayerPawn(this ,0);
	if (IsValid(TargetPawn))
	{
		MoveToActor(TargetPawn,AcceptanceRadius);
	}
}

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsValid(TargetPawn))
	{
		//Playerが取得できていない場合再度取得を試みる
		TargetPawn = UGameplayStatics::GetPlayerPawn(this ,0);
		UE_LOG(LogTemp, Warning, TEXT("TargetPawn is not valid"));
	}
	
	TimeSinceLastRepath += DeltaTime;
	if (TimeSinceLastRepath >= RepathInterval)
	{
		MoveToActor(TargetPawn,AcceptanceRadius);
		TimeSinceLastRepath = 0.0f;
	}
}
