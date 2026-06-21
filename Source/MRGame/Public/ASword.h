// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ASword.generated.h"

class UArrowComponent;
class UStaticMeshComponent;
class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
class MRGAME_API ASword : public AActor
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	ASword();

	virtual void BeginPlay() override;
protected:
	// Called when the game starts or when spawned
	virtual void Tick(float DeltaTime) override;

public:	
	
	UFUNCTION()
	void OnHitBoxBeginOverlap(UPrimitiveComponent* OVerlappedComp,AActor* OtherActor,
		UPrimitiveComponent* OtherComp,int32 OtherBodyIndex,bool bFromSweep, const FHitResult& SweepResult);
	
	UFUNCTION(BlueprintPure, Category = "Sword|Swing")
	bool IsSwinging() const { return bIsSwing; }

	// 刃先の速度ベクトル(cm/s)。大きさ=スピード、向き=振った方向
	UFUNCTION(BlueprintPure, Category = "Sword|Swing")
	FVector GetBladeVelocity() const { return BladeVelocity; }
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UArrowComponent> Root;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UStaticMeshComponent> BladeMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UStaticMesh> SwordMeshAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UBoxComponent> SwordColl;

	UPROPERTY(EditAnywhere, Category = "Sword|Haptics")
	float HapticIntensity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Sword|Haptics")
	float HapticDuration = 0.15f;


	UPROPERTY(EditAnywhere,Category="Sword|Swing")
	float SwingSpeedOnThreshold = 250.0f;
	UPROPERTY(EditAnywhere,Category="Sword|Swing")
	float SwingSpeedOffThreshold = 100.0f;
	//Debug用
	UPROPERTY(EditAnywhere,Category="Sword|Swing")
	bool bDebugDrawSwing = false;

private:
	FVector PrevTipLocation = FVector::ZeroVector;
	FVector BladeVelocity = FVector::ZeroVector;
	bool bIsSwing = false;
	float DebugMaxSwingSpeed = 0.0f;

	void UpdateSwing(float DeltaTime);
	void SetSwordCollActive(bool bActive);
};
