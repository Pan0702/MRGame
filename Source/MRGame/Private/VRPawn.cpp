// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPawn.h"
#include "UCameraComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "ASword.h"
#include "DrawDebugHelpers.h"
#include "UObject/ConstructorHelpers.h"

// Sets default values
AVRPawn::AVRPawn()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	SetRootComponent(VROrigin);

	VRCamera = CreateDefaultSubobject<UVRCamera>(TEXT("Camera"));
	VRCamera->SetupAttachment(VROrigin);

	LeftController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftCtrl"));
	LeftController->SetupAttachment(VROrigin);
	LeftController->MotionSource = FName("Left");

	RightController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightCtrl"));
	RightController->SetupAttachment(VROrigin);
	RightController->MotionSource = FName("Right");

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
}

// Called when the game starts or when spawned
void AVRPawn::BeginPlay()
{
	Super::BeginPlay();
	//原点を床基準にセット
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);

	if (SwordClass)
	{
		//SpawnActorに渡すオブジェクト群
		FActorSpawnParameters Params;
		//Spawnした剣をSet
		Params.Owner = this;
		//スポーン位置に何があっても生成する
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		//実態を生成し、EquippedSwordに保存
		EquippedSword = GetWorld()->SpawnActor<ASword>(SwordClass, FVector::ZeroVector,
		                                               FRotator::ZeroRotator, Params);

		if (EquippedSword)
		{
			UMotionControllerComponent* SwordAttachController = GetSwordAttachController();
			if (!SwordAttachController)
			{
				UE_LOG(LogTemp, Error, TEXT("AVRPawn:Sword attach controller is not valid"));
				return;
			}

			if (!EquippedSword->AttachToComponent(SwordAttachController,
			                                      FAttachmentTransformRules::SnapToTargetNotIncludingScale))
			{
				UE_LOG(LogTemp, Error, TEXT("AVRPawn:Sword is not attached"));
			}
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AVRPawn:SwordClass isnot set"));
	}
}

// Called every frame
void AVRPawn::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bDebugDrawControllers)
	{
		return;
	}

	if (LeftController)
	{
		DrawDebugSphere(
			GetWorld(),

			LeftController->GetComponentLocation(),
			3.f, 12, FColor::Red,
			false, -1.f, 0, 0.5f
		);
	}

	if (RightController)
	{
		DrawDebugSphere(
			GetWorld(),

			RightController->GetComponentLocation(),
			3.f, 12, FColor::Green,
			false, -1.f, 0, 0.5f
		);
	}
}

// Called to bind functionality to input
void AVRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

UMotionControllerComponent* AVRPawn::GetSwordAttachController() const
{
	return bEquipSwordInLeftHand ? LeftController : RightController;
}
