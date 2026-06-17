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
	virtual void OnUnPossess() override;

	// NavMesh 経路でプレイヤーへ向かう。タイマーで定期的に目標位置を更新する
	//（毎Tickではなく一定間隔で十分。PathFollowingComponent が経路追従する）。
	void UpdateChase();

	// プレイヤーへ近づく際の到達半径(cm)。この距離まで来たら止まる（＝攻撃間合い）。
	UPROPERTY(EditAnywhere, Category = "AI|Steering")
	float StopDistance = 120.0f;

	// 目標位置（プレイヤー）の再探索間隔(秒)。短いほど追従が速いが負荷増。
	UPROPERTY(EditAnywhere, Category = "AI|Steering")
	float ChaseUpdateInterval = 0.3f;

	UPROPERTY()
	TObjectPtr<APawn> TargetPawn;

private:
	// 追跡の定期更新タイマー。
	FTimerHandle ChaseTimerHandle;
};
