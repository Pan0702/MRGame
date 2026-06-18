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

	// プレイヤーがこの距離(cm)以上動いたら経路を引き直す。これ未満なら追従中は再発行しない
	//（毎回 MoveToActor すると経路リセットで足踏みするのを防ぐ）。
	UPROPERTY(EditAnywhere, Category = "AI|Steering")
	float GoalRefreshDistance = 100.0f;

	UPROPERTY()
	TObjectPtr<APawn> TargetPawn;

private:
	// 追跡の定期更新タイマー。
	FTimerHandle ChaseTimerHandle;

	// 前回 MoveToActor を発行した時のプレイヤー位置。これからの移動量で再発行要否を判定する。
	FVector LastChaseGoal = FVector(FLT_MAX);

	// 診断用: 前回ログした「MoveToActor結果＋プレイヤーがNav上か」の状態。
	// 0.3秒ごとのログ洪水を避けるため、状態が前回と変わった時だけログする。
	// 値: -1=未ログ, それ以外は (MoveResult*10 + (playerOnNav?1:0)) のような複合キー。
	int32 LastChaseDiagKey = -1;
};
