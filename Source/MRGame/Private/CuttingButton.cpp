// Fill out your copyright notice in the Description page of Project Settings.


#include "CuttingButton.h"

#include "ASword.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "TimerManager.h"

// Sets default values
ACuttingButton::ACuttingButton()
{
	// Tickで「振り中の剣との重なり」を直接判定するため有効化
	PrimaryActorTick.bCanEverTick = true;

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

	if (bDebugDrawCutVolume)
	{
		// 切断判定メッシュのワールドAABBを描画(緑=未切断、赤=切断済み)
		const FBoxSphereBounds B = CuttingBeforeButton->Bounds;
		DrawDebugBox(GetWorld(), B.Origin, B.BoxExtent, FQuat::Identity,
			bIsCut ? FColor::Red : FColor::Green, false, -1.f, 0, 0.3f);
	}

	if (bIsCut) return;

	// BeginOverlap は「振り判定ONの瞬間に既に剣先がボタン内にある」場合に発火しない。
	// その取りこぼしを拾うため、振り中の剣のHitBoxとの重なりを毎フレーム直接判定する。
	ASword* Sword = CachedSword.Get();
	if (!Sword)
	{
		for (TActorIterator<ASword> It(GetWorld()); It; ++It)
		{
			Sword = *It;
			CachedSword = Sword;
			break;
		}
	}
	if (!Sword || !Sword->IsSwinging() || !Sword->SwordColl) return;

	const UBoxComponent* Box = Sword->SwordColl;
	const FCollisionShape Shape = FCollisionShape::MakeBox(Box->GetScaledBoxExtent());
	if (CuttingBeforeButton->OverlapComponent(
			Box->GetComponentLocation(), Box->GetComponentQuat(), Shape))
	{
		UE_LOG(LogTemp, Log, TEXT("CuttingButton %s: Tick経路で切断 (剣速 %.0f cm/s)"),
			*GetName(), Sword->GetBladeVelocity().Size());
		DoCut(JudgeCutType(Sword->GetBladeVelocity().GetSafeNormal()));
	}
}

void ACuttingButton::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                    const FHitResult& SweepResult)
{
	if (bIsCut) return; // 二重発火防止//

	ASword* Sword = Cast<ASword>(OtherActor);
	if (!Sword) return; // 剣以外は無視//

	 const FVector Velocity = Sword->GetBladeVelocity();
	// if (Velocity.Size() < 100.0f) return; // 速度不足は切れない//

	UE_LOG(LogTemp, Log, TEXT("CuttingButton %s: BeginOverlap経路で切断 (剣速 %.0f cm/s)"),
		*GetName(), Velocity.Size());
	const ECutDirection Type = JudgeCutType(Velocity.GetSafeNormal());
	DoCut(Type);
}

FText ACuttingButton::GetTargetLevelDisplayName() const
{
	return FText::FromString(TargetLevel.GetAssetName());
}

ECutDirection ACuttingButton::JudgeCutType(const FVector& BladeDir) const
{
	// 剣の移動方向をボタンの正面2軸(Right/Up)に投影（ワールド回転に依存しない）
	const float R = FVector::DotProduct(BladeDir, GetActorRightVector());
	const float U = FVector::DotProduct(BladeDir, GetActorUpVector());

	// 切断「線」の角度を 0..180度 に畳む（振る向きの正負は無視）
	float Angle = FMath::RadiansToDegrees(FMath::Atan2(U, R));
	Angle = FMath::Fmod(Angle + 180.0f, 180.0f); // 0..180//

	if (Angle < 22.5f || Angle >= 157.5f)
	{
		return ECutDirection::Horizontal; // ≒0/180 真横//
	}
	if (Angle < 90.0f)
	{
		return ECutDirection::Slash; // ≒45 //
	}
	return ECutDirection::BackSlash; // ≒135 //
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

	// 着地しなかった場合の保険：DisappearDelay秒後に完了させる
	GetWorldTimerManager().SetTimer(
		DisappearTimerHandle, this, &ACuttingButton::FinishCut, DisappearDelay, false);
}

void ACuttingButton::LaunchHalf(UStaticMeshComponent* Half, const FVector& Dir)
{
	if (!Half) return;

	Half->SetVisibility(true);
	Half->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Half->SetCollisionProfileName(TEXT("PhysicsActor"));
	Half->SetSimulatePhysics(true);

	// 着地(Sleep)を検知できるようにする
	Half->BodyInstance.bGenerateWakeEvents = true;
	Half->OnComponentSleep.AddDynamic(this, &ACuttingButton::OnHalfSleep);

	// bVelChange=true で質量に依存しない速度変化として与える
	Half->AddImpulse(Dir * SeparationImpulse, NAME_None, true);
}

void ACuttingButton::OnHalfSleep(UPrimitiveComponent* SleepingComp, FName BoneName)
{
	// 半身が止まった＝着地 → 完了（タイマーより早ければこちらが先に走る）
	FinishCut();
}

void ACuttingButton::HideHalf(UStaticMeshComponent* Half)
{
	if (!Half) return;

	Half->SetSimulatePhysics(false);
	Half->SetVisibility(false);
	Half->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACuttingButton::FinishCut()
{
	if (bFinished) return;   // 着地 or タイムアウトのどちらか早い方で1回だけ
	bFinished = true;

	// 保険タイマーが残っていれば止める
	GetWorldTimerManager().ClearTimer(DisappearTimerHandle);

	// 演出フック（BPで実装：SE/スコア/エフェクトなど）
	OnCutFinished();

	// 表示中の半身（実際に飛んでいるもの）をまとめて消す
	HideHalf(AfterHorizontalA);
	HideHalf(AfterHorizontalB);
	HideHalf(AfterSlashA);
	HideHalf(AfterSlashB);
	HideHalf(AfterBackSlashA);
	HideHalf(AfterBackSlashB);
}
