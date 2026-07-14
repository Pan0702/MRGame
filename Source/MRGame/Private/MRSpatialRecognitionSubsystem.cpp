// Fill out your copyright notice in the Description page of Project Settings.

#include "MRSpatialRecognitionSubsystem.h"

#include "MRUtilityKit.h"
#include "MRUtilityKitAnchor.h"
#include "MRUtilityKitBPLibrary.h"
#include "MRUtilityKitRoom.h"
#include "MRUtilityKitSubsystem.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "NavigationSystem.h"

#if PLATFORM_ANDROID
#include "AndroidPermissionCallbackProxy.h"
#include "AndroidPermissionFunctionLibrary.h"
#endif

void UMRSpatialRecognitionSubsystem::Deinitialize()
{
	for (TObjectPtr<AActor>& Blocker : FurnitureNavBlockers)
	{
		if (IsValid(Blocker))
		{
			Blocker->Destroy();
		}
	}
	FurnitureNavBlockers.Reset();

	if (CachedMRUKSubsystem && bDelegatesBound)
	{
		CachedMRUKSubsystem->OnRoomCreated.RemoveDynamic(this, &UMRSpatialRecognitionSubsystem::HandleRoomChanged);
		CachedMRUKSubsystem->OnRoomUpdated.RemoveDynamic(this, &UMRSpatialRecognitionSubsystem::HandleRoomChanged);
		CachedMRUKSubsystem->OnRoomRemoved.RemoveDynamic(this, &UMRSpatialRecognitionSubsystem::HandleRoomChanged);
	}

	bDelegatesBound = false;
	CachedMRUKSubsystem = nullptr;
	RecognizedAnchors.Reset();

	Super::Deinitialize();
}

bool UMRSpatialRecognitionSubsystem::LoadSceneFromDevice()
{
	if (!EnsureScenePermissionOrRequest())
	{
		return false;
	}

	UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	if (!MRUKSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("MRUK subsystem is not available"));
		OnSceneLoaded.Broadcast(false);
		return false;
	}

	// Room変更 + OnSceneLoaded を購読する。
	// Async版のSuccess/Failureが発火しない場合の保険として OnSceneLoaded も直接拾う
	// （HandleSceneLoaded 側で二重発火をガードする）。
	if (!bDelegatesBound)
	{
		MRUKSubsystem->OnSceneLoaded.AddUniqueDynamic(this, &UMRSpatialRecognitionSubsystem::HandleSceneLoaded);
		MRUKSubsystem->OnRoomCreated.AddUniqueDynamic(this, &UMRSpatialRecognitionSubsystem::HandleRoomChanged);
		MRUKSubsystem->OnRoomUpdated.AddUniqueDynamic(this, &UMRSpatialRecognitionSubsystem::HandleRoomChanged);
		MRUKSubsystem->OnRoomRemoved.AddUniqueDynamic(this, &UMRSpatialRecognitionSubsystem::HandleRoomChanged);
		bDelegatesBound = true;
	}

	// 新しいロード試行ごとに処理済みフラグをリセット。
	bSceneLoadHandled = false;

	// 既にロード済みで、かつ部屋アクターが実在する場合のみ即成功扱い。
	// 注意: UMRUKSubsystem は GameInstance サブシステムでレベル遷移をまたいで生き残るため、
	// 前のレベル(Title)でロード済みだと SceneLoadStatus は Complete のまま。しかし OpenLevel で
	// 前ワールドの部屋アクター(Rooms)は破棄されるので、新ワールドでは Rooms が空のことがある。
	// その状態で即 HandleSceneLoaded(true) すると「認識アンカー0/壁なし/NavMesh生成されず」で
	// 敵が出ない（Title→Play 遷移時に再現）。Rooms が空なら下の通常ロードに進んで再取得させる。
	if (MRUKSubsystem->SceneLoadStatus == EMRUKInitStatus::Complete && MRUKSubsystem->Rooms.Num() > 0)
	{
		HandleSceneLoaded(true);
		return true;
	}

	// ステータスは Complete なのに Rooms が空 = 前レベルのロード結果が残っているだけで、
	// 新ワールドには部屋アクターが無い状態。このまま LoadSceneFromDeviceAsync を呼んでも
	// MRUK が「もう Complete」と判断して再ディスカバリーせず、部屋が永久に空のままになりうる。
	// ClearScene() でステータスをリセットし、確実に再ロード(再ディスカバリー)を走らせる。
	if (MRUKSubsystem->SceneLoadStatus == EMRUKInitStatus::Complete && MRUKSubsystem->Rooms.Num() == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("LoadSceneFromDevice: status=Complete but Rooms empty (level transition). ClearScene and reload."));
		MRUKSubsystem->ClearScene();
	}

	++LoadAttemptCount;

	// 同期版 LoadSceneFromDevice は BeginPlay 直後に呼ぶと OpenXR セッション確立前に
	// StartDiscovery が走り error -2 になる。非同期版(UMRUKLoadFromDevice)は
	// UBlueprintAsyncActionBase 経由で準備完了を待ってから discovery を始めるため -2 を回避できる。
	UWorld* World = GetWorld();
	if (!World)
	{
		HandleSceneLoaded(false);
		return false;
	}

	UMRUKLoadFromDevice* AsyncLoad = UMRUKLoadFromDevice::LoadSceneFromDeviceAsync(World, EMRUKSceneModel::V1);
	if (!AsyncLoad)
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadSceneFromDeviceAsync returned null; falling back to sync load"));
		MRUKSubsystem->LoadSceneFromDevice(EMRUKSceneModel::V1);
		return true;
	}

	// GC回収を防ぐためメンバで保持（保持しないとSuccess/Failureが発火しないことがある）。
	ActiveAsyncLoad = AsyncLoad;
	AsyncLoad->Success.AddDynamic(this, &UMRSpatialRecognitionSubsystem::HandleAsyncLoadSucceeded);
	AsyncLoad->Failure.AddDynamic(this, &UMRSpatialRecognitionSubsystem::HandleAsyncLoadFailed);
	// 重要: LoadSceneFromDeviceAsync はオブジェクト生成のみで、実処理は Activate() が行う。
	// Blueprintノードとして使うとエンジンが自動で呼ぶが、C++から使う場合は明示的に呼ぶ必要がある。
	// （これを呼ばないと MRUK の StartDiscovery 自体が一度も実行されず、無音で部屋ゼロになる。）
	AsyncLoad->Activate();
	UE_LOG(LogTemp, Log, TEXT("LoadSceneFromDevice: started async load (attempt %d)"), LoadAttemptCount);

	// 保険: Async の Success/Failure が発火しない場合に備え、MRUKSubsystem->Rooms が
	// 埋まるのをタイマーでポーリングする。埋まったら HandleSceneLoaded(true) を呼ぶ。
	RoomsPollAttempts = 0;
	if (UWorld* PollWorld = GetWorld())
	{
		PollWorld->GetTimerManager().ClearTimer(RoomsPollTimerHandle);
		PollWorld->GetTimerManager().SetTimer(
			RoomsPollTimerHandle, this, &UMRSpatialRecognitionSubsystem::PollForRooms, 0.5f, true);
	}
	return true;
}

