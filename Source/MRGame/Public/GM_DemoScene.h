// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GM_DemoScene.generated.h"

/**
 * 
 */
class AEnemySpawner;
UCLASS()
class MRGAME_API AGM_DemoScene : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGM_DemoScene();
	virtual void BeginPlay() override;
	bool CreateEnemies();
	void NotifyEnemyKilled();
	void DestoroyEnemies();

	UFUNCTION(BlueprintPure, Category = "EnemyNum")
	int32 GetTotalKills() const { return TotalKills; }
	
protected:
	
	
	UPROPERTY(EditAnywhere, Category = "EnemyNum")
	int32 DesiredAliveCount = 4;
	
	UPROPERTY(EditAnywhere, Category = "EnemyNum")
	int32 TotalKills = 0;

	UPROPERTY(EditAnywhere, Category = "MR|Passthrough")
	bool bInitializePassthrough = true;
	
	UPROPERTY()
	TArray<TObjectPtr<AEnemySpawner>> Spawner;
	
	int32 AliveCount = 0;
	bool bLoopActive = false;
	
private:
	void InitializePassthrough();
	void StartLoop();
	void MaintainDesiredAliveCount();

};
