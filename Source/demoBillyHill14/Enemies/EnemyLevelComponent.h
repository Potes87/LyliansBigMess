// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyLevelComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyLevelSignal, int32, Level);

/**
 * Gives an enemy a level and scales its stats once, when it spawns. Lives on BP_Enemy_Base, so every enemy has it.
 *
 * Where the level comes from (first match wins):
 *   1. LevelOverride on this placed enemy (0 = not set). For a mini-boss guarding a key, etc.
 *   2. The highest-priority AEnemyLevelZone box the enemy starts inside (+ that zone's random variance).
 *   3. DefaultLevel (1).
 *
 * What it scales (the numbers you set on each enemy Blueprint are its level 1 stats):
 *   Health  x HealthGrowth ^ (Level - 1)      compound: punishing on purpose
 *   Damage  x (1 + DamagePerLevel * (Level - 1))   straight line: moderate
 *   XP      x (1 + XPPerLevel * (Level - 1))       straight line: low
 * It edits the Blueprint values "BaseMaxHealth" on the health component, "AttackDamage" and "XPReward" on the enemy.
 * Projectiles and other damage that doesn't read AttackDamage can multiply by GetDamageMultiplierFor(Enemy).
 *
 * Set the growth numbers once on BP_Enemy_Base's component and every enemy follows them.
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UEnemyLevelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyLevelComponent();

	// Fired once after the level is decided and the stats are scaled (for an "Lv 4" label on the health bar).
	UPROPERTY(BlueprintAssignable, Category = "Enemy Level")
	FEnemyLevelSignal OnLevelApplied;

	UFUNCTION(BlueprintPure, Category = "Enemy Level")
	int32 GetLevel() const { return Level; }

	UFUNCTION(BlueprintPure, Category = "Enemy Level")
	float GetHealthMultiplier() const { return HealthMult; }

	UFUNCTION(BlueprintPure, Category = "Enemy Level")
	float GetDamageMultiplier() const { return DamageMult; }

	UFUNCTION(BlueprintPure, Category = "Enemy Level")
	float GetXPMultiplier() const { return XPMult; }

	// Damage multiplier of any enemy (1 if it has no level component). For projectiles: Damage x this, using the shooter.
	UFUNCTION(BlueprintPure, Category = "Enemy Level")
	static float GetDamageMultiplierFor(const AActor* Enemy);

	// Level of any enemy (1 if it has no level component).
	UFUNCTION(BlueprintPure, Category = "Enemy Level")
	static int32 GetLevelFor(const AActor* Enemy);

	// ---- This enemy ----

	// Force this placed enemy to a level, ignoring zones. 0 = use the zone (or DefaultLevel).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Level", meta = (ClampMin = "0"))
	int32 LevelOverride = 0;

	// Level used outside every zone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Level", meta = (ClampMin = "1"))
	int32 DefaultLevel = 1;

	// ---- Scaling (set these on BP_Enemy_Base so every enemy shares them) ----

	// Health is multiplied by this once per level above 1. 1.12 = +12% compounding: lv 5 x1.6, lv 10 x2.8, lv 15 x4.9, lv 20 x8.6, lv 30 x27.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Level|Scaling", meta = (ClampMin = "1"))
	float HealthGrowth = 1.12f;

	// Damage gained per level above 1, as a fraction of level 1 damage. 0.2: lv 5 x1.8, lv 10 x2.8, lv 15 x3.8, lv 20 x4.8, lv 30 x6.8.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Level|Scaling", meta = (ClampMin = "0"))
	float DamagePerLevel = 0.2f;

	// XP gained per level above 1, as a fraction of level 1 XP. 0.1: lv 10 x1.9, lv 20 x2.9, lv 30 x3.9.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Level|Scaling", meta = (ClampMin = "0"))
	float XPPerLevel = 0.1f;

	// Highest level anything can be, whatever the zone says.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Level|Scaling", meta = (ClampMin = "1"))
	int32 MaxLevel = 50;

protected:
	virtual void BeginPlay() override;
	// Every enemy has this component, so it is also what tells the respawn system an enemy started or ended.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	int32 DecideLevel() const;
	void ApplyHealth();

	int32 Level = 1;
	float HealthMult = 1.f;
	float DamageMult = 1.f;
	float XPMult = 1.f;
	bool bApplied = false;
};
