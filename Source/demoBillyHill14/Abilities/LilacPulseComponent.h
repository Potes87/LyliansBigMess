// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Templates/SubclassOf.h"
#include "LilacPulseComponent.generated.h"

class UCameraShakeBase;
class UNiagaraSystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnLilacPulseFired);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnLilacPulseHit, AActor*, Enemy, float, Damage, bool, bKilled);

/**
 * Lilac Pulse: a short-range burst of lilac force in a cone in front of the camera. It doesn't touch reloading,
 * sprinting or dashing, so it can be used at any time. Normal (white) enemies die and fly; Rare, Elite and Unique
 * enemies take FixedDamage and don't move. It can't crit and never triggers gun passives.
 * Add this to the player character and call TryPulse from the F input.
 * Enemies are recognised by their Blueprint "Tier" variable (EEnemyTier).
 */
UCLASS(ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API ULilacPulseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULilacPulseComponent();

	// Fires the pulse if it's off cooldown. Returns true if it fired.
	UFUNCTION(BlueprintCallable, Category = "Lilac Pulse")
	bool TryPulse();

	UFUNCTION(BlueprintPure, Category = "Lilac Pulse")
	bool IsReady() const;

	// 0 right after a pulse, 1 when ready again. For the HUD icon.
	UFUNCTION(BlueprintPure, Category = "Lilac Pulse")
	float GetCooldownProgress() const;

	// Fires on every pulse, hit or miss. Play the pulse sound here.
	UPROPERTY(BlueprintAssignable, Category = "Lilac Pulse")
	FOnLilacPulseFired OnPulseFired;

	// Fires once per enemy hit. Good for hit and kill markers.
	UPROPERTY(BlueprintAssignable, Category = "Lilac Pulse")
	FOnLilacPulseHit OnPulseHit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse", meta = (ClampMin = "0.1"))
	float Cooldown = 4.f;

	// How far the pulse reaches from the camera, in Unreal units (100 = 1 m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse", meta = (ClampMin = "0"))
	float Range = 350.f;

	// Half the cone's width in degrees. 55 makes a 110 degree cone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse", meta = (ClampMin = "1", ClampMax = "180"))
	float ConeHalfAngle = 55.f;

	// Damage to Rare, Elite and Unique enemies. Normal enemies always die.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse", meta = (ClampMin = "0"))
	float FixedDamage = 40.f;

	// Used on Normal enemies only if their current health can't be read.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse", meta = (ClampMin = "0"))
	float KillDamage = 10000.f;

	// Speed given to a Normal enemy's ragdoll, in units per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse", meta = (ClampMin = "0"))
	float KnockbackSpeed = 2600.f;

	// How much of the knockback goes upward, 0 to 1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse", meta = (ClampMin = "0", ClampMax = "1"))
	float KnockbackLift = 0.4f;

	// Spawned in front of the camera, facing where the player looks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse|Feel")
	TObjectPtr<UNiagaraSystem> PulseEffect;

	// How far in front of the camera the effect spawns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse|Feel")
	float EffectForwardOffset = 60.f;

	// The camera punch. Defaults to ULilacPulseCameraShake.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse|Feel")
	TSubclassOf<UCameraShakeBase> PulseShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse|Feel", meta = (ClampMin = "0"))
	float ShakeScale = 1.f;

	// Draws the cone and hits for 2 seconds. Testing only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lilac Pulse|Debug")
	bool bDrawDebug = false;

private:
	bool GetView(FVector& OutLocation, FRotator& OutRotation) const;
	void HitEnemy(AActor* Enemy, uint8 Tier, const FVector& From);
	static bool ReadTier(const AActor* Actor, uint8& OutTier);
	static bool ReadCurrentHealth(const AActor* Actor, float& OutHealth);
	void FlingRagdoll(TWeakObjectPtr<AActor> Enemy, FVector Velocity, bool bRetry);

	FTimerHandle CooldownTimer;
};
