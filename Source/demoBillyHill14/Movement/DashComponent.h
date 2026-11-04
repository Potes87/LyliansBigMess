// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "DashComponent.generated.h"

/**
 * Gives a character a quick burst of movement, on the ground or in the air. It goes the way the player is moving,
 * or forward if they aren't moving. Add this to a Character and call TryDash from an input event.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UDashComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDashComponent();

	// Dashes if a charge is available. Returns true if it did.
	UFUNCTION(BlueprintCallable, Category = "Dash")
	bool TryDash();

	UFUNCTION(BlueprintPure, Category = "Dash")
	int32 GetChargesLeft() const { return ChargesLeft; }

	// How far the current charge is through its cooldown, from 0 to 1. Returns 1 when no charge is recovering.
	UFUNCTION(BlueprintPure, Category = "Dash")
	float GetCooldownProgress() const;

	// How far a dash travels, in Unreal units. 100 units is 1 metre.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0"))
	float DashDistance = 650.f;

	// How long a dash takes, in seconds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01"))
	float DashDuration = 0.2f;

	// How long one charge takes to come back, in seconds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "0.01"))
	float Cooldown = 2.f;

	// How many dashes can be saved up.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash", meta = (ClampMin = "1"))
	int32 MaxCharges = 1;

protected:
	virtual void BeginPlay() override;

private:
	void RestoreCharge();

	int32 ChargesLeft = 0;
	FTimerHandle CooldownTimer;
};
