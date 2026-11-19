// Fill out your copyright notice in the Description page of Project Settings.

#include "CheckpointShrine.h"

#include "Components/ArrowComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "RespawnSubsystem.h"

ACheckpointShrine::ACheckpointShrine()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	TouchRange = CreateDefaultSubobject<USphereComponent>(TEXT("TouchRange"));
	TouchRange->SetupAttachment(Root);
	TouchRange->SetSphereRadius(220.f);
	TouchRange->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	TouchRange->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TouchRange->SetCollisionObjectType(ECC_WorldDynamic);
	TouchRange->SetCollisionResponseToAllChannels(ECR_Ignore);
	TouchRange->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TouchRange->SetGenerateOverlapEvents(true);
	TouchRange->SetCanEverAffectNavigation(false);

	WakePoint = CreateDefaultSubobject<UArrowComponent>(TEXT("WakePoint"));
	WakePoint->SetupAttachment(Root);
	WakePoint->SetRelativeLocation(FVector(160.f, 0.f, 110.f));   // in front of the shrine, about standing height
	WakePoint->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));    // facing the shrine
}

void ACheckpointShrine::BeginPlay()
{
	Super::BeginPlay();
	// AddUnique: World Partition can stream a shrine out and back in while the same actor object is still alive,
	// which runs BeginPlay a second time. A plain AddDynamic then trips an ensure (already bound).
	TouchRange->OnComponentBeginOverlap.AddUniqueDynamic(this, &ACheckpointShrine::HandleOverlap);
}

FName ACheckpointShrine::GetShrineId() const
{
	return ShrineId.IsNone() ? GetFName() : ShrineId;
}

bool ACheckpointShrine::IsCurrent() const
{
	const URespawnSubsystem* Respawn = URespawnSubsystem::Get(this);
	return Respawn && Respawn->HasCheckpoint() && Respawn->GetCheckpointId() == GetShrineId();
}

void ACheckpointShrine::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bActivateOnTouch) return;
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn && Pawn->IsPlayerControlled())
	{
		Activate(Pawn);
	}
}

void ACheckpointShrine::Activate(APawn* Player)
{
	UWorld* World = GetWorld();
	URespawnSubsystem* Respawn = URespawnSubsystem::Get(this);
	if (!World || !Respawn) return;

	const double Now = World->GetTimeSeconds();
	if (Now - LastActivated < RearmSeconds) return;
	LastActivated = Now;

	Respawn->ActivateCheckpoint(GetShrineId(), WakePoint->GetComponentTransform(), Player);
	OnTouched(Player);
}
