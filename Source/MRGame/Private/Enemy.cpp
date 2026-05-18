// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy.h"
#include "EnemyAIController.h" 
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"      
#include "Engine/SkeletalMesh.h"          
#include "UObject/ConstructorHelpers.h"   

// Sets default values
AEnemy::AEnemy()
{
	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
	//CharactorMovementの設定
	auto CharaMove = GetCharacterMovement();
	CharaMove->MaxWalkSpeed = MoveSpeed;
	CharaMove->bUseControllerDesiredRotation = false;
	//進行方向を向く
	CharaMove->bOrientRotationToMovement = true;
	
	//　AIControllerを指定
	AIControllerClass = AEnemyAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	
}
	


// Called when the game starts or when spawned
void AEnemy::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
}

// Called every frame
void AEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void AEnemy::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

