// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"

#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

AEnemyAIController::AEnemyAIController()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	
	TargetPawn = UGameplayStatics::GetPlayerPawn(this ,0);
}

void AEnemyAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsValid(TargetPawn))
	{
		//Playerが取得できていない場合再度取得を試みる
		TargetPawn = UGameplayStatics::GetPlayerPawn(this ,0);
	}

	APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn) || !IsValid(TargetPawn))
	{
		return;
	}

	FVector ToTarget = TargetPawn->GetActorLocation() - ControlledPawn->GetActorLocation();
	ToTarget.Z = 0.0f;

	const float Distance = ToTarget.Size();
	if (Distance <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector MoveDirection = ToTarget / Distance;
	const FRotator DesiredRotation = MoveDirection.Rotation();
	const FRotator NewRotation = FMath::RInterpTo(
		ControlledPawn->GetActorRotation(),
		DesiredRotation,
		DeltaTime,
		RotationInterpSpeed);
	ControlledPawn->SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));

	if (Distance > StopDistance)
	{
		ControlledPawn->AddMovementInput(MoveDirection);
	}
}