void UMRSpatialRecognitionSubsystem::PollForRooms()
{
	// 既に処理済みなら停止。
	if (bSceneLoadHandled)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RoomsPollTimerHandle);
		}
		return;
	}

	++RoomsPollAttempts;

	UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	const bool bHasRoom = MRUKSubsystem && MRUKSubsystem->Rooms.Num() > 0;
	const bool bComplete = MRUKSubsystem && MRUKSubsystem->SceneLoadStatus == EMRUKInitStatus::Complete;

	if (bHasRoom || bComplete)
	{
		UE_LOG(LogTemp, Log, TEXT("PollForRooms: rooms ready (Rooms=%d, status=%d) after %d polls"),
		       MRUKSubsystem ? MRUKSubsystem->Rooms.Num() : 0,
		       MRUKSubsystem ? static_cast<int32>(MRUKSubsystem->SceneLoadStatus) : -1,
		       RoomsPollAttempts);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RoomsPollTimerHandle);
		}
		HandleSceneLoaded(true);
		return;
	}

	if (RoomsPollAttempts >= MaxRoomsPollAttempts)
	{
		UE_LOG(LogTemp, Warning, TEXT("PollForRooms: gave up after %d polls, no rooms"), RoomsPollAttempts);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(RoomsPollTimerHandle);
		}

		// discoveryが完了イベントを返さないままMRUKがBusyで固まると、以後の
		// LoadSceneFromDevice が全て弾かれて再試行不能になる。状態をリセットして
		// HandleSceneLoaded(false) 経由のリトライ（MaxLoadAttemptsまで）に繋げる。
		if (MRUKSubsystem && MRUKSubsystem->SceneLoadStatus == EMRUKInitStatus::Busy)
		{
			MRUKSubsystem->ClearScene();
		}
		ActiveAsyncLoad = nullptr;
		HandleSceneLoaded(false);
	}
}

void UMRSpatialRecognitionSubsystem::HandleAsyncLoadSucceeded()
{
	UE_LOG(LogTemp, Log, TEXT("Async scene load succeeded"));
	ActiveAsyncLoad = nullptr;
	HandleSceneLoaded(true);
}

void UMRSpatialRecognitionSubsystem::HandleAsyncLoadFailed()
{
	UE_LOG(LogTemp, Warning, TEXT("Async scene load failed"));
	ActiveAsyncLoad = nullptr;
	HandleSceneLoaded(false);
}

bool UMRSpatialRecognitionSubsystem::LaunchRoomScan()
{
	if (!EnsureScenePermissionOrRequest())
	{
		return false;
	}

	UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	if (!MRUKSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("LaunchRoomScan: MRUK subsystem is not available"));
		return false;
	}

	// スキャン完了後に発火する OnSceneLoaded / OnRoom* を確実に拾えるよう、デリゲートを張っておく。
	if (!bDelegatesBound)
	{
		MRUKSubsystem->OnSceneLoaded.AddUniqueDynamic(this, &UMRSpatialRecognitionSubsystem::HandleSceneLoaded);
		MRUKSubsystem->OnRoomCreated.AddUniqueDynamic(this, &UMRSpatialRecognitionSubsystem::HandleRoomChanged);
		MRUKSubsystem->OnRoomUpdated.AddUniqueDynamic(this, &UMRSpatialRecognitionSubsystem::HandleRoomChanged);
		MRUKSubsystem->OnRoomRemoved.AddUniqueDynamic(this, &UMRSpatialRecognitionSubsystem::HandleRoomChanged);
		bDelegatesBound = true;
	}

	// LaunchSceneCapture はスキャン画面を出すだけで、完了時にシーンを自動ロードしない。
	// 完了通知(OnCaptureComplete)を購読し、その中で LoadSceneFromDevice を呼ぶ必要がある。
	MRUKSubsystem->OnCaptureComplete.AddUniqueDynamic(this, &UMRSpatialRecognitionSubsystem::HandleCaptureComplete);

	// アプリ内からスペース設定（部屋スキャン）画面を起動する。
	const bool bLaunched = MRUKSubsystem->LaunchSceneCapture();
	UE_LOG(LogTemp, Log, TEXT("LaunchRoomScan: scene capture launched=%s"), bLaunched ? TEXT("true") : TEXT("false"));
	if (!bLaunched)
	{
		// 起動直後でXRセッションが整っていない等でスキャン画面が開けないことがある。
		// その場合は保存済みの部屋データのロードに切り替える（部屋ロードが完全に止まるのを防ぐ）。
		LoadSceneFromDeviceDeferred(1.0f);
	}
	return bLaunched;
}

void UMRSpatialRecognitionSubsystem::HandleCaptureComplete(bool bSuccess)
{
	UE_LOG(LogTemp, Log, TEXT("HandleCaptureComplete: success=%s"), bSuccess ? TEXT("true") : TEXT("false"));

	// スキャン画面から復帰した直後はXRセッションがまだFOCUSEDに戻っていない
	// （実測でdiscovery発行がフォーカス回復より44ms早く、ランタイムに黙殺された）。
	// セッションのフォーカス回復を待ってからdiscoveryを開始する。
	LoadSceneFromDeviceDeferred(1.0f);
}

void UMRSpatialRecognitionSubsystem::LoadSceneFromDeviceDeferred(float DelaySeconds)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DeferredLoadTimerHandle);
		World->GetTimerManager().SetTimer(
			DeferredLoadTimerHandle,
			this,
			&UMRSpatialRecognitionSubsystem::RetryLoadSceneFromDevice,
			FMath::Max(0.1f, DelaySeconds),
			false);
	}
}

bool UMRSpatialRecognitionSubsystem::CalibrateFrontWall(const FVector& PlayerLocation, const FVector& PlayerForward)
{
	bWallBaseValid = false;

	const UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	const AMRUKRoom* PrimaryRoom = GetPrimaryRoom();
	if (!PrimaryRoom)
	{
		UE_LOG(LogTemp, Log, TEXT("CalibrateFrontWall: no primary room (room not ready?)"));
		return false;
	}

	// 部屋の全壁アンカーの中から、プレイヤーから最も遠い壁を選ぶ。
	// その壁際から敵を出して、プレイヤーに近づいてくる演出にする。
	const AMRUKAnchor* FarthestWall = nullptr;
	float FarthestDistSq = -1.0f;

	for (const AMRUKRoom* Room : MRUKSubsystem->Rooms)
	{
		if (!Room)
		{
			continue;
		}
		if (Room != PrimaryRoom)
		{
			continue;
		}

		for (const AMRUKAnchor* Wall : Room->WallAnchors)
		{
			if (!Wall)
			{
				continue;
			}

			// 壁面の中心とプレイヤーの水平距離で「遠さ」を判定する。
			const FVector WallCenter = Wall->GetActorLocation();
			const float DistSq = FVector::DistSquared2D(WallCenter, PlayerLocation);
			if (DistSq > FarthestDistSq)
			{
				FarthestDistSq = DistSq;
				FarthestWall = Wall;
			}
		}
	}

	if (!FarthestWall)
	{
		UE_LOG(LogTemp, Log, TEXT("CalibrateFrontWall: no wall anchors (room not ready?)"));
		return false;
	}

	// 壁面の中心を基準点に、法線は壁の前方向（部屋内側を向く）を使う。
	// MRUKの壁アンカーは ActorForwardVector が面の法線（部屋の内側向き）。
	CachedWallPoint = FarthestWall->GetActorLocation();
	CachedWallNormal = FarthestWall->GetActorForwardVector();
	// 高さはプレイヤー基準に寄せる（湧きは床へ別途接地させるため、ここはXY基準点として使う）。
	CachedWallPoint.Z = PlayerLocation.Z;

	// 壁の水平半幅をキャッシュする（Spawnerを壁端から飛び出させないクランプ用）。
	// MRUK壁アンカーの PlaneBounds は面ローカルの2D矩形で、X が水平方向の半幅。
	CachedWallHalfWidth = 0.0f;
	if (FarthestWall->PlaneBounds.bIsValid)
	{
		CachedWallHalfWidth = FarthestWall->PlaneBounds.GetExtent().X;
	}
	else if (FarthestWall->VolumeBounds.IsValid)
	{
		CachedWallHalfWidth = FarthestWall->VolumeBounds.GetExtent().X;
	}

	bWallBaseValid = true;

	UE_LOG(LogTemp, Log, TEXT("CalibrateFrontWall: farthest wall at %s (dist=%.0f)"),
	       *CachedWallPoint.ToString(), FMath::Sqrt(FarthestDistSq));
	return true;
}

