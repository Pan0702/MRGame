// Fill out your copyright notice in the Description page of Project Settings.


#include "ASword.h"
#include "CombatDirectorSubsystem.h"
#include "CuttingButton.h"
#include "Enemy.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "MotionControllerComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

// Sets default values
ASword::ASword()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	//柄
	Root = CreateDefaultSubobject<UArrowComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Root->SetVisibility(false);
	//モデル
	BladeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));
	BladeMesh->SetupAttachment(Root);
	BladeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SwordColl = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	SwordColl->SetupAttachment(BladeMesh);
	//debug機能 trueにすると見えなくなる
	SwordColl->SetHiddenInGame(true);
	SwordColl->OnComponentBeginOverlap.AddDynamic(this, &ASword::OnHitBoxBeginOverlap);
}

void ASword::BeginPlay()
{
	Super::BeginPlay();
	PrevTipLocation = SwordColl->GetComponentLocation();
	SetSwordCollActive(false);
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

	// 敵 or CuttingButton に当たったら斬撃音を鳴らす。
	// SwordColl は振っている間(=しきい値超え)だけ Overlap 有効なので、
	// ここに来た時点で十分な速度は満たしている。
	if (CutSound && (OtherActor->IsA<AEnemy>() || OtherActor->IsA<ACuttingButton>()))
	{
		UGameplayStatics::PlaySoundAtLocation(this, CutSound, GetActorLocation());
	}
}

void ASword::UpdateSwing(float DeltaTime)
{
	if (DeltaTime <= KINDA_SMALL_NUMBER) return;

	const bool bWasSwinging = bIsSwing;
	const FVector CurrentTip = SwordColl->GetComponentLocation();
	BladeVelocity = (CurrentTip - PrevTipLocation) / DeltaTime;
	const float Speed = BladeVelocity.Size();
	PrevTipLocation = CurrentTip;
	const float OffThreshold = FMath::Max(0.0f, SwingSpeedOffThreshold);
	const float OnThreshold = FMath::Max(SwingSpeedOnThreshold, OffThreshold + 1.0f);

	if (bIsSwing && Speed <= OffThreshold)
	{
		bIsSwing = false;
	}
	else if (!bIsSwing && Speed >= OnThreshold)
	{
		bIsSwing = true;
	}
	if (bWasSwinging != bIsSwing)
	{
		if (UCombatDirectorSubsystem* Director = GetWorld()->GetSubsystem<UCombatDirectorSubsystem>())
		{
			if (bIsSwing)
			{
				Director->BeginPlayerAttack();
			}
			else
			{
				Director->EndPlayerAttack();
			}
		}
	}
	SetSwordCollActive(bIsSwing);
	if (bDebugDrawSwing)
	{
		const FColor C = bIsSwing ? FColor::Red : FColor::White;
		DrawDebugSphere(GetWorld(), CurrentTip, 5.f, 12, C, false, -1.f, 0, 0.5f);
		DebugMaxSwingSpeed = FMath::Max(DebugMaxSwingSpeed, Speed);
		UE_LOG(LogTemp, Verbose, TEXT("Sword Speed: %.1f cm/s Swinging: %s MaxSpeed: %.1f"),
			Speed,
			bIsSwing ? TEXT("YES") : TEXT("NO"),
			DebugMaxSwingSpeed);
	}
	
}

void ASword::SetSwordCollActive(bool bActive)
{
	SwordColl->SetGenerateOverlapEvents(bActive);
}
