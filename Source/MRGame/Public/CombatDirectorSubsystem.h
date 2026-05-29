// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CombatDirectorSubsystem.generated.h"

class AEnemy;
/**
 * 
 */
UCLASS()
class MRGAME_API UCombatDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

protected:
	void Initialize(FSubsystemCollectionBase& Collection) override;
public:
	void RegisterEnemy(AEnemy* Enemy);
	void UnregisterEnemy(AEnemy* Enemy);
	void BeginPlayerAttack();
	void EndPlayerAttack();
	bool CanDamageEnemy(AEnemy* Enemy) const;
	
private:
	
	void CleanupEnemies();
	TWeakObjectPtr<AEnemy> CurrentAttackTarget;
	bool bAttackWindowOpen = false;
	
	TArray<TWeakObjectPtr<AEnemy>> Enemies;
};
