// Fill out your copyright notice in the Description page of Project Settings.


#include "GM_DemoScene.h"
#include "Enemy.h"
#include "EnemySpawner.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationInvokerComponent.h"
#include "DrawDebugHelpers.h"
#include "MRSpatialRecognitionSubsystem.h"
#include "OculusXRPassthroughLayerComponent.h"
#include "OculusXRPassthroughSubsystem.h"
#include "OculusXRPersistentPassthroughInstance.h"
#include "TimerManager.h"
#include "OculusXRFunctionLibrary.h"
#include "BlueprintStatsLibrary.h"

namespace
{
const TCHAR* GetDepthOcclusionModeName(bool bUseSoftDepthOcclusion)
{
	return bUseSoftDepthOcclusion ? TEXT("SoftOcclusions") : TEXT("HardOcclusions");
}
}

AGM_DemoScene::AGM_DemoScene()
{
	// NavMeshデバッグ描画のため Tick を有効化（既定では GameMode は Tick しない）。
	PrimaryActorTick.bCanEverTick = true;
}

void AGM_DemoScene::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDebugDrawNavMesh)
	{
		DebugDrawNavMesh();
	}

	if (bDebugDrawSpawners)
	{
		DebugDrawSpawners();
	}

	if (bDebugDrawGround)
	{
		DebugDrawGround();
	}
}

void AGM_DemoScene::BeginPlay()
{
	Super::BeginPlay();

	InitializePassthrough();
	InitializeDepthOcclusion();

	// Runtime NavMesh should be built from a stable synthetic floor aligned to the visible upper floor.
	// Existing BP assets may still carry the old default false, so force this before MRUK mesh setup.
	if (bProjectSpawnToNavMesh && !bSpawnGroundCollision)
	{
		UE_LOG(LogTemp, Warning, TEXT("BeginPlay: forcing bSpawnGroundCollision=true so NavMesh uses the visible upper floor height"));
		bSpawnGroundCollision = true;
	}

	// 敵BPクラスが1つも設定されていなければ湧かせない。BP_GM_Main の EnemyClasses に BP_Enemy を設定すること。
	if (EnemyClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemyClasses is empty. Set BP_Enemy on the GameMode (BP_GM_Main)."));
		return;
	}

	// Scene方式: 部屋スキャン/ロードの完了(OnSceneLoaded)を待ってから、壁を測りループを開始する。
	// 部屋メッシュが無いと壁/床/障害物のLineTraceが当たらないため、必ずロード後に始める。
	if (UWorld* World = GetWorld())
	{
		if (UMRSpatialRecognitionSubsystem* Spatial = World->GetSubsystem<UMRSpatialRecognitionSubsystem>())
		{
			Spatial->OnSceneLoaded.AddUniqueDynamic(this, &AGM_DemoScene::HandleSceneReady);
		}
	}

	// InitializeOcclusion 内で部屋スキャン/ロードを起動する（完了で上の HandleSceneReady が呼ばれる）。
	InitializeOcclusion();

	// 保険: 部屋ロードが SceneLoadTimeout 秒で完了しない／完了通知が来ない場合でも、
	// 強制的にループを開始して敵を出す（部屋メッシュ無しのフォールバック湧き）。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SceneLoadFallbackTimerHandle,
			this,
			&AGM_DemoScene::StartLoopFallbackIfNeeded,
			FMath::Max(0.5f, SceneLoadTimeout),
			false);
	}
}

void AGM_DemoScene::HandleSceneReady(bool bSuccess)
{
	// 既にループ開始済み（フォールバックタイマー等で）なら二重に始めない。
	if (bLoopActive)
	{
		return;
	}

	// フォールバックタイマーは不要になったので止める。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SceneLoadFallbackTimerHandle);
	}

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("HandleSceneReady: scene load failed; starting loop without room mesh (fallback spawn)"));
		StartLoop();
		return;
	}

	// 部屋メッシュが揃った。プレイヤーからXY距離が最も遠い壁を測り、接地用の床を作ってからループを開始する。
	// （関数名は CalibrateFrontWall だが、実態は「正面」ではなく「最遠壁」を選ぶ。）
	const bool bCalibrated = CalibrateFrontWallFromPlayer();
	UE_LOG(LogTemp, Log, TEXT("HandleSceneReady: room loaded. Front wall calibrated: %s"),
	       bCalibrated ? TEXT("true") : TEXT("false"));
	if (bSpawnGroundCollision)
	{
		SpawnGroundCollision();

		// MRUKアンカーはWorld Lock補正でロード後も動くため、固定床が床メッシュ（アンカー追従）から
		// 上下にズレて取り残されることがある。定期的にズレを確認し、超えたら作り直して追従させる。
		if (bRealignGroundOnDrift)
		{
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(
					GroundRealignTimerHandle,
					this,
					&AGM_DemoScene::RealignGroundCollisionIfDrifted,
					FMath::Max(0.25f, GroundRealignCheckInterval),
					true);
			}
		}
	}

	// NavMesh 投影を使うなら、MRUK 床メッシュ周囲に NavMesh を生成させる Invoker を先に置く。
	// （Spawner 配置や ResolveSpawnTransform の投影が成立する前提。SpawnWallSpawners より前に呼ぶ。）
	if (bProjectSpawnToNavMesh)
	{
		SpawnNavInvoker();
	}

	// 壁沿いに Spawner を並べる（CalibrateFrontWall 成功後でないと意味がないのでここで）。
	if (bUseWallSpawners && bCalibrated)
	{
		SpawnWallSpawners();
	}

	StartLoop();
}

void AGM_DemoScene::StartLoopFallbackIfNeeded()
{
	// 既にループが始まっていれば何もしない（部屋ロードが間に合った）。
	if (bLoopActive)
	{
		return;
	}

	UE_LOG(LogTemp, Warning,
	       TEXT("StartLoopFallbackIfNeeded: scene load did not complete within %.1fs; starting fallback loop"),
	       SceneLoadTimeout);
	StartLoop();
}