bool UMRSpatialRecognitionSubsystem::FindClearSpawnPoint(const FVector& PlayerLocation, const FVector& PlayerForward, FVector& OutSpawnLocation, bool& bUsedFallback)
{
	bUsedFallback = false;

	// 壁が測れていない場合のフォールバック点（プレイヤー正面固定距離）。
	const FVector PlayerForwardFlat = PlayerForward.GetSafeNormal2D();
	const FVector FallbackPoint = PlayerLocation + PlayerForwardFlat * FallbackSpawnDistance;

	if (!bWallBaseValid)
	{
		OutSpawnLocation = FallbackPoint;
		bUsedFallback = true;
		return false;
	}

	// 最遠壁の内向き法線（部屋内側＝プレイヤー側を向く）を湧き方向の基準にする。
	const FVector WallInward = CachedWallNormal.GetSafeNormal2D();
	// 横ずらし方向 = 壁面に沿う水平方向（内向き法線に直交）。
	const FVector RightDir = FVector::CrossProduct(FVector::UpVector, WallInward).GetSafeNormal();

	if (WallInward.IsNearlyZero() || RightDir.IsNearlyZero())
	{
		OutSpawnLocation = FallbackPoint;
		bUsedFallback = true;
		return false;
	}

	// 最遠壁の手前(部屋内側へ WallOffset)を基準点とし、横は壁沿い(RightDir)に振る。
	// 高さは後で各候補ごとに床をLineTraceで測って決める（壁中心高に置くと床が無く落下するため）。
	FVector SpawnBase = CachedWallPoint + WallInward * WallOffset;
	// まず暫定でプレイヤーの足元高さ（後段の床測定が失敗した時のフォールバックにも使う）。
	SpawnBase.Z = PlayerLocation.Z;

	// 中央(0) → +1, -1, +2, -2 ... の順で横ずらし候補を試す。
	for (int32 Step = 0; Step <= MaxSideSteps; ++Step)
	{
		// Step==0 は中央のみ。それ以外は右(+1)→左(-1)の2候補。
		const int32 Signs[2] = { 1, -1 };
		const int32 NumSigns = (Step == 0) ? 1 : 2;

		for (int32 i = 0; i < NumSigns; ++i)
		{
			const int32 Sign = (Step == 0) ? 0 : Signs[i];
			FVector Candidate = SpawnBase + RightDir * (SideStepDistance * Step * Sign);
			if (IsCandidateClear(Candidate, PlayerLocation, RightDir))
			{
				// 遮蔽なしの点が見つかった。真下の床を測ってそのZに接地させる。
				float FloorZ = 0.0f;
				if (MeasureFloorHeight(Candidate, FloorZ))
				{
					Candidate.Z = FloorZ;
				}
				OutSpawnLocation = Candidate;
				return true;
			}
		}
	}

	// 全候補が塞がれていた → 中央(壁前)にそのまま湧かす（フォールバック）。床は測れれば反映。
	float FloorZ = 0.0f;
	if (MeasureFloorHeight(SpawnBase, FloorZ))
	{
		SpawnBase.Z = FloorZ;
	}
	OutSpawnLocation = SpawnBase;
	bUsedFallback = true;
	return false;
}

bool UMRSpatialRecognitionSubsystem::GetSpawnPointsAlongFarthestWall(int32 NumPoints, float Spacing,
	TArray<FVector>& OutPoints, FVector& OutWallInward) const
{
	OutPoints.Reset();
	OutWallInward = FVector::ZeroVector;

	if (!bWallBaseValid || NumPoints <= 0)
	{
		return false;
	}

	const FVector WallInward = CachedWallNormal.GetSafeNormal2D();
	const FVector RightDir = FVector::CrossProduct(FVector::UpVector, WallInward).GetSafeNormal();
	if (WallInward.IsNearlyZero() || RightDir.IsNearlyZero())
	{
		return false;
	}

	// 壁前 WallOffset の中心点を基準に、左右に等間隔で並べる。
	const FVector Base = CachedWallPoint + WallInward * WallOffset;
	float HalfRange = Spacing * (NumPoints - 1) * 0.5f;

	// 壁幅を超えて壁の端から飛び出さないようにクランプする（バグ: Spawner数/間隔を増やすと
	// 壁の前でもない所に並ぶ問題）。壁端ぴったりだとカプセルが角に埋まるので EnemyRadius ぶん内側に寄せる。
	if (CachedWallHalfWidth > 0.0f)
	{
		const float UsableHalf = FMath::Max(0.0f, CachedWallHalfWidth - EnemyRadius);
		HalfRange = FMath::Min(HalfRange, UsableHalf);
	}

	// クランプ後の実効間隔（NumPoints==1 のときは 0）。
	const float EffectiveSpacing = (NumPoints > 1) ? (HalfRange * 2.0f / (NumPoints - 1)) : 0.0f;

	OutPoints.Reserve(NumPoints);
	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float Side = -HalfRange + EffectiveSpacing * i;
		FVector P = Base + RightDir * Side;
		// 床高さに合わせる（取れなければ CachedWallPoint.Z のままにしておく）。
		float FloorZ = 0.0f;
		// 非const内部実装にアクセスするため、ここだけ const を外して呼ぶ。
		if (const_cast<UMRSpatialRecognitionSubsystem*>(this)->MeasureFloorHeight(P, FloorZ))
		{
			P.Z = FloorZ;
		}
		OutPoints.Add(P);
	}

	OutWallInward = WallInward;
	return true;
}

