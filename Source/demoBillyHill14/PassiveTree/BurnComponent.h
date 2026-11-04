// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "BurnComponent.generated.h"

/**
 * Deals damage to its owner over time. New burns add to the damage still waiting and restart the timer, so
 * no damage is lost, and repeated hits make the burn last longer. Effects add this to an actor as needed.
 */
UCLASS()
class DEMOBILLYHILL14_API UBurnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBurnComponent();

	// Finds the actor's burn, or adds one if it doesn't have one yet, then adds this damage to it.
	static void ApplyBurn(AActor* Target, float Damage, float Duration);

private:
	void AddBurn(float Damage, float Duration);
	void DealBurnDamage();

	float DamageLeft = 0.f;
	float TimeLeft = 0.f;
	FTimerHandle BurnTimer;
};