bool AGM_DemoScene::CreateEnemies()
{
	if (!bLoopActive)
	{
		return false;
	}

	if (EnemyClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("CreateEnemies: spawn failed because EnemyClasses is empty"));
		return false;
	}

	if (AliveCount >= DesiredAliveCount)
	{
		return false;
	}

	// 壁沿い Spawner 方式: 一番遠い壁沿いに並んだ Spawner を、ランダムな開始位置から巡回して試す。
	// 1 個だけ引いて失敗で諦めると、その Spawner がたまたま NavMesh 外だった時に取りこぼすため、
	// どれか 1 個が成功するまで順に試して成功率を上げる（敵の実 Spawn 位置は SpawnOne 側で NavMesh 補正）。
	if (bUseWallSpawners && WallSpawners.Num() > 0)
	{
		// 生きている Spawner だけを選ぶ。
		TArray<TObjectPtr<AEnemySpawner>> Alive;
		Alive.Reserve(WallSpawners.Num());
		for (const TObjectPtr<AEnemySpawner>& S : WallSpawners)
		{
			if (S)
			{
				Alive.Add(S);
			}
		}
		if (Alive.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("CreateEnemies: spawn failed because all WallSpawners are invalid. StoredWallSpawners=%d"), WallSpawners.Num());
			return false;
		}

		// 偏りを避けるため開始 Index だけランダムにし、そこから一巡する。
		const int32 StartIndex = FMath::RandRange(0, Alive.Num() - 1);
		for (int32 Offset = 0; Offset < Alive.Num(); ++Offset)
		{
			const int32 Index = (StartIndex + Offset) % Alive.Num();
			AEnemySpawner* Picked = Alive[Index];
			if (Picked && Picked->SpawnOne() != nullptr)
			{
				++AliveCount;
				UE_LOG(LogTemp, Log, TEXT("Enemy spawned via Spawner. AliveCount:%d DesiredAliveCount:%d"), AliveCount, DesiredAliveCount);
				return true;
			}
		}

		// 全 Spawner が失敗 = この時点では壁沿いのどこも NavMesh に乗っていない。
		// NavMesh 頂点数も併記して「NavMeshがまだ空(=生成待ち)なのか、生成済みでも壁沿いに無いのか」を切り分ける。
		// verts=0 なら NavMesh 未生成、verts>0 なのに失敗なら投影距離/位置の問題。
		UE_LOG(LogTemp, Warning,
			TEXT("CreateEnemies: spawn failed; all %d wall spawners failed this attempt. DesiredAliveCount=%d AliveCount=%d NavMeshVerts=%d"),
			Alive.Num(),
			DesiredAliveCount,
			AliveCount,
			GetNavMeshVertCount());
		return false;
	}

	// 従来方式: Depth/フォールバックで湧き位置を決め、GM が直接生成する。
	if (bUseWallSpawners)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("CreateEnemies: waiting for wall spawners; direct fallback spawn is disabled while bUseWallSpawners=true. NavMeshVerts=%d"),
			GetNavMeshVertCount());
		return false;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	if (!ResolveSpawnTransform(SpawnLocation, SpawnRotation))
	{
		UE_LOG(LogTemp, Warning, TEXT("CreateEnemies: spawn failed because ResolveSpawnTransform failed"));
		return false;
	}

	if (SpawnEnemyAt(SpawnLocation, SpawnRotation))
	{
		++AliveCount;
		UE_LOG(LogTemp, Log, TEXT("Enemy spawned. AliveCount:%d DesiredAliveCount:%d"), AliveCount, DesiredAliveCount);
		return true;
	}

	UE_LOG(LogTemp, Error,
		TEXT("CreateEnemies: spawn failed because SpawnEnemyAt failed. Location=%s Rotation=%s"),
		*SpawnLocation.ToCompactString(),
		*SpawnRotation.ToCompactString());
	return false;
}

bool AGM_DemoScene::ResolveSpawnTransform(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("ResolveSpawnTransform: failed because player pawn is null"));
		return false;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	FVector Forward = PlayerPawn->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning, TEXT("ResolveSpawnTransform: failed because player forward vector is zero. Player=%s"), *GetNameSafe(PlayerPawn));
		return false;
	}

	FVector SpawnLoc = FVector::ZeroVector;
	bool bResolvedByDepth = false;

	// Depth(MRUK Environment Raycaster)で「壁の手前かつプレイヤーとの間に障害物が無い」点を探す。
	// 部屋スキャン不要。CalibrateFrontWall済みでなければ Subsystem 側がフォールバックを返す（bUsedFallback=true）。
	if (const UWorld* World = GetWorld())
	{
		if (UMRSpatialRecognitionSubsystem* Spatial = World->GetSubsystem<UMRSpatialRecognitionSubsystem>())
		{
			FVector ClearLoc = FVector::ZeroVector;
			bool bUsedFallback = false;
			if (Spatial->FindClearSpawnPoint(PlayerLocation, Forward, ClearLoc, bUsedFallback))
			{
				SpawnLoc = ClearLoc + FVector(0.0f, 0.0f, SpawnHeightOffset);
				bResolvedByDepth = true;
			}
		}
	}

	// Depthで決められなければプレイヤー正面に固定距離＋左右ばらつきで湧かす。
	if (!bResolvedByDepth)
	{
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
		const float SideOffset = FMath::RandRange(-FallbackHorizontalSpread, FallbackHorizontalSpread);
		SpawnLoc = PlayerLocation
			+ Forward * FallbackSpawnDistance
			+ Right * SideOffset
			+ FVector(0.0f, 0.0f, SpawnHeightOffset);
	}

	// 湧き候補点を NavMesh 上に投影して、歩行可能領域（床）の外に湧かないようにする。
	// 投影できなければ「範囲外」なので、その回のスポーンはキャンセルする。
	if (bProjectSpawnToNavMesh)
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation ProjectedLoc;
			if (NavSys->ProjectPointToNavigation(SpawnLoc, ProjectedLoc, NavProjectExtent))
			{
				// 投影後のZは床面。SpawnHeightOffset は既に乗っているので投影点へ差し替える。
				if (FVector::DistSquared2D(SpawnLoc, ProjectedLoc.Location) > FMath::Square(MaxNavProjectionDistance))
				{
					UE_LOG(LogTemp, Log,
						TEXT("ResolveSpawnTransform: projected point was too far; trying nearby reachable point. Candidate=%s Projected=%s MaxNavProjectionDistance=%.1f"),
						*SpawnLoc.ToCompactString(),
						*ProjectedLoc.Location.ToCompactString(),
						MaxNavProjectionDistance);
					if (NavFallbackSearchRadius <= 0.0f ||
						!NavSys->GetRandomReachablePointInRadius(SpawnLoc, NavFallbackSearchRadius, ProjectedLoc))
					{
						UE_LOG(LogTemp, Warning,
							TEXT("ResolveSpawnTransform: spawn failed; no reachable NavMesh point near far projection. Candidate=%s NavFallbackSearchRadius=%.1f"),
							*SpawnLoc.ToCompactString(),
							NavFallbackSearchRadius);
						return false;
					}
					UE_LOG(LogTemp, Log,
						TEXT("ResolveSpawnTransform: recovered spawn on NavMesh. Candidate=%s Recovered=%s SearchRadius=%.1f"),
						*SpawnLoc.ToCompactString(),
						*ProjectedLoc.Location.ToCompactString(),
						NavFallbackSearchRadius);
				}
				SpawnLoc = ProjectedLoc.Location + FVector(0.0f, 0.0f, SpawnHeightOffset);
			}
			else
			{
				UE_LOG(LogTemp, Log,
					TEXT("ResolveSpawnTransform: candidate is off NavMesh; trying nearby reachable point. Candidate=%s NavProjectExtent=%s"),
					*SpawnLoc.ToCompactString(),
					*NavProjectExtent.ToCompactString());
				if (NavFallbackSearchRadius <= 0.0f ||
					!NavSys->GetRandomReachablePointInRadius(SpawnLoc, NavFallbackSearchRadius, ProjectedLoc))
				{
					UE_LOG(LogTemp, Warning,
						TEXT("ResolveSpawnTransform: spawn failed; no NavMesh point near candidate. Candidate=%s NavProjectExtent=%s NavFallbackSearchRadius=%.1f"),
						*SpawnLoc.ToCompactString(),
						*NavProjectExtent.ToCompactString(),
						NavFallbackSearchRadius);
					return false;
				}
				UE_LOG(LogTemp, Log,
					TEXT("ResolveSpawnTransform: recovered spawn on NavMesh. Candidate=%s Recovered=%s SearchRadius=%.1f"),
					*SpawnLoc.ToCompactString(),
					*ProjectedLoc.Location.ToCompactString(),
					NavFallbackSearchRadius);
				SpawnLoc = ProjectedLoc.Location + FVector(0.0f, 0.0f, SpawnHeightOffset);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ResolveSpawnTransform: spawn failed because NavigationSystem is unavailable"));
			return false;
		}
	}

	OutLocation = SpawnLoc;
	OutRotation = (PlayerLocation - SpawnLoc).GetSafeNormal2D().Rotation();
	return true;
}

