// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "UCameraComponent.generated.h"

UCLASS(ClassGroup=(VR),meta=(BlueprintSpawnableComponent))
class MRGAME_API UVRCamera : public UCameraComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	UVRCamera();

};
