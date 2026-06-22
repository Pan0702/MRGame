// Fill out your copyright notice in the Description page of Project Settings.


#include "VRPawn.h"
#include "UCameraComponent.h"
#include "MotionControllerComponent.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "ASword.h"
#include "GI_MR.h"
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
	// 原点を Stage（実空間に固定された床原点）にセットする。
	// MRUK の部屋アンカー（壁/床/家具）は Stage 空間基準で配置されるため、ここを LocalFloor
	// （アプリ起動時のヘッド足元基準）にすると部屋全体が現実の物体からずれて MR が破綻する。
	// Stage に合わせることで部屋メッシュ・敵の湧き位置がパススルーの実物体と一致する。
	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::Stage);

	// 診断: 仮想空間が現実に対して回転してずれる件の切り分け。
	// VRPawn 自身の Yaw が 0 でないと、トラッキング空間全体が回って部屋メッシュ/敵が現実から回転して見える。
	{
		const FRotator PawnRot = GetActorRotation();
		const FRotator ControlRot = GetControlRotation();
		UE_LOG(LogTemp, Warning,
			TEXT("VRPawn alignment: ActorLoc=%s ActorYaw=%.1f ControlYaw=%.1f (ActorYaw!=0 -> tracking space rotated -> MR misaligned)"),
			*GetActorLocation().ToCompactString(), PawnRot.Yaw, ControlRot.Yaw);
	}

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
	// GameInstance(GI_MR)に保存された「どちらの手に持つか」を見て左右を決める。
	// GI のHandはMotionControllerコンポーネントのポインタだが、レベル遷移で前のPawnごと
	// 破棄されると無効になるため、ポインタを直接使わず MotionSource(Left/Right)だけを読み取り、
	// 「今の」Pawnの対応するコントローラを返す。
	// Handが未設定(null)、または左右どちらでもない場合は右手をデフォルトにする。
	if (const UGI_MR* GI = Cast<UGI_MR>(GetGameInstance()))
	{
		if (const UMotionControllerComponent* HandFromGI = GI->GetHand())
		{
			if (HandFromGI->MotionSource == FName("Left"))
			{
				return LeftController;
			}
			if (HandFromGI->MotionSource == FName("Right"))
			{
				return RightController;
			}
		}
	}

	// どちらでもなければ右手。
	return RightController;
}
