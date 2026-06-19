// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TitleBillboard.generated.h"

class USceneComponent;
class UMaterialBillboardComponent;
class UMaterialInterface;

/**
 * ボックスの上に浮かべるタイトル画像（背景透過）。
 * UMaterialBillboardComponent を使い、常にHMD(カメラ)へ正対し画面に対して直立する
 * （＝位置は固定、向きだけ追従。Tick不要）。
 * 表示する透過画像は Unlit + Translucent/Masked のマテリアルにして、BP/エディタで割り当てる。
 */
UCLASS()
class MRGAME_API ATitleBillboard : public AActor
{
	GENERATED_BODY()

public:
	ATitleBillboard();

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** 現在のマテリアル/サイズでビルボードのスプライトを作り直す。 */
	void RebuildBillboard();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Title")
	TObjectPtr<USceneComponent> Root;

	/** 常にカメラ(HMD)へ正対するスプライト。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Title")
	TObjectPtr<UMaterialBillboardComponent> Billboard;

	/** 表示するタイトル画像のマテリアル（背景透過：Unlit + Translucent もしくは Masked）。BPで割り当てる。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Title")
	TObjectPtr<UMaterialInterface> TitleMaterial;

	/** 画像の横幅(ワールド単位cm)。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Title")
	float SizeX = 40.f;

	/** 画像の縦幅(ワールド単位cm)。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Title")
	float SizeY = 20.f;
};
