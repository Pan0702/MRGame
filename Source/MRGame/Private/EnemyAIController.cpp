// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"

#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

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

	// 既に経路追従中で、かつプレイヤーが前回の目標位置からあまり動いていなければ、
	// MoveToActor を再発行しない。0.3秒ごとに無条件で再発行すると PathFollowingComponent が
	// 毎回経路をリセットし、敵が動き出してはリセットされ「その場で足踏みして進まない」原因になる。
	const bool bMoving = (GetMoveStatus() == EPathFollowingStatus::Moving);
	const float TargetDriftSq = FVector::DistSquared(TargetPawn->GetActorLocation(), LastChaseGoal);
	if (bMoving && TargetDriftSq < FMath::Square(GoalRefreshDistance))
	{
		// 追従継続中・目標もほぼ動いていない → 経路をいじらず追従に任せる。
		return;
	}
	LastChaseGoal = TargetPawn->GetActorLocation();

	// NavMesh 経路でプレイヤーへ。StopDistance まで来たら停止（攻撃間合い）。
	// bUsePathfinding=true で机/壁を回り込む。部分経路も許可して、完全到達できなくても近づく。
	const EPathFollowingRequestResult::Type MoveResult = MoveToActor(
		TargetPawn,
		StopDistance,
		/*bStopOnOverlap=*/true,
		/*bUsePathfinding=*/true,
		/*bCanStrafe=*/false,
		/*FilterClass=*/nullptr,
		/*bAllowPartialPath=*/true);

	// 診断: 「経路要求の結果」と「敵/プレイヤーが NavMesh 上に投影できるか」をログする。
	// MoveResult: 0=Failed(経路引けず) / 1=AlreadyAtGoal(既に到達圏内) / 2=RequestSuccessful(経路追従開始)。
	// これで「経路が出てない(=プレイヤーがNav外でFailed)」のか「経路は出る(Successful)が机で物理ブロックされて来ない」のかを切り分ける。
	// 0.3秒ごとの洪水を避けるため、状態が前回と変わった時だけ出す。
	{
		bool bPlayerOnNav = false;
		bool bEnemyOnNav = false;
		FVector PlayerProj = FVector::ZeroVector;
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			const FVector ProjExtent(150.0f, 150.0f, 300.0f);
			FNavLocation Loc;
			if (NavSys->ProjectPointToNavigation(TargetPawn->GetActorLocation(), Loc, ProjExtent))
			{
				bPlayerOnNav = true;
				PlayerProj = Loc.Location;
			}
			FNavLocation EnemyLoc;
			bEnemyOnNav = NavSys->ProjectPointToNavigation(GetPawn()->GetActorLocation(), EnemyLoc, ProjExtent);
		}

		const int32 DiagKey = (int32)MoveResult * 10 + (bPlayerOnNav ? 2 : 0) + (bEnemyOnNav ? 1 : 0);
		if (DiagKey != LastChaseDiagKey)
		{
			LastChaseDiagKey = DiagKey;
			UE_LOG(LogTemp, Warning,
				TEXT("EnemyChase: MoveResult=%d playerOnNav=%d enemyOnNav=%d enemyLoc=%s playerLoc=%s playerProj=%s dist=%.0f"),
				(int32)MoveResult,
				bPlayerOnNav ? 1 : 0,
				bEnemyOnNav ? 1 : 0,
				*GetPawn()->GetActorLocation().ToCompactString(),
				*TargetPawn->GetActorLocation().ToCompactString(),
				bPlayerOnNav ? *PlayerProj.ToCompactString() : TEXT("(none)"),
				FVector::Dist(GetPawn()->GetActorLocation(), TargetPawn->GetActorLocation()));
		}
	}
}
