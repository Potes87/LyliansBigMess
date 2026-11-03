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
