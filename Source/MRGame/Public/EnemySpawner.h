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
	UFUNCTION(BlueprintCallable, Category = "Spawn")
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

	// 重なり回避を厳格にするか。
	// true: 全試行で重なりを弾く。離れた点が見つからなければこの回は湧かさない（重なってSpawnして
	//   敵同士が押し合い動けなくなるのを確実に防ぐ）。狭い壁沿いに多数湧かす時に重要。
	// false: 従来どおり最後の試行で妥協して受け入れる（重なる可能性あり）。
	UPROPERTY(EditAnywhere, Category = "Spawn")
	bool bStrictSeparation = true;

	// 空間チェックで敵カプセルから差し引く余裕(cm)。床/壁にわずかに触れただけで弾かないため。
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float SpawnFitClearance = 1.0f;

	// Extra horizontal clearance so the visible enemy mesh does not start inside furniture or walls.
	// This only affects spawn validation; the movement capsule stays unchanged.
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float SpawnFitRadiusPadding = 10.0f;

	// Additional distance kept between the spawn capsule and the scanned floor edge.
	UPROPERTY(EditAnywhere, Category = "Spawn", meta = (ClampMin = "0"))
	float SpawnFloorEdgeMargin = 5.0f;

private:
	mutable bool bSpawnFloorBoundsCached = false;
	mutable bool bHasSpawnFloorBounds = false;
	mutable FVector CachedSpawnFloorCenter = FVector::ZeroVector;
	mutable FVector2D CachedSpawnFloorHalfXY = FVector2D::ZeroVector;

	// 候補点が既存の敵と近すぎる（MinEnemySeparation未満）かを返す。重なり回避に使う。
	bool IsTooCloseToExistingEnemy(const FVector& Location) const;

	// FloorLocation(床上)に EnemyType の敵カプセルが壁/家具に埋まらず収まるかを返す。
	bool CanEnemyFitAt(const TSubclassOf<AEnemy>& EnemyType, const FVector& FloorLocation) const;
};
