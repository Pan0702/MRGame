// Fill out your copyright notice in the Description page of Project Settings.

#include "MRSpatialRecognitionSubsystem.h"

#include "MRUtilityKit.h"
#include "MRUtilityKitAnchor.h"
#include "MRUtilityKitBPLibrary.h"
#include "MRUtilityKitRoom.h"
#include "MRUtilityKitSubsystem.h"
#include "Engine/Engine.h"
#include "ProceduralMeshComponent.h"
#include "TimerManager.h"

#if PLATFORM_ANDROID
#include "AndroidPermissionCallbackProxy.h"
#include "AndroidPermissionFunctionLibrary.h"
#endif

void UMRSpatialRecognitionSubsystem::Deinitialize()
{
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

	// 既にロード済みなら即成功扱い。
	if (MRUKSubsystem->SceneLoadStatus == EMRUKInitStatus::Complete)
	{
		HandleSceneLoaded(true);
		return true;
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

	UMRUKSubsystem* MRUKSubsystem = GetMRUKSubsystem();
	if (!MRUKSubsystem)
	{
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
	const float HalfRange = Spacing * (NumPoints - 1) * 0.5f;

	OutPoints.Reserve(NumPoints);
	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float Side = -HalfRange + Spacing * i;
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

bool UMRSpatialRecognitionSubsystem::MeasureFloorHeight(const FVector& FromLocation, float& OutFloorZ)
{
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
			for (const AMRUKAnchor* Floor : Room->FloorAnchors)
			{
				if (!Floor)
				{
					continue;
				}

				++FloorAnchorCount;
				const FVector AnchorLoc = Floor->GetActorLocation();
				const FString Labels = FString::Join(Floor->SemanticClassifications, TEXT(","));
				UE_LOG(LogTemp, Log,
				       TEXT("MeasureFloorHeight: FloorAnchor[%d] Z=%.1f loc=(%.1f,%.1f,%.1f) labels=[%s]"),
				       FloorAnchorCount - 1, AnchorLoc.Z, AnchorLoc.X, AnchorLoc.Y, AnchorLoc.Z, *Labels);

				// 最も高い床アンカーを採用する（低い余計なアンカーは無視）。
				if (AnchorLoc.Z > BestZ)
				{
					BestZ = AnchorLoc.Z;
					BestFloor = Floor;
				}
			}
		}

		if (BestFloor)
		{
			OutFloorZ = BestFloor->GetActorLocation().Z;
			UE_LOG(LogTemp, Log,
			       TEXT("MeasureFloorHeight: selected highest FloorAnchor Z=%.1f (of %d anchors)"),
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

	for (const AMRUKRoom* Room : MRUKSubsystem->Rooms)
	{
		if (!Room)
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

	int32 NumConfigured = 0;

	for (AMRUKRoom* Room : MRUKSubsystem->Rooms)
	{
		if (!Room)
		{
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

			// デバッグ可視化は壁/天井を除いて適用（壁/天井は常に不可視オクルージョン）。
			const bool bVisualizeThis = bDebugVisualizeMesh && !bIsWall && !bIsCeiling;
			if (bVisualizeThis)
			{
				// デバッグ: 部屋メッシュをメインパスでも描画して目視確認できるようにする。
				Mesh->SetRenderInMainPass(true);
			}
			else
			{
				// オクルージョン設定の肝:
				// - メインパス(色)では描かない → 見えない（パススルーの現実映像が透ける）
				// - 深度パスには書き込む → このメッシュより奥にある敵が深度テストで隠される
				Mesh->SetRenderInMainPass(false);
			}
			Mesh->SetRenderInDepthPass(true);
			Mesh->bRenderInDepthPass = true;
			Mesh->SetCastShadow(false);
			Mesh->SetVisibility(true); // Visibility自体はtrue（描画判断はMainPassフラグ側で行う）。

			// 床メッシュだけ NavMesh の生成対象にする（敵AIが床の上だけを歩けるように）。
			// 壁/天井/家具は床の歩行可能領域から除外したいので Nav 非対象のままにする。
			Mesh->SetCanEverAffectNavigation(bIsFloor);

			Mesh->MarkRenderStateDirty();

			++NumConfigured;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("BuildOcclusionMeshes: configured %d occlusion anchors"), NumConfigured);
	return NumConfigured;
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