float UMRSpatialRecognitionSubsystem::GetFloorSurfaceZ(const AMRUKAnchor* FloorAnchor) const
{
	if (!FloorAnchor)
	{
		return 0.0f;
	}
	const float ActorFloorZ = FloorAnchor->GetActorLocation().Z;
	bool bUsePlaneBoundsForFloorHeight = false;

	// 床アンカーは Pitch=-90 で寝ているため、GetActorLocation().Z（原点）は実床面とズレる
	// （実機で原点90 / 実床162）。PlaneBounds の4隅をワールド変換し、その Z 平均を実床面とする。
	if (bUsePlaneBoundsForFloorHeight && FloorAnchor->PlaneBounds.bIsValid)
	{
		const FVector2D PMin = FloorAnchor->PlaneBounds.Min;
		const FVector2D PMax = FloorAnchor->PlaneBounds.Max;
		const FVector LocalCorners[4] = {
			FVector(PMin.X, PMin.Y, 0.0f),
			FVector(PMax.X, PMin.Y, 0.0f),
			FVector(PMax.X, PMax.Y, 0.0f),
			FVector(PMin.X, PMax.Y, 0.0f)
		};
		const FTransform& Xform = FloorAnchor->GetActorTransform();
		float SumZ = 0.0f;
		for (const FVector& LC : LocalCorners)
		{
			SumZ += Xform.TransformPosition(LC).Z;
		}
		return SumZ * 0.25f;
	}

	// PlaneBounds が無ければ原点Zにフォールバック。
	return ActorFloorZ;
}

bool UMRSpatialRecognitionSubsystem::GetFloorRect(FVector& OutCenter, FVector2D& OutHalfXY) const
{
	const UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	if (!MRUKSubsystem)
	{
		return false;
	}
	const AMRUKRoom* PrimaryRoom = GetPrimaryRoom();
	if (!PrimaryRoom)
	{
		return false;
	}

	// MeasureFloorHeight と同様、最も Z が高い床アンカー＝本物の床を採用する。
	// ※PrimaryRoom（プレイヤーがいる部屋）以外は見ない。デバイスに残った古い部屋の床矩形を
	//   拾うと、固定床(GroundActor)の中心・サイズが別の部屋基準になるため。
	const AMRUKAnchor* BestFloor = nullptr;
	float BestZ = -TNumericLimits<float>::Max();
	for (const AMRUKRoom* Room : MRUKSubsystem->Rooms)
	{
		if (!Room)
		{
			continue;
		}
		if (Room != PrimaryRoom)
		{
			continue;
		}
		for (const AMRUKAnchor* Floor : Room->FloorAnchors)
		{
			if (!Floor)
			{
				continue;
			}
			// 比較は MeasureFloorHeight と同様、実床面Z（4隅ワールド変換）で行い、採用アンカーを一致させる。
			const float Z = GetFloorSurfaceZ(Floor);
			if (Z > BestZ)
			{
				BestZ = Z;
				BestFloor = Floor;
			}
		}
	}

	if (!BestFloor)
	{
		return false;
	}

	const FVector AnchorLoc = BestFloor->GetActorLocation();

	// PlaneBounds（面ローカルの2D矩形）の4隅を、アンカーのワールドTransformで変換し、
	// その XY の min/max から「ワールド軸に揃った床矩形」を求める。
	// これにより床アンカーの回転(Pitch=-90/Yaw)を一切考慮せず、4隅を全部カバーする無回転Boxになる
	// → 生成床が傾く/ズレる問題が原理的に消える。
	if (BestFloor->PlaneBounds.bIsValid)
	{
		const FVector2D PMin = BestFloor->PlaneBounds.Min;
		const FVector2D PMax = BestFloor->PlaneBounds.Max;
		// 面ローカル2D(X,Y)。MRUK床アンカーは Pitch=-90 で寝ているので、面ローカルの (X,Y) は
		// アクターTransform上は (X, Z) 平面に乗る。ローカル点は (X, Y, 0) として変換すれば、
		// アクターの回転がそのまま適用されてワールドXYに落ちる。
		const FVector LocalCorners[4] = {
			FVector(PMin.X, PMin.Y, 0.0f),
			FVector(PMax.X, PMin.Y, 0.0f),
			FVector(PMax.X, PMax.Y, 0.0f),
			FVector(PMin.X, PMax.Y, 0.0f)
		};
		const FTransform& Xform = BestFloor->GetActorTransform();
		float MinX = TNumericLimits<float>::Max();
		float MinY = TNumericLimits<float>::Max();
		float MaxX = -TNumericLimits<float>::Max();
		float MaxY = -TNumericLimits<float>::Max();
		for (const FVector& LC : LocalCorners)
		{
			const FVector WC = Xform.TransformPosition(LC);
			MinX = FMath::Min(MinX, WC.X);
			MinY = FMath::Min(MinY, WC.Y);
			MaxX = FMath::Max(MaxX, WC.X);
			MaxY = FMath::Max(MaxY, WC.Y);
		}
		// 床面Zはアンカー原点(寝た姿勢でズレる)ではなく、4隅をワールド変換した実床面Zを使う。
		const float SurfaceZ = GetFloorSurfaceZ(BestFloor);
		OutCenter = FVector((MinX + MaxX) * 0.5f, (MinY + MaxY) * 0.5f, SurfaceZ);
		OutHalfXY = FVector2D((MaxX - MinX) * 0.5f, (MaxY - MinY) * 0.5f);

		UE_LOG(LogTemp, Warning,
			TEXT("GetFloorRect: worldRect center=%s halfXY=(%.1f,%.1f) anchorZ=%.1f surfaceZ=%.1f [from 4 corners, no rotation]"),
			*OutCenter.ToCompactString(), OutHalfXY.X, OutHalfXY.Y, AnchorLoc.Z, SurfaceZ);
		return true;
	}

	// PlaneBounds が無ければ VolumeBounds → それも無ければ広めフォールバック。
	// 高さは実床面Z（PlaneBounds が無ければ原点Zにフォールバックされる）。
	OutCenter = FVector(AnchorLoc.X, AnchorLoc.Y, GetFloorSurfaceZ(BestFloor));
	if (BestFloor->VolumeBounds.IsValid)
	{
		const FVector Ext = BestFloor->VolumeBounds.GetExtent();
		OutHalfXY = FVector2D(FMath::Abs(Ext.X), FMath::Abs(Ext.Y));
	}
	else
	{
		// 実寸が取れない場合は広め(5m四方)のフォールバック。
		OutHalfXY = FVector2D(500.0f, 500.0f);
	}
	return true;
}