AEnemy* AGM_DemoScene::SpawnEnemyAt(const FVector& Location, const FRotator& Rotation)
{
	UWorld* World = GetWorld();
	if (!World || EnemyClasses.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnEnemyAt: failed because World or EnemyClasses is invalid. World=%s EnemyClasses=%d"),
			World ? TEXT("valid") : TEXT("null"),
			EnemyClasses.Num());
		return nullptr;
	}

	const int32 Index = FMath::RandRange(0, EnemyClasses.Num() - 1);
	const TSubclassOf<AEnemy> PickedClass = EnemyClasses[Index];
	if (!PickedClass)
	{
		UE_LOG(LogTemp, Error, TEXT("EnemyClasses[%d] is null"), Index);
		return nullptr;
	}

	FActorSpawnParameters Params;
	// 壁際/障害物近くでも湧かせるよう、衝突しても調整して生成する。
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AEnemy* Enemy = World->SpawnActor<AEnemy>(PickedClass, Location, Rotation, Params);
	if (!Enemy)
	{
		UE_LOG(LogTemp, Error,
			TEXT("SpawnEnemyAt: SpawnActor failed. Class=%s Location=%s Rotation=%s"),
			*GetNameSafe(PickedClass.Get()),
			*Location.ToCompactString(),
			*Rotation.ToCompactString());
		return nullptr;
	}

	// 渡された Location は床面の高さ(Z)。Characterのカプセル中心は床から HalfHeight 上にあるので、
	// その分だけ持ち上げて足が床に接地するようにする（カプセルが床にめり込まないように）。
	if (const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent())
	{
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		FVector Adjusted = Enemy->GetActorLocation();
		Adjusted.Z = Location.Z + HalfHeight;
		Enemy->SetActorLocation(Adjusted, false, nullptr, ETeleportType::TeleportPhysics);
	}

	return Enemy;
}

void AGM_DemoScene::NotifyEnemyKilled()
{
	if (!bLoopActive)
	{
		return;
	}

	AliveCount = FMath::Max(0, AliveCount - 1);
	++TotalKills;

	// Spawn can fail transiently while NavMesh/spawner candidates settle, so use the same retry path as initial fill.
	if (AliveCount < DesiredAliveCount)
	{
		MaintainDesiredAliveCount();
	}
}

void AGM_DemoScene::DestoroyEnemies()
{
	NotifyEnemyKilled();
}

void AGM_DemoScene::InitializePassthrough()
{
	UOculusXRPassthroughSubsystem* PassthroughSubsystem = UOculusXRPassthroughSubsystem::GetPassthroughSubsystem(GetWorld());
	if (!PassthroughSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Passthrough subsystem is not available"));
		return;
	}

	// レイヤー自体は常に生成し、起動時の可視状態だけ bInitializePassthrough で決める。
	// こうしておくことで、後からコンソールコマンドでいつでもON/OFFを切り替えられる。
	FOculusXRPersistentPassthroughParameters Parameters;
	Parameters.bVisible = bInitializePassthrough;
	Parameters.Priority = -1;
	Parameters.Shape = NewObject<UOculusXRStereoLayerShapeReconstructed>(this);
	if (Parameters.Shape)
	{
		Parameters.Shape->LayerOrder = PassthroughLayerOrder_Underlay;
		Parameters.Shape->TextureOpacityFactor = 1.0f;
		Parameters.Shape->bEnableEdgeColor = false;
		Parameters.Shape->bEnableColorMap = false;
		Parameters.ApplyShape();
	}

	const FOculusXRPassthrough_LayerResumed_Single LayerResumed;
	PassthroughSubsystem->InitializePersistentPassthrough(Parameters, LayerResumed);
}

void AGM_DemoScene::InitializeDepthOcclusion()
{
	if (!bEnableDepthOcclusion)
	{
		UOculusXRFunctionLibrary::SetXROcclusionsMode(this, EOculusXROcclusionsMode::Disabled);
		UOculusXRFunctionLibrary::StopEnvironmentDepth();
		UE_LOG(LogTemp, Log, TEXT("DepthOcclusion: disabled"));
		return;
	}

	UOculusXRFunctionLibrary::StartEnvironmentDepth();

	const EOculusXROcclusionsMode Mode = bUseSoftDepthOcclusion
		? EOculusXROcclusionsMode::SoftOcclusions
		: EOculusXROcclusionsMode::HardOcclusions_Deprecated;
	UOculusXRFunctionLibrary::SetXROcclusionsMode(this, Mode);

	UE_LOG(LogTemp, Log, TEXT("DepthOcclusion: StartEnvironmentDepth requested. Mode=%s"),
	       GetDepthOcclusionModeName(bUseSoftDepthOcclusion));

	if (UWorld* World = GetWorld())
	{
		FTimerHandle DepthStatusTimerHandle;
		World->GetTimerManager().SetTimer(
			DepthStatusTimerHandle,
			this,
			&AGM_DemoScene::LogDepthOcclusionStatus,
			1.0f,
			false);
	}
}

void AGM_DemoScene::LogDepthOcclusionStatus()
{
	UE_LOG(LogTemp, Log, TEXT("DepthOcclusion: IsEnvironmentDepthStarted=%s Mode=%s"),
	       UOculusXRFunctionLibrary::IsEnvironmentDepthStarted() ? TEXT("true") : TEXT("false"),
	       bEnableDepthOcclusion ? GetDepthOcclusionModeName(bUseSoftDepthOcclusion) : TEXT("Disabled"));
}

