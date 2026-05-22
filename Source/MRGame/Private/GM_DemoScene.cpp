// Fill out your copyright notice in the Description page of Project Settings.


#include "GM_DemoScene.h"
#include "EnemySpawner.h"

AGM_DemoScene::AGM_DemoScene()
{
	Spawner = CreateDefaultSubobject<AEnemySpawner>(TEXT("EnemySpawner"));
}

void AGM_DemoScene::BeginPlay()
{
	Super::BeginPlay();

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
		Spawner->SpawnOne();
	}
}

void AGM_DemoScene::DestoroyEnemies()
{
	CurrentNum++;
	CreateEnemies();
}