bool UMRSpatialRecognitionSubsystem::MeasureFloorHeight(const FVector& FromLocation, float& OutFloorZ)
{
	const AMRUKRoom* PrimaryRoom = GetPrimaryRoom();
	// 床高さは、まず MRUK の床アンカーの高さを直接使う。
	// （真下LineTraceだと机/椅子の天面に当たって「机の上＝床」と誤認し、敵が浮くため。）
	if (UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem())
	{
		const AMRUKAnchor* BestFloor = nullptr;
		// 複数の床アンカーがある場合、95.7 より低い「余計な床アンカー」が混ざっていて
		// そこに敵が落ちてスポーンする問題があった。最も Z が高い床アンカー＝本物の床を採用し、
		// 低いアンカーは無視する。
		float BestZ = -TNumericLimits<float>::Max();

		// デバッグ: 床アンカーが何枚あり、それぞれの Z・ラベルが何かを出して
		// 「95.7より低いアンカーが本当に Floor ラベルなのか」を確認する。
		int32 FloorAnchorCount = 0;

		for (const AMRUKRoom* Room : MRUKSubsystem->Rooms)
		{
			if (!Room)
			{
				continue;
			}
			if (Room != PrimaryRoom)
			{
				continue;
			}
			for (const AMRUKAnchor* Floor : Room->FloorAnchors)
			{
				if (!Floor)
				{
					continue;
				}

				++FloorAnchorCount;
				const FVector AnchorLoc = Floor->GetActorLocation();
				// 床面Zはアンカー原点ではなく、4隅をワールド変換した実床面Zを使う（寝た姿勢の原点ズレ対策）。
				const float SurfaceZ = GetFloorSurfaceZ(Floor);
				const FString Labels = FString::Join(Floor->SemanticClassifications, TEXT(","));
				UE_LOG(LogTemp, Log,
				       TEXT("MeasureFloorHeight: FloorAnchor[%d] anchorZ=%.1f surfaceZ=%.1f loc=(%.1f,%.1f,%.1f) labels=[%s]"),
				       FloorAnchorCount - 1, AnchorLoc.Z, SurfaceZ, AnchorLoc.X, AnchorLoc.Y, AnchorLoc.Z, *Labels);

				// 最も高い床アンカーを採用する（低い余計なアンカーは無視）。比較も実床面Zで行う。
				if (SurfaceZ > BestZ)
				{
					BestZ = SurfaceZ;
					BestFloor = Floor;
				}
			}
		}

		if (BestFloor)
		{
			OutFloorZ = GetFloorSurfaceZ(BestFloor);
			UE_LOG(LogTemp, Log,
			       TEXT("MeasureFloorHeight: selected highest FloorAnchor surfaceZ=%.1f (of %d anchors)"),
			       OutFloorZ, FloorAnchorCount);
			return true;
		}
	}

	// 床アンカーが無い場合のフォールバック: 部屋メッシュへ真下LineTrace（机に当たる可能性あり）。
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector Start = FromLocation + FVector(0.0f, 0.0f, FloorScanStartHeight);
	const FVector End = FromLocation - FVector(0.0f, 0.0f, FloorScanMaxDistance);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MRFloorTrace), /*bTraceComplex=*/false);
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		return false;
	}

	OutFloorZ = Hit.ImpactPoint.Z;
	return true;
}

bool UMRSpatialRecognitionSubsystem::IsCandidateClear(const FVector& Candidate, const FVector& PlayerLocation, const FVector& RightDir)
{
	// 論点: 中央＋左右(敵半径分)の3本。1本でも遮蔽ありなら不可。
	const FVector Offsets[3] = {
		FVector::ZeroVector,
		RightDir * EnemyRadius,
		RightDir * -EnemyRadius
	};

	for (const FVector& Offset : Offsets)
	{
		if (IsRayBlocked(Candidate + Offset, PlayerLocation + Offset))
		{
			return false;
		}
	}
	return true;
}

bool UMRSpatialRecognitionSubsystem::IsRayBlocked(const FVector& From, const FVector& To)
{
	const FVector Delta = To - From;
	const float Dist = Delta.Size();
	if (Dist <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Scene方式: 生成された部屋メッシュ(壁/家具, WorldStatic)へ From→To をLineTrace。
	// 何も当たらなければ通る。部屋ロード前は当たらず「遮蔽なし」扱い（湧きを止めない）。
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MRObstacleTrace), /*bTraceComplex=*/false);
	if (!World->LineTraceSingleByChannel(Hit, From, To, ECC_WorldStatic, Params))
	{
		return false;
	}

	// Hit はしたが、プレイヤー至近(ObstacleClearance)より奥のヒットだけを障害物とみなす。
	const float HitDist = FVector::Dist(From, Hit.ImpactPoint);
	return HitDist < (Dist - ObstacleClearance);
}

