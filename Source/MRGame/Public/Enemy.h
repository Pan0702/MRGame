// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Enemy.generated.h"
class AGM_DemoScene;
class USoundBase;
UCLASS()
class MRGAME_API AEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AEnemy();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	
	UFUNCTION()
	void OnHitCapsuleBeginOverlap(UPrimitiveComponent* OverlappedComp,AActor* OtherActor,
		UPrimitiveComponent* OtherComp,int32 OtherBodyIndex,bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION(BlueprintPure, Category="Enemy")
	bool GetDyFlag();


protected:
	// 死亡演出を開始する（当たり判定・移動を止めて Death アニメに任せる）
	void StartDeath();

	// Death アニメ末尾の AnimNotify から呼ぶ。実際にアクターを消す。
	UFUNCTION(BlueprintCallable, Category="Enemy")
	void FinishDeath();


private:

	
	UPROPERTY(EditAnywhere, Category = "Enemy")
	float MoveSpeed = 150.f;

	UPROPERTY(EditAnywhere, Category = "Enemy|Feedback")
	TObjectPtr<USoundBase> DeathSound;
	
	UPROPERTY()
	TObjectPtr<AGM_DemoScene> CachedGM;
	
	UPROPERTY(EditAnywhere, Category = "State")
	bool bIsDead = false;
};
