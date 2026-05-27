// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemySpawner.h"

#include "Components/BoxComponent.h"
#include "Enemy.h"
#include "Kismet/KismetMathLibrary.h"



// Sets default values
AEnemySpawner::AEnemySpawner()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
	SetRootComponent(SpawnVolume);
	SpawnVolume->SetBoxExtent(FVector(300.f, 300.f,100.f)); 
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

	//ランダム座標を計算
	const FVector SpawnLocation = SpawnVolume->GetComponentLocation();
	const FVector Extent = SpawnVolume->GetScaledBoxExtent();
	const FVector SpawnLoc = UKismetMathLibrary::RandomPointInBoundingBox(SpawnLocation, Extent);

	//生成して返す
	return GetWorld()->SpawnActor<AEnemy>(
		PickedClass,
		SpawnLoc,
		FRotator::ZeroRotator
	);
}
