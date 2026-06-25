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
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// TimerSubsystem の時間切れ通知(OnTimeUp)を受けて EndCombat する。
	UFUNCTION()
	void HandleTimeUp();
public:
	void RegisterEnemy(AEnemy* Enemy);
	void UnregisterEnemy(AEnemy* Enemy);
	void BeginPlayerAttack();
	void EndPlayerAttack();
	bool CanDamageEnemy(AEnemy* Enemy) const;

	// 時間切れ等でゲームを終了させる。全敵のAIを止め(その場に立つ)、以後の被ダメージを無効化する。
	// BP から OnTimeUp / OnPhaseChanged(Finished) を購読して呼ぶ想定。
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void EndCombat();

	// 再戦/レベル再開時に終了状態を解除する。
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetCombat();

	// 戦闘が終了済みか（敵は倒せない・動かない）。
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsCombatEnded() const { return bCombatEnded; }

private:

	void CleanupEnemies();
	TWeakObjectPtr<AEnemy> CurrentAttackTarget;
	bool bAttackWindowOpen = false;

	// 時間切れ等で戦闘が終了したか。true の間は CanDamageEnemy が常に false を返す。
	bool bCombatEnded = false;

	TArray<TWeakObjectPtr<AEnemy>> Enemies;
};
