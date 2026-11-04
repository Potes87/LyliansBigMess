// Fill out your copyright notice in the Description page of Project Settings.

#include "PassiveEffect.h"
#include "BurnComponent.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
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

bool UPassiveEffect::CanFireShot_Implementation() const
{
	return true;
}

int32 UPassiveEffect::GetAmmoCost_Implementation(int32 Cost) const
{
	return Cost;
}

bool UPassiveEffect::CanReload_Implementation() const
{
	return true;
}

float UPassiveEffect::GetAmmoRegen_Implementation(float DeltaSeconds, int32 MagazineSize) const
{
	return 0.f;
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

float UPassiveEffect_IncendiaryRounds::OnEnemyHit_Implementation(AActor* Enemy, float Damage)
{
	if (!Enemy) return Damage;

	const float BurnDamage = Damage * BurnFraction;
	UBurnComponent::ApplyBurn(Enemy, BurnDamage, BurnDuration);
	return Damage - BurnDamage;
}

float UPassiveEffect_BloodRounds::OnShotFired_Implementation(float Damage)
{
	SpendHealth(HealthPerShot);
	return Damage;
}

float UPassiveEffect_BloodRounds::OnEnemyHit_Implementation(AActor* Enemy, float Damage)
{
	RestoreHealth(HealthPerHit);
	return Damage;
}

bool UPassiveEffect_BloodRounds::CanFireShot_Implementation() const
{
	return GetCurrentHealth() > HealthPerShot;
}

int32 UPassiveEffect_BloodRounds::GetAmmoCost_Implementation(int32 Cost) const
{
	return 0;
}

float UPassiveEffect_BottomlessMag::OnShotFired_Implementation(float Damage)
{
	if (const UWorld* World = GetWorld())
	{
		LastShotTime = World->GetTimeSeconds();
	}
	return Damage;
}

bool UPassiveEffect_BottomlessMag::CanReload_Implementation() const
{
	return false;
}

float UPassiveEffect_BottomlessMag::GetAmmoRegen_Implementation(float DeltaSeconds, int32 MagazineSize) const
{
	const UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() - LastShotTime < RegenDelay) return 0.f;

	return MagazineSize * RegenPerSecond * DeltaSeconds;
}

float UPassiveEffect_SteadyBreath::OnShotFired_Implementation(float Damage)
{
	const UWorld* World = GetWorld();
	if (!World) return Damage;

	const double CurrentTime = World->GetTimeSeconds();
	const float Charge = FMath::Clamp(static_cast<float>((CurrentTime - LastShotTime) / ChargeTime), 0.f, 1.f);
	LastShotTime = CurrentTime;

	return Damage * (1.f + MaxDamageBonus * Charge);
}
