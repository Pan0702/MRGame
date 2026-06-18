// Fill out your copyright notice in the Description page of Project Settings.


#include "TitleGameMode.h"
#include "VRPawn.h"

ATitleGameMode::ATitleGameMode()
{
	// DefaultPawn は C++ の AVRPawn を直接使う。
	// （BP_VRPawn は破損して invalid import でクラッシュしたため依存しない。
	//   AVRPawn はカメラ・コントローラ・グリップ掴み入力を全てC++で持つので BP ラッパー不要）
	DefaultPawnClass = AVRPawn::StaticClass();
}
