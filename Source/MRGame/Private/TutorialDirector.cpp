// Fill out your copyright notice in the Description page of Project Settings.


#include "TutorialDirector.h"

#include "Enemy.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ATutorialDirector::ATutorialDirector()
{
	// 練習敵の撃破をポーリングするため Tick を有効化
	PrimaryActorTick.bCanEverTick = true;
}

void ATutorialDirector::BeginPlay()
{
	Super::BeginPlay();
	BeginSwingStep();
}

void ATutorialDirector::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 振って倒すステップ中、練習敵が倒されたら次へ
	if (Step == ETutorialStep::Swing && IsPracticeEnemyDefeated())
	{
		BeginTimeLimitStep();
	}
}

void ATutorialDirector::BeginSwingStep()
{
	Step = ETutorialStep::Swing;
	ShowInstruction(SwingText);
	SpawnPracticeEnemy();

	// 敵が出せなかった場合（クラス未設定など）は、止まらないよう次へ進める
	if (!PracticeEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("TutorialDirector: 練習敵を出せませんでした。PracticeEnemyClass を確認してください。"));
		BeginTimeLimitStep();
	}
}

void ATutorialDirector::SpawnPracticeEnemy()
{
	if (!PracticeEnemyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("TutorialDirector: PracticeEnemyClass が未設定です。"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// プレイヤーの足元位置とカメラ向き（水平）を基準に、前方へ湧かす
	FVector PlayerLoc = GetActorLocation();
	FVector Forward = GetActorForwardVector();

	if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		PlayerLoc = Pawn->GetActorLocation();
	}
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(World, 0))
	{
		FRotator CamRot = Cam->GetCameraRotation();
		CamRot.Pitch = 0.0f;
		CamRot.Roll = 0.0f;
		Forward = CamRot.Vector();
	}

	FVector SpawnLoc = PlayerLoc + Forward * SpawnDistance;
	SpawnLoc.Z += SpawnHeightOffset;

	// プレイヤーの方を向かせる（水平のみ）
	FRotator SpawnRot = (PlayerLoc - SpawnLoc).Rotation();
	SpawnRot.Pitch = 0.0f;
	SpawnRot.Roll = 0.0f;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	PracticeEnemy = World->SpawnActor<AEnemy>(PracticeEnemyClass, SpawnLoc, SpawnRot, Params);

	// MR空間は床が無く落下するので、その場固定の的にする（移動・重力を無効化）
	if (PracticeEnemy)
	{
		if (UCharacterMovementComponent* Move = PracticeEnemy->GetCharacterMovement())
		{
			Move->DisableMovement();
		}
	}
}

bool ATutorialDirector::IsPracticeEnemyDefeated() const
{
	// 倒されると Destroy される → 無効、もしくは死亡フラグが立つ
	return !IsValid(PracticeEnemy) || PracticeEnemy->GetDyFlag();
}

void ATutorialDirector::BeginTimeLimitStep()
{
	Step = ETutorialStep::TimeLimit;
	ShowInstruction(TimeLimitText);

	GetWorldTimerManager().SetTimer(
		StepTimerHandle, this, &ATutorialDirector::BeginStartStep,
		FMath::Max(0.5f, TimeLimitMessageSeconds), false);
}

void ATutorialDirector::BeginStartStep()
{
	Step = ETutorialStep::Starting;
	ShowInstruction(StartText);

	GetWorldTimerManager().SetTimer(
		StepTimerHandle, this, &ATutorialDirector::FinishTutorial,
		FMath::Max(0.5f, StartMessageSeconds), false);
}

void ATutorialDirector::FinishTutorial()
{
	Step = ETutorialStep::Done;
	HideInstruction();

	if (!NextLevelName.IsNone())
	{
		UGameplayStatics::OpenLevel(this, NextLevelName);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("TutorialDirector: NextLevelName 未設定。本編へ遷移しません。"));
	}
}
