// Fill out your copyright notice in the Description page of Project Settings.


#include "EnemyAIController.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"

AEnemyAIController::AEnemyAIController()
{
	// 移動は MoveToLocation + PathFollowingComponent（NavMesh経路追従）に任せるので毎Tickは不要。
	PrimaryActorTick.bCanEverTick = false;
}

FVector AEnemyAIController::GetChaseTargetLocation() const
{
	if (!TargetPawn)
	{
		return FVector::ZeroVector;
	}

	if (bUseCameraLocationAsTarget)
	{
		if (const UCameraComponent* Camera = TargetPawn->FindComponentByClass<UCameraComponent>())
		{
			return Camera->GetComponentLocation();
		}
	}

	return TargetPawn->GetActorLocation();
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

	const FVector TargetLocation = GetChaseTargetLocation();

	// 引っかかり(stuck)検出: 経路追従中(Moving)なのに前回からほとんど進めていなければ、
	// 家具/壁のコリジョン角に物理的に引っかかって止まっている可能性が高い。
	// 一定時間続いたら下の再発行ガードを無視して経路を引き直し、別ルートで復帰させる。
	const bool bMoving = (GetMoveStatus() == EPathFollowingStatus::Moving);
	bool bStuck = false;
	float Moved2D = 0.0f;
	{
		const FVector SelfLoc = GetPawn()->GetActorLocation();
		const bool bHasLastSelfLocation = LastSelfLocation.X < FLT_MAX * 0.5f;
		const float MovedSq = bHasLastSelfLocation ? FVector::DistSquared2D(SelfLoc, LastSelfLocation) : TNumericLimits<float>::Max();
		Moved2D = bHasLastSelfLocation ? FMath::Sqrt(MovedSq) : -1.0f;
		if (bMoving && MovedSq < FMath::Square(StuckMoveThreshold))
		{
			StuckAccumTime += FMath::Max(0.05f, ChaseUpdateInterval);
			if (StuckAccumTime >= StuckTimeThreshold)
			{
				bStuck = true;
			}
		}
		else
		{
			StuckAccumTime = 0.0f;
		}
		LastSelfLocation = SelfLoc;
	}

	// 既に経路追従中で、かつプレイヤーが前回の目標位置からあまり動いていなければ、
	// MoveTo を再発行しない。0.3秒ごとに無条件で再発行すると PathFollowingComponent が
	// 毎回経路をリセットし、敵が動き出してはリセットされ「その場で足踏みして進まない」原因になる。
	// ただし stuck 中は引っかかり復帰のため、このガードを無視して必ず引き直す。
	const float TargetDriftSq = FVector::DistSquared2D(TargetLocation, LastChaseGoal);
	if (!bStuck && bMoving && TargetDriftSq < FMath::Square(GoalRefreshDistance))
	{
		// 追従継続中・目標もほぼ動いていない → 経路をいじらず追従に任せる。
		return;
	}
	LastChaseGoal = TargetLocation;
	if (bStuck)
	{
		// 引っかかりからの復帰。プレイヤーへ同じ経路を引き直すだけでは同じ角に戻るので、
		// まず近くの歩ける点へ少しだけ「ずらし移動」して物理的な引っかかりを外す。
		// 次回 UpdateChase で通常のプレイヤー追従に戻る（LastChaseGoal をリセットして必ず再発行）。
		UE_LOG(LogTemp, Warning,
			TEXT("EnemyChaseStuck: pawn=%s controller=%s loc=%s moved2D=%.1f speed2D=%.1f stuckTime=%.2f"),
			*GetNameSafe(GetPawn()),
			*GetNameSafe(this),
			*GetPawn()->GetActorLocation().ToCompactString(),
			Moved2D,
			GetPawn()->GetVelocity().Size2D(),
			StuckAccumTime);

		StopMovement();
		StuckAccumTime = 0.0f;
		LastChaseGoal = FVector(FLT_MAX);

		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation Nudge;
			if (NavSys->GetRandomReachablePointInRadius(GetPawn()->GetActorLocation(), StuckNudgeRadius, Nudge))
			{
				MoveToLocation(Nudge.Location, /*AcceptanceRadius=*/-1.0f,
					/*bStopOnOverlap=*/true, /*bUsePathfinding=*/true,
					/*bProjectDestinationToNavigation=*/false, /*bCanStrafe=*/false);
				return;
			}
		}
		// ずらし先が取れなければ、このまま下の通常経路要求にフォールバックする。
	}

	// NavMesh 経路でプレイヤーへ。StopDistance まで来たら停止（攻撃間合い）。
	// bUsePathfinding=true で机/壁を回り込む。
	// bAllowPartialPath=false: 完全経路が引けない時は部分経路で「途中の空間」まで歩いて
	// その終端で恒久停止するのを防ぐ。完全経路が出ない間は下のリトライで生成/位置変化を待つ。
	FVector ChaseGoal = TargetLocation;
	bool bPlayerOnNav = false;
	bool bEnemyOnNav = false;
	FVector PlayerProj = FVector::ZeroVector;
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Loc;
		if (NavSys->ProjectPointToNavigation(TargetLocation, Loc, TargetNavProjectExtent))
		{
			bPlayerOnNav = true;
			PlayerProj = Loc.Location;
			ChaseGoal = PlayerProj;
		}

		FNavLocation EnemyLoc;
		bEnemyOnNav = NavSys->ProjectPointToNavigation(GetPawn()->GetActorLocation(), EnemyLoc, TargetNavProjectExtent);
	}

	const EPathFollowingRequestResult::Type MoveResult = MoveToLocation(
		ChaseGoal,
		StopDistance,
		/*bStopOnOverlap=*/false,
		/*bUsePathfinding=*/true,
		/*bProjectDestinationToNavigation=*/!bPlayerOnNav,
		/*bCanStrafe=*/false,
		/*FilterClass=*/nullptr,
		/*bAllowPartialPath=*/false);

	// 完全経路が引けなかった(Failed) = この瞬間はプレイヤーまでの NavMesh が繋がっていない
	// （MR の NavMesh は非同期生成で、敵〜プレイヤー間のタイルがまだ無いことがある）。
	// タイマー(ChaseUpdateInterval)で次回また試すので、ここでは何もしない。
	// LastChaseGoal は更新済みなので、プレイヤーが動けば次回必ず再試行される。
	if (MoveResult == EPathFollowingRequestResult::Failed)
	{
		// 次回のガードに引っかからないよう、目標を「未設定」に戻して必ず再試行させる。
		LastChaseGoal = FVector(FLT_MAX);
	}

	// 診断: 「経路要求の結果」と「敵/プレイヤーが NavMesh 上に投影できるか」をログする。
	// MoveResult: 0=Failed(経路引けず) / 1=AlreadyAtGoal(既に到達圏内) / 2=RequestSuccessful(経路追従開始)。
	// これで「経路が出てない(=プレイヤーがNav外でFailed)」のか「経路は出る(Successful)が机で物理ブロックされて来ない」のかを切り分ける。
	// 0.3秒ごとの洪水を避けるため、状態が前回と変わった時だけ出す。
	{
		const EPathFollowingStatus::Type MoveStatus = GetMoveStatus();
		const FVector EnemyLoc = GetPawn()->GetActorLocation();
		const FVector EnemyVelocity = GetPawn()->GetVelocity();
		const float EnemySpeed2D = EnemyVelocity.Size2D();
		const float DistanceToPlayer = FVector::Dist(EnemyLoc, TargetLocation);
		const int32 DiagKey = (int32)MoveResult * 100 + (int32)MoveStatus * 10 + (bPlayerOnNav ? 2 : 0) + (bEnemyOnNav ? 1 : 0);
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		const bool bIntervalElapsed = bLogChaseDiagnostics &&
			(Now - LastChaseDiagLogTime >= FMath::Max(0.1f, ChaseDiagnosticsInterval));
		if (bLogChaseDiagnostics && (DiagKey != LastChaseDiagKey || bIntervalElapsed || bStuck))
		{
			LastChaseDiagKey = DiagKey;
			LastChaseDiagLogTime = Now;
			UE_LOG(LogTemp, Warning,
				TEXT("EnemyChase: pawn=%s controller=%s MoveResult=%d MoveStatus=%d playerOnNav=%d enemyOnNav=%d stuck=%d stuckTime=%.2f moved2D=%.1f speed2D=%.1f enemyLoc=%s targetLoc=%s pawnLoc=%s playerProj=%s dist=%.0f"),
				*GetNameSafe(GetPawn()),
				*GetNameSafe(this),
				(int32)MoveResult,
				(int32)MoveStatus,
				bPlayerOnNav ? 1 : 0,
				bEnemyOnNav ? 1 : 0,
				bStuck ? 1 : 0,
				StuckAccumTime,
				Moved2D,
				EnemySpeed2D,
				*EnemyLoc.ToCompactString(),
				*TargetLocation.ToCompactString(),
				*TargetPawn->GetActorLocation().ToCompactString(),
				bPlayerOnNav ? *PlayerProj.ToCompactString() : TEXT("(none)"),
				DistanceToPlayer);
		}
	}
}
