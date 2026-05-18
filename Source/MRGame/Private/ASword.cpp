// Fill out your copyright notice in the Description page of Project Settings.


#include "ASword.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
// Sets default values
ASword::ASword()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
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
	//debug機能
	SwordColl->SetHiddenInGame(false);
	SwordColl->OnComponentBeginOverlap.AddDynamic(this, &ASword::OnHitBoxBeginOverlap);
	// //剣の根本
	// BladeBase = CreateDefaultSubobject<USceneComponent>(TEXT("BladeBase"));
	// BladeBase->SetupAttachment(BladeMesh);
	// //剣の先端
	// BladeTip = CreateDefaultSubobject<USceneComponent>(TEXT("BladeTip"));
	// BladeTip->SetupAttachment(BladeMesh);
}

void ASword::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}


void ASword::OnHitBoxBeginOverlap(UPrimitiveComponent* OVerlappedComp, AActor* OtherActor,
                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
                                  const FHitResult& SweepResult)
{
	UE_LOG(LogTemp, Log, TEXT("CallHitBox"));
}
