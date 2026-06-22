// Fill out your copyright notice in the Description page of Project Settings.


#include "GM_Selection.h"
#include "VRPawn.h"

AGM_Selection::AGM_Selection()
{
	// 選択画面は敵ループ不要なので Tick も不要。
	PrimaryActorTick.bCanEverTick = false;

	// オクルージョン(部屋スキャン)はOFF。毎回スキャンさせない。
	bEnableOcclusion = false;

	// DefaultPawn は C++ の AVRPawn を直接使う（TitleGameMode と同様）。
	// AVRPawn はカメラ・コントローラ・グリップ掴み入力を全てC++で持つので BP ラッパー不要。
	DefaultPawnClass = AVRPawn::StaticClass();
}

void AGM_Selection::BeginPlay()
{
	// 親 AGM_DemoScene::BeginPlay の「敵ループ/スポーン初期化/フォールバックタイマー」は
	// 走らせたくないので、祖先 AGameModeBase の BeginPlay を直接呼ぶ。
	AGameModeBase::BeginPlay();

	// MR表示の初期化はパススルーだけ行う（親の protected 関数を再利用）。
	InitializePassthrough();
}
