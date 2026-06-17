// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "VRPawn.generated.h"

class UVRCamera;
class UMotionControllerComponent;
class ASword;

UCLASS()
class MRGAME_API AVRPawn : public APawn
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AVRPawn();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<USceneComponent> VROrigin;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UVRCamera> VRCamera;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UMotionControllerComponent> RightController;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR")
	TObjectPtr<UMotionControllerComponent> LeftController;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Sword")
	TSubclassOf<ASword> SwordClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Sword")
	bool bEquipSwordInLeftHand = false;

	UPROPERTY(EditAnywhere, Category = "VR|Debug")
	bool bDebugDrawControllers = false;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VR|Sword")
	TObjectPtr<ASword> EquippedSword;

private:
	UMotionControllerComponent* GetSwordAttachController() const;
};
