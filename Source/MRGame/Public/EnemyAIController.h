// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "EnemyAIController.generated.h"

/**
 * 
 */
UCLASS()
class MRGAME_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController();

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, Category = "AI|Steering")
	float StopDistance = 120.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Steering")
	float RotationInterpSpeed = 8.0f;

	UPROPERTY()
	TObjectPtr<APawn> TargetPawn;
};