void AGM_DemoScene::InitializeOcclusion()
{
	if (!bEnableOcclusion)
	{
		return;
	}

	// Scene/MRUK は部屋形状、スポーン、NavMesh のためにロードする。
	// 見た目の遮蔽は InitializeDepthOcclusion の Depth API 側で行う。
	if (UWorld* World = GetWorld())
	{
		if (UMRSpatialRecognitionSubsystem* Spatial = World->GetSubsystem<UMRSpatialRecognitionSubsystem>())
		{
			// デバッグ可視化の設定を、部屋ロード（→BuildOcclusionMeshes）が走る前に渡す。
			Spatial->bDebugVisualizeMesh = bDebugVisualizeRoomMesh;
			Spatial->DebugMeshMaterial = DebugRoomMeshMaterial;

			// 固定床(SpawnGroundCollision)を使う場合は、MRUK床メッシュを Nav 非対象にして
			// NavMesh の二重生成と World Lock ドリフトによる NavMesh の揺れを防ぐ。
			// 固定床を使わない場合のみ MRUK床メッシュを Nav 面にする。
			Spatial->bFloorMeshAffectsNavigation = !bSpawnGroundCollision;

			// 家具をNavMeshの穴にするかをBP(GameMode)から制御できるようにSubsystemへ流し込む。
			// 机だらけの部屋では穴でNavMeshが寸断され敵がプレイヤーへ到達できなくなるため、
			// まずは無効化して接続性を確認できるようにする。
			Spatial->bFurnitureBlocksNavigation = bFurnitureBlocksNavigation;

			if (bScanRoomOnStart)
			{
				// 起動毎にアプリ内から部屋スキャン（スペース設定）を起動する。
				// スキャン完了後、OnCaptureComplete→LoadSceneFromDevice→OnSceneLoaded 経由で
				// BuildOcclusionMeshes が自動的に走る。
				Spatial->LaunchRoomScan();
				UE_LOG(LogTemp, Log, TEXT("Occlusion: launched in-app room scan"));
			}
			else
			{
				// スキャンせず、保存済みの部屋データをロードして使う。
				Spatial->LoadSceneFromDevice();
				UE_LOG(LogTemp, Log, TEXT("Occlusion: requested MRUK scene load (no scan)"));
			}
		}
	}
}

void AGM_DemoScene::SetPassthroughEnabled(bool bEnabled)
{
	UOculusXRPassthroughSubsystem* PassthroughSubsystem = UOculusXRPassthroughSubsystem::GetPassthroughSubsystem(GetWorld());
	if (!PassthroughSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Passthrough subsystem is not available"));
		return;
	}

	UOculusXRPersistentPassthroughInstance* Instance = PassthroughSubsystem->GetPersistentPassthrough();
	if (!Instance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Passthrough instance is not available"));
		return;
	}

	Instance->SetVisible(bEnabled);
	UE_LOG(LogTemp, Log, TEXT("Passthrough visibility set to %s"), bEnabled ? TEXT("true") : TEXT("false"));
}

void AGM_DemoScene::TogglePassthrough()
{
	UOculusXRPassthroughSubsystem* PassthroughSubsystem = UOculusXRPassthroughSubsystem::GetPassthroughSubsystem(GetWorld());
	if (!PassthroughSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Passthrough subsystem is not available"));
		return;
	}

	UOculusXRPersistentPassthroughInstance* Instance = PassthroughSubsystem->GetPersistentPassthrough();
	if (!Instance)
	{
		UE_LOG(LogTemp, Warning, TEXT("Passthrough instance is not available"));
		return;
	}

	SetPassthroughEnabled(!Instance->IsVisible());
}

void AGM_DemoScene::StopSpawning()
{
	bLoopActive = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpawnRetryTimerHandle);
		World->GetTimerManager().ClearTimer(WallSpawnerRetryTimerHandle);
	}
	UE_LOG(LogTemp, Log, TEXT("GM: StopSpawning - spawn loop halted"));
}

void AGM_DemoScene::CountAllBlueprintNodes()
{
	// エディタのコンソールから実行されるラッパー。集計の実体はライブラリ側（エディタ限定）。
	// 結果は UBlueprintStatsLibrary 側の UE_LOG（LogTemp Warning「Total BP nodes: ...」）に出る。
	const int32 Total = UBlueprintStatsLibrary::CountAllBlueprintNodes();
	UE_LOG(LogTemp, Warning, TEXT("CountAllBlueprintNodes (exec): Total BP nodes = %d"), Total);
}

void AGM_DemoScene::StartLoop()
{
	AliveCount = 0;
	TotalKills = 0;
	bLoopActive = true;

	MaintainDesiredAliveCount();
}

void AGM_DemoScene::MaintainDesiredAliveCount()
{
	if (!bLoopActive)
	{
		return;
	}

	// 足りない数ぶん湧かす。各体で位置決めが失敗する場合に備え、余裕をもって試行する。
	const int32 MissingCount = FMath::Max(0, DesiredAliveCount - AliveCount);
	const int32 MaxAttempts = MissingCount * 3;

	for (int32 Attempt = 0; Attempt < MaxAttempts && AliveCount < DesiredAliveCount; ++Attempt)
	{
		CreateEnemies();
	}

	// まだ足りない場合 = この瞬間は湧き位置が NavMesh に乗っていない可能性が高い。
	// MR空間の Runtime NavMesh は Invoker 起動から生成までに数フレーム〜数百ms遅れるため、
	// 1フレームで諦めず、短間隔のタイマーで生成完了を待って再試行する（敵が出るまで粘る）。
	if (bLoopActive && AliveCount < DesiredAliveCount && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			SpawnRetryTimerHandle,
			this,
			&AGM_DemoScene::MaintainDesiredAliveCount,
			SpawnRetryInterval,
			false);
	}
	else if (GetWorld())
	{
		// 充足したらリトライタイマーは止める。
		GetWorld()->GetTimerManager().ClearTimer(SpawnRetryTimerHandle);
	}
}

