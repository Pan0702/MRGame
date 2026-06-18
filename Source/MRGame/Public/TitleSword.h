// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TitleSword.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class USphereComponent;
class UPrimitiveComponent;
class UInputAction;
class UInputMappingContext;
class UMotionControllerComponent;
struct FInputActionValue;

/**
 * タイトルレベルのGrab対応の剣。
 * ロジックはC++、メッシュアセットの割り当てはBP/エディタ側で行う想定（BP_TitleSword）。
 *
 * Grab の流れ:
 *   1. 手(MotionController)が GrabCollision に重なっている間 bHandOverlapping = true
 *   2. グリップ押下で StartGrab(Hand) を呼ぶ（入力バインドは Pawn / BP 側、TODO参照）
 *   3. Grab 中は毎フレーム手の Transform に追従し、移動速度ベクトルを保持
 *   4. グリップ解除で StopGrab()
 */
UCLASS()
class MRGAME_API ATitleSword : public AActor
{
	GENERATED_BODY()

public:
	ATitleSword();

	virtual void Tick(float DeltaTime) override;

	/** 指定した手コンポーネントに掴ませる。Hand が GrabCollision に重なっていなくても強制追従は可能。 */
	UFUNCTION(BlueprintCallable, Category = "Sword|Grab")
	void StartGrab(USceneComponent* Hand);

	/** Grab を解除する。 */
	UFUNCTION(BlueprintCallable, Category = "Sword|Grab")
	void StopGrab();

	UFUNCTION(BlueprintPure, Category = "Sword|Grab")
	bool IsGrabbed() const { return bIsGrabbed; }

	/** 手が掴める範囲(GrabCollision)に入っているか。グリップ入力側の判定に使う。 */
	UFUNCTION(BlueprintPure, Category = "Sword|Grab")
	bool IsHandOverlapping() const { return bHandOverlapping; }

	/** BOX切断方向の判定に使う、直近のワールド速度ベクトル(cm/s)。 */
	UFUNCTION(BlueprintPure, Category = "Sword|Grab")
	FVector GetSwingVelocity() const { return SwingVelocity; }

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnGrabBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnGrabEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// --- グリップ入力（剣自身が受け取り、押した手に強制フィット） ---
	UFUNCTION() void OnGripRightPressed(const FInputActionValue& Value);
	UFUNCTION() void OnGripRightReleased(const FInputActionValue& Value);
	UFUNCTION() void OnGripLeftPressed(const FInputActionValue& Value);
	UFUNCTION() void OnGripLeftReleased(const FInputActionValue& Value);

	/** プレイヤーPawnから MotionSource("Right"/"Left") のコントローラを探す。 */
	UMotionControllerComponent* FindPlayerController(FName MotionSource) const;

public:
	// --- 入力アセット（C++デフォルト割り当て、BPで上書き可） ---
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword|Input")
	TObjectPtr<UInputMappingContext> InputMapping;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword|Input")
	TObjectPtr<UInputAction> GrabRightPressed;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword|Input")
	TObjectPtr<UInputAction> GrabRightReleased;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword|Input")
	TObjectPtr<UInputAction> GrabLeftPressed;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword|Input")
	TObjectPtr<UInputAction> GrabLeftReleased;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UStaticMeshComponent> BladeMesh;

	/** Grab 判定用コリジョン（柄あたりに配置する想定）。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<USphereComponent> GrabCollision;

	/** 剣メッシュ。BP/エディタで割り当てる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UStaticMesh> SwordMeshAsset;

private:
	/** 現在掴んでいる手。 */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> GrabbingHand;

	bool bIsGrabbed = false;
	bool bHandOverlapping = false;

	/** 速度計算用の前フレーム位置。 */
	FVector PrevLocation = FVector::ZeroVector;

	/** 直近のワールド速度。 */
	FVector SwingVelocity = FVector::ZeroVector;
};
