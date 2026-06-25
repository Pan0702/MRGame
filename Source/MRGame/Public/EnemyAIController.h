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

	// 経路追従中なのに、この距離(cm)未満しか進んでいない状態が StuckTimeThreshold 秒続いたら
	//「引っかかり(stuck)」とみなして経路を引き直す。家具/壁のコリジョン角に引っかかって
	// その場で止まる（NavMesh上は経路があるのに物理で進めない）状態からの復帰用。
	UPROPERTY(EditAnywhere, Category = "AI|Steering")
	float StuckMoveThreshold = 5.0f;

	// 上記の「ほとんど進んでいない」が継続したら stuck と判定するまでの秒数。
	UPROPERTY(EditAnywhere, Category = "AI|Steering")
	float StuckTimeThreshold = 0.8f;

	// stuck 検出時に、引っかかりを外すため近くの歩ける点へ「ずらし移動」する探索半径(cm)。
	UPROPERTY(EditAnywhere, Category = "AI|Steering")
	float StuckNudgeRadius = 100.0f;

	UPROPERTY(EditAnywhere, Category = "AI|Debug")
	bool bLogChaseDiagnostics = true;

	UPROPERTY(EditAnywhere, Category = "AI|Debug", meta = (ClampMin = "0.1"))
	float ChaseDiagnosticsInterval = 1.0f;

	UPROPERTY()
	TObjectPtr<APawn> TargetPawn;

private:
	// 追跡の定期更新タイマー。
	FTimerHandle ChaseTimerHandle;

	// 前回 MoveToActor を発行した時のプレイヤー位置。これからの移動量で再発行要否を判定する。
	FVector LastChaseGoal = FVector(FLT_MAX);

	// 引っかかり検出用: 前回 UpdateChase 時の自分(敵)の位置と、ほとんど進めていない経過時間。
	FVector LastSelfLocation = FVector(FLT_MAX);
	float StuckAccumTime = 0.0f;

	// 診断用: 前回ログした「MoveToActor結果＋プレイヤーがNav上か」の状態。
	// 0.3秒ごとのログ洪水を避けるため、状態が前回と変わった時だけログする。
	// 値: -1=未ログ, それ以外は (MoveResult*10 + (playerOnNav?1:0)) のような複合キー。
	int32 LastChaseDiagKey = -1;
	float LastChaseDiagLogTime = -FLT_MAX;
};
