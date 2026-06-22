// Fill out your copyright notice in the Description page of Project Settings.


#include "GM_Tutorial.h"

AGM_Tutorial::AGM_Tutorial()
{
	// チュートリアルは敵ループ不要なので Tick も不要。
	PrimaryActorTick.bCanEverTick = false;

	// 既定ではオクルージョン(部屋スキャン)はOFF。毎回スキャンさせないため。
	// 練習敵はプレイヤー前方に出るので必須ではない。必要なら BP_GM_Tutorial で
	// bEnableOcclusion=true（＋bScanRoomOnStart）に設定する。
	bEnableOcclusion = false;
}

void AGM_Tutorial::BeginPlay()
{
	// 親 AGM_DemoScene::BeginPlay の「敵ループ/スポーン初期化/フォールバックタイマー」は
	// 走らせたくないので、祖先 AGameModeBase の BeginPlay を直接呼ぶ。
	AGameModeBase::BeginPlay();

	// MR表示の初期化だけ行う（親の protected 関数を再利用）
	InitializePassthrough();
	InitializeOcclusion();   // bEnableOcclusion=false の場合は内部で即 return
}