void AGM_DemoScene::SpawnWallSpawners()
{
	UWorld* World = GetWorld();
	UMRSpatialRecognitionSubsystem* Spatial = World ? World->GetSubsystem<UMRSpatialRecognitionSubsystem>() : nullptr;
	if (!World || !Spatial)
	{
		return;
	}

	// 既に生成済みなら作り直し（部屋が変わった可能性に備えて）。
	for (AEnemySpawner* Old : WallSpawners)
	{
		if (Old)
		{
			Old->Destroy();
		}
	}
	WallSpawners.Reset();

	TArray<FVector> Points;
	FVector WallInward = FVector::ZeroVector;
	if (!Spatial->GetSpawnPointsAlongFarthestWall(SpawnerCount, SpawnerSpacing, Points, WallInward))
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnWallSpawners: GetSpawnPointsAlongFarthestWall failed"));
		return;
	}

	TSubclassOf<AEnemySpawner> ClassToSpawn = SpawnerClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AEnemySpawner::StaticClass();
	}
	// 壁の内向き法線方向＝部屋内側 → プレイヤー側を向くように回転を作る。
	const FRotator Rot = WallInward.Rotation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// bProjectSpawnToNavMesh が false のときは「湧けるか」を判定する床/Navが無いので、
	// 従来どおり全候補に Spawner を置く（フォールバック動作）。
	const UNavigationSystemV1* NavSys = bProjectSpawnToNavMesh ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (bProjectSpawnToNavMesh && !NavSys)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnWallSpawners: NavigationSystem unavailable"));
		return;
	}

	// 検証に使う敵カプセル寸法を、実際に湧かす敵クラスの CDO から取る（BPごとに違うため）。
	const float CapsuleRadius = GetEnemyCapsuleRadius();
	const float CapsuleHalfHeight = GetEnemyCapsuleHalfHeight();
	const FVector WallRight = FVector::CrossProduct(FVector::UpVector, WallInward).GetSafeNormal2D();

	int32 NumRejectedOffNav = 0;
	int32 NumRejectedBlocked = 0;
	int32 NumRelocatedToClearSpace = 0;
	for (const FVector& P : Points)
	{
		FVector SpawnPoint = P + FVector(0.0f, 0.0f, SpawnHeightOffset);

		if (NavSys)
		{
			// 最遠壁の正面が机などで塞がれていても SpawnerCount=1 で詰まらないように、
			// 基準点を最優先に「壁沿いの左右 → 少し室内側」の順で空き地点を探す。
			// 各候補は必ず NavMesh 投影と敵カプセルの空間チェックを通すため、床外や家具内には置かない。
			bool bFoundClearPoint = false;
			int32 ResolvedSideStep = 0;
			int32 ResolvedInwardStep = 0;
			FNavLocation ResolvedNavLoc;

			auto TryCandidate = [&](const FVector& Candidate, int32 SideStep, int32 InwardStep) -> bool
			{
				FNavLocation ProjectedLoc;
				const bool bProjected = NavSys->ProjectPointToNavigation(Candidate, ProjectedLoc, NavProjectExtent);
				const bool bProjectedTooFar = bProjected &&
					FVector::DistSquared2D(Candidate, ProjectedLoc.Location) > FMath::Square(MaxNavProjectionDistance);
				if (!bProjected || bProjectedTooFar)
				{
					++NumRejectedOffNav;
					return false;
				}

				const FVector CapsuleCenter = ProjectedLoc.Location + FVector(0.0f, 0.0f, CapsuleHalfHeight);
				if (!CanEnemyFitAt(CapsuleCenter, CapsuleRadius, CapsuleHalfHeight))
				{
					++NumRejectedBlocked;
					return false;
				}

				ResolvedNavLoc = ProjectedLoc;
				ResolvedSideStep = SideStep;
				ResolvedInwardStep = InwardStep;
				return true;
			};

			const int32 MaxInwardSteps = FMath::Max(0, WallSpawnerMaxInwardSearchSteps);
			const int32 MaxSideSteps = WallRight.IsNearlyZero()
				? 0
				: FMath::Max(0, WallSpawnerMaxSideSearchSteps);
			for (int32 InwardStep = 0; InwardStep <= MaxInwardSteps && !bFoundClearPoint; ++InwardStep)
			{
				const FVector InwardBase = P + WallInward * (WallSpawnerInwardSearchStep * InwardStep);

				// まず壁前の中央を試し、その後 +右/-左を近い順に交互に試す。
				bFoundClearPoint = TryCandidate(InwardBase, 0, InwardStep);
				for (int32 SideStep = 1; SideStep <= MaxSideSteps && !bFoundClearPoint; ++SideStep)
				{
					const FVector SideOffset = WallRight * (WallSpawnerSideSearchStep * SideStep);
					bFoundClearPoint = TryCandidate(InwardBase + SideOffset, SideStep, InwardStep);
					if (!bFoundClearPoint)
					{
						bFoundClearPoint = TryCandidate(InwardBase - SideOffset, -SideStep, InwardStep);
					}
				}
			}

			if (!bFoundClearPoint)
			{
				continue;
			}

			if (ResolvedSideStep != 0 || ResolvedInwardStep != 0)
			{
				++NumRelocatedToClearSpace;
				UE_LOG(LogTemp, Log,
					TEXT("SpawnWallSpawners: moved blocked base point to clear NavMesh point. Base=%s Resolved=%s SideStep=%d InwardStep=%d"),
					*P.ToCompactString(),
					*ResolvedNavLoc.Location.ToCompactString(),
					ResolvedSideStep,
					ResolvedInwardStep);
			}

			SpawnPoint = ResolvedNavLoc.Location + FVector(0.0f, 0.0f, SpawnHeightOffset);
		}

		AEnemySpawner* S = World->SpawnActor<AEnemySpawner>(ClassToSpawn, SpawnPoint, Rot, Params);
		if (S)
		{
			// GMの EnemyClasses を Spawner に流し込む（C++生成時はBPデフォルトでは無いため）。
			S->SetEnemyClasses(EnemyClasses);
			WallSpawners.Add(S);
		}
	}
	UE_LOG(LogTemp, Log,
		TEXT("SpawnWallSpawners: placed %d/%d valid spawners along farthest wall (relocated-to-clear-space=%d, rejected candidates: off-NavMesh=%d, blocked/no-space=%d)"),
		WallSpawners.Num(), Points.Num(), NumRelocatedToClearSpace, NumRejectedOffNav, NumRejectedBlocked);

	// 検証で全候補を弾いて 0 個になった場合 = この瞬間はまだ NavMesh タイルが生成されていない
	// （Invoker 起動から生成まで数フレーム〜数百ms遅れる）か、壁前に空間が無い可能性が高い。
	// NavMesh 生成を待って配置をリトライする（敵が出る Spawner が出来るまで粘る）。
	// 1個でも置けたらリトライは止める。
	if (bProjectSpawnToNavMesh && WallSpawners.Num() == 0)
	{
		++WallSpawnerRetryCount;
		if (WallSpawnerRetryCount <= MaxWallSpawnerRetries && World)
		{
			UE_LOG(LogTemp, Log,
				TEXT("SpawnWallSpawners: 0 valid spawners (NavMesh likely not ready). Retrying (%d/%d) in %.2fs"),
				WallSpawnerRetryCount, MaxWallSpawnerRetries, SpawnRetryInterval);
			World->GetTimerManager().SetTimer(
				WallSpawnerRetryTimerHandle,
				this,
				&AGM_DemoScene::SpawnWallSpawners,
				SpawnRetryInterval,
				false);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("SpawnWallSpawners: gave up after %d retries; no valid spawner placement along farthest wall"),
				WallSpawnerRetryCount);
		}
	}
	else if (World)
	{
		WallSpawnerRetryCount = 0;
		World->GetTimerManager().ClearTimer(WallSpawnerRetryTimerHandle);
	}
}

