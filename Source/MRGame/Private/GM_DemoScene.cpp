// Fill out your copyright notice in the Description page of Project Settings.


#include "GM_DemoScene.h"
#include "Enemy.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavMesh/RecastNavMesh.h"
#include "DrawDebugHelpers.h"
#include "MRSpatialRecognitionSubsystem.h"
#include "OculusXRPassthroughLayerComponent.h"
#include "OculusXRPassthroughSubsystem.h"
#include "OculusXRPersistentPassthroughInstance.h"
#include "TimerManager.h"

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
}

void AGM_DemoScene::BeginPlay()
{
	Super::BeginPlay();

	InitializePassthrough();

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

	// 部屋メッシュが揃った。正面の壁を測り、接地用の床を作ってからループを開始する。
	const bool bCalibrated = CalibrateFrontWallFromPlayer();
	UE_LOG(LogTemp, Log, TEXT("HandleSceneReady: room loaded. Front wall calibrated: %s"),
	       bCalibrated ? TEXT("true") : TEXT("false"));
	if (bSpawnGroundCollision)
	{
		SpawnGroundCollision();
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
	if (!bLoopActive || EnemyClasses.Num() == 0 || AliveCount >= DesiredAliveCount)
	{
		return false;
	}

	// Depth(壁前のクリア点) or フォールバックで湧き位置を決める。
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	if (!ResolveSpawnTransform(SpawnLocation, SpawnRotation))
	{
		return false;
	}

	if (SpawnEnemyAt(SpawnLocation, SpawnRotation))
	{
		++AliveCount;
		UE_LOG(LogTemp, Log, TEXT("Enemy spawned. AliveCount:%d DesiredAliveCount:%d"), AliveCount, DesiredAliveCount);
		return true;
	}

	return false;
}

bool AGM_DemoScene::ResolveSpawnTransform(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return false;
	}

	const FVector PlayerLocation = PlayerPawn->GetActorLocation();
	FVector Forward = PlayerPawn->GetActorForwardVector();
	Forward.Z = 0.0f;
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
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
		if (const UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			FNavLocation ProjectedLoc;
			if (NavSys->ProjectPointToNavigation(SpawnLoc, ProjectedLoc, NavProjectExtent))
			{
				// 投影後のZは床面。SpawnHeightOffset は既に乗っているので投影点へ差し替える。
				SpawnLoc = ProjectedLoc.Location + FVector(0.0f, 0.0f, SpawnHeightOffset);
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("ResolveSpawnTransform: spawn point off NavMesh; skipping this spawn"));
				return false;
			}
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

	// 1体死んだら1体だけ補充する（DesiredAliveCount を超えない範囲で）。
	// MaintainDesiredAliveCount だと不足ぶんを一気に湧かせてしまうため、ここでは使わない。
	if (AliveCount < DesiredAliveCount)
	{
		CreateEnemies();
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

void AGM_DemoScene::InitializeOcclusion()
{
	if (!bEnableOcclusion)
	{
		return;
	}

	// 注意: Meta の Soft Occlusion (SetXROcclusionsMode/StartEnvironmentDepth による
	// リアルタイムDepthオクルージョン) は WITH_OCULUS_BRANCH 版（Metaフォークの特別なUE）
	// でしか動作せず、Epic公式UEでは中身が空で何も起きない（呼んでも机/壁に隠れない）。
	// そのため公式UEでは Scene(MRUK) の部屋メッシュに透明オクルージョンマテリアルを貼る方式を採る。
	//
	// このプロジェクトでは MRUKAnchorActorSpawner をレベルに配置し、その ProceduralMaterial に
	// オクルージョンマテリアルを指定することで、部屋ロード時に壁/机/椅子の形に透明メッシュが生成され、
	// 敵がその後ろに隠れる。ここでは部屋データのロードだけを起動する（Spawnerが OnSceneLoaded を拾う）。
	if (UWorld* World = GetWorld())
	{
		if (UMRSpatialRecognitionSubsystem* Spatial = World->GetSubsystem<UMRSpatialRecognitionSubsystem>())
		{
			// デバッグ可視化の設定を、部屋ロード（→BuildOcclusionMeshes）が走る前に渡す。
			Spatial->bDebugVisualizeMesh = bDebugVisualizeRoomMesh;
			Spatial->DebugMeshMaterial = DebugRoomMeshMaterial;

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
	// 床面に貼りつくと見にくいので少しだけ上げて描画する。
	const FVector Lift(0.0f, 0.0f, 3.0f);
	const FColor NavColor = FColor::Green;

	for (int32 i = 0; i + 2 < Indices.Num(); i += 3)
	{
		if (!Verts.IsValidIndex(Indices[i]) || !Verts.IsValidIndex(Indices[i + 1]) || !Verts.IsValidIndex(Indices[i + 2]))
		{
			continue;
		}
		const FVector A = Verts[Indices[i]] + Lift;
		const FVector B = Verts[Indices[i + 1]] + Lift;
		const FVector C = Verts[Indices[i + 2]] + Lift;
		DrawDebugLine(World, A, B, NavColor, false, -1.0f, 0, 0.5f);
		DrawDebugLine(World, B, C, NavColor, false, -1.0f, 0, 0.5f);
		DrawDebugLine(World, C, A, NavColor, false, -1.0f, 0, 0.5f);
	}
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

	// 床高さを決める。Depthで足元の床が測れればそれを使い、ダメならVRPawn（LocalFloor原点）のZ。
	float FloorZ = PlayerLocation.Z;
	if (UMRSpatialRecognitionSubsystem* Spatial = World->GetSubsystem<UMRSpatialRecognitionSubsystem>())
	{
		float MeasuredZ = 0.0f;
		if (Spatial->MeasureFloorHeight(PlayerLocation, MeasuredZ))
		{
			FloorZ = MeasuredZ;
		}
	}

	// 床天面が FloorZ に来るよう、中心を厚みの半分だけ下げる。
	const FVector GroundCenter(PlayerLocation.X, PlayerLocation.Y, FloorZ - GroundThickness);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	GroundActor = World->SpawnActor<AActor>(AActor::StaticClass(), GroundCenter, FRotator::ZeroRotator, Params);
	if (!GroundActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnGroundCollision: failed to spawn ground actor"));
		return;
	}

	// 見えないコリジョンBoxを付ける（描画なし・コリジョンのみ）。
	UBoxComponent* Box = NewObject<UBoxComponent>(GroundActor);
	if (!Box)
	{
		return;
	}
	GroundActor->SetRootComponent(Box);
	Box->RegisterComponent();
	Box->SetBoxExtent(FVector(GroundHalfExtent, GroundHalfExtent, GroundThickness));
	Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Box->SetCollisionObjectType(ECC_WorldStatic);
	Box->SetCollisionResponseToAllChannels(ECR_Block);
	Box->SetHiddenInGame(true); // パススルーの床が見えているので描画は不要。

	UE_LOG(LogTemp, Log, TEXT("SpawnGroundCollision: ground at Z=%.1f (player Z=%.1f)"), FloorZ, PlayerLocation.Z);
}
