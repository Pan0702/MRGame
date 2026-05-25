// Fill out your copyright notice in the Description page of Project Settings.


#include "ASword.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MotionControllerComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ASword::ASword()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	//柄
	Root = CreateDefaultSubobject<UArrowComponent>(TEXT("Root"));
	SetRootComponent(Root);
	//モデル
	BladeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));
	BladeMesh->SetupAttachment(Root);
	BladeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SwordColl = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	SwordColl->SetupAttachment(BladeMesh);
	SwordColl->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SwordColl->SetCollisionProfileName(TEXT("OverlapAll"));
	SwordColl->SetGenerateOverlapEvents(true);
	//debug機能 trueにすると見えなくなる
	SwordColl->SetHiddenInGame(true);
	SwordColl->OnComponentBeginOverlap.AddDynamic(this, &ASword::OnHitBoxBeginOverlap);
}

void ASword::BeginPlay()
{
	Super::BeginPlay();
	PrevTipLocation = SwordColl->GetComponentLocation();
}

void ASword::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateSwing(DeltaTime);
}


void ASword::OnHitBoxBeginOverlap(UPrimitiveComponent* OVerlappedComp, AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                  const FHitResult& SweepResult)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC) return;

	// 剣がアタッチされている MotionController から利き手判定
	bool bLeftHand = false;
	if (USceneComponent* Parent = GetRootComponent() ? GetRootComponent()->GetAttachParent() : nullptr)
	{
		if (const UMotionControllerComponent* MC = Cast<UMotionControllerComponent>(Parent))
		{
			bLeftHand = (MC->MotionSource == FName("Left"));
		}
	}

	PC->PlayDynamicForceFeedback(
		HapticIntensity,
		HapticDuration,
		/*bAffectsLeftLarge=*/ bLeftHand,
		/*bAffectsLeftSmall=*/ bLeftHand,
		/*bAffectsRightLarge=*/ !bLeftHand,
		/*bAffectsRightSmall=*/ !bLeftHand,
		EDynamicForceFeedbackAction::Start);
}

void ASword::UpdateSwing(float DeltaTime)
{
	static float maxspeed = 100.f;
	if (DeltaTime <= KINDA_SMALL_NUMBER) return;

	const FVector CurrentTip = SwordColl->GetComponentLocation();
	const float Speed = FVector::Dist(CurrentTip, PrevTipLocation) / DeltaTime;
	PrevTipLocation = CurrentTip;

	if (bIsSwing && Speed <= SwingSpeedOffThreshold)
	{
		bIsSwing = false;
	}
	else if (!bIsSwing && Speed >= SwingSpeedOffThreshold)
	{
		bIsSwing = true;
	}
	SetSwordCollActive(bIsSwing);
	if (bDebugDrawSwing)
	{
		const FColor C = bIsSwing ? FColor::Red : FColor::White;
		DrawDebugSphere(GetWorld(), CurrentTip, 5.f, 12, C, false, -1.f, 0, 0.5f);
	}
	if (maxspeed < Speed)
		maxspeed = Speed;
	UE_LOG(LogTemp, Warning, TEXT("Sword Speed: %.1f cm/sSwinging: %s MaxSpeed : %.1f"),Speed, bIsSwing ? TEXT("YES") : TEXT("NO"), maxspeed);
	
}

void ASword::SetSwordCollActive(bool bActive)
{
	SwordColl->SetGenerateOverlapEvents(bActive);
}