float AGM_DemoScene::GetEnemyCapsuleRadius() const
{
	for (const TSubclassOf<AEnemy>& Cls : EnemyClasses)
	{
		if (const AEnemy* CDO = Cls ? Cls->GetDefaultObject<AEnemy>() : nullptr)
		{
			if (const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
			{
				return Capsule->GetScaledCapsuleRadius();
			}
		}
	}
	// CDO から取れない場合の保険（ACharacter 既定値）。
	return 34.0f;
}

float AGM_DemoScene::GetEnemyCapsuleHalfHeight() const
{
	for (const TSubclassOf<AEnemy>& Cls : EnemyClasses)
	{
		if (const AEnemy* CDO = Cls ? Cls->GetDefaultObject<AEnemy>() : nullptr)
		{
			if (const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
			{
				return Capsule->GetScaledCapsuleHalfHeight();
			}
		}
	}
	return 88.0f;
}

bool AGM_DemoScene::CanEnemyFitAt(const FVector& CapsuleCenter, float Radius, float HalfHeight) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 壁/床/家具のオクルージョンメッシュ（WorldStatic）に敵カプセルが重ならないか調べる。
	// 重なる＝その点では敵がメッシュに埋まる → Spawner を置かない。
	// 床自体(WorldStatic)に半径ぶん触れても埋まり扱いにならないよう、底面を少し上げて判定する。
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(
		FMath::Max(1.0f, Radius - SpawnFitClearance),
		FMath::Max(1.0f, HalfHeight - SpawnFitClearance));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpawnFitTest), /*bTraceComplex=*/false);

	const bool bBlocked = World->OverlapBlockingTestByChannel(
		CapsuleCenter,
		FQuat::Identity,
		ECC_WorldStatic,
		Capsule,
		QueryParams);

	return !bBlocked;
}

void AGM_DemoScene::DebugDrawNavMesh() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return;
	}

	// const版（引数なし）の getter を使う。MainNavData を返すだけで生成はしない。
	const ANavigationData* NavData = NavSys->GetDefaultNavDataInstance();
	if (!NavData)
	{
		return;
	}

	const ARecastNavMesh* RecastNav = Cast<ARecastNavMesh>(NavData);
	if (!RecastNav)
	{
		return;
	}

	// NavMeshの歩行可能ポリゴンを三角形メッシュとして取得し、緑のワイヤーで描く。
	// 床(Floorアンカー)だけが Nav 対象なので、緑の面＝敵が歩ける範囲。
	// 無効な FNavTileRef を渡すと全タイル分のジオメトリをまとめて集める（API仕様）。
	FRecastDebugGeometry Geometry;
	RecastNav->GetDebugGeometryForTile(Geometry, FNavTileRef());

	const TArray<FVector>& Verts = Geometry.MeshVerts;
	const TArray<int32>& Indices = Geometry.AreaIndices[RECAST_DEFAULT_AREA];
	// 床面に貼りつくと見にくいので少し持ち上げて描画する。
	// MR/パススルーでは細線(0.5)は深度負け・視認困難なので太くして実機で見えるようにする。
	const FVector Lift(0.0f, 0.0f, 5.0f);
	const FColor NavColor = FColor::Green;
	const float NavLineThickness = 3.0f;
	const int32 TriCount = Indices.Num() / 3;

	// 診断: 取得した頂点数・歩行可能三角形数をログ。
	// verts=0 → NavMesh のデータが無い（描く三角形ゼロ＝緑線が出ないのは当然）。
	// verts>0 なのに実機で緑が見えない → 描画はされているが見える位置/設定の問題（高さ・線の太さ・パススルー深度）。
	if (Verts.Num() == 0 || TriCount == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DebugDrawNavMesh: NavMesh has NO geometry yet (verts=%d tris=%d). 非同期ビルド未完 or NavMesh未生成。"),
			Verts.Num(), TriCount);
		return;
	}

	// 描いた範囲(バウンディング)も出すと「緑がどの高さ/位置にあるか」が分かる（足元70cm下に埋もれている等の切り分け）。
	FBox NavBounds(ForceInit);
	for (const FVector& V : Verts)
	{
		NavBounds += V;
	}
	UE_LOG(LogTemp, Warning,
		TEXT("DebugDrawNavMesh: drawing verts=%d tris=%d boundsMin=%s boundsMax=%s thickness=%.1f lift=%.0f"),
		Verts.Num(), TriCount,
		*NavBounds.Min.ToCompactString(), *NavBounds.Max.ToCompactString(),
		NavLineThickness, Lift.Z);

	for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
	{
		if (!Verts.IsValidIndex(Indices[i]) || !Verts.IsValidIndex(Indices[i + 1]) || !Verts.IsValidIndex(Indices[i + 2]))
		{
			continue;
		}
		const FVector A = Verts[Indices[i]] + Lift;
		const FVector B = Verts[Indices[i + 1]] + Lift;
		const FVector C = Verts[Indices[i + 2]] + Lift;
		DrawDebugLine(World, A, B, NavColor, false, -1.0f, 0, NavLineThickness);
		DrawDebugLine(World, B, C, NavColor, false, -1.0f, 0, NavLineThickness);
		DrawDebugLine(World, C, A, NavColor, false, -1.0f, 0, NavLineThickness);
		// 頂点に点も打つ（線が見えづらくても点群でNavMeshの位置/高さが掴める）。
		DrawDebugPoint(World, A, 6.0f, FColor::Cyan, false, -1.0f, 0);
	}
}

void AGM_DemoScene::DebugDrawSpawners() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 各 WallSpawner の位置を可視化する。Spawner が壁沿いに並んでいるか目視確認するため。
	// - 黄の球   : Spawner の配置位置（壁沿いの意図位置）。
	// - 水色の矢印: Spawner の向き（プレイヤー側を向くはず＝壁の内向き法線）。
	// - 赤の球   : Spawner 配列に居るが無効(null)になったスロット（数の食い違い確認用、通常は出ない）。
	int32 ValidCount = 0;
	for (const TObjectPtr<AEnemySpawner>& S : WallSpawners)
	{
		if (!S)
		{
			continue;
		}
		++ValidCount;

		const FVector Loc = S->GetActorLocation();
		DrawDebugSphere(World, Loc, 12.0f, 12, FColor::Yellow, false, -1.0f, 0, 1.0f);
		// 向き（プレイヤー側）を矢印で。
		const FVector Forward = S->GetActorForwardVector();
		DrawDebugDirectionalArrow(World, Loc, Loc + Forward * 40.0f, 8.0f, FColor::Cyan, false, -1.0f, 0, 1.0f);
		// 床から立ち上がる縦線（床に埋もれても位置が見えるように）。
		DrawDebugLine(World, Loc, Loc + FVector(0.0f, 0.0f, 60.0f), FColor::Yellow, false, -1.0f, 0, 1.0f);
	}

	// 配置数の食い違いがあれば、プレイヤー足元付近に件数を文字で出す（HMDで読める位置）。
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector TextLoc = PlayerPawn->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
		const FString Msg = FString::Printf(TEXT("WallSpawners valid=%d / stored=%d"), ValidCount, WallSpawners.Num());
		DrawDebugString(World, TextLoc, Msg, nullptr, FColor::White, 0.0f, false);
	}
}

