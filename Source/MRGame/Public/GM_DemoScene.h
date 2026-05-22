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
	void CreateEnemies() const;
	void DestoroyEnemies();
	
protected:
	
	
	UPROPERTY(EditAnywhere,Category="EnemyNum | MaxNum")
	int32 MaxNum;
	
	UPROPERTY(EditAnywhere,Category="EnemyNum | GroupNum")
	int32 GroupNum;
	
	UPROPERTY()
	TArray<TObjectPtr<AEnemySpawner>> Spawner;
	
	int32 CurrentNum = 0;
	

};
