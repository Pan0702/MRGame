// Fill out your copyright notice in the Description page of Project Settings.


#include "GM_DemoScene.h"
#include "EnemySpawner.h"
#include "Kismet/GameplayStatics.h"
#include "OculusXRPassthroughLayerComponent.h"
#include "OculusXRPassthroughSubsystem.h"

AGM_DemoScene::AGM_DemoScene()
{
}

void AGM_DemoScene::BeginPlay()
{
	Super::BeginPlay();

	InitializePassthrough();

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
	StartLoop();
}

bool AGM_DemoScene::CreateEnemies()
{
	if (!bLoopActive || Spawner.Num() == 0 || AliveCount >= DesiredAliveCount)
	{
		return false;
	}

	const int32 Index = FMath::RandRange(0, Spawner.Num() - 1);
	if (Spawner[Index] && Spawner[Index]->SpawnOne())
	{
		++AliveCount;
		UE_LOG(LogTemp, Log, TEXT("Enemy spawned. AliveCount:%d DesiredAliveCount:%d"), AliveCount, DesiredAliveCount);
		return true;
	}

	return false;
}

void AGM_DemoScene::NotifyEnemyKilled()
{
	if (!bLoopActive)
	{
		return;
	}

	AliveCount = FMath::Max(0, AliveCount - 1);
	++TotalKills;

	MaintainDesiredAliveCount();
}

void AGM_DemoScene::DestoroyEnemies()
{
	NotifyEnemyKilled();
}

void AGM_DemoScene::InitializePassthrough()
{
	if (!bInitializePassthrough)
	{
		return;
	}

	UOculusXRPassthroughSubsystem* PassthroughSubsystem = UOculusXRPassthroughSubsystem::GetPassthroughSubsystem(GetWorld());
	if (!PassthroughSubsystem)
	{
		UE_LOG(LogTemp, Warning, TEXT("Passthrough subsystem is not available"));
		return;
	}

	FOculusXRPersistentPassthroughParameters Parameters;
	Parameters.bVisible = true;
	Parameters.Priority = -1;
	Parameters.Shape = NewObject<UOculusXRStereoLayerShapeReconstructed>(this);
	if (Parameters.Shape)
	{
		Parameters.Shape->LayerOrder = PassthroughLayerOrder_Underlay;
		Parameters.Shape->TextureOpacityFactor = 1.0f;
		Parameters.Shape->bEnableEdgeColor = false;
		Parameters.Shape->bEnableColorMap = false;
		Parameters.ApplyShape();
	}

	const FOculusXRPassthrough_LayerResumed_Single LayerResumed;
	PassthroughSubsystem->InitializePersistentPassthrough(Parameters, LayerResumed);
}

void AGM_DemoScene::StartLoop()
{
	AliveCount = 0;
	TotalKills = 0;
	bLoopActive = true;

	MaintainDesiredAliveCount();
}

void AGM_DemoScene::MaintainDesiredAliveCount()
{
	if (!bLoopActive)
	{
		return;
	}

	const int32 MissingCount = FMath::Max(0, DesiredAliveCount - AliveCount);
	const int32 MaxAttempts = MissingCount * FMath::Max(1, Spawner.Num());

	for (int32 Attempt = 0; Attempt < MaxAttempts && AliveCount < DesiredAliveCount; ++Attempt)
	{
		CreateEnemies();
	}
}
