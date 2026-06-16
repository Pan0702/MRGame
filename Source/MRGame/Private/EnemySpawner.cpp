// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemySpawner.h"

#include "Components/BoxComponent.h"
#include "Enemy.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
	SetRootComponent(SpawnVolume);
	SpawnVolume->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	SpawnVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

AEnemy* AEnemySpawner::SpawnOne()
{
	if (EnemyClass.Num() == 0)
	{
		UE_LOG(LogActor, Error, TEXT("EnemySpawner: EnemyClass is empty"));
		return nullptr;
	}

	const int32 Index = FMath::RandRange(0, EnemyClass.Num() - 1);
	TSubclassOf<AEnemy> PickedClass = EnemyClass[Index];
	if (!PickedClass)
	{
		UE_LOG(LogActor, Error, TEXT("EnemySpawner: picked enemy class is null"));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World || !SpawnVolume)
	{
		UE_LOG(LogActor, Error, TEXT("EnemySpawner: spawn failed because World or SpawnVolume is invalid. World=%s SpawnVolume=%s"),
			World ? TEXT("valid") : TEXT("null"),
			SpawnVolume ? TEXT("valid") : TEXT("null"));
		return nullptr;
	}

	const FVector VolumeCenter = SpawnVolume->GetComponentLocation();
	const FVector VolumeExtent = SpawnVolume->GetScaledBoxExtent();
	FVector SpawnLoc = VolumeCenter + FVector(0.0f, 0.0f, SpawnHeightOffset);
	bool bFoundSpawnLoc = false;

	UNavigationSystemV1* NavSys = bProjectSpawnToNavMesh ? FNavigationSystem::GetCurrent<UNavigationSystemV1>(World) : nullptr;
	if (bProjectSpawnToNavMesh && !NavSys)
	{
		UE_LOG(LogActor, Warning, TEXT("EnemySpawner: NavigationSystem unavailable"));
		return nullptr;
	}

	const int32 Attempts = FMath::Max(1, SpawnPointAttempts);
	int32 NumProjectionFailed = 0;
	int32 NumProjectedTooFar = 0;
	for (int32 Attempt = 0; Attempt < Attempts; ++Attempt)
	{
		const FVector Candidate = (Attempt == 0)
			? VolumeCenter
			: UKismetMathLibrary::RandomPointInBoundingBox(VolumeCenter, VolumeExtent);

		if (!NavSys)
		{
			SpawnLoc = Candidate + FVector(0.0f, 0.0f, SpawnHeightOffset);
			bFoundSpawnLoc = true;
			break;
		}

		FNavLocation ProjectedLoc;
		if (!NavSys->ProjectPointToNavigation(Candidate, ProjectedLoc, NavProjectExtent))
		{
			++NumProjectionFailed;
			if (NavFallbackSearchRadius <= 0.0f ||
				!NavSys->GetRandomReachablePointInRadius(Candidate, NavFallbackSearchRadius, ProjectedLoc))
			{
				continue;
			}
		}

		const float ProjectedDistSq = FVector::DistSquared2D(Candidate, ProjectedLoc.Location);
		if (ProjectedDistSq > FMath::Square(MaxNavProjectionDistance))
		{
			++NumProjectedTooFar;
			if (NavFallbackSearchRadius <= 0.0f ||
				!NavSys->GetRandomReachablePointInRadius(Candidate, NavFallbackSearchRadius, ProjectedLoc))
			{
				continue;
			}
		}

		SpawnLoc = ProjectedLoc.Location + FVector(0.0f, 0.0f, SpawnHeightOffset);
		bFoundSpawnLoc = true;
		break;
	}

	if (!bFoundSpawnLoc)
	{
		UE_LOG(LogActor, Warning,
			TEXT("EnemySpawner: spawn failed; no valid NavMesh point. Spawner=%s Center=%s Extent=%s Attempts=%d ProjectionFailed=%d ProjectedTooFar=%d NavExtent=%s MaxNavProjectionDistance=%.1f NavFallbackSearchRadius=%.1f"),
			*GetName(),
			*VolumeCenter.ToCompactString(),
			*VolumeExtent.ToCompactString(),
			Attempts,
			NumProjectionFailed,
			NumProjectedTooFar,
			*NavProjectExtent.ToCompactString(),
			MaxNavProjectionDistance,
			NavFallbackSearchRadius);
		return nullptr;
	}

	FRotator SpawnRot = GetActorRotation();
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const FVector ToPlayer = (PlayerPawn->GetActorLocation() - SpawnLoc).GetSafeNormal2D();
		if (!ToPlayer.IsNearlyZero())
		{
			SpawnRot = ToPlayer.Rotation();
		}
	}

	AEnemy* SpawnedEnemy = GetWorld()->SpawnActor<AEnemy>(
		PickedClass,
		SpawnLoc,
		SpawnRot
	);
	if (!SpawnedEnemy)
	{
		UE_LOG(LogActor, Error,
			TEXT("EnemySpawner: SpawnActor failed. Spawner=%s Class=%s Location=%s Rotation=%s"),
			*GetName(),
			*GetNameSafe(PickedClass.Get()),
			*SpawnLoc.ToCompactString(),
			*SpawnRot.ToCompactString());
	}

	return SpawnedEnemy;
}
