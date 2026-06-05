// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemySpawner.h"

#include "Components/BoxComponent.h"
#include "Enemy.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "MRSpatialRecognitionSubsystem.h"



// Sets default values
AEnemySpawner::AEnemySpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
	SetRootComponent(SpawnVolume);
	SpawnVolume->SetBoxExtent(FVector(100.f, 100.f,100.f)); 
	SpawnVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

AEnemy* AEnemySpawner::SpawnOne()
{
	//敵がなにもセットされてないならNullをかえす。
	if (EnemyClass.Num() == 0)
	{
		UE_LOG(LogActor,Error,TEXT("empty"));
		return nullptr;
	}

	//配列から抽出
	const int32 Index = FMath::RandRange(0, EnemyClass.Num() - 1);
	TSubclassOf<AEnemy> PickedClass = EnemyClass[Index];
	//nullCheck
	if (!PickedClass)
	{
		UE_LOG(LogActor,Error,TEXT("cannot pickUp"));
		return nullptr;
	}

	FVector SpawnLoc = SpawnVolume->GetComponentLocation();
	FRotator SpawnRot = FRotator::ZeroRotator;
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector PlayerLocation = PlayerPawn->GetActorLocation();
		FVector Forward = PlayerPawn->GetActorForwardVector();
		Forward.Z = 0.0f;
		Forward = Forward.GetSafeNormal();

		bool bResolvedByDepth = false;

		// Depth(MRUK Environment Raycaster)で「壁の手前かつプレイヤーとの間に障害物が無い」点を探す。
		// 部屋スキャン不要。CalibrateFrontWall済みでなければ Subsystem 側がフォールバック点を返す。
		if (UWorld* World = GetWorld())
		{
			if (UMRSpatialRecognitionSubsystem* Spatial = World->GetSubsystem<UMRSpatialRecognitionSubsystem>())
			{
				FVector ClearLoc = FVector::ZeroVector;
				bool bUsedFallback = false;
				// 遮蔽の無いクリアな点が見つかった時だけ採用する。
				// false（bUsedFallback=true、壁未測定や全候補が塞がれていた等）の場合は
				// 下の従来ロジック（プレイヤー正面固定）に落とす。
				if (Spatial->FindClearSpawnPoint(PlayerLocation, Forward, ClearLoc, bUsedFallback))
				{
					SpawnLoc = ClearLoc + FVector(0.0f, 0.0f, SpawnHeightOffset);
					bResolvedByDepth = true;
				}
			}
		}

		// Depthで決められなかった場合は従来どおりプレイヤー正面に固定距離で湧かす。
		if (!bResolvedByDepth)
		{
			const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward).GetSafeNormal();
			const float SideOffset = FMath::RandRange(-SpawnHorizontalSpread, SpawnHorizontalSpread);

			SpawnLoc = PlayerLocation
				+ Forward * SpawnDistanceFromPlayer
				+ Right * SideOffset
				+ FVector(0.0f, 0.0f, SpawnHeightOffset);
		}

		const FVector ToPlayer = (PlayerLocation - SpawnLoc).GetSafeNormal2D();
		SpawnRot = ToPlayer.Rotation();
	}
	else
	{
		const FVector SpawnLocation = SpawnVolume->GetComponentLocation();
		const FVector Extent = SpawnVolume->GetScaledBoxExtent();
		SpawnLoc = UKismetMathLibrary::RandomPointInBoundingBox(SpawnLocation, Extent);
	}

	//生成して返す
	return GetWorld()->SpawnActor<AEnemy>(
		PickedClass,
		SpawnLoc,
		SpawnRot
	);
}
