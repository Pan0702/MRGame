// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MRUtilityKit.h"
#include "Subsystems/WorldSubsystem.h"
#include "MRSpatialRecognitionSubsystem.generated.h"

class AMRUKAnchor;
class AMRUKRoom;
class UMRUKSubsystem;
class UAndroidPermissionCallbackProxy;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct FMRSpatialAnchorInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "MR|Spatial")
	FString PrimaryLabel;

	UPROPERTY(BlueprintReadOnly, Category = "MR|Spatial")
	TArray<FString> Labels;

	UPROPERTY(BlueprintReadOnly, Category = "MR|Spatial")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "MR|Spatial")
	FVector PlaneSize = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "MR|Spatial")
	FVector VolumeSize = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "MR|Spatial")
	bool bHasPlane = false;

	UPROPERTY(BlueprintReadOnly, Category = "MR|Spatial")
	bool bHasVolume = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMRSpatialSceneLoaded, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMRSpatialAnchorsUpdated, const TArray<FMRSpatialAnchorInfo>&, Anchors);

UCLASS()
class MRGAME_API UMRSpatialRecognitionSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category = "MR|Spatial")
	FMRSpatialSceneLoaded OnSceneLoaded;

	UPROPERTY(BlueprintAssignable, Category = "MR|Spatial")
	FMRSpatialAnchorsUpdated OnRecognizedAnchorsUpdated;

	UFUNCTION(BlueprintCallable, Category = "MR|Spatial")
	bool LoadSceneFromDevice();

	/**
	 * アプリ内から Meta のスペース設定（部屋スキャン）画面を起動する。
	 * スキャン完了後はMRUK側がシーンを更新し、OnSceneLoaded/OnRoomCreated 経由で
	 * 認識アンカーの更新とオクルージョンメッシュ生成が自動的に走る。
	 * @return スキャン起動に成功したら true
	 */
	UFUNCTION(BlueprintCallable, Category = "MR|Spatial")
	bool LaunchRoomScan();

	UFUNCTION(BlueprintCallable, Category = "MR|Spatial")
	void LoadSceneFromDeviceDeferred(float DelaySeconds = 2.0f);

	/**
	 * 起動時に1回だけ、MRUKの全 WallAnchors からプレイヤーとのXY距離が最も遠い壁を選び、
	 * その壁の中心(CachedWallPoint)と内向き法線(CachedWallNormal)をキャッシュする。
	 * 関数名は履歴上 FrontWall だが、実態は「正面の壁」ではなく「プレイヤーから最も遠い壁」。
	 * その場立ち固定（壁は動かない）前提なので、以後は再測定しない。
	 * 部屋ロード前に呼ぶと WallAnchors が空で失敗し、bWallBaseValid は false のまま。
	 * @param PlayerLocation   プレイヤー（VRPawn）のワールド座標。最遠壁の選択にこのXYを使う。
	 * @param PlayerForward    互換のため受け取るが最遠壁の選択には未使用（フォールバック用途のみ）。
	 * @return 最遠壁の取得に成功したら true
	 */
	UFUNCTION(BlueprintCallable, Category = "MR|Spatial|NoScan")
	bool CalibrateFrontWall(const FVector& PlayerLocation, const FVector& PlayerForward);

	/** CalibrateFrontWall が成功して壁点をキャッシュ済みか。 */
	UFUNCTION(BlueprintPure, Category = "MR|Spatial|NoScan")
	bool IsFrontWallCalibrated() const { return bWallBaseValid; }

	/**
	 * 最遠壁（CalibrateFrontWallでキャッシュ済み）の壁面に沿って、等間隔のN点を返す。
	 * 各点は壁面より WallOffset だけ部屋内側へオフセットされ、Zは床高さ（床アンカーがあれば）。
	 * Spawnerアクターを壁沿いに自動配置するのに使う。
	 * @param NumPoints  生成する点数（>=1）
	 * @param Spacing    点と点の間隔(cm)
	 * @param OutPoints  ワールド座標の配列
	 * @param OutWallInward 壁の内向き法線（向きを揃えるのに使う）
	 * @return 取得できたら true
	 */
	UFUNCTION(BlueprintCallable, Category = "MR|Spatial|NoScan")
	bool GetSpawnPointsAlongFarthestWall(int32 NumPoints, float Spacing,
		TArray<FVector>& OutPoints, FVector& OutWallInward) const;

	/**
	 * 指定したワールド座標の真下へDepthレイを撃ち、床面の高さ(Z)を測る。
	 * パススルー空間には物理コリジョンが無いため、敵を床に接地させる高さを得るのに使う。
	 * @param FromLocation  測定の起点（この水平位置の真下を見る）。Z は頭上側から下ろすため少し上げて使う。
	 * @param OutFloorZ     測れた床のワールドZ
	 * @return 床がDepthで取れたら true
	 */
	UFUNCTION(BlueprintCallable, Category = "MR|Spatial|NoScan")
	bool MeasureFloorHeight(const FVector& FromLocation, float& OutFloorZ);

	/** 床を測るレイの最大距離(cm)。起点から真下にこの距離まで探す。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|NoScan")
	float FloorScanMaxDistance = 300.0f;

	/** 床測定レイの起点を、基準点からこの高さ(cm)だけ上げてから真下に撃つ。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|NoScan")
	float FloorScanStartHeight = 50.0f;

	/**
	 * 壁沿いに横ずらししながら「壁⇔プレイヤーの間に障害物が無い」スポーン点を探す（論点A=平面近似 / B=Right）。
	 * 各候補に対し中央＋左右(敵半径分)の3本レイで遮蔽を確認する。
	 * 全候補が塞がっていた場合は bUsedFallback=true で最後の候補（または正面固定点）を返す。
	 * CalibrateFrontWall 未実施または失敗時は、正面固定距離のフォールバック点を返す。
	 * @param PlayerLocation   プレイヤーのワールド座標
	 * @param PlayerForward    プレイヤーの正面ベクトル
	 * @param OutSpawnLocation 求まったスポーン座標
	 * @param bUsedFallback    遮蔽なしの点が見つからずフォールバックした場合 true
	 * @return 遮蔽なしのクリアな点が見つかれば true（bUsedFallback=false）
	 */
	UFUNCTION(BlueprintCallable, Category = "MR|Spatial|NoScan")
	bool FindClearSpawnPoint(const FVector& PlayerLocation, const FVector& PlayerForward, FVector& OutSpawnLocation, bool& bUsedFallback);

	/** 壁から手前に湧かすオフセット(cm)。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|NoScan")
	float WallOffset = 30.0f;

	/** 壁⇔プレイヤー間の遮蔽判定レイ等で使う最大距離(cm)。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|NoScan")
	float WallScanMaxDistance = 500.0f;

	/** 3本レイの左右オフセット = 敵カプセル半径(cm)。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|NoScan")
	float EnemyRadius = 30.0f;

	/** 横ずらし1回あたりの移動量(cm)。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|NoScan")
	float SideStepDistance = 40.0f;

	/** 左右それぞれの最大横ずらし回数。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|NoScan")
	int32 MaxSideSteps = 5;

	/** プレイヤー手前のこの距離(cm)以内のヒットは「自分の体/至近」とみなし遮蔽から除外する。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|NoScan")
	float ObstacleClearance = 20.0f;

	/** 壁を測れなかった時のフォールバック湧き距離(cm)。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|NoScan")
	float FallbackSpawnDistance = 200.0f;

	UFUNCTION(BlueprintCallable, Category = "MR|Spatial")
	void RefreshRecognizedAnchors();

	UFUNCTION(BlueprintPure, Category = "MR|Spatial")
	const TArray<FMRSpatialAnchorInfo>& GetRecognizedAnchors() const { return RecognizedAnchors; }

	UFUNCTION(BlueprintCallable, Category = "MR|Spatial")
	void GetAnchorsByLabel(const FString& Label, TArray<FMRSpatialAnchorInfo>& OutAnchors) const;

	UFUNCTION(BlueprintCallable, Category = "MR|Spatial")
	void GetTables(TArray<FMRSpatialAnchorInfo>& OutAnchors) const;

	UFUNCTION(BlueprintCallable, Category = "MR|Spatial")
	void GetSeats(TArray<FMRSpatialAnchorInfo>& OutAnchors) const;

	/**
	 * ロード済みの部屋の全アンカー(壁/床/天井/机/椅子)に、コリジョン付きのプロシージャルメッシュを生成し、
	 * 「メインパスでは描画しない（=見えない/パススルーが透ける）が、深度パスには書き込む（=後ろの物を隠す）」
	 * オクルージョン状態に設定する。マテリアル不要でScene方式オクルージョンを実現する。
	 * 部屋がまだロードされていなければ何もしない（OnSceneLoaded後に呼ぶこと）。
	 * @return オクルージョンメッシュを設定したアンカー数
	 */
	UFUNCTION(BlueprintCallable, Category = "MR|Spatial|Occlusion")
	int32 BuildOcclusionMeshes();

	/** 部屋ロード完了時に自動でオクルージョンメッシュを生成するか。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|Occlusion")
	bool bAutoBuildOcclusionOnSceneLoaded = true;

	/** 床アンカーにもオクルージョンメッシュを生成するか（敵の接地にも使える）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|Occlusion")
	bool bIncludeFloorInOcclusion = true;

	/** 天井アンカーをオクルージョンに含めるか（見上げた時に敵を隠したくなければ false）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|Occlusion")
	bool bIncludeCeilingInOcclusion = true;

	/**
	 * デバッグ用: true にすると、オクルージョンメッシュをメインパスでも描画する（部屋が認識
	 * できているか目視確認できる）。DebugMeshMaterial が設定されていればそれを、無ければ
	 * 既定のプロシージャルマテリアルを使う。確認が済んだら false に戻すこと（本番は不可視）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|Occlusion")
	bool bDebugVisualizeMesh = false;

	/** デバッグ可視化時にメッシュへ適用するマテリアル（任意。未設定なら描画のみ有効化）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MR|Spatial|Occlusion")
	TObjectPtr<UMaterialInterface> DebugMeshMaterial;

private:
	UFUNCTION()
	void HandleSceneLoaded(bool bSuccess);

	UFUNCTION()
	void HandleRoomChanged(AMRUKRoom* Room);

	UFUNCTION()
	void HandleScenePermissionsGranted(const TArray<FString>& Permissions, const TArray<bool>& GrantResults);

	// 部屋スキャン(LaunchSceneCapture)完了時に呼ばれる。成功したら保存済みシーンをロードする。
	UFUNCTION()
	void HandleCaptureComplete(bool bSuccess);

	// 非同期ロード(UMRUKLoadFromDevice)の Success/Failure デリゲート(引数なし)に対応するハンドラ。
	UFUNCTION()
	void HandleAsyncLoadSucceeded();
	UFUNCTION()
	void HandleAsyncLoadFailed();

	// MRUKSubsystem->Rooms が埋まるのをタイマーでポーリングする（デリゲートが発火しない保険）。
	void PollForRooms();

	UMRUKSubsystem* GetMRUKSubsystem() const;

	bool EnsureScenePermissionOrRequest();
	void RetryLoadSceneFromDevice();
	FMRSpatialAnchorInfo BuildAnchorInfo(const AMRUKAnchor* Anchor) const;
	bool AnchorMatchesAnyLabel(const FMRSpatialAnchorInfo& Info, const TArray<FString>& Labels) const;

	/** Candidate(壁前) から Player までの間に障害物が無いか、中央＋左右3本のLineTraceで確認する。 */
	bool IsCandidateClear(const FVector& Candidate, const FVector& PlayerLocation, const FVector& RightDir);
	/** 1本ぶんの遮蔽判定。プレイヤーより手前(ObstacleClearance考慮)でHitしたら遮蔽ありとみなす。 */
	bool IsRayBlocked(const FVector& From, const FVector& To);

	UPROPERTY()
	TObjectPtr<UMRUKSubsystem> CachedMRUKSubsystem;

	UPROPERTY()
	TArray<FMRSpatialAnchorInfo> RecognizedAnchors;

	UPROPERTY()
	TObjectPtr<UAndroidPermissionCallbackProxy> ScenePermissionProxy;

	// 非同期ロードオブジェクトをGC回収から守るため保持する（保持しないとデリゲートが発火しないことがある）。
	UPROPERTY()
	TObjectPtr<class UMRUKLoadFromDevice> ActiveAsyncLoad;

	FTimerHandle DeferredLoadTimerHandle;
	int32 LoadAttemptCount = 0;
	int32 MaxLoadAttempts = 5;
	bool bDelegatesBound = false;
	// HandleSceneLoaded が一度処理されたか（Async版Success/Failureと直接購読の二重発火を防ぐ）。
	bool bSceneLoadHandled = false;

	// Rooms ポーリング用タイマーと試行回数。
	FTimerHandle RoomsPollTimerHandle;
	int32 RoomsPollAttempts = 0;
	int32 MaxRoomsPollAttempts = 40; // 0.5s間隔 × 40 = 20秒まで待つ。

	/** 起動時に選んだ「プレイヤーから最も遠い壁」の中心点（CalibrateFrontWall でキャッシュ）。 */
	FVector CachedWallPoint = FVector::ZeroVector;
	/** その壁の内向き法線（MRUK壁アンカーの ForwardVector＝部屋内側向き）。 */
	FVector CachedWallNormal = FVector::ZeroVector;
	bool bWallBaseValid = false;
};
