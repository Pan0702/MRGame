// Fill out your copyright notice in the Description page of Project Settings.


#include "CuttingButton.h"

#include "ASword.h"
#include "Components/StaticMeshComponent.h"
#include "TimerManager.h"

// Sets default values
ACuttingButton::ACuttingButton()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// 空のSceneComponentをRootにして、各メッシュをその子にする//
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = SceneRoot;

	CuttingBeforeButton = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CuttingBeforeButton"));
	AfterHorizontalA    = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AfterHorizontalA"));
	AfterHorizontalB    = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AfterHorizontalB"));
	AfterSlashA         = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AfterSlashA"));
	AfterSlashB         = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AfterSlashB"));
	AfterBackSlashA     = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AfterBackSlashA"));
	AfterBackSlashB     = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AfterBackSlashB"));

	CuttingBeforeButton->SetupAttachment(SceneRoot);

	// 半身は全部Rootの子にして、初期は非表示・物理OFF・当たり判定なし
	UStaticMeshComponent* Halves[] = {
		AfterHorizontalA, AfterHorizontalB,
		AfterSlashA, AfterSlashB,
		AfterBackSlashA, AfterBackSlashB
	};
	for (UStaticMeshComponent* Half : Halves)
	{
		Half->SetupAttachment(SceneRoot);
		Half->SetVisibility(false);
		Half->SetSimulatePhysics(false);
		Half->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// CuttingBeforeButtonだけ当たり判定（Overlap）をつける
	CuttingBeforeButton->SetGenerateOverlapEvents(true);
	CuttingBeforeButton->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CuttingBeforeButton->SetCollisionResponseToAllChannels(ECR_Overlap);
	CuttingBeforeButton->OnComponentBeginOverlap.AddDynamic(this, &ACuttingButton::OnOverlapBegin);
}

// Called when the game starts or when spawned
void ACuttingButton::BeginPlay()
{
	Super::BeginPlay();
}

// Called every frame
void ACuttingButton::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void ACuttingButton::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                    const FHitResult& SweepResult)
{
	if (bIsCut) return; // 二重発火防止//

	ASword* Sword = Cast<ASword>(OtherActor);
	if (!Sword) return; // 剣以外は無視//

	const FVector Velocity = Sword->GetBladeVelocity();
	if (Velocity.Size() < CuttingSpeedThreshold) return; // 速度不足は切れない//

	const ECutDirection Type = JudgeCutType(Velocity.GetSafeNormal());
	DoCut(Type);
}

ECutDirection ACuttingButton::JudgeCutType(const FVector& BladeDir) const
{
	// 剣の移動方向をボタンの正面2軸(Right/Up)に投影（ワールド回転に依存しない）
	const float R = FVector::DotProduct(BladeDir, GetActorRightVector());
	const float U = FVector::DotProduct(BladeDir, GetActorUpVector());

	// 切断「線」の角度を 0..180度 に畳む（振る向きの正負は無視）
	float Angle = FMath::RadiansToDegrees(FMath::Atan2(U, R));
	Angle = FMath::Fmod(Angle + 180.0f, 180.0f); // 0..180

	if (Angle < 22.5f || Angle >= 157.5f)
	{
		return ECutDirection::Horizontal; // ≒0/180 真横
	}
	if (Angle < 90.0f)
	{
		return ECutDirection::Slash; // ≒45  /
	}
	return ECutDirection::BackSlash; // ≒135 \ //
}

void ACuttingButton::DoCut(ECutDirection Type)
{
	bIsCut = true;

	// Beforeを消して当たり判定も切る
	CuttingBeforeButton->SetVisibility(false);
	CuttingBeforeButton->SetGenerateOverlapEvents(false);
	CuttingBeforeButton->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 切断タイプごとに、使う半身ペアと「分離方向(切断線に垂直)」を決める
	UStaticMeshComponent* HalfA = nullptr;
	UStaticMeshComponent* HalfB = nullptr;
	FVector SepDir = FVector::ZeroVector;

	switch (Type)
	{
	case ECutDirection::Horizontal: // 切断線=横 → 上下に分離
		HalfA = AfterHorizontalA;
		HalfB = AfterHorizontalB;
		SepDir = GetActorUpVector();
		break;
	case ECutDirection::Slash: // 切断線=/ → 垂直方向に分離
		HalfA = AfterSlashA;
		HalfB = AfterSlashB;
		SepDir = (GetActorRightVector() - GetActorUpVector()).GetSafeNormal();
		break;
	case ECutDirection::BackSlash: // 切断線=\ → 垂直方向に分離
		HalfA = AfterBackSlashA;
		HalfB = AfterBackSlashB;
		SepDir = (GetActorRightVector() + GetActorUpVector()).GetSafeNormal();
		break;
	}

	// 2つの半身を逆向きに飛ばす
	LaunchHalf(HalfA, SepDir);
	LaunchHalf(HalfB, -SepDir);

	//  ここで演出（パーティクル/SE/スコア加算など）

	// 一定時間後に半身を消す
	GetWorldTimerManager().SetTimer(
		DisappearTimerHandle, this, &ACuttingButton::OnDisappear, DisappearDelay, false);
}

void ACuttingButton::LaunchHalf(UStaticMeshComponent* Half, const FVector& Dir)
{
	if (!Half) return;

	Half->SetVisibility(true);
	Half->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Half->SetCollisionProfileName(TEXT("PhysicsActor"));
	Half->SetSimulatePhysics(true);

	// bVelChange=true で質量に依存しない速度変化として与える
	Half->AddImpulse(Dir * SeparationImpulse, NAME_None, true);
}

void ACuttingButton::HideHalf(UStaticMeshComponent* Half)
{
	if (!Half) return;

	Half->SetSimulatePhysics(false);
	Half->SetVisibility(false);
	Half->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACuttingButton::OnDisappear()
{
	// 表示中の半身（実際に飛んでいるもの）をまとめて消す
	HideHalf(AfterHorizontalA);
	HideHalf(AfterHorizontalB);
	HideHalf(AfterSlashA);
	HideHalf(AfterSlashB);
	HideHalf(AfterBackSlashA);
	HideHalf(AfterBackSlashB);
}
