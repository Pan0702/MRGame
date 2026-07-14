// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CuttingButton.generated.h"

class ASword;

// 切られ方（切断線の向き）//
UENUM(BlueprintType)
enum class ECutDirection : uint8
{
	Horizontal,   // 真横 —
	Slash,        // 右上→左下 「/」
	BackSlash     // 左上→右下 「\」
};

UCLASS()
class MRGAME_API ACuttingButton : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACuttingButton();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                    bool bFromSweep, const FHitResult& SweepResult);

	// 半身が止まった(着地)時に物理から呼ばれる
	UFUNCTION()
	void OnHalfSleep(UPrimitiveComponent* SleepingComp, FName BoneName);

	// 「着地」or「DisappearDelayでのタイムアウト」のどちらか早い方で1回だけ呼ばれる演出フック。
	// BP側で実装すると、SE/スコア加算/エフェクトなどを差し込める
	UFUNCTION(BlueprintImplementableEvent, Category = "Button")
	void OnCutFinished();

	// 切る前のボタン（当たり判定はこれが持つ）
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Button")
	TObjectPtr<UStaticMeshComponent> CuttingBeforeButton;

	// --- 切られた後の半身（各タイプ 上下/左右の2枚。同じ位置に重ねて配置）---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Button|Horizontal")
	TObjectPtr<UStaticMeshComponent> AfterHorizontalA;   // 真横 半身A
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Button|Horizontal")
	TObjectPtr<UStaticMeshComponent> AfterHorizontalB;   // 真横 半身B

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Button|Slash")
	TObjectPtr<UStaticMeshComponent> AfterSlashA;        // 半身A
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Button|Slash")
	TObjectPtr<UStaticMeshComponent> AfterSlashB;        // 半身B

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Button|BackSlash")
	TObjectPtr<UStaticMeshComponent> AfterBackSlashA;    // 半身A
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Button|BackSlash")
	TObjectPtr<UStaticMeshComponent> AfterBackSlashB;    // 半身B

	// 切った後に遷移するレベル。インスタンスごとに詳細パネルで設定する
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button|Level")
	TSoftObjectPtr<UWorld> TargetLevel;

	// TargetLevelのアセット名(例: "Play")。Widgetの表示テキストに使う
	UFUNCTION(BlueprintPure, Category = "Button|Level")
	FText GetTargetLevelDisplayName() const;

	// 切れる最低スピード(cm/s)。これ未満の速度では切れない
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	float CuttingSpeedThreshold = 200.0f;

	// 分離時に半身へ加える速度（cm/s 相当・質量非依存）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	float SeparationImpulse = 150.0f;

	// 切ってから半身が消えるまでの秒数
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Button")
	float DisappearDelay = 3.0f;

	// Debug用: 切断判定ボックスを常時描画する(緑=未切断、赤=切断済み)
	UPROPERTY(EditAnywhere, Category = "Button|Debug")
	bool bDebugDrawCutVolume = true;

private:
	// 振り判定ONの瞬間に既に剣が重なっているケースをTickで拾うための剣キャッシュ
	TWeakObjectPtr<ASword> CachedSword;

	bool bIsCut = false;
	bool bFinished = false;   // 完了処理が走ったか（着地 or タイムアウトで1回だけ）
	FTimerHandle DisappearTimerHandle;

	// 剣の移動方向から切断タイプを判定
	ECutDirection JudgeCutType(const FVector& BladeDir) const;

	// 実際に切替を行う
	void DoCut(ECutDirection Type);

	// 半身を表示＋物理ON＋インパルスで飛ばす
	void LaunchHalf(UStaticMeshComponent* Half, const FVector& Dir);

	// 半身を非表示＋物理OFFにする
	void HideHalf(UStaticMeshComponent* Half);

	// 着地 or タイムアウトのどちらか早い方で1回だけ実行：演出フック→半身を消す
	UFUNCTION(BlueprintCallable, Category = "Button")
	void FinishCut();
};
