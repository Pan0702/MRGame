// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemySpawner.generated.h"

class UBoxComponent;
class AEnemy;
UCLASS()
class MRGAME_API AEnemySpawner : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEnemySpawner();
	AEnemy* SpawnOne();
	
protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> SpawnVolume;
	
	UPROPERTY(EditAnywhere,Category="SpawnCharactor")
	TArray<TSubclassOf<AEnemy>> EnemyClass;
};
