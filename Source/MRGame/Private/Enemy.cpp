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
			Destroy();
		}
	}
}

bool AEnemy::GetDyFlag()
{
	return bIsDead;
}

