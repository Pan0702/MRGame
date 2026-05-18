// Fill out your copyright notice in the Description page of Project Settings.


#include "UCameraComponent.h"
#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"

// Sets default values
UVRCamera::UVRCamera()
{
	//HMDに自動追従
	bLockToHmd = true;
	bUsePawnControlRotation = false;
}
