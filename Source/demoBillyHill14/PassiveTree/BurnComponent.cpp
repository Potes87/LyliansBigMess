// Fill out your copyright notice in the Description page of Project Settings.

#include "BurnComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	// Seconds between damage numbers.
	constexpr float BurnInterval = 0.5f;
}

UBurnComponent::UBurnComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBurnComponent::ApplyBurn(AActor* Target, float Damage, float Duration)
{
	if (!Target || Damage <= 0.f || Duration <= 0.f) return;

	UBurnComponent* Burn = Target->FindComponentByClass<UBurnComponent>();
	if (!Burn)
	{
		Burn = NewObject<UBurnComponent>(Target);
		Burn->RegisterComponent();
	}
	Burn->AddBurn(Damage, Duration);
}

void UBurnComponent::AddBurn(float Damage, float Duration)
{
	DamageLeft += Damage;
	TimeLeft = Duration;

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	if (!TimerManager.IsTimerActive(BurnTimer))
	{
		TimerManager.SetTimer(BurnTimer, this, &UBurnComponent::DealBurnDamage, BurnInterval, true);
	}
}

void UBurnComponent::DealBurnDamage()
{
	// The last interval takes everything that's left.
	const bool bIsLastInterval = TimeLeft <= BurnInterval;
	const float Damage = bIsLastInterval ? DamageLeft : DamageLeft * (BurnInterval / TimeLeft);

	DamageLeft -= Damage;
	TimeLeft -= BurnInterval;

	if (bIsLastInterval)
	{
		DamageLeft = 0.f;
		GetWorld()->GetTimerManager().ClearTimer(BurnTimer);
	}

	UGameplayStatics::ApplyDamage(GetOwner(), Damage, nullptr, nullptr, UDamageType::StaticClass());
}
