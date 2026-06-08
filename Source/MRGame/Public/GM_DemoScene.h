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
	bool CreateEnemies();
	void NotifyEnemyKilled();
	void DestoroyEnemies();

	UFUNCTION(BlueprintPure, Category = "EnemyNum")
	int32 GetTotalKills() const { return TotalKills; }

	// 部屋スキャン/ロード完了(OnSceneLoaded)で呼ばれる。壁を測ってループを開始する。
	UFUNCTION()
	void HandleSceneReady(bool bSuccess);

	// デバッグ用コンソールコマンド: パススルーのON/OFFを切り替える
	UFUNCTION(Exec)
	void TogglePassthrough();

	// デバッグ用コンソールコマンド: パススルーの可視状態を明示指定する
	UFUNCTION(Exec)
	void SetPassthroughEnabled(bool bEnabled);

protected:
	
	
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

	// 湧かす敵のBPクラス候補。BP_GM_Demo のディテールで BP_Enemy を1つ以上セットする。
	// （EnemySpawner Actor を介さず、GameModeが直接ここから敵をスポーンする。）
	UPROPERTY(EditAnywhere, Category = "Spawn")
	TArray<TSubclassOf<AEnemy>> EnemyClasses;

	// Depthで壁前のクリア点が見つからなかった時のフォールバック: プレイヤー正面のこの距離(cm)に湧かす。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float FallbackSpawnDistance = 450.0f;

	// フォールバック時の左右ばらつき(cm)。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float FallbackHorizontalSpread = 120.0f;

	// 湧き位置に加える高さオフセット(cm)。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnHeightOffset = 0.0f;

	// 接地用の見えないコリジョン床を生成するか。パススルー空間には物理床が無いため、
	// これが無いと敵(Character)は重力で落下し続ける。
	UPROPERTY(EditAnywhere, Category = "MR|Floor")
	bool bSpawnGroundCollision = true;

	// 生成する見えない床の一辺の半分(cm)。プレイヤー中心にこの範囲を覆う。
	UPROPERTY(EditAnywhere, Category = "MR|Floor")
	float GroundHalfExtent = 1000.0f;

	// 生成する見えない床の厚みの半分(cm)。
	UPROPERTY(EditAnywhere, Category = "MR|Floor")
	float GroundThickness = 10.0f;

	int32 AliveCount = 0;
	bool bLoopActive = false;

	// 生成した見えない床アクター（重複生成しないよう保持）。
	UPROPERTY()
	TObjectPtr<AActor> GroundActor;

private:
	void InitializePassthrough();
	// 部屋スキャン/ロードを起動してオクルージョン(部屋メッシュ方式)を準備する。
	void InitializeOcclusion();
	void StartLoop();
	void MaintainDesiredAliveCount();

	// 部屋メッシュへのLineTraceで、壁前のクリアな湧き位置・向きを1つ求める（無理ならフォールバック）。
	bool ResolveSpawnTransform(FVector& OutLocation, FRotator& OutRotation) const;

	// 求めた位置に EnemyClasses からランダムに選んだ敵を1体スポーンする。失敗時 nullptr。
	AEnemy* SpawnEnemyAt(const FVector& Location, const FRotator& Rotation);

	// 正面の壁をキャリブレーションする（プレイヤーのVRPawnの位置・正面を使う）。
	bool CalibrateFrontWallFromPlayer();

	// プレイヤー足元の床高さを測り、その高さに見えないコリジョン床を生成する。
	// 敵(Character)が落下せず床を歩けるようにするため。
	void SpawnGroundCollision();

};
