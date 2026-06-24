// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy.h"

#include "ASword.h"
#include "CombatDirectorSubsystem.h"
#include "EnemyAIController.h"
#include "GI_MR.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GM_DemoScene.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

// Sets default values
AEnemy::AEnemy()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//CharactorMovementの設定
	auto CharaMove = GetCharacterMovement();
	CharaMove->MaxWalkSpeed = MoveSpeed;
	CharaMove->bUseControllerDesiredRotation = false;
	//進行方向を向く
	CharaMove->bOrientRotationToMovement = true;

	// RVOアバイダンス: 敵同士が互いの進路を予測して避け合う。
	// これが無いと複数の敵が同じ経路でプレイヤーへ向かったとき、狭い通路で「団子」になって
	// カプセル同士が押し合い前に進めなくなる。また先頭の敵が家具に引っかかって止まると、
	// 後続はそれを動的障害物として回り込めず後ろで渋滞する。RVOで両方を緩和する。
	CharaMove->bUseRVOAvoidance = true;
	// 回避をどれだけ優先するか(0〜1)。高いほど周囲を強く避ける＝団子になりにくいが、
	// 全員が高いと譲り合って進みが鈍るので中庸に。
	CharaMove->AvoidanceWeight = 0.5f;
	// この半径(cm)内の他エージェントを回避対象として考慮する。
	// 敵カプセル＋αにして、近づきすぎる前に避け始める。
	CharaMove->AvoidanceConsiderationRadius = 100.0f;

	//　AIControllerを指定
	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCapsuleComponent()->OnComponentBeginOverlap.AddDynamic(this, &AEnemy::OnHitCapsuleBeginOverlap);

	// 敵同士の衝突・移動はカプセル(Radius≈15cm)だけで行う。
	// スケルタルメッシュ（腕/胴体のフィジックスアセット）が物理衝突に参加していると、
	// カプセルが細くてもメッシュ同士が当たって深く重なり(stuck)・隙間を通れなくなる。
	// 剣ヒットはカプセルの Overlap で判定しており、メッシュのコリジョンには依存しないので無効化してよい。
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}


// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();
	CachedGM = Cast<AGM_DemoScene>(UGameplayStatics::GetGameMode(this));
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	// 診断: BP適用後のカプセル実サイズと、メッシュ側コリジョンが移動を妨げていないかを1回出す。
	// 「腕までCollision」「隙間を通れない」の切り分け用。Radius が太すぎると机の間を通れない。
	if (const UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		const USkeletalMeshComponent* MeshComp = GetMesh();
		UE_LOG(LogTemp, Log,
			TEXT("Enemy capsule: Radius=%.1f HalfHeight=%.1f | MeshCollisionEnabled=%d"),
			Capsule->GetScaledCapsuleRadius(),
			Capsule->GetScaledCapsuleHalfHeight(),
			MeshComp ? (int32)MeshComp->GetCollisionEnabled() : -1);
	}
	UCombatDirectorSubsystem* Director = GetWorld()->GetSubsystem<UCombatDirectorSubsystem>();
	if (Director)
	{
		Director->RegisterEnemy(this);
	}
}

void AEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	UCombatDirectorSubsystem* Director = GetWorld()->GetSubsystem<UCombatDirectorSubsystem>();
	if (Director)
	{
		Director->UnregisterEnemy(this);
	}
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AEnemy::OnHitCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                      UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                      const FHitResult& SweepResult)
{
	//当たったものが剣か？
	if (ASword* Sword = Cast<ASword>(OtherActor))
	{
		//剣が振ってる場外だったら当たりにして敵を消す
		if (Sword->IsSwinging() && !bIsDead)
		{
			bIsDead = true;

			if (DeathSound)
			{
				UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
			}

			if (CachedGM)
			{
					CachedGM->NotifyEnemyKilled();
			}
			UGI_MR* Gi = Cast<UGI_MR>(GetWorld()->GetGameInstance());
			if (Gi)
			{
				constexpr int32 KillCount = 1;
				Gi->AddScore(KillCount);
			}
			// 即 Destroy せず死亡演出を開始する。
			// 実際の Destroy は Death アニメ末尾の AnimNotify → FinishDeath() で行う。
			StartDeath();
		}
	}
}

void AEnemy::StartDeath()
{
	// 倒れている最中に剣やプレイヤーが再ヒットしないよう当たり判定を切る。
	SetActorEnableCollision(false);

	// AI で移動中だと Death アニメ再生中も滑ってしまうので止める。
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	// ここでは Destroy しない。bIsDead により AnimBP が Death に遷移し、
	// アニメ末尾の AnimNotify が FinishDeath() を呼ぶ。
}

void AEnemy::FinishDeath()
{
	Destroy();
}

bool AEnemy::GetDyFlag()
{
	return bIsDead;
}

