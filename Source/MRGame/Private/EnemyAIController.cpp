// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"

#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AEnemyAIController::AEnemyAIController()
{
	// 移動は MoveToActor + PathFollowingComponent（NavMesh経路追従）に任せるので毎Tickは不要。
	PrimaryActorTick.bCanEverTick = false;
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	TargetPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	// 一定間隔でプレイヤーへの NavMesh 経路を引き直す。これにより机/壁を回り込んで近づく
	//（AddMovementInput の直進と違い、隙間や障害物をパスファインディングで避ける）。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ChaseTimerHandle, this, &AEnemyAIController::UpdateChase,
			FMath::Max(0.05f, ChaseUpdateInterval), /*bLoop=*/true);
	}

	// 初回は即実行して動き出しの遅延を無くす。
	UpdateChase();
}

void AEnemyAIController::OnUnPossess()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChaseTimerHandle);
	}
	StopMovement();

	Super::OnUnPossess();
}

void AEnemyAIController::UpdateChase()
{
	if (!IsValid(TargetPawn))
	{
		// プレイヤーが取得できていない場合は再取得を試みる。
		TargetPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!IsValid(TargetPawn))
		{
			return;
		}
	}

	if (!GetPawn())
	{
		return;
	}

	// NavMesh 経路でプレイヤーへ。StopDistance まで来たら停止（攻撃間合い）。
	// bUsePathfinding=true で机/壁を回り込む。部分経路も許可して、完全到達できなくても近づく。
	MoveToActor(
		TargetPawn,
		StopDistance,
		/*bStopOnOverlap=*/true,
		/*bUsePathfinding=*/true,
		/*bCanStrafe=*/false,
		/*FilterClass=*/nullptr,
		/*bAllowPartialPath=*/true);
}
