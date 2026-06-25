// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemySpawner.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Enemy.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "NavigationSystem.h"
#include "EngineUtils.h"

AEnemySpawner::AEnemySpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	SpawnVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnVolume"));
	SetRootComponent(SpawnVolume);
	SpawnVolume->SetBoxExtent(FVector(100.0f, 100.0f, 100.0f));
	SpawnVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool AEnemySpawner::CanEnemyFitAt(const TSubclassOf<AEnemy>& EnemyType, const FVector& FloorLocation) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 湧かす敵クラスの CDO からカプセル寸法を取る（BPごとに違うため）。
	float Radius = 34.0f;        // ACharacter 既定値（CDOから取れない場合の保険）。
	float HalfHeight = 88.0f;
	if (const AEnemy* CDO = EnemyType ? EnemyType->GetDefaultObject<AEnemy>() : nullptr)
	{
		if (const UCapsuleComponent* Capsule = CDO->GetCapsuleComponent())
		{
			Radius = Capsule->GetScaledCapsuleRadius();
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
	}

	// カプセル中心は床から HalfHeight 上。床にわずかに触れただけで弾かないよう SpawnFitClearance ぶん縮める。
	const FVector CapsuleCenter = FloorLocation + FVector(0.0f, 0.0f, HalfHeight);
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(
		FMath::Max(1.0f, Radius - SpawnFitClearance),
		FMath::Max(1.0f, HalfHeight - SpawnFitClearance));

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SpawnerFitTest), /*bTraceComplex=*/false);

	// 壁/床/家具のオクルージョンメッシュ(WorldStatic)に埋まらなければ収まる。
	const bool bBlocked = World->OverlapBlockingTestByChannel(
		CapsuleCenter,
		FQuat::Identity,
		ECC_WorldStatic,
		Capsule,
		QueryParams);

	return !bBlocked;
}

