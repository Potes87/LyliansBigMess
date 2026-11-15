// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyLevelZone.h"

#include "Components/BoxComponent.h"

AEnemyLevelZone::AEnemyLevelZone()
{
	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetBoxExtent(FVector(2000.f, 2000.f, 1000.f)); // 40 x 40 x 20 m to start
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);
	Box->SetGenerateOverlapEvents(false);
	Box->ShapeColor = FColor(201, 140, 60); // gold outline in the editor
	Box->SetLineThickness(6.f);
	SetActorHiddenInGame(true);
}

bool AEnemyLevelZone::ContainsPoint(const FVector& Point) const
{
	if (!Box) return false;
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(Point);
	const FVector Extent = Box->GetUnscaledBoxExtent();
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}
