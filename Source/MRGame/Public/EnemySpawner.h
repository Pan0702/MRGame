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

	// GMから動的に Spawner を生成する時に、湧かす敵クラス配列を流し込むためのセッター。
	void SetEnemyClasses(const TArray<TSubclassOf<AEnemy>>& InClasses) { EnemyClass = InClasses; }
	
protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> SpawnVolume;
	
	UPROPERTY(EditAnywhere,Category="SpawnCharactor")
	TArray<TSubclassOf<AEnemy>> EnemyClass;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnDistanceFromPlayer = 450.0f;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnHorizontalSpread = 120.0f;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	float SpawnHeightOffset = 0.0f;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	bool bProjectSpawnToNavMesh = true;

	UPROPERTY(EditAnywhere, Category = "Spawn")
	FVector NavProjectExtent = FVector(120.0f, 120.0f, 250.0f);

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float MaxNavProjectionDistance = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float NavFallbackSearchRadius = 500.0f;

	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "1"))
	int32 SpawnPointAttempts = 8;

	// 既存の敵とこの水平距離(cm)未満には湧かさない（重なって貫通し、浮く/動けない/落下するのを防ぐ）。
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float MinEnemySeparation = 80.0f;

private:
	// 候補点が既存の敵と近すぎる（MinEnemySeparation未満）かを返す。重なり回避に使う。
	bool IsTooCloseToExistingEnemy(const FVector& Location) const;
};