void UMRSpatialRecognitionSubsystem::RefreshRecognizedAnchors()
{
	RecognizedAnchors.Reset();

	const UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	if (!MRUKSubsystem)
	{
		return;
	}
	const AMRUKRoom* PrimaryRoom = GetPrimaryRoom();

	for (const AMRUKRoom* Room : MRUKSubsystem->Rooms)
	{
		if (!Room)
		{
			continue;
		}
		if (PrimaryRoom && Room != PrimaryRoom)
		{
			continue;
		}

		for (const AMRUKAnchor* Anchor : Room->AllAnchors)
		{
			if (Anchor)
			{
				RecognizedAnchors.Add(BuildAnchorInfo(Anchor));
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Spatial scene recognized %d anchors"), RecognizedAnchors.Num());
	OnRecognizedAnchorsUpdated.Broadcast(RecognizedAnchors);
}

void UMRSpatialRecognitionSubsystem::GetAnchorsByLabel(const FString& Label, TArray<FMRSpatialAnchorInfo>& OutAnchors) const
{
	OutAnchors.Reset();

	for (const FMRSpatialAnchorInfo& Info : RecognizedAnchors)
	{
		if (Info.Labels.Contains(Label))
		{
			OutAnchors.Add(Info);
		}
	}
}

void UMRSpatialRecognitionSubsystem::GetTables(TArray<FMRSpatialAnchorInfo>& OutAnchors) const
{
	OutAnchors.Reset();

	for (const FMRSpatialAnchorInfo& Info : RecognizedAnchors)
	{
		if (AnchorMatchesAnyLabel(Info, { FMRUKLabels::Table }))
		{
			OutAnchors.Add(Info);
		}
	}
}

void UMRSpatialRecognitionSubsystem::GetSeats(TArray<FMRSpatialAnchorInfo>& OutAnchors) const
{
	OutAnchors.Reset();

	for (const FMRSpatialAnchorInfo& Info : RecognizedAnchors)
	{
		if (AnchorMatchesAnyLabel(Info, { FMRUKLabels::Couch, TEXT("CHAIR"), TEXT("SEAT") }))
		{
			OutAnchors.Add(Info);
		}
	}
}

int32 UMRSpatialRecognitionSubsystem::BuildOcclusionMeshes()
{
	UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	if (!MRUKSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("BuildOcclusionMeshes: MRUK subsystem unavailable"));
		return 0;
	}
	const AMRUKRoom* PrimaryRoom = GetPrimaryRoom();

	for (TObjectPtr<AActor>& Blocker : FurnitureNavBlockers)
	{
		if (IsValid(Blocker))
		{
			Blocker->Destroy();
		}
	}
	FurnitureNavBlockers.Reset();

	int32 NumConfigured = 0;
	float HighestFloorZ = -TNumericLimits<float>::Max();
	for (const AMRUKRoom* Room : MRUKSubsystem->Rooms)
	{
		if (!Room)
		{
			continue;
		}
		if (Room != PrimaryRoom)
		{
			continue;
		}
		for (const AMRUKAnchor* Floor : Room->FloorAnchors)
		{
			if (Floor)
			{
				HighestFloorZ = FMath::Max(HighestFloorZ, GetFloorSurfaceZ(Floor));
			}
		}
	}

	for (AMRUKRoom* Room : MRUKSubsystem->Rooms)
	{
		if (!Room)
		{
			continue;
		}
		// PrimaryRoom（プレイヤーがいる部屋）以外のアンカーにはメッシュ/コリジョン/NavBlockerを作らない。
		// 古い部屋の床が「2枚目の床」として出現し、余計なコリジョンとNavMeshの穴を生む原因だった。
		if (Room != PrimaryRoom)
		{
			UE_LOG(LogTemp, Log, TEXT("BuildOcclusionMeshes: skipped non-primary room %s (anchors=%d)"),
			       *Room->GetName(), Room->AllAnchors.Num());
			continue;
		}

		for (AMRUKAnchor* Anchor : Room->AllAnchors)
		{
			if (!Anchor)
			{
				continue;
			}

			const bool bIsCeiling = Anchor->SemanticClassifications.Contains(FMRUKLabels::Ceiling);
			const bool bIsFloor = Anchor->SemanticClassifications.Contains(FMRUKLabels::Floor);
			const bool bIsWall = Anchor->SemanticClassifications.Contains(FMRUKLabels::WallFace);
			if (bIsFloor && HighestFloorZ > -TNumericLimits<float>::Max() * 0.5f &&
				GetFloorSurfaceZ(Anchor) < HighestFloorZ - 50.0f)
			{
				UE_LOG(LogTemp, Log,
					TEXT("BuildOcclusionMeshes: skipped lower floor %s floorZ=%.1f highestFloorZ=%.1f"),
					*Anchor->GetName(),
					GetFloorSurfaceZ(Anchor),
					HighestFloorZ);
				continue;
			}

			// 床に立つ「障害物家具」かどうか。これらは NavMesh に穴を開けて、敵が回り込むようにする。
			// （壁掛けの WallArt、開口の Window/Door、部屋全体の GlobalMesh、不定の Other は含めない。）
			const bool bIsFurnitureObstacle =
				Anchor->SemanticClassifications.Contains(FMRUKLabels::Table) ||
				Anchor->SemanticClassifications.Contains(FMRUKLabels::Storage) ||
				Anchor->SemanticClassifications.Contains(FMRUKLabels::Couch) ||
				Anchor->SemanticClassifications.Contains(FMRUKLabels::Bed) ||
				Anchor->SemanticClassifications.Contains(FMRUKLabels::Screen) ||
				Anchor->SemanticClassifications.Contains(FMRUKLabels::Plant) ||
				Anchor->SemanticClassifications.Contains(FMRUKLabels::Lamp);

			// 設定で天井/床を除外できるようにする。
			if (!bIncludeCeilingInOcclusion && bIsCeiling)
			{
				continue;
			}
			if (!bIncludeFloorInOcclusion && bIsFloor)
			{
				continue;
			}

			// アンカーの形状に沿ったコリジョン付きプロシージャルメッシュを生成する。
			// 窓/ドア/開口は穴を開けて、その部分は隠さない。
			// マテリアルの貼り分け:
			// - 壁/天井: デバッグ可視化時でもマテリアルを貼らない（純粋なオクルージョン専用）。
			// - 床/家具: デバッグ可視化が有効なら DebugMeshMaterial を貼って目視確認できる。
			const TArray<FString> CutHoleLabels = { FMRUKLabels::WindowFrame, FMRUKLabels::DoorFrame, FMRUKLabels::Opening };
			const bool bAllowMaterial = bDebugVisualizeMesh && !bIsWall && !bIsCeiling;
			UMaterialInterface* MeshMaterial = bAllowMaterial ? DebugMeshMaterial.Get() : nullptr;
			Anchor->AttachProceduralMesh(CutHoleLabels, /*GenerateCollision=*/true, MeshMaterial);

			UProceduralMeshComponent* Mesh = Anchor->ProceduralMeshComponent;
			if (!Mesh)
			{
				continue;
			}

			// オクルージョンメッシュの位置を生成時点で固定する。
			// MRUK アンカーはトラッキングのドリフト補正で実行中もジワジワ動き続けるため、
			// アンカーの子のままだと壁/床メッシュも一緒に動いて見える（敵基準で壁が動く現象）。
			// 生成直後にアンカーからデタッチしてワールド位置を保持し、以後追従させない。
			if (bFreezeOcclusionMeshTransform)
			{
				Mesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
			}

			// デバッグ可視化は壁/天井を除いて適用（壁/天井は常に不可視）。
			const bool bVisualizeThis = bDebugVisualizeMesh && !bIsWall && !bIsCeiling;
			if (bVisualizeThis)
			{
				// デバッグ: 部屋メッシュをメインパスでも描画して目視確認できるようにする。
				Mesh->SetRenderInMainPass(true);
				Mesh->SetRenderInDepthPass(true);
				Mesh->bRenderInDepthPass = true;
				Mesh->SetVisibility(true);
			}
			else if (bSceneMeshVisualOcclusion)
			{
				// Scene方式オクルージョン（旧挙動）:
				// - メインパス(色)では描かない → 見えない（パススルーの現実映像が透ける）
				// - 深度パスには書き込む → このメッシュより奥にある敵が深度テストで隠される
				// ※ ただし Scene メッシュはトラッキング補正(World Lock)で実行中に上下ドリフトし、
				//   それが深度パス経由で「現実の床が上下して見える」原因になる（Meta も visual 用途は非推奨）。
				Mesh->SetRenderInMainPass(false);
				Mesh->SetRenderInDepthPass(true);
				Mesh->bRenderInDepthPass = true;
				Mesh->SetVisibility(true);
			}
			else
			{
				// 完全不可視（推奨・既定）:
				// メインパスも深度パスも描画しない。メッシュは NavMesh 土台と衝突専用にする。
				// これで Scene メッシュのドリフトが画面に一切出ない（床/壁が上下して見えない）。
				// 現実物体による敵のオクルージョンが必要なら Depth API 側(SetXROcclusionsMode)で行う。
				Mesh->SetRenderInMainPass(false);
				Mesh->SetRenderInDepthPass(false);
				Mesh->bRenderInDepthPass = false;
				Mesh->SetVisibility(false);
			}
			Mesh->SetCastShadow(false);

			// NavMesh の歩行可能面の扱い。家具/壁/天井等は常に Nav 非対象。
			// 床は bFloorMeshAffectsNavigation 次第:
			//  - false（GMの固定床を使う場合・既定）: MRUK床メッシュも Nav 非対象にする。
			//    MRUK床メッシュは World Lock で上下ドリフトするので Nav 面にすると NavMesh が揺れるため。
			//  - true（単体利用時）: 床メッシュを Nav 面にする。
			Mesh->SetCanEverAffectNavigation(bIsFloor && bFloorMeshAffectsNavigation);

			// 家具(机等)は「足元の薄い矩形」だけを NavMesh の穴(歩行不可エリア)にして障害物化する。
			// メッシュのコリジョン(縦に厚い3D Box)を NavModifier に見させると、机だらけの部屋で
			// Null 領域が縦横に広がり NavMesh が全滅した(verts=14)。そこで MRUK アンカーの実寸
			// (VolumeBounds/PlaneBounds)から机の足元サイズを取り、NavModifier の FailsafeExtent に
			// 「XY=机サイズ / Z=薄く」を指定する。FailsafeExtent はコリジョンを持たないアクターで
			// 使われる代替Extentなので、メッシュコリジョンに引きずられず確実に薄い板にできる。
			if (bFurnitureBlocksNavigation && bIsFurnitureObstacle)
			{
				SpawnFurnitureNavBlocker(Anchor);
			}

			Mesh->MarkRenderStateDirty();

			++NumConfigured;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BuildOcclusionMeshes: configured %d occlusion anchors"), NumConfigured);

	// 診断: NavMesh タイルが生成されるのを待って（2秒後）、各家具ブロッカーで穴が開いたか確認する。
	if (bFurnitureBlocksNavigation && FurnitureNavBlockers.Num() > 0)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerHandle VerifyHandle;
			World->GetTimerManager().SetTimer(
				VerifyHandle, this, &UMRSpatialRecognitionSubsystem::VerifyFurnitureBlockers, 2.0f, false);
		}
	}

	return NumConfigured;
}

void UMRSpatialRecognitionSubsystem::SpawnFurnitureNavBlocker(AMRUKAnchor* Anchor)
{
	UWorld* World = GetWorld();
	if (!World || !Anchor)
	{
		return;
	}

	// 机の足元サイズ(XY)を MRUK アンカーの実寸から取る。
	// MRUK VolumeアンカーはPitch=-90度の倒れた座標系になることがあるため、
	// 水平矩形として扱いやすいPlaneBoundsを優先する。メッシュのコリジョンには頼らない。
	FVector2D HalfXY(0.0f, 0.0f);
	bool bUsedPlaneBounds = false;
	if (Anchor->PlaneBounds.bIsValid)
	{
		HalfXY = Anchor->PlaneBounds.GetExtent();
		bUsedPlaneBounds = true;
	}
	else if (Anchor->VolumeBounds.IsValid)
	{
		const FVector Ext = Anchor->VolumeBounds.GetExtent(); // ローカル半径
		HalfXY = FVector2D(Ext.X, Ext.Y);
	}
	else
	{
		// 実寸が取れない家具はスキップ（無理に障害物化しない）。
		UE_LOG(LogTemp, Warning,
		       TEXT("FurnitureNavBlocker: skipped %s labels=[%s] AnchorLoc=%s no valid VolumeBounds/PlaneBounds"),
		       *Anchor->GetName(),
		       *FString::Join(Anchor->SemanticClassifications, TEXT(",")),
		       *Anchor->GetActorLocation().ToString());
		return;
	}

	// 薄い板にする: XY は机サイズ、Z は床面付近だけ（FurnitureBlockerHalfHeight）。
	// これにより NavMesh は机の足元だけ削れ、天板の高さや隣の机まで広がらない。
	const FVector Failsafe(
		FMath::Max(5.0f, HalfXY.X),
		FMath::Max(5.0f, HalfXY.Y),
		FMath::Max(2.0f, FurnitureBlockerHalfHeight));

	// 障害物アクターを机の位置(床高さ)に置く。アンカーの XY を使い、Z は床面に合わせる。
	const FVector AnchorLoc = Anchor->GetActorLocation();
	float FloorZ = AnchorLoc.Z;
	bool bMeasuredFloor = false;
	{
		float MeasuredZ = 0.0f;
		if (MeasureFloorHeight(AnchorLoc, MeasuredZ))
		{
			FloorZ = MeasuredZ;
			bMeasuredFloor = true;
		}
	}
	const FVector BlockerLoc(AnchorLoc.X, AnchorLoc.Y, FloorZ + FurnitureBlockerHalfHeight);

	UE_LOG(LogTemp, Log,
	       TEXT("FurnitureNavBlocker: %s labels=[%s] AnchorLoc=%s AnchorRot=%s BlockerRot=%s VolumeValid=%d VolumeMin=%s VolumeMax=%s VolumeExtent=%s PlaneValid=%d PlaneMin=%s PlaneMax=%s PlaneExtent=%s UsedBounds=%s HalfXY=(%.1f,%.1f) bMeasuredFloor=%s FloorZ=%.1f BlockerLoc=%s Failsafe=%s"),
	       *Anchor->GetName(),
	       *FString::Join(Anchor->SemanticClassifications, TEXT(",")),
	       *AnchorLoc.ToString(),
	       *Anchor->GetActorRotation().ToString(),
	       *FRotator::ZeroRotator.ToString(),
	       Anchor->VolumeBounds.IsValid ? 1 : 0,
	       *Anchor->VolumeBounds.Min.ToString(),
	       *Anchor->VolumeBounds.Max.ToString(),
	       *Anchor->VolumeBounds.GetExtent().ToString(),
	       Anchor->PlaneBounds.bIsValid ? 1 : 0,
	       *Anchor->PlaneBounds.Min.ToString(),
	       *Anchor->PlaneBounds.Max.ToString(),
	       *Anchor->PlaneBounds.GetExtent().ToString(),
	       bUsedPlaneBounds ? TEXT("PlaneBounds") : TEXT("VolumeBounds"),
	       HalfXY.X,
	       HalfXY.Y,
	       bMeasuredFloor ? TEXT("true") : TEXT("false"),
	       FloorZ,
	       *BlockerLoc.ToString(),
	       *Failsafe.ToString());

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Blocker = World->SpawnActor<AActor>(AActor::StaticClass(), BlockerLoc, FRotator::ZeroRotator, Params);
	if (!Blocker)
	{
		return;
	}

	// ルートを付けて NavModifier を載せる。NavModifier はコリジョンが無いので FailsafeExtent を使う。
	USceneComponent* Root = NewObject<USceneComponent>(Blocker);
	Blocker->SetRootComponent(Root);
	// SpawnActor に渡した座標はルート無しの空アクターでは適用されないため、後付けルートを明示的に置く。
	// （これを怠ると NavModifier の穴が家具の位置ではなくワールド原点に開く。）
	Root->SetRelativeLocation(BlockerLoc);
	Root->RegisterComponent();

	UNavModifierComponent* NavMod = NewObject<UNavModifierComponent>(Blocker);
	if (NavMod)
	{
		// コリジョンを持たないアクターなので FailsafeExtent(薄い板)が領域になる。
		NavMod->FailsafeExtent = Failsafe;
		// 下方へエージェント高さ分広げる挙動を止める（これが効くと床を縦に削りすぎる）。
		NavMod->bIncludeAgentHeight = false;
		NavMod->SetAreaClass(UNavArea_Null::StaticClass());
		NavMod->RegisterComponent();
	}

	FurnitureNavBlockers.Add(Blocker);
}

void UMRSpatialRecognitionSubsystem::VerifyFurnitureBlockers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("VerifyFurnitureBlockers: NavSys unavailable"));
		return;
	}

	// 各ブロッカー中心を NavMesh に投影し、穴が開いているか確認する。
	// 投影「成功」= その点に歩行可能な NavMesh が残っている = 穴が開いていない（NavModifier 効いてない）。
	// 投影「失敗」= その点は歩行不可（穴）= NavModifier が効いている。
	int32 HoleOk = 0;
	int32 NoHole = 0;
	const FVector ProjExtent(20.0f, 20.0f, 40.0f); // 薄い範囲で「その真上に床NavがあるかR」を見る
	for (const TObjectPtr<AActor>& Blocker : FurnitureNavBlockers)
	{
		if (!IsValid(Blocker))
		{
			continue;
		}
		const FVector Loc = Blocker->GetActorLocation();
		FNavLocation Proj;
		const bool bOnNav = NavSys->ProjectPointToNavigation(Loc, Proj, ProjExtent);
		if (bOnNav)
		{
			++NoHole;
			UE_LOG(LogTemp, Warning,
				TEXT("VerifyFurnitureBlockers: NO HOLE at %s (projected to %s) -> NavModifier NOT applied here"),
				*Loc.ToCompactString(), *Proj.Location.ToCompactString());
		}
		else
		{
			++HoleOk;
		}
	}
	UE_LOG(LogTemp, Warning,
		TEXT("VerifyFurnitureBlockers: total=%d hole_ok=%d no_hole=%d (hole_ok=NavModifier効いてる / no_hole=効いてない)"),
		FurnitureNavBlockers.Num(), HoleOk, NoHole);
}

