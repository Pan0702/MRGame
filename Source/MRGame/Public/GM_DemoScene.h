// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GM_DemoScene.generated.h"

/**
 *
 */
class AEnemy;
UCLASS()
class MRGAME_API AGM_DemoScene : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGM_DemoScene();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	bool CreateEnemies();
	void NotifyEnemyKilled();
	void DestoroyEnemies();

	// 湧きループを止める（時間切れ等）。以後 CreateEnemies/補充/リトライが走らない。
	void StopSpawning();

	UFUNCTION(BlueprintPure, Category = "EnemyNum")
	int32 GetTotalKills() const { return TotalKills; }

	// 歩行範囲（NavMesh）のデバッグ描画を ON/OFF する。
	UFUNCTION(BlueprintCallable, Category = "Spawn|Debug")
	void SetDebugDrawNavMesh(bool bEnabled) { bDebugDrawNavMesh = bEnabled; }

	// 歩行範囲（NavMesh）のデバッグ描画をトグルする。
	UFUNCTION(BlueprintCallable, Category = "Spawn|Debug")
	void ToggleDebugDrawNavMesh() { bDebugDrawNavMesh = !bDebugDrawNavMesh; }

	// 壁沿い Spawner の位置可視化を ON/OFF する。
	UFUNCTION(BlueprintCallable, Category = "Spawn|Debug")
	void SetDebugDrawSpawners(bool bEnabled) { bDebugDrawSpawners = bEnabled; }

	// 壁沿い Spawner の位置可視化をトグルする。
	UFUNCTION(BlueprintCallable, Category = "Spawn|Debug")
	void ToggleDebugDrawSpawners() { bDebugDrawSpawners = !bDebugDrawSpawners; }

	// 部屋スキャン/ロード完了(OnSceneLoaded)で呼ばれる。壁を測ってループを開始する。
	UFUNCTION()
	void HandleSceneReady(bool bSuccess);

	// デバッグ用コンソールコマンド: パススルーのON/OFFを切り替える
	UFUNCTION(Exec)
	void TogglePassthrough();

	// デバッグ用コンソールコマンド: パススルーの可視状態を明示指定する
	UFUNCTION(Exec)
	void SetPassthroughEnabled(bool bEnabled);

	// デバッグ用コンソールコマンド: プロジェクト内の全BPノード数を数えてログ出力する。
	// エディタのコンソール(`)で「CountAllBlueprintNodes」と打つと実行できる。
	// 実体は UBlueprintStatsLibrary::CountAllBlueprintNodes（エディタ限定）。
	UFUNCTION(Exec)
	void CountAllBlueprintNodes();

