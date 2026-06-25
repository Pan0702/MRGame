// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatDirectorSubsystem.h"

#include "Enemy.h"
#include "GM_DemoScene.h"
#include "TimerSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UCombatDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Warning,
	       TEXT("CombatDirectorSubsystem Initialized!"));
}

void UCombatDirectorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 時間切れで自動的に EndCombat するため、TimerSubsystem の OnTimeUp を購読する。
	// BeginPlay 時点なら TimerSubsystem も生成済み。
	if (UTimerSubsystem* Timer = InWorld.GetSubsystem<UTimerSubsystem>())
	{
		Timer->OnTimeUp.AddDynamic(this, &UCombatDirectorSubsystem::HandleTimeUp);
	}
}

void UCombatDirectorSubsystem::HandleTimeUp()
{
	EndCombat();
}

void UCombatDirectorSubsystem::RegisterEnemy(AEnemy* Enemy)
{
	if (!Enemy) return;
	Enemies.Push(Enemy);
}

void UCombatDirectorSubsystem::UnregisterEnemy(AEnemy* Enemy)
{
	Enemies.RemoveAll([Enemy](const TWeakObjectPtr<AEnemy>& E)
	{
		return !E.IsValid() || E.Get() == Enemy;
	});

	if (CurrentAttackTarget.Get() == Enemy)
	{
		CurrentAttackTarget = nullptr;
	}
}

void UCombatDirectorSubsystem::BeginPlayerAttack()
{
	CleanupEnemies();

	bAttackWindowOpen = true;
	CurrentAttackTarget = nullptr;

	if (Enemies.Num() > 0)
	{
		CurrentAttackTarget = Enemies[0];
	}
}

void UCombatDirectorSubsystem::EndPlayerAttack()
{
	bAttackWindowOpen = false;
	CurrentAttackTarget = nullptr;
	CleanupEnemies();
}

bool UCombatDirectorSubsystem::CanDamageEnemy(AEnemy* Enemy) const
{
	// 戦闘終了後（時間切れ等）はどの敵も倒せない。
	return !bCombatEnded
		&& bAttackWindowOpen
		&& Enemy
		&& CurrentAttackTarget.IsValid()
		&& CurrentAttackTarget.Get() == Enemy;
}

void UCombatDirectorSubsystem::EndCombat()
{
	if (bCombatEnded)
	{
		return;
	}
	bCombatEnded = true;

	// 攻撃ウィンドウも閉じる（念のため）。
	bAttackWindowOpen = false;
	CurrentAttackTarget = nullptr;

	// 全敵のAIを止めてその場に立たせる。
	CleanupEnemies();
	for (const TWeakObjectPtr<AEnemy>& E : Enemies)
	{
		if (AEnemy* Enemy = E.Get())
		{
			Enemy->StopForCombatEnd();
		}
	}

	// 新しい敵が湧き続けないよう、GMの湧きループも止める。
	if (AGM_DemoScene* GM = Cast<AGM_DemoScene>(UGameplayStatics::GetGameMode(this)))
	{
		GM->StopSpawning();
	}

	UE_LOG(LogTemp, Warning, TEXT("CombatDirector: EndCombat - stopped %d enemies, damage disabled, spawning halted"), Enemies.Num());
}

void UCombatDirectorSubsystem::ResetCombat()
{
	bCombatEnded = false;
	bAttackWindowOpen = false;
	CurrentAttackTarget = nullptr;
}

void UCombatDirectorSubsystem::CleanupEnemies()
{
	Enemies.RemoveAll([](const TWeakObjectPtr<AEnemy>& E)
	{
		return !E.IsValid();
	});
}
