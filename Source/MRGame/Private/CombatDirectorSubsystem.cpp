// Fill out your copyright notice in the Description page of Project Settings.


#include "CombatDirectorSubsystem.h"

#include "Enemy.h"

void UCombatDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Warning,
	       TEXT("CombatDirectorSubsystem Initialized!"));
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
	return bAttackWindowOpen
		&& Enemy
		&& CurrentAttackTarget.IsValid()
		&& CurrentAttackTarget.Get() == Enemy;
}

void UCombatDirectorSubsystem::CleanupEnemies()
{
	Enemies.RemoveAll([](const TWeakObjectPtr<AEnemy>& E)
	{
		return !E.IsValid();
	});
}