protected:

	// MR初期化（チュートリアル等のサブクラスから再利用するため protected）
	void InitializePassthrough();
	// 部屋スキャン/ロードを起動してオクルージョン(部屋メッシュ方式)を準備する。
	void InitializeOcclusion();

	UPROPERTY(EditAnywhere, Category = "EnemyNum")
	int32 DesiredAliveCount = 4;
	
	UPROPERTY(EditAnywhere, Category = "EnemyNum")
	int32 TotalKills = 0;

	UPROPERTY(EditAnywhere, Category = "MR|Passthrough")
	bool bInitializePassthrough = true;

	// 起動時に MRUK の部屋データを準備してオクルージョン(部屋メッシュ方式)を有効化するか。
	// 部屋ロード/スキャン完了後、部屋の壁/机/椅子の形に透明オクルージョンメッシュが自動生成され、
	// 現実の物体の後ろに回った敵がその物体に隠れて見えるようになる（コードで完結・マテリアル不要）。
	// ※ Meta の Soft Occlusion (リアルタイムDepth) は公式UEでは動かないためこの方式を採用。
	UPROPERTY(EditAnywhere, Category = "MR|Occlusion")
	bool bEnableOcclusion = true;

	// 起動時にアプリ内で部屋スキャン（スペース設定）を起動するか。
	// true: 起動毎にスキャン画面を出す。false: 保存済みの部屋データをロードして使う。
	UPROPERTY(EditAnywhere, Category = "MR|Occlusion")
	bool bScanRoomOnStart = true;

	// デバッグ用: 部屋メッシュを可視マテリアルで描画して、ロードできているか目視確認する。
	// 本番では false（不可視オクルージョン）に戻すこと。
	UPROPERTY(EditAnywhere, Category = "MR|Occlusion")
	bool bDebugVisualizeRoomMesh = false;

	// デバッグ可視化時に部屋メッシュへ適用するマテリアル（任意。未設定なら描画のみ有効化）。
	UPROPERTY(EditAnywhere, Category = "MR|Occlusion")
	TObjectPtr<class UMaterialInterface> DebugRoomMeshMaterial;

	// 湧かす敵のBPクラス候補。BP_GM_Demo のディテールで BP_Enemy を1つ以上セットする。
	// （EnemySpawner Actor を介さず、GameModeが直接ここから敵をスポーンする。）
	UPROPERTY(EditAnywhere, Category = "Spawn")
	TArray<TSubclassOf<AEnemy>> EnemyClasses;

	// 部屋ロード後に最遠壁沿いへ Spawner を自動配置するか。
	// true: SpawnerCount 個の AEnemySpawner を壁前に並べ、CreateEnemies はそれらをランダム選択。
	// false: 従来どおりプレイヤー基準で都度湧き位置を計算する。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	bool bUseWallSpawners = true;

	// 壁沿いに配置する Spawner の個数。
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "1"))
	int32 SpawnerCount = 5;

	// 壁沿い Spawner の間隔(cm)。中央を基準に左右対称に並べる。
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "1"))
	float SpawnerSpacing = 120.0f;

	// 壁沿いに配置する Spawner のクラス（未指定なら AEnemySpawner を使う）。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	TSubclassOf<class AEnemySpawner> SpawnerClass;

	// Depthで壁前のクリア点が見つからなかった時のフォールバック: プレイヤー正面のこの距離(cm)に湧かす。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float FallbackSpawnDistance = 450.0f;

	// フォールバック時の左右ばらつき(cm)。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float FallbackHorizontalSpread = 120.0f;

	// 湧き位置に加える高さオフセット(cm)。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnHeightOffset = 0.0f;

	// 湧き位置を NavMesh 上に投影して、歩行可能領域（床）の外に湧かないようにするか。
	// 投影に失敗（NavMesh外）した場合、その回のスポーンはキャンセルする（範囲外には出さない）。
	// ※ true の時は SpawnNavInvoker で MRUK 床メッシュ周囲に NavMesh を生成させる。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	bool bProjectSpawnToNavMesh = true;

	// NavMesh 投影時の許容ズレ(cm)。湧き候補点からこの範囲内で最寄りの歩行可能点を探す。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	FVector NavProjectExtent = FVector(150.0f, 150.0f, 300.0f);

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float MaxNavProjectionDistance = 80.0f;

	// NavMesh投影が失敗した時に、候補点の近くで歩ける代替点を探す半径(cm)。
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float NavFallbackSearchRadius = 500.0f;

	// Spawner 配置時の空間チェックで、敵カプセルから差し引く余裕(cm)。
	// 壁/床にカプセルがわずかに触れただけで「埋まり」と誤判定して全 Spawner を弾くのを防ぐ。
	// 大きくすると判定が甘く（埋まり気味でも置く）、小さくすると厳しく（クリアな点だけ置く）なる。
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float SpawnFitClearance = 5.0f;

	// デバッグ用: 敵が歩ける範囲（NavMesh）を毎フレーム描画して目視確認する。
	// 実行中に BP から ON/OFF できる（SetDebugDrawNavMesh / 直接書き込み）。本番では false に。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Debug")
	bool bDebugDrawNavMesh = false;

	// デバッグ用: 壁沿い Spawner の位置を毎フレーム描画して、Spawner が設置されているか目視確認する。
	// 黄=Spawner位置, 水色矢印=向き, 文字=有効/保持数。実行中に BP から ON/OFF 可。本番では false に。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn|Debug")
	bool bDebugDrawSpawners = false;

	// 接地用の見えないコリジョン床を生成するか。床アンカーの位置・サイズに合わせた固定平面を1枚敷き、
	// それを敵の接地＋NavMesh土台にする。MRUK床メッシュ(World Lockで上下ドリフトする)に頼らないため
	// 既定で ON。これと組で MRUK床メッシュ側は Nav 非対象にして二重生成を避ける（下の bFloorMeshAffectsNav）。
	UPROPERTY(EditAnywhere, Category = "MR|Floor")
	bool bSpawnGroundCollision = true;

	// 生成する見えない床の一辺の半分(cm)。プレイヤー中心にこの範囲を覆う。
	UPROPERTY(EditAnywhere, Category = "MR|Floor")
	float GroundHalfExtent = 1000.0f;

	// 生成する見えない床の厚みの半分(cm)。
	UPROPERTY(EditAnywhere, Category = "MR|Floor")
	float GroundThickness = 10.0f;

	// NavMesh 生成用 Invoker の生成半径(cm)。プレイヤー足元中心に、この半径ぶんだけ
	// MRUK 床メッシュ上に NavMesh タイルを張る。最遠壁まで届くよう十分に大きく取る。
	UPROPERTY(EditAnywhere, Category = "MR|Floor")
	float NavInvokerGenerationRadius = 1500.0f;

	// NavMesh 生成時のエージェント半径(cm)。敵カプセル(≈15cm)に合わせて細くすると、
	// 細い隙間も歩行可能領域になり「通れそうな隙間を通らない」が解消する。既定35は太すぎる。
	UPROPERTY(EditAnywhere, Category = "MR|Floor")
	float NavAgentRadius = 20.0f;

	// NavMesh 生成時の最大ステップ高さ(cm)。既定8だと急斜面でNavMeshに穴が空く警告が出るため少し上げる。
	UPROPERTY(EditAnywhere, Category = "MR|Floor")
	float NavAgentMaxStepHeight = 12.0f;

	int32 AliveCount = 0;
	bool bLoopActive = false;

	// 部屋ロード時に生成した壁沿い Spawner たち。
	UPROPERTY()
	TArray<TObjectPtr<class AEnemySpawner>> WallSpawners;

	// 生成した見えない床アクター（重複生成しないよう保持）。
	UPROPERTY()
	TObjectPtr<AActor> GroundActor;

	// NavMesh 生成用 Invoker を載せたアクター（重複生成しないよう保持）。
	UPROPERTY()
	TObjectPtr<AActor> NavInvokerActor;