void UMRSpatialRecognitionSubsystem::HandleSceneLoaded(bool bSuccess)
{
	// Async版のSuccess/Failureと、保険のOnSceneLoaded直接購読が二重に来ることがある。
	// 一度「成功」を処理したら以降は無視する（失敗はリトライのため通す）。
	if (bSuccess && bSceneLoadHandled)
	{
		return;
	}

	if (bSuccess)
	{
		bSceneLoadHandled = true;
		RefreshRecognizedAnchors();

		// 部屋がロードできたら、その形に沿った透明オクルージョンメッシュを生成する。
		// これで敵が現実の壁/机/椅子の後ろに隠れる（Scene方式オクルージョン）。
		if (bAutoBuildOcclusionOnSceneLoaded)
		{
			BuildOcclusionMeshes();
		}
	}
	else
	{
		RecognizedAnchors.Reset();
		UE_LOG(LogTemp, Warning, TEXT("Spatial scene load failed"));
		LoadSceneFromDeviceDeferred();
	}

	OnSceneLoaded.Broadcast(bSuccess);
}

void UMRSpatialRecognitionSubsystem::HandleRoomChanged(AMRUKRoom* Room)
{
	RefreshRecognizedAnchors();
}

void UMRSpatialRecognitionSubsystem::HandleScenePermissionsGranted(const TArray<FString>& Permissions, const TArray<bool>& GrantResults)
{
	bool bAllGranted = true;
	for (bool bGranted : GrantResults)
	{
		bAllGranted &= bGranted;
	}

	if (!bAllGranted)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spatial scene permission denied"));
		OnSceneLoaded.Broadcast(false);
		return;
	}

	// 権限が許可されたら部屋データのロードを開始する（少し遅延させてMRUKの準備を待つ）。
	LoadSceneFromDeviceDeferred(0.2f);
}

