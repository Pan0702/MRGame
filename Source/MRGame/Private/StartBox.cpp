// Fill out your copyright notice in the Description page of Project Settings.


#include "StartBox.h"
#include "TitleSword.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	// 遷移先レベル名。本編マップ Content/Scene/DemoScene.umap に合わせる。
	const FName NEXT_LEVEL_NAME = TEXT("DemoScene");
}

AStartBox::AStartBox()
{
	PrimaryActorTick.bCanEverTick = true; // ふわふわ浮遊のため毎フレーム更新

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DefaultMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DefaultMesh"));
	DefaultMesh->SetupAttachment(Root);
	DefaultMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 切断後の2ピースは初期非表示・物理オフ
	PieceMeshA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceMeshA"));
	PieceMeshA->SetupAttachment(Root);
	PieceMeshA->SetVisibility(false);
	PieceMeshA->SetSimulatePhysics(false);
	PieceMeshA->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	PieceMeshB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PieceMeshB"));
	PieceMeshB->SetupAttachment(Root);
	PieceMeshB->SetVisibility(false);
	PieceMeshB->SetSimulatePhysics(false);
	PieceMeshB->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HitBox = CreateDefaultSubobject<UBoxComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(Root);
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetCollisionProfileName(TEXT("OverlapAll"));
	HitBox->SetGenerateOverlapEvents(true);

	StartWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("StartWidget"));
	StartWidget->SetupAttachment(Root);
	// TODO: BP_StartBox で START テキストの UUserWidget を WidgetClass に割り当てる。

	// 箱の表面に書く「START」3Dテキスト（青）。UMGオーバーレイではなく実体の文字なので
	// パススルーでも自然に見える。位置/向き/サイズは BP_StartBox/エディタで箱の面に合わせて調整する。
	StartText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StartText"));
	StartText->SetupAttachment(Root);
	StartText->SetText(FText::FromString(TEXT("START")));
	StartText->SetTextRenderColor(FColor(0, 102, 255)); // 青
	StartText->SetHorizontalAlignment(EHTA_Center);
	StartText->SetVerticalAlignment(EVRTA_TextCenter);
	StartText->SetWorldSize(150.f);
	StartText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 前面に少し出した初期姿勢（エディタで箱の面にぴったり合わせて微調整する想定）。
	StartText->SetRelativeLocation(FVector(-55.f, 0.f, 0.f));
	StartText->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
}

void AStartBox::BeginPlay()
{
	Super::BeginPlay();

	HitBox->OnComponentBeginOverlap.AddDynamic(this, &AStartBox::OnHitBoxBeginOverlap);

	if (Mesh_Default)
	{
		DefaultMesh->SetStaticMesh(Mesh_Default);
	}

	// 浮遊の基準位置を保持。
	InitialLocation = GetActorLocation();
}

void AStartBox::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 初期位置を中心に上下へゆっくりサイン揺れさせる（ふわふわ浮遊）。
	if (bFloatEnabled && FloatPeriod > KINDA_SMALL_NUMBER)
	{
		FloatTime += DeltaTime;
		const float Omega = 2.f * PI / FloatPeriod;
		const float ZOffset = FloatAmplitude * FMath::Sin(FloatTime * Omega);
		SetActorLocation(InitialLocation + FVector(0.f, 0.f, ZOffset));
	}
}

void AStartBox::OnHitBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 診断: 箱に何かが重なったら必ずログ
	UE_LOG(LogTemp, Warning, TEXT("AStartBox: HitBox overlap by %s (comp %s)"),
		*GetNameSafe(OtherActor), *GetNameSafe(OtherComp));

	if (bCut)
	{
		return;
	}

	// 剣以外との接触は無視
	ATitleSword* Sword = Cast<ATitleSword>(OtherActor);
	if (!Sword)
	{
		UE_LOG(LogTemp, Warning, TEXT("AStartBox: overlap is not ATitleSword -> ignore"));
		return;
	}

	const FVector SwingVelocity = Sword->GetSwingVelocity();
	UE_LOG(LogTemp, Warning, TEXT("AStartBox: sword swing speed = %.1f (threshold %.1f)"),
		SwingVelocity.Size(), MinSwingSpeed);
	if (SwingVelocity.Size() < MinSwingSpeed)
	{
		// スイングが弱すぎる場合は切断しない
		UE_LOG(LogTemp, Warning, TEXT("AStartBox: swing too slow -> no cut"));
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("AStartBox: CUT! -> transition to next level in %.1fs"), TravelDelay);
	bCut = true;

	// 斬撃音（BP_StartBox の CutSound に割り当て）を切断位置で再生。
	if (CutSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CutSound, GetActorLocation());
	}

	const ECutType CutType = ResolveCutType(SwingVelocity);
	PerformCut(CutType);

	// START テキストを非表示
	if (StartWidget)
	{
		StartWidget->SetVisibility(false);
	}
	if (StartText)
	{
		StartText->SetVisibility(false);
	}

	// 一定時間後にゲームレベルへ遷移
	GetWorldTimerManager().SetTimer(TravelTimerHandle, this, &AStartBox::TravelToNextLevel, TravelDelay, false);
}