private:
	void StartLoop();
	void MaintainDesiredAliveCount();

	// 最遠壁沿いに Spawner を SpawnerCount 個生成する。
	// ただし NavMesh に乗らない／敵カプセルが埋まる点には置かない（湧かせない置物 Spawner を作らない）。
	void SpawnWallSpawners();

	// 実際に湧かす敵クラス(EnemyClasses)の CDO からカプセル半径/半高さを取る。配置検証に使う。
	float GetEnemyCapsuleRadius() const;
	float GetEnemyCapsuleHalfHeight() const;

	// 指定位置(カプセル中心)に敵カプセルが壁/家具(WorldStatic)に埋まらず収まるかを返す。
	bool CanEnemyFitAt(const FVector& CapsuleCenter, float Radius, float HalfHeight) const;

	// デバッグ用: 敵が歩ける範囲（NavMesh）の境界をワイヤーフレームで描画する。
	void DebugDrawNavMesh() const;

	// デバッグ用: 壁沿い Spawner の位置・向きを描画して、設置されているか目視確認する。
	void DebugDrawSpawners() const;

	// 診断用: 現在の NavMesh の頂点数を返す。0=未生成/空、>0=生成済み、負値=NavSys/RecastNav取得失敗。
	int32 GetNavMeshVertCount() const;

	// 部屋メッシュへのLineTraceで、壁前のクリアな湧き位置・向きを1つ求める（無理ならフォールバック）。
	bool ResolveSpawnTransform(FVector& OutLocation, FRotator& OutRotation) const;

	// 求めた位置に EnemyClasses からランダムに選んだ敵を1体スポーンする。失敗時 nullptr。
	AEnemy* SpawnEnemyAt(const FVector& Location, const FRotator& Rotation);

	// プレイヤーからXY距離が最も遠い壁をキャリブレーションする（VRPawnの位置を使う。向きは未使用）。
	// 関数名は履歴上 FrontWall だが、実態は「正面」ではなく「最遠壁」。
	bool CalibrateFrontWallFromPlayer();

	// プレイヤー足元の床高さを測り、その高さに見えないコリジョン床を生成する。
	// 敵(Character)が落下せず床を歩けるようにするため。
	void SpawnGroundCollision();

	// プレイヤー足元に NavigationInvoker アクターを置き、MRUK 床メッシュ周囲に
	// 実行時 NavMesh を生成させる。bProjectSpawnToNavMesh が true の時に使う。
	void SpawnNavInvoker();

	// 部屋ロードが一定時間で完了しない場合に、フォールバックでループを開始する。
	void StartLoopFallbackIfNeeded();

	// 部屋ロード完了待ちの最大秒数。これを過ぎたら部屋メッシュ無しでループ開始（敵は出る）。
	// Subsystem側のRoomsポーリング(最大20秒)より長くして、部屋ロードを優先的に待つ。
	UPROPERTY(EditAnywhere, Category = "MR|Occlusion")
	float SceneLoadTimeout = 25.0f;

	// ロード完了待ちのフォールバックタイマー。
	FTimerHandle SceneLoadFallbackTimerHandle;

	// MR空間の Runtime NavMesh は Invoker 起動から生成まで遅延するため、湧きが充足するまで
	// この間隔(秒)で MaintainDesiredAliveCount を再試行する。NavMesh生成を待って敵を出す。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnRetryInterval = 0.5f;

	// 湧き再試行タイマー。充足したら停止する。
	FTimerHandle SpawnRetryTimerHandle;

	// 壁沿い Spawner の配置が 0 個だった時（NavMesh 未生成等）の再試行タイマーと回数。
	FTimerHandle WallSpawnerRetryTimerHandle;
	int32 WallSpawnerRetryCount = 0;

	// 壁沿い Spawner 配置の最大リトライ回数（SpawnRetryInterval 間隔）。
	// 0 個のまま諦めるまでの上限。NavMesh 生成遅延を吸収できる程度に確保する。
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	int32 MaxWallSpawnerRetries = 20;

};
