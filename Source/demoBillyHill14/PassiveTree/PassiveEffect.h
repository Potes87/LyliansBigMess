// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PassiveEffect.generated.h"

class AActor;

/**
 * Behaviour that plain stat modifiers can't express, such as a keystone. A node names one of these classes in
 * its Effect field. The tree creates it when the node is allocated and removes it when the node is refunded.
 * Subclass it in C++ or Blueprint and override only the events you need.
 */
UCLASS(Abstract, Blueprintable)
class DEMOBILLYHILL14_API UPassiveEffect : public UObject
{
	GENERATED_BODY()

public:
	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintNativeEvent, Category = "Passive Effect")
	void OnActivated();

	UFUNCTION(BlueprintNativeEvent, Category = "Passive Effect")
	void OnDeactivated();

	// Runs just before a bullet spawns. Returns the damage that bullet should carry.
	UFUNCTION(BlueprintNativeEvent, Category = "Passive Effect")
	float OnShotFired(float Damage);

	// Runs when a bullet reaches an enemy, before the damage lands. Returns the damage to deal.
	UFUNCTION(BlueprintNativeEvent, Category = "Passive Effect")
	float OnEnemyHit(AActor* Enemy, float Damage);

	// Runs when a reload finishes and the magazine is full.
	UFUNCTION(BlueprintNativeEvent, Category = "Passive Effect")
	void OnReloaded();

	// Return false to stop the gun firing right now.
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Passive Effect")
	bool CanFireShot() const;

	// Takes the ammo a shot would cost, and returns what it should cost instead.
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Passive Effect")
	int32 GetAmmoCost(int32 Cost) const;

	// Return false to stop the gun reloading.
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Passive Effect")
	bool CanReload() const;

	// Returns how much ammo to add this frame. Fractions build up until they make a whole round.
	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Passive Effect")
	float GetAmmoRegen(float DeltaSeconds, int32 MagazineSize) const;

	// The actor that owns the passive tree, which is the player's character.
	UFUNCTION(BlueprintPure, Category = "Passive Effect")
	AActor* GetOwningActor() const;
};

/**
 * Keystone: the first shot after a reload deals far more damage.
 */
UCLASS()
class DEMOBILLYHILL14_API UPassiveEffect_OneCleanShot : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual float OnShotFired_Implementation(float Damage) override;
	virtual void OnReloaded_Implementation() override;

	// What the first shot's damage is multiplied by.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "One Clean Shot", meta = (ClampMin = "0"))
	float FirstShotMultiplier = 4.f;

private:
	bool bIsShotReady = false;
};

/**
 * Keystone: hits deal more damage up close and less damage far away.
 */
UCLASS()
class DEMOBILLYHILL14_API UPassiveEffect_PointBlank : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual float OnEnemyHit_Implementation(AActor* Enemy, float Damage) override;

	// Enemies this close, in Unreal units, take CloseMultiplier. 100 units is 1 metre.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Point Blank", meta = (ClampMin = "0"))
	float CloseRange = 600.f;

	// Enemies further than this take FarMultiplier.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Point Blank", meta = (ClampMin = "0"))
	float FarRange = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Point Blank", meta = (ClampMin = "0"))
	float CloseMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Point Blank", meta = (ClampMin = "0"))
	float FarMultiplier = 0.5f;
};

/**
 * Keystone: shots cost health instead of ammo, and hits give some back. A shot that would kill you won't fire.
 *
 * The player's health lives in a Blueprint component, so make a Blueprint child of this class and implement the
 * three health events. The node's Effect field names that child.
 */
UCLASS(Abstract)
class DEMOBILLYHILL14_API UPassiveEffect_BloodRounds : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual float OnShotFired_Implementation(float Damage) override;
	virtual float OnEnemyHit_Implementation(AActor* Enemy, float Damage) override;
	virtual bool CanFireShot_Implementation() const override;
	virtual int32 GetAmmoCost_Implementation(int32 Cost) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blood Rounds", meta = (ClampMin = "0"))
	float HealthPerShot = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Blood Rounds", meta = (ClampMin = "0"))
	float HealthPerHit = 3.f;

protected:
	UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "Blood Rounds")
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Blood Rounds")
	void SpendHealth(float Amount);

	UFUNCTION(BlueprintImplementableEvent, Category = "Blood Rounds")
	void RestoreHealth(float Amount);
};

/**
 * Keystone: the gun can't reload, and refills on its own once you stop firing.
 */
UCLASS()
class DEMOBILLYHILL14_API UPassiveEffect_BottomlessMag : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual float OnShotFired_Implementation(float Damage) override;
	virtual bool CanReload_Implementation() const override;
	virtual float GetAmmoRegen_Implementation(float DeltaSeconds, int32 MagazineSize) const override;

	// How much of the magazine comes back each second: 0.2 refills an empty magazine in 5 seconds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bottomless Mag", meta = (ClampMin = "0"))
	float RegenPerSecond = 0.2f;

	// How long after a shot the refill starts, in seconds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Bottomless Mag", meta = (ClampMin = "0"))
	float RegenDelay = 0.5f;

private:
	double LastShotTime = -1000.0;
};

/**
 * Keystone: damage builds up for as long as you hold your fire, and the next shot spends it.
 */
UCLASS()
class DEMOBILLYHILL14_API UPassiveEffect_SteadyBreath : public UPassiveEffect
{
	GENERATED_BODY()

public:
	virtual float OnShotFired_Implementation(float Damage) override;

	// How long without firing it takes to reach the full bonus, in seconds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steady Breath", meta = (ClampMin = "0.01"))
	float ChargeTime = 3.f;

	// The bonus at full charge: 1 means 100% more damage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Steady Breath", meta = (ClampMin = "0"))
	float MaxDamageBonus = 1.f;

private:
	double LastShotTime = -1000.0;
};
