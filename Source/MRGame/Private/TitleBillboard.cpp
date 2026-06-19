// Fill out your copyright notice in the Description page of Project Settings.


#include "TitleBillboard.h"
#include "Components/SceneComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Materials/MaterialInterface.h"

ATitleBillboard::ATitleBillboard()
{
	// ビルボードは描画側で自動的にカメラへ正対するため Tick 不要。
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Billboard = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("Billboard"));
	Billboard->SetupAttachment(Root);
	Billboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ATitleBillboard::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// エディタで配置・プロパティ変更したらプレビューを更新。
	RebuildBillboard();
}

void ATitleBillboard::BeginPlay()
{
	Super::BeginPlay();
	RebuildBillboard();
}

void ATitleBillboard::RebuildBillboard()
{
	if (!Billboard)
	{
		return;
	}

	// 既存要素をクリアしてから1枚だけ追加（重複防止）。
	Billboard->SetElements(TArray<FMaterialSpriteElement>());

	if (TitleMaterial)
	{
		// bSizeIsInScreenSpace=false → ワールド単位の固定サイズ。距離カーブは未使用。
		Billboard->AddElement(TitleMaterial, /*DistanceToOpacityCurve=*/nullptr,
			/*bSizeIsInScreenSpace=*/false, SizeX, SizeY, /*DistanceToSizeCurve=*/nullptr);
	}
}
