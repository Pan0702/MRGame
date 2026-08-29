// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StartBox.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UTextRenderComponent;
class UPrimitiveComponent;
class USoundBase;

/** スイング方向から決まる切断パターン。 */
UENUM(BlueprintType)
enum class ECutType : uint8
{
	/** 概ね水平 → 上半分 + 下半分 */
	Horizontal,
	/** 右上→左下 → Box_D_TopRight + Box_D_BotLeft */
	DiagTopRightToBotLeft,
	/** 左上→右下 → Box_D_TopLeft + Box_D_BotRight */
	DiagTopLeftToBotRight
};

/**
 * タイトルの START BOX。剣で切ると2ピースに分割→分離→1秒後にゲームレベルへ遷移する。
 * ロジックはC++、メッシュ/Widgetの割り当てはBP_StartBox側で行う想定。
 */
UCLASS()
class MRGAME_API AStartBox : public AActor
{
	GENERATED_BODY()

public:
	AStartBox();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnHitBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	/** ワールド速度から切断パターンを判定する。 */
	ECutType ResolveCutType(const FVector& SwingVelocity) const;

	/** 判定結果に応じてメッシュを差し替え、2ピースを分離させる。 */
	void PerformCut(ECutType CutType);

	/** 遷移先レベルを開く（Delay経由で呼ばれる）。bTestLoopMode 中はリセットのみ。 */
	void TravelToNextLevel();

	/** テスト用: 箱を切断前の状態へ戻し、再度斬れるようにする。 */
	void ResetBox();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	TObjectPtr<USceneComponent> Root;

	/** 切断前の初期表示メッシュ。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	TObjectPtr<UStaticMeshComponent> DefaultMesh;

	/** 切断後の片割れ1。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	TObjectPtr<UStaticMeshComponent> PieceMeshA;

	/** 切断後の片割れ2。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	TObjectPtr<UStaticMeshComponent> PieceMeshB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	TObjectPtr<UBoxComponent> HitBox;

	/** START テキスト表示用。切断後に非表示にする。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	TObjectPtr<UWidgetComponent> StartWidget;

	/** 箱の表面に書く「START」3Dテキスト（青）。位置/向き/サイズはエディタで箱の面に合わせて調整可。
	 *  切断後は非表示にする。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Box")
	TObjectPtr<UTextRenderComponent> StartText;

	// --- メッシュアセット（box.fbx インポート後に BP/エディタで割り当てる） ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Mesh")
	TObjectPtr<UStaticMesh> Mesh_Default;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Mesh")
	TObjectPtr<UStaticMesh> Mesh_H_Top;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Mesh")
	TObjectPtr<UStaticMesh> Mesh_H_Bottom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Mesh")
	TObjectPtr<UStaticMesh> Mesh_D_TopRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Mesh")
	TObjectPtr<UStaticMesh> Mesh_D_TopLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Mesh")
	TObjectPtr<UStaticMesh> Mesh_D_BotRight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Mesh")
	TObjectPtr<UStaticMesh> Mesh_D_BotLeft;

	/** 切断とみなす最低スイング速度(cm/s)。これ未満は無視。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Cut")
	float MinSwingSpeed = 50.f;

	/** 剣で切った瞬間に鳴らす斬撃音(BP_StartBoxで指定)。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Cut")
	TObjectPtr<USoundBase> CutSound;

	/** 分離時に各ピースへ与えるImpulseの大きさ。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Cut")
	float SeparationImpulse = 200.f;

	/** 切断からレベル遷移までの待ち時間(秒)。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Cut")
	float TravelDelay = 1.0f;

	/** 切断後に遷移する先のレベル。エディタ/BP からレベルアセットを直接選ぶ。
	 *  未設定の場合はフォールバックとして DemoScene を開く。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Box|Cut", meta = (AllowedClasses = "/Script/Engine.World"))
	TSoftObjectPtr<UWorld> NextLevel;

	/** 切断ピースのスケール補正。DefaultMesh相対スケール × この値を各ピースに適用する。
	 *  断片メッシュが大きい場合に 1 未満へ下げる（例: 0.5）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Cut")
	float PieceScaleMultiplier = 1.f;

	/** 切断ピースのヨー補正(度)。ピースが90°ずれている場合に -90 / 90 などを設定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Cut")
	float PieceYawCorrection = 0.f;

	/** テスト用ループ。true の間は切断後にレベル遷移せず、箱を元に戻して繰り返しテストできる。
	 *  本番は false にすると NextLevel（未設定ならフォールバック）へ遷移する（切替はこのフラグのみ）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Test")
	bool bTestLoopMode = true;

	// --- ふわふわ浮遊（上下のサイン揺れ） ---
	/** 浮遊アニメを有効にする。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Float")
	bool bFloatEnabled = true;

	/** 上下の揺れ幅(cm)。初期位置を中心に ±この値で揺れる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Float")
	float FloatAmplitude = 3.0f;

	/** 1往復にかける時間(秒)。大きいほどゆっくり。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Box|Float")
	float FloatPeriod = 3.5f;

private:
	bool bCut = false;

	FTimerHandle TravelTimerHandle;

	/** 浮遊の基準位置（BeginPlay時のアクター位置）。 */
	FVector InitialLocation = FVector::ZeroVector;

	/** 浮遊アニメの経過時間。 */
	float FloatTime = 0.f;
};