ECutType AStartBox::ResolveCutType(const FVector& SwingVelocity) const
{
	// BOX のローカル軸（右=Y, 上=Z）に速度を投影し、スラッシュ線の角度で分類する。
	const float Right = FVector::DotProduct(SwingVelocity, GetActorRightVector());
	const float Up = FVector::DotProduct(SwingVelocity, GetActorUpVector());

	// スラッシュ線は向きを持たない（180度周期）ので [0,180) に正規化
	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Up, Right));
	if (AngleDeg < 0.f)
	{
		AngleDeg += 180.f;
	}

	// 0/180付近 = 水平、45付近 = 右上↔左下、135付近 = 左上↔右下
	if (AngleDeg < 30.f || AngleDeg >= 150.f)
	{
		return ECutType::Horizontal;
	}
	if (AngleDeg < 90.f)
	{
		return ECutType::DiagTopRightToBotLeft;
	}
	return ECutType::DiagTopLeftToBotRight;
}

void AStartBox::PerformCut(ECutType CutType)
{
	DefaultMesh->SetVisibility(false);

	UStaticMesh* MeshA = nullptr;
	UStaticMesh* MeshB = nullptr;
	FVector DirA = FVector::ZeroVector; // ピースAの分離方向
	FVector DirB = FVector::ZeroVector; // ピースBの分離方向

	const FVector Up = GetActorUpVector();
	const FVector RightV = GetActorRightVector();

	switch (CutType)
	{
	case ECutType::Horizontal:
		MeshA = Mesh_H_Top;
		MeshB = Mesh_H_Bottom;
		DirA = Up;
		DirB = -Up;
		break;

	case ECutType::DiagTopRightToBotLeft:
		MeshA = Mesh_D_TopRight;
		MeshB = Mesh_D_BotLeft;
		DirA = (RightV + Up).GetSafeNormal();
		DirB = (-RightV - Up).GetSafeNormal();
		break;

	case ECutType::DiagTopLeftToBotRight:
		MeshA = Mesh_D_TopLeft;
		MeshB = Mesh_D_BotRight;
		DirA = (-RightV + Up).GetSafeNormal();
		DirB = (RightV - Up).GetSafeNormal();
		break;
	}

	auto SetupPiece = [this](UStaticMeshComponent* Piece, UStaticMesh* Mesh, const FVector& ImpulseDir)
	{
		if (!Piece)
		{
			return;
		}
		if (Mesh)
		{
			Piece->SetStaticMesh(Mesh);
		}
		// サイズ・向きの補正（断片メッシュが大きい/90°ずれている場合の調整）。
		// DefaultMesh の相対スケール・回転を基準に合わせ、断片が「切る前の箱」と一致するようにする。
		// 残差は PieceScaleMultiplier（倍率）/ PieceYawCorrection（度）で微調整する。
		const FVector  BaseScale = DefaultMesh ? DefaultMesh->GetRelativeScale3D() : FVector::OneVector;
		const FRotator BaseRot   = DefaultMesh ? DefaultMesh->GetRelativeRotation() : FRotator::ZeroRotator;
		Piece->SetRelativeScale3D(BaseScale * PieceScaleMultiplier);
		Piece->SetRelativeRotation(BaseRot + FRotator(0.f, PieceYawCorrection, 0.f));
		Piece->SetVisibility(true);
		// 物理を有効化して分離Impulseをかける
		Piece->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Piece->SetCollisionProfileName(TEXT("PhysicsActor"));
		Piece->SetSimulatePhysics(true);
		// bVelChange=true で質量に依存しない速度変化として扱う（軽い分離）
		Piece->AddImpulse(ImpulseDir * SeparationImpulse, NAME_None, true);
	};

	SetupPiece(PieceMeshA, MeshA, DirA);
	SetupPiece(PieceMeshB, MeshB, DirB);
}

void AStartBox::TravelToNextLevel()
{
	// テスト用ループ: 遷移せず箱を元に戻して繰り返し斬れるようにする。
	// 本番化は bTestLoopMode を false にするだけ（この分岐が唯一の切替点）。
	if (bTestLoopMode)
	{
		UE_LOG(LogTemp, Log, TEXT("AStartBox: test loop -> reset box (no level travel)"));
		ResetBox();
		return;
	}

	// エディタ/BP で NextLevel が設定されていればそれを開く。未設定ならフォールバック。
	if (!NextLevel.IsNull())
	{
		UE_LOG(LogTemp, Log, TEXT("AStartBox: OpenLevel (soft) %s"), *NextLevel.ToString());
		UGameplayStatics::OpenLevelBySoftObjectPtr(this, NextLevel);
		return;
	}

	UE_LOG(LogTemp, Warning, TEXT("AStartBox: NextLevel 未設定 -> フォールバックで %s を開く"), *NEXT_LEVEL_NAME.ToString());
	UGameplayStatics::OpenLevel(this, NEXT_LEVEL_NAME);
}

void AStartBox::ResetBox()
{
	auto ResetPiece = [this](UStaticMeshComponent* Piece)
	{
		if (!Piece)
		{
			return;
		}
		// 物理を止める（速度も解除される）。
		Piece->SetSimulatePhysics(false);
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Piece->SetVisibility(false);
		// 物理シミュレーションでワールド空間駆動になった状態を解消し、Root へ追従し直す。
		// これをしないと2回目以降、親(アクター)のスケール(例:0.01)・回転(例:Yaw90)を
		// 継承せず、ワールド原寸(約100倍)・回転ズレでピースが出現してしまう。
		Piece->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		// 位置0・回転0・スケール1 へ完全初期化（スケール戻し漏れ対策）。
		Piece->SetRelativeTransform(FTransform::Identity);
	};
	ResetPiece(PieceMeshA);
	ResetPiece(PieceMeshB);

	if (DefaultMesh)
	{
		DefaultMesh->SetVisibility(true);
	}
	if (StartWidget)
	{
		StartWidget->SetVisibility(true);
	}
	if (StartText)
	{
		StartText->SetVisibility(true);
	}

	// 再度斬れるようにフラグを戻す。
	bCut = false;
}
