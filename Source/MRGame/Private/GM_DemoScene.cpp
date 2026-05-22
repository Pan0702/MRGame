// Fill out your copyright notice in the Description page of Project Settings.


#include "GM_DemoScene.h"
#include "EnemySpawner.h"
#include "Kismet/GameplayStatics.h"

AGM_DemoScene::AGM_DemoScene()
{
}

void AGM_DemoScene::BeginPlay()
{
	Super::BeginPlay();

	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsOfClass(this, AEnemySpawner::StaticClass(), AllActors);

	Spawner.Reserve(AllActors.Num());
	for (AActor* Actor : AllActors)
	{
		if (AEnemySpawner* s = Cast<AEnemySpawner>(Actor))
		{
			Spawner.Add(s);
		}
	}
	if (Spawner.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Spawner is empty"));
		return;
	}
	for (int i = 0; i < 3; i++)
	{
		CreateEnemies();
	}
}

void AGM_DemoScene::CreateEnemies() const
{
	UE_LOG(LogTemp, Warning, TEXT("CurrentNum:%d"), CurrentNum);
	if (CurrentNum < MaxNum)
	{
		int32 num = FMath::RandRange(0, Spawner.Num() - 1);
		Spawner[num]->SpawnOne();
		UE_LOG(LogTemp, Error, TEXT("CallSuccess"));
	}
}

void AGM_DemoScene::DestoroyEnemies()
{
	CurrentNum++;
	CreateEnemies();
}