void AGM_DemoScene::DebugDrawGround() const
{
	const UWorld* World = GetWorld();
	if (!World || !GroundActor)
	{
		return;
	}

	const UBoxComponent* Box = Cast<UBoxComponent>(GroundActor->GetRootComponent());
	if (!Box)
	{
		return;
	}

	const FVector Center = Box->GetComponentLocation();
	const FVector Extent = Box->GetScaledBoxExtent();

	// 床コリジョンBox全体を水色のワイヤーで描く（厚みも見える）。
	DrawDebugBox(World, Center, Extent, FQuat::Identity, FColor::Cyan, false, -1.0f, 0, 1.0f);

	// 敵が立つ「床天面」を緑の矩形で強調する（中心Z + 厚み半分）。
	const float TopZ = Center.Z + Extent.Z;
	const FVector C0(Center.X - Extent.X, Center.Y - Extent.Y, TopZ);
	const FVector C1(Center.X + Extent.X, Center.Y - Extent.Y, TopZ);
	const FVector C2(Center.X + Extent.X, Center.Y + Extent.Y, TopZ);
	const FVector C3(Center.X - Extent.X, Center.Y + Extent.Y, TopZ);
	DrawDebugLine(World, C0, C1, FColor::Green, false, -1.0f, 0, 2.0f);
	DrawDebugLine(World, C1, C2, FColor::Green, false, -1.0f, 0, 2.0f);
	DrawDebugLine(World, C2, C3, FColor::Green, false, -1.0f, 0, 2.0f);
	DrawDebugLine(World, C3, C0, FColor::Green, false, -1.0f, 0, 2.0f);

	// サイズ・位置を文字で出す（HMDで読める高さに）。
	const FString Msg = FString::Printf(TEXT("Ground top=%.0f size=%.0fx%.0fcm center=(%.0f,%.0f)"),
		TopZ, Extent.X * 2.0f, Extent.Y * 2.0f, Center.X, Center.Y);
	DrawDebugString(World, FVector(Center.X, Center.Y, TopZ + 20.0f), Msg, nullptr, FColor::Cyan, 0.0f, false);
}

bool AGM_DemoScene::CalibrateFrontWallFromPlayer()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UMRSpatialRecognitionSubsystem* Spatial = World->GetSubsystem<UMRSpatialRecognitionSubsystem>();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Spatial || !PlayerPawn)
	{
		return false;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	// 最遠壁の選択はプレイヤー位置のXY距離だけで決まり、向き(Forward)は使わない。
	// （API互換のため引数は渡すが、Subsystem 側はフォールバック時以外 Forward を参照しない。）
	FVector Forward = PlayerPawn->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();

	return Spatial->CalibrateFrontWall(PlayerLocation, Forward);
}

void AGM_DemoScene::SpawnGroundCollision()
{
	UWorld* World = GetWorld();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !PlayerPawn)
	{
		return;
	}

	// 既に作ってあれば作り直さない。
	if (GroundActor)
	{
		return;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();

	// 床の中心・サイズを床アンカーから取る。これにより「床アンカーぴったりの固定平面」を1枚敷ける。
	// MRUK の床メッシュ(World Lockで上下ドリフトする)に頼らず、自前の固定コリジョン床を NavMesh 土台にする。
	// 床アンカーが取れなければ従来どおりプレイヤー足元中心＋GroundHalfExtent四方にフォールバック。
	FVector GroundCenterXY = PlayerLocation;
	FVector2D HalfXY(GroundHalfExtent, GroundHalfExtent);
	float FloorZ = PlayerLocation.Z;
	if (UMRSpatialRecognitionSubsystem* Spatial = World->GetSubsystem<UMRSpatialRecognitionSubsystem>())
	{
		FVector FloorCenter = FVector::ZeroVector;
		FVector2D FloorHalfXY = FVector2D::ZeroVector;
		if (Spatial->GetFloorRect(FloorCenter, FloorHalfXY))
		{
			GroundCenterXY = FloorCenter;
			FloorZ = FMath::Max(FloorCenter.Z, PlayerLocation.Z);
			const FVector2D FloorBasedHalfXY = FloorHalfXY + FVector2D(20.0f, 20.0f);
			// 床アンカー実寸が取れたら「部屋の床ぴったり＋余白」にする。
			// 以前は GroundHalfExtent(10m) を最低サイズにしていたが、それだと NavMesh 床が
			// 部屋よりはるかに大きくなり、壁の外側の点でも「NavMeshに乗る」検証に合格して
			// Spawner/敵が部屋の外に立ててしまう（壁メッシュはNav非対象なのでNavMesh上は素通し）。
			// 実寸が異常に小さい場合（過去にX=30cmに潰れるバグがあった）だけ1mで下支えする。
			HalfXY = FVector2D(
				FMath::Max(100.0f, FloorBasedHalfXY.X),
				FMath::Max(100.0f, FloorBasedHalfXY.Y));
		}
		else
		{
			// 床矩形が取れない場合は高さだけ測る。
			float MeasuredZ = 0.0f;
			if (Spatial->MeasureFloorHeight(PlayerLocation, MeasuredZ))
			{
				FloorZ = MeasuredZ;
			}
		}

		float MeasuredFloorZ = 0.0f;
		if (Spatial->MeasureFloorHeight(PlayerLocation, MeasuredFloorZ))
		{
			if (!FMath::IsNearlyEqual(FloorZ, MeasuredFloorZ, 1.0f))
			{
				UE_LOG(LogTemp, Warning,
					TEXT("SpawnGroundCollision: overriding floor Z from %.1f to measured upper floor %.1f"),
					FloorZ,
					MeasuredFloorZ);
			}
			FloorZ = MeasuredFloorZ;
		}
	}

	// 床アンカー実測を最終値として採用する。見えている床メッシュ（アンカー追従）と固定床を
	// 一致させるため、ポーンZへのクランプ（持ち上げ）はしない。極端に低い場合だけ診断用に警告。
	if (FloorZ < PlayerLocation.Z - 30.0f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SpawnGroundCollision: floor anchor Z %.1f is far below player Z %.1f (using anchor Z as-is)"),
			FloorZ,
			PlayerLocation.Z);
	}

	// 床天面が FloorZ に来るよう、中心を厚みの半分だけ下げる。
	const FVector GroundCenter(GroundCenterXY.X, GroundCenterXY.Y, FloorZ - GroundThickness);

	// GetFloorRect が4隅のワールドXY min/max から無回転の矩形を返すので、Boxも無回転で置く。
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GroundActor = World->SpawnActor<AActor>(AActor::StaticClass(), GroundCenter, FRotator::ZeroRotator, Params);
	if (!GroundActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnGroundCollision: failed to spawn ground actor"));
		return;
	}

	// 見えないコリジョンBoxを付ける（描画なし・コリジョンのみ）。サイズは床アンカー実寸に合わせる。
	// ※ プロパティは RegisterComponent の「前」に設定する。RegisterComponent 時点で
	//    NavMesh への登録(エクスポート)が行われるため、後から SetBoxExtent や
	//    SetCanEverAffectNavigation を変えても NavMesh に反映されず、NavMesh が生成されない
	//    （実機ログで verts=0 のままだった原因）。
	UBoxComponent* Box = NewObject<UBoxComponent>(GroundActor);
	if (!Box)
	{
		return;
	}
	GroundActor->SetRootComponent(Box);
	// ※ SpawnActor に渡した座標は「ルート無しの空アクター」では捨てられる（適用先が無い）。
	//   後付けしたルートBoxは原点(0,0,0)に居るため、登録前に明示的に床位置へ置く。
	//   （これを怠ると、床の高さ計算が正しくても箱は常に原点＝NavMeshがZ=厚み位置に張られる。）
	Box->SetRelativeLocation(GroundCenter);
	// 静的配置（動かさない）として登録する。
	Box->SetMobility(EComponentMobility::Static);
	Box->SetBoxExtent(FVector(FMath::Max(10.0f, HalfXY.X), FMath::Max(10.0f, HalfXY.Y), GroundThickness));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	// この固定床を NavMesh の歩行可能面にする。MRUK 床メッシュ側は Nav 非対象にして二重を避ける。
	Box->SetCanEverAffectNavigation(true);
	Box->SetHiddenInGame(true); // 見せない（パススルーの現実床が見えている）。
	// プロパティ設定後に登録 → この時点の正しい extent/nav設定で NavMesh にエクスポートされる。
	Box->RegisterComponent();

	// 明示的に NavMesh の再生成を促す（床Boxを歩行面として焼き直す）。
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		NavSys->UpdateComponentInNavOctree(*Box);
	}

	UE_LOG(LogTemp, Warning, TEXT("SpawnGroundCollision: fixed ground at center=%s halfXY=(%.1f,%.1f) Z=%.1f (no rotation)"),
	       *GroundCenter.ToCompactString(), HalfXY.X, HalfXY.Y, FloorZ);
}

