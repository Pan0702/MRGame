// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialDirector.generated.h"

class AEnemy;

// チュートリアルの進行ステップ
UENUM(BlueprintType)
enum class ETutorialStep : uint8
{
	None,
	Swing,       // 「腕を振って敵を倒そう」＋練習敵
	TimeLimit,   // 「時間制限があるよ」
	Starting,    // 「スタート！」
	Done
};

/**
 * 専用チュートリアルレベルに配置するディレクター。
 * 「腕を振って敵を倒す」「時間制限がある」の2点を、実演＋実践で教える。
 * UIテキストの表示は BP の ShowInstruction / HideInstruction に委譲する（WBP でも 3Dテキストでも可）。
 */
UCLASS()
class MRGAME_API ATutorialDirector : public AActor
{
	GENERATED_BODY()

public:
	ATutorialDirector();
	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

	// 練習用に出す敵クラス（本編と同じ BP_Enemy でOK）
	UPROPERTY(EditAnywhere, Category = "Tutorial")
	TSubclassOf<AEnemy> PracticeEnemyClass;

	// プレイヤー前方どれくらい(cm)に練習敵を湧かすか。
	// 剣のリーチ内（腕＋刃でおよそ1m）に出して、その場で斬れる距離にする。
	UPROPERTY(EditAnywhere, Category = "Tutorial")
	float SpawnDistance = 100.0f;

	// 湧き高さオフセット(cm)
	UPROPERTY(EditAnywhere, Category = "Tutorial")
	float SpawnHeightOffset = 0.0f;

	// 練習敵を何体倒したら次のステップへ進むか。0 なら無限に再スポーンし続ける
	//（その場合レベル遷移しないので、BP等の外部トリガーで進行させること）。
	UPROPERTY(EditAnywhere, Category = "Tutorial", meta = (ClampMin = "0"))
	int32 RequiredKills = 3;

	// 練習敵を倒してから次の1体を再スポーンするまでの秒数。
	UPROPERTY(EditAnywhere, Category = "Tutorial", meta = (ClampMin = "0.0"))
	float RespawnDelay = 1.0f;

	// 「時間制限があるよ」を見せておく秒数
	UPROPERTY(EditAnywhere, Category = "Tutorial")
	float TimeLimitMessageSeconds = 4.0f;

	// 「スタート！」を見せてから本編へ遷移するまでの秒数
	UPROPERTY(EditAnywhere, Category = "Tutorial")
	float StartMessageSeconds = 2.0f;

	// チュートリアル後に開くゲーム本編レベル。エディタのドロップダウンからマップ資産を選ぶ。
	// （名前の打ち間違いやマップ改名で遷移が壊れないよう、文字列ではなく参照で持つ）
	UPROPERTY(EditAnywhere, Category = "Tutorial")
	TSoftObjectPtr<UWorld> NextLevel;

	// --- 表示テキスト（エディタで編集可） ---
	UPROPERTY(EditAnywhere, Category = "Tutorial|Text", meta = (MultiLine = true))
	FText SwingText = FText::FromString(TEXT("腕を振って敵を倒そう！"));

	UPROPERTY(EditAnywhere, Category = "Tutorial|Text", meta = (MultiLine = true))
	FText TimeLimitText = FText::FromString(TEXT("制限時間があるよ！\n時間内にできるだけ倒そう"));

	UPROPERTY(EditAnywhere, Category = "Tutorial|Text", meta = (MultiLine = true))
	FText StartText = FText::FromString(TEXT("スタート！"));

	// 説明テキストを表示する（BPでワールドUI/3Dテキストに反映する）
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial")
	void ShowInstruction(const FText& Text);

	// 説明テキストを隠す（BPで実装）
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial")
	void HideInstruction();

	// 練習敵を1体倒すたびに呼ばれる（BPで効果音やカウント表示に使える。実装は任意）。
	UFUNCTION(BlueprintImplementableEvent, Category = "Tutorial")
	void OnPracticeEnemyKilled(int32 Kills);

public:
	// 現在の練習敵撃破数（UI表示用）。
	UFUNCTION(BlueprintPure, Category = "Tutorial")
	int32 GetPracticeKills() const { return PracticeKills; }

protected:

private:
	ETutorialStep Step = ETutorialStep::None;

	UPROPERTY()
	TObjectPtr<AEnemy> PracticeEnemy;

	FTimerHandle StepTimerHandle;

	// 再スポーン待ちタイマー。倒してから RespawnDelay 秒後に次の1体を出す。
	FTimerHandle RespawnTimerHandle;

	// 撃破済みの練習敵の数。
	int32 PracticeKills = 0;

	// 再スポーン待機中フラグ。倒した直後〜次スポーンまでの間、撃破判定を多重発火させないため。
	bool bRespawnPending = false;

	void BeginSwingStep();        // 練習敵を出して「振って倒せ」
	void SpawnPracticeEnemy();    // プレイヤー前方に1体スポーン
	void HandlePracticeEnemyDefeated(); // 撃破カウント→再スポーン or 次ステップ
	void RespawnPracticeEnemy();  // RespawnDelay 後に次の1体を出す
	bool IsPracticeEnemyDefeated() const;
	void BeginTimeLimitStep();    // 「時間制限があるよ」
	void BeginStartStep();        // 「スタート！」
	void FinishTutorial();        // 本編レベルへ遷移
};