bool AEnemySpawner::IsTooCloseToExistingEnemy(const FVector& Location) const
{
	const UWorld* World = GetWorld();
	if (!World || MinEnemySeparation <= 0.0f)
	{
		return false;
	}

	// 既存の敵の水平距離が MinEnemySeparation 未満なら「近すぎる」。
	const float MinSepSq = FMath::Square(MinEnemySeparation);
	for (TActorIterator<AEnemy> It(World); It; ++It)
	{
		const AEnemy* Other = *It;
		if (!IsValid(Other))
		{
			continue;
		}
		if (FVector::DistSquared2D(Other->GetActorLocation(), Location) < MinSepSq)
		{
			return true;
		}
	}
	return false;
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
		// 常に Box 内ランダムにする。以前は Attempt==0 で VolumeCenter 固定だったため、
		// 投影が即成功して毎回まったく同じ点に湧き、複数体が重なって貫通→浮く/動けない/落下していた。
		const FVector Candidate = UKismetMathLibrary::RandomPointInBoundingBox(VolumeCenter, VolumeExtent);

		if (!NavSys)
		{
			// NavMesh 投影を使わない設定。床が無い空中に湧いて落下する原因になりうるので警告する。
			SpawnLoc = Candidate + FVector(0.0f, 0.0f, SpawnHeightOffset);
			bFoundSpawnLoc = true;
			UE_LOG(LogActor, Warning,
				TEXT("EnemySpawner: NavMesh projection DISABLED (bProjectSpawnToNavMesh=false). Spawning without floor check at %s -> may fall. Spawner=%s"),
				*SpawnLoc.ToCompactString(), *GetName());
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

		// 既に居る敵と近すぎる点は弾く（重なって貫通し、互いに押し合って動けなくなるのを防ぐ）。
		// bStrictSeparation=true の場合は最後の試行でも妥協せず弾く（離れた点が無ければこの回は湧かさない）。
		// false の場合のみ、従来どおり最後の試行で妥協して受け入れる。
		const bool bLastAttempt = (Attempt == Attempts - 1);
		if ((bStrictSeparation || !bLastAttempt) && IsTooCloseToExistingEnemy(ProjectedLoc.Location))
		{
			continue;
		}

		// 敵カプセルが壁/家具に埋まらず収まる点か確認する（空間不足でMeshに埋まる/動けないのを防ぐ）。
		// こちらは最後の試行では妥協して受け入れる（どこにも収まらないなら巡回元のGMが別Spawnerを試す）。
		if (!bLastAttempt && !CanEnemyFitAt(PickedClass, ProjectedLoc.Location))
		{
			continue;
		}

		SpawnLoc = ProjectedLoc.Location + FVector(0.0f, 0.0f, SpawnHeightOffset);
		bFoundSpawnLoc = true;
		// 投影/フォールバックで決まった最終位置を診断出力（落下個体の出所切り分け用）。
		UE_LOG(LogActor, Verbose,
			TEXT("EnemySpawner: resolved spawn. Spawner=%s Candidate=%s -> NavLoc=%s projFailed=%d tooFar=%d"),
			*GetName(), *Candidate.ToCompactString(), *ProjectedLoc.Location.ToCompactString(), NumProjectionFailed, NumProjectedTooFar);
		break;
	}

	if (!bFoundSpawnLoc)
	{
		// 個々の Spawner が NavMesh 点を見つけられないのは巡回方式では正常な過程
		// （GM 側が次の Spawner を試す）。全滅したかは GM 側の Warning で分かるので、
		// ここは調査時のみ見える Verbose に留めてログ連発を避ける。
		UE_LOG(LogActor, Verbose,
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

	// MRUK の床/壁オクルージョンメッシュにはコリジョンが付いているため、湧き位置に敵カプセルが
	// 接触して通常の SpawnActor では「collision at spawn location」で生成失敗する。
	// NavMesh 上の正しい位置まで来ているので、接触しても押し出して必ず生成する。
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AEnemy* SpawnedEnemy = GetWorld()->SpawnActor<AEnemy>(
		PickedClass,
		SpawnLoc,
		SpawnRot,
		SpawnParams
	);
	if (!SpawnedEnemy)
	{
		UE_LOG(LogActor, Error,
			TEXT("EnemySpawner: SpawnActor failed. Spawner=%s Class=%s Location=%s Rotation=%s"),
			*GetName(),
			*GetNameSafe(PickedClass.Get()),
			*SpawnLoc.ToCompactString(),
			*SpawnRot.ToCompactString());
		return nullptr;
	}

	// AdjustIfPossibleButAlwaysSpawn は床/壁メッシュのコリジョンを避けるためにカプセルを
	// 上下「だけでなく水平にも」押し出す。その結果、検証済みの SpawnLoc から横にずれて
	// 床の縁を越え、NavMesh外（床の無い空中）に着地して落下する個体が出ていた。
	// 対策: 押し出し後の水平ドリフトを捨て、検証済みの NavMesh 点(SpawnLoc)の XY に引き戻す。
	// Z はカプセル HalfHeight ぶん持ち上げて床に接地させる（めり込み/浮き防止）。
	FVector FinalLoc = SpawnLoc;
	if (const UCapsuleComponent* Capsule = SpawnedEnemy->GetCapsuleComponent())
	{
		FinalLoc.Z = SpawnLoc.Z + Capsule->GetScaledCapsuleHalfHeight();
	}
	SpawnedEnemy->SetActorLocation(FinalLoc, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogActor, Log,
		TEXT("EnemySpawner: spawned enemy. Spawner=%s Enemy=%s Class=%s SpawnLoc=%s FinalLoc=%s SpawnHeightOffset=%.1f"),
		*GetName(),
		*GetNameSafe(SpawnedEnemy),
		*GetNameSafe(PickedClass.Get()),
		*SpawnLoc.ToCompactString(),
		*FinalLoc.ToCompactString(),
		SpawnHeightOffset);

	return SpawnedEnemy;
}