void AGM_DemoScene::RealignGroundCollisionIfDrifted()
{
	if (!bSpawnGroundCollision)
	{
		return;
	}

	UWorld* World = GetWorld();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !PlayerPawn)
	{
		return;
	}

	// 何らかの理由でまだ床が無ければ作るだけ。
	if (!GroundActor)
	{
		SpawnGroundCollision();
		return;
	}

	// 現在の床アンカー高さ（アンカー追従の床メッシュと同じ高さ）を取る。
	// アンカーが取れない場合のLineTraceフォールバックは固定床自身に当たり得るため、比較しない。
	UMRSpatialRecognitionSubsystem* Spatial = World->GetSubsystem<UMRSpatialRecognitionSubsystem>();
	float FloorZ = 0.0f;
	if (!Spatial || !Spatial->MeasureFloorHeight(PlayerPawn->GetActorLocation(), FloorZ))
	{
		return;
	}

	// 固定床の天面Z（Box中心Z＋厚み半分）。
	float GroundTopZ = GroundActor->GetActorLocation().Z + GroundThickness;
	if (const UBoxComponent* Box = Cast<UBoxComponent>(GroundActor->GetRootComponent()))
	{
		GroundTopZ = Box->GetComponentLocation().Z + Box->GetScaledBoxExtent().Z;
	}

	const float DriftCm = FMath::Abs(GroundTopZ - FloorZ);
	if (DriftCm <= GroundRealignToleranceCm)
	{
		return;
	}

	// World Lock補正でアンカーが動き、固定床が取り残された。作り直して床メッシュに追従させる。
	UE_LOG(LogTemp, Warning,
	       TEXT("RealignGroundCollisionIfDrifted: floor anchor Z=%.1f vs ground top Z=%.1f (drift %.1fcm). Respawning fixed ground."),
	       FloorZ, GroundTopZ, DriftCm);
	GroundActor->Destroy();
	GroundActor = nullptr;
	SpawnGroundCollision();
}

void AGM_DemoScene::SpawnNavInvoker()
{
	UWorld* World = GetWorld();
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!World || !PlayerPawn)
	{
		return;
	}

	// 既に作ってあれば作り直さない。
	if (NavInvokerActor)
	{
		return;
	}

	// MR空間には NavMeshBoundsVolume を置けない（レベルジオメトリが無い）。
	// 代わりに NavigationInvoker を独立アクターとしてプレイヤー足元に置き、その周囲だけ
	// 実行時に NavMesh を生成させる。MRUK の床メッシュ（Nav対象）の上にタイルが張られる。
	// （bGenerateNavigationOnlyAroundNavigationInvokers=True と組で機能する。DefaultEngine.ini 参照。）
	const FVector PlayerLocation = PlayerPawn->GetActorLocation();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	NavInvokerActor = World->SpawnActor<AActor>(AActor::StaticClass(), PlayerLocation, FRotator::ZeroRotator, Params);
	if (!NavInvokerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnNavInvoker: failed to spawn invoker actor"));
		return;
	}

	USceneComponent* Root = NewObject<USceneComponent>(NavInvokerActor);
	NavInvokerActor->SetRootComponent(Root);
	// 固定床と同じ罠: 後付けルートはスポーン座標を引き継がず原点に居るので、明示的に置く。
	// （Invoker の生成半径はこの位置基準。原点のままでも半径15mで偶然部屋を覆えていたが、正しく足元に置く。）
	Root->SetRelativeLocation(PlayerLocation);
	Root->RegisterComponent();

	UNavigationInvokerComponent* Invoker = NewObject<UNavigationInvokerComponent>(NavInvokerActor);
	if (Invoker)
	{
		// 生成半径は部屋全体（最遠壁まで）をカバーできる大きさにする（cm）。除去半径はそれより少し広く。
		Invoker->SetGenerationRadii(NavInvokerGenerationRadius, NavInvokerGenerationRadius + 300.0f);
		Invoker->RegisterComponent();
	}

	// 敵カプセル(≈15cm)に対し NavMesh の AgentRadius 既定(35)が太すぎると、実際は通れる細い隙間が
	// 歩行可能領域から外れ、敵が「通れそうな隙間を通らない」。AgentRadius は DefaultEngine.ini の
	// [/Script/NavigationSystem.RecastNavMesh] AgentRadius=20 で設定する（生成時に反映される）。
	// ※ ここで RecastNav->RebuildAll() を呼ぶと全タイル再生成で数秒ゲームスレッドをブロックし、
	//    MR起動直後に HMD の BeginFrame4 が連続失敗(-1000)して画面が出なくなるため呼ばない。
	//    Invoker による差分生成(Build)だけで十分。
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		NavSys->Build();
	}

	UE_LOG(LogTemp, Log, TEXT("SpawnNavInvoker: invoker at %s (radius=%.1f). NavMesh verts now=%d"),
	       *PlayerLocation.ToCompactString(), NavInvokerGenerationRadius, GetNavMeshVertCount());
}

int32 AGM_DemoScene::GetNavMeshVertCount() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return -1;
	}
	const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys)
	{
		return -1;
	}
	const ARecastNavMesh* RecastNav = Cast<ARecastNavMesh>(NavSys->GetDefaultNavDataInstance());
	if (!RecastNav)
	{
		return -2;
	}
	FRecastDebugGeometry Geometry;
	RecastNav->GetDebugGeometryForTile(Geometry, FNavTileRef());
	return Geometry.MeshVerts.Num();
}
