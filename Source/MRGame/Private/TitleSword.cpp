// Fill out your copyright notice in the Description page of Project Settings.


#include "TitleSword.h"
#include "GI_MR.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "MotionControllerComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "UObject/ConstructorHelpers.h"

ATitleSword::ATitleSword()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	BladeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));
	BladeMesh->SetupAttachment(Root);
	// 刃でも箱を斬れるよう、Overlapのみ生成する当たり判定を持たせる（物理はしない）。
	// StartBox 側の HitBox が ATitleSword の Overlap を検出して切断する。
	BladeMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BladeMesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	BladeMesh->SetGenerateOverlapEvents(true);

	GrabCollision = CreateDefaultSubobject<USphereComponent>(TEXT("GrabCollision"));
	GrabCollision->SetupAttachment(Root);
	GrabCollision->SetSphereRadius(8.f);
	GrabCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	GrabCollision->SetCollisionProfileName(TEXT("OverlapAll"));
	GrabCollision->SetGenerateOverlapEvents(true);

	// Meta XR テンプレートの入力アセットをデフォルト割り当て（BPで上書き可）
	static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMC(
		TEXT("/Game/Meta/XRFramework/Input/IMC_Default"));
	if (IMC.Succeeded()) { InputMapping = IMC.Object; }
	static ConstructorHelpers::FObjectFinder<UInputAction> IA_RP(
		TEXT("/Game/Meta/XRFramework/Input/Actions/IA_Grab_Right_Pressed"));
	if (IA_RP.Succeeded()) { GrabRightPressed = IA_RP.Object; }
	static ConstructorHelpers::FObjectFinder<UInputAction> IA_RR(
		TEXT("/Game/Meta/XRFramework/Input/Actions/IA_Grab_Right_Released"));
	if (IA_RR.Succeeded()) { GrabRightReleased = IA_RR.Object; }
	static ConstructorHelpers::FObjectFinder<UInputAction> IA_LP(
		TEXT("/Game/Meta/XRFramework/Input/Actions/IA_Grab_Left_Pressed"));
	if (IA_LP.Succeeded()) { GrabLeftPressed = IA_LP.Object; }
	static ConstructorHelpers::FObjectFinder<UInputAction> IA_LR(
		TEXT("/Game/Meta/XRFramework/Input/Actions/IA_Grab_Left_Released"));
	if (IA_LR.Succeeded()) { GrabLeftReleased = IA_LR.Object; }
}

void ATitleSword::BeginPlay()
{
	Super::BeginPlay();

	GrabCollision->OnComponentBeginOverlap.AddDynamic(this, &ATitleSword::OnGrabBeginOverlap);
	GrabCollision->OnComponentEndOverlap.AddDynamic(this, &ATitleSword::OnGrabEndOverlap);

	if (SwordMeshAsset)
	{
		BladeMesh->SetStaticMesh(SwordMeshAsset);
	}

	PrevLocation = GetActorLocation();

	// 剣自身がグリップ入力を受け取れるようにする（Pawn改変不要）。
	if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		EnableInput(PC);

		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (InputMapping)
			{
				Subsystem->AddMappingContext(InputMapping, 1);
			}
		}

		if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent))
		{
			if (GrabRightPressed)  { EIC->BindAction(GrabRightPressed,  ETriggerEvent::Triggered, this, &ATitleSword::OnGripRightPressed); }
			if (GrabRightReleased) { EIC->BindAction(GrabRightReleased, ETriggerEvent::Triggered, this, &ATitleSword::OnGripRightReleased); }
			if (GrabLeftPressed)   { EIC->BindAction(GrabLeftPressed,   ETriggerEvent::Triggered, this, &ATitleSword::OnGripLeftPressed); }
			if (GrabLeftReleased)  { EIC->BindAction(GrabLeftReleased,  ETriggerEvent::Triggered, this, &ATitleSword::OnGripLeftReleased); }
			UE_LOG(LogTemp, Warning, TEXT("ATitleSword: grip input bound"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("ATitleSword: no EnhancedInputComponent (InputComponent cast failed)"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("ATitleSword: no PlayerController to EnableInput"));
	}
}

UMotionControllerComponent* ATitleSword::FindPlayerController(FName MotionSource) const
{
	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		return nullptr;
	}

	TArray<UMotionControllerComponent*> Controllers;
	Pawn->GetComponents<UMotionControllerComponent>(Controllers);

	// BP_XRPawn は MotionSource が "LeftGrip/RightGrip", "LeftAim/RightAim" 等。
	// 完全一致ではなく "Left"/"Right" の部分一致で判定し、握りに適した "Grip" を優先する。
	const FString Want = MotionSource.ToString(); // "Right" or "Left"
	UMotionControllerComponent* Fallback = nullptr;
	for (UMotionControllerComponent* MC : Controllers)
	{
		if (!MC) { continue; }
		const FString Src = MC->MotionSource.ToString();
		if (Src.Contains(Want))
		{
			if (Src.Contains(TEXT("Grip")))
			{
				return MC; // 握り姿勢を最優先
			}
			if (!Fallback) { Fallback = MC; }
		}
	}
	return Fallback;
}

