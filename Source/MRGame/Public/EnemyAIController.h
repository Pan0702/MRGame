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

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float RepathInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "AI")
	float AcceptanceRadius = 100.0f;

	UPROPERTY()
	TObjectPtr<APawn> TargetPawn;

private:
	float TimeSinceLastRepath = 0.0f;
};
