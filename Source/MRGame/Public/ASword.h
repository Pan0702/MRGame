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

protected:
	// Called when the game starts or when spawned
	virtual void Tick(float DeltaTime) override;

public:	
	
	UFUNCTION()
	void OnHitBoxBeginOverlap(UPrimitiveComponent* OVerlappedComp,AActor* OtherActor,
		UPrimitiveComponent* OtherComp,int32 OtherBodyIndex,bool bFromSweep, const FHitResult& SweepResult);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UArrowComponent> Root;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UStaticMeshComponent> BladeMesh;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UStaticMesh> SwordMeshAsset;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sword")
	TObjectPtr<UBoxComponent> SwordColl;
	
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	// TObjectPtr<USceneComponent> BladeTip;
	//
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sword")
	// TObjectPtr<USceneComponent> BladeBase;

private:
	void ApplySwordMesh();
};