void ATitleSword::OnGripRightPressed(const FInputActionValue& /*Value*/)
{
	UE_LOG(LogTemp, Warning, TEXT("ATitleSword: Grip pressed (Right) -> force fit"));
	if (UMotionControllerComponent* Hand = FindPlayerController(FName("Right")))
	{
		StartGrab(Hand);
	}
}

void ATitleSword::OnGripRightReleased(const FInputActionValue& /*Value*/)
{
	StopGrab();
}

void ATitleSword::OnGripLeftPressed(const FInputActionValue& /*Value*/)
{
	UE_LOG(LogTemp, Warning, TEXT("ATitleSword: Grip pressed (Left) -> force fit"));
	if (UMotionControllerComponent* Hand = FindPlayerController(FName("Left")))
	{
		StartGrab(Hand);
	}
}

void ATitleSword::OnGripLeftReleased(const FInputActionValue& /*Value*/)
{
	StopGrab();
}

void ATitleSword::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsGrabbed && GrabbingHand)
	{
		// 持ち手(GrabCollision)を手の Transform に合わせる。
		// GrabCollision を柄の位置に置けば「柄を握る」形になる（先端ではなく）。
		const FTransform HandT = GrabbingHand->GetComponentTransform();
		const FTransform GrabRel = GrabCollision ? GrabCollision->GetRelativeTransform() : FTransform::Identity;
		SetActorTransform(GrabRel.Inverse() * HandT);
	}

	// 速度ベクトルを毎フレーム更新（切断方向の判定に使う）
	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		const FVector CurrentLocation = GetActorLocation();
		SwingVelocity = (CurrentLocation - PrevLocation) / DeltaTime;
		PrevLocation = CurrentLocation;
	}
}

void ATitleSword::StartGrab(USceneComponent* Hand)
{
	if (!Hand)
	{
		return;
	}

	GrabbingHand = Hand;
	bIsGrabbed = true;

	// どちらの手で掴んだかをGIに保存する。Title以外のレベルでは
	// AVRPawn::GetSwordAttachController がこれを読んで剣のバインド先を決める
	if (UGI_MR* GI = Cast<UGI_MR>(GetGameInstance()))
	{
		if (UMotionControllerComponent* MC = Cast<UMotionControllerComponent>(Hand))
		{
			GI->SetHand(MC);
		}
	}

	// 追従開始時の速度ノイズを避けるため位置を初期化
	PrevLocation = GetActorLocation();

	UE_LOG(LogTemp, Log, TEXT("ATitleSword: StartGrab"));
}

void ATitleSword::StopGrab()
{
	GrabbingHand = nullptr;
	bIsGrabbed = false;
	SwingVelocity = FVector::ZeroVector;

	UE_LOG(LogTemp, Log, TEXT("ATitleSword: StopGrab"));
}

void ATitleSword::OnGrabBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// TODO: 手(MotionController)以外の Overlap を除外するため、コリジョンチャンネル/タグで絞り込む
	bHandOverlapping = true;
}

void ATitleSword::OnGrabEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	bHandOverlapping = false;
}