bool UMRSpatialRecognitionSubsystem::EnsureScenePermissionOrRequest()
{
#if PLATFORM_ANDROID
	const TArray<FString> RequiredPermissions = {
		TEXT("com.oculus.permission.USE_SCENE")
	};

	TArray<FString> MissingPermissions;
	for (const FString& Permission : RequiredPermissions)
	{
		if (!UAndroidPermissionFunctionLibrary::CheckPermission(Permission))
		{
			MissingPermissions.Add(Permission);
		}
	}

	if (MissingPermissions.Num() == 0)
	{
		return true;
	}

	UE_LOG(LogTemp, Log, TEXT("Requesting spatial scene permissions"));
	ScenePermissionProxy = UAndroidPermissionFunctionLibrary::AcquirePermissions(MissingPermissions);
	if (ScenePermissionProxy)
	{
		ScenePermissionProxy->OnPermissionsGrantedDynamicDelegate.AddDynamic(this, &UMRSpatialRecognitionSubsystem::HandleScenePermissionsGranted);
	}
	return false;
#else
	return true;
#endif
}

void UMRSpatialRecognitionSubsystem::RetryLoadSceneFromDevice()
{
	if (LoadAttemptCount >= MaxLoadAttempts)
	{
		UE_LOG(LogTemp, Warning, TEXT("Spatial scene load gave up after %d attempts"), LoadAttemptCount);
		return;
	}

	UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	if (MRUKSubsystem && MRUKSubsystem->SceneLoadStatus == EMRUKInitStatus::Busy)
	{
		return;
	}

	LoadSceneFromDevice();
}

UMRUKSubsystem* UMRSpatialRecognitionSubsystem::GetMRUKSubsystem() const
{
	if (CachedMRUKSubsystem)
	{
		return CachedMRUKSubsystem;
	}

	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (!GameInstance)
	{
		return nullptr;
	}

	UMRSpatialRecognitionSubsystem* MutableThis = const_cast<UMRSpatialRecognitionSubsystem*>(this);
	MutableThis->CachedMRUKSubsystem = GameInstance->GetSubsystem<UMRUKSubsystem>();
	return MutableThis->CachedMRUKSubsystem;
}

const AMRUKRoom* UMRSpatialRecognitionSubsystem::GetPrimaryRoom() const
{
	const UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	if (!MRUKSubsystem)
	{
		return nullptr;
	}

	// MRUK が「ヘッドセットが今いる部屋」を返す。デバイスに古い部屋データが複数残っていても、
	// プレイヤーが実際にいる部屋を正として採用する（床の高さ比べで古い部屋を拾う事故を防ぐ）。
	if (const AMRUKRoom* CurrentRoom = MRUKSubsystem->GetCurrentRoom())
	{
		return CurrentRoom;
	}

	// フォールバック: GetCurrentRoom が取れない場合のみ、床アンカーが最も高い部屋を選ぶ旧ヒューリスティック。
	const AMRUKRoom* BestRoom = nullptr;
	float BestFloorZ = -TNumericLimits<float>::Max();
	for (const AMRUKRoom* Room : MRUKSubsystem->Rooms)
	{
		if (!Room)
		{
			continue;
		}

		for (const AMRUKAnchor* Floor : Room->FloorAnchors)
		{
			if (!Floor)
			{
				continue;
			}

			const float FloorZ = GetFloorSurfaceZ(Floor);
			if (FloorZ > BestFloorZ)
			{
				BestFloorZ = FloorZ;
				BestRoom = Room;
			}
		}
	}

	if (BestRoom)
	{
		return BestRoom;
	}

	for (const AMRUKRoom* Room : MRUKSubsystem->Rooms)
	{
		if (Room)
		{
			return Room;
		}
	}

	return nullptr;
}

FMRSpatialAnchorInfo UMRSpatialRecognitionSubsystem::BuildAnchorInfo(const AMRUKAnchor* Anchor) const
{
	FMRSpatialAnchorInfo Info;
	if (!Anchor)
	{
		return Info;
	}

	Info.Labels = Anchor->SemanticClassifications;
	Info.PrimaryLabel = Info.Labels.Num() > 0 ? Info.Labels[0] : TEXT("UNKNOWN");
	Info.Transform = Anchor->GetActorTransform();

	Info.bHasPlane = Anchor->PlaneBounds.bIsValid;
	if (Info.bHasPlane)
	{
		const FVector2D PlaneExtent = Anchor->PlaneBounds.GetSize();
		Info.PlaneSize = FVector(PlaneExtent.X, PlaneExtent.Y, 0.0);
	}

	Info.bHasVolume = Anchor->VolumeBounds.IsValid != 0;
	if (Info.bHasVolume)
	{
		Info.VolumeSize = Anchor->VolumeBounds.GetSize();
	}

	return Info;
}

bool UMRSpatialRecognitionSubsystem::AnchorMatchesAnyLabel(const FMRSpatialAnchorInfo& Info, const TArray<FString>& Labels) const
{
	for (const FString& Label : Labels)
	{
		if (Info.Labels.Contains(Label))
		{
			return true;
		}
	}

	return false;
}
