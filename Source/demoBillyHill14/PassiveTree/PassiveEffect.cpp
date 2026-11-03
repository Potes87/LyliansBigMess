// Fill out your copyright notice in the Description page of Project Settings.

#include "PassiveEffect.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"

UWorld* UPassiveEffect::GetWorld() const
{
	// The class default object has no world, and the editor checks it to decide which Blueprint nodes to offer.
	if (HasAnyFlags(RF_ClassDefaultObject)) return nullptr;

	const AActor* OwningActor = GetOwningActor();
	return OwningActor ? OwningActor->GetWorld() : nullptr;
}

AActor* UPassiveEffect::GetOwningActor() const
{
	const UActorComponent* OwningComponent = Cast<UActorComponent>(GetOuter());
	return OwningComponent ? OwningComponent->GetOwner() : nullptr;
}

void UPassiveEffect::OnActivated_Implementation()
{
}

void UPassiveEffect::OnDeactivated_Implementation()
{
}

float UPassiveEffect::OnShotFired_Implementation(float Damage)
{
	return Damage;
}

float UPassiveEffect::OnEnemyHit_Implementation(AActor* Enemy, float Damage)
{
	return Damage;
}

void UPassiveEffect::OnReloaded_Implementation()
{
}

float UPassiveEffect_OneCleanShot::OnShotFired_Implementation(float Damage)
{
	if (!bIsShotReady) return Damage;

	bIsShotReady = false;
	return Damage * FirstShotMultiplier;
}

void UPassiveEffect_OneCleanShot::OnReloaded_Implementation()
{
	bIsShotReady = true;
}

float UPassiveEffect_PointBlank::OnEnemyHit_Implementation(AActor* Enemy, float Damage)
{
	const AActor* OwningActor = GetOwningActor();
	if (!OwningActor || !Enemy) return Damage;

	const float Distance = OwningActor->GetDistanceTo(Enemy);
	if (Distance <= CloseRange) return Damage * CloseMultiplier;
	if (Distance >= FarRange) return Damage * FarMultiplier;
	return Damage;
}
