// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "RangedBrainComponent.generated.h"

class UEnemyAwarenessComponent;
class AAIController;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRangedWindUpSignal, bool, bBurst);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRangedFireSignal, AActor*, Projectile);

/**
 * Movement + shooting brain for a mobile ranged enemy (the gunner). Add it to the enemy Blueprint next to the
 * Awareness component, turn Awareness "Drive Movement" OFF, and give the enemy a plain AIController (not
 * AIC_Enemy / AIC_EnemyRanged, those have their own move and attack logic).
 *
 * Awareness decides WHEN it is awake and what it can see. This component decides what it does while awake:
 *  - Range: stays between MinRange and MaxRange from the player. Backs off if rushed, closes in if they retreat.
 *  - Strafe: always side-steps around the player, switching direction at random, facing the player while it moves.
 *  - Lost sight: walks to where the player was last seen. Gives up after Awareness' GiveUpTime and walks home.
 *  - Shoot: glow wind-up (OnWindUp), then fires ProjectileClass at where the player IS (no leading), so any
 *    movement dodges it. Never fires without line of sight.
 *  - Standing still punish: if the player has barely moved for StillTimeToPunish seconds, the next attack is a
 *    BurstCount-shot burst with a shorter wind-up. Moving players get single, dodgeable shots.
 * The enemy is passed to the projectile as its Owner and Instigator, so enemy level damage scaling works.
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API URangedBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URangedBrainComponent();

	// Wind-up started (bBurst = the punishing burst is coming). Use for the glow, a charge sound, an aim animation.
	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedWindUpSignal OnWindUp;

	// A projectile was just fired (once per shot, so 3 times in a burst). Use for muzzle flash, sound, recoil anim.
	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedFireSignal OnFire;

	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	bool IsWindingUp() const { return bWindingUp; }

	// True while winding up or in the middle of a burst.
	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	bool IsShooting() const { return bWindingUp || ShotsLeft > 0; }

	// How long the player has been (nearly) standing still, in seconds.
	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	float GetPlayerStillTime() const { return StillTime; }

	// ---- Range (100 units = 1 metre) ----

	// Closer than this and it backs away.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Range", meta = (ClampMin = "0"))
	float MinRange = 700.f;

	// The distance it walks back to when it is too close or too far.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Range", meta = (ClampMin = "0"))
	float PreferredRange = 1000.f;

	// Further than this and it closes in.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Range", meta = (ClampMin = "0"))
	float MaxRange = 1400.f;

	// It only shoots when the player is within this distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Range", meta = (ClampMin = "0"))
	float MaxShootRange = 2500.f;

	// ---- Strafe ----

	// How far sideways it aims each step. Bigger = faster circling.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Strafe", meta = (ClampMin = "0"))
	float StrafeStep = 400.f;

	// It switches strafe direction after a random time between these (seconds).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Strafe", meta = (ClampMin = "0.1"))
	float StrafeSwitchMin = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Strafe", meta = (ClampMin = "0.1"))
	float StrafeSwitchMax = 2.0f;

	// Keeps facing the player while moving (turns off "orient to movement" on the character).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Strafe")
	bool bFacePlayer = true;

	// ---- Shooting ----

	// What it fires. Make a child or copy of BP_EnemyProjectile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting")
	TSubclassOf<AActor> ProjectileClass;

	// Projectile speed in units per second. 0 = keep the projectile's own speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting", meta = (ClampMin = "0"))
	float ProjectileSpeed = 3000.f;

	// Seconds of glow warning before a normal shot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting", meta = (ClampMin = "0"))
	float WindUpTime = 0.35f;

	// Seconds between attacks (plus up to CooldownRandom extra so a group doesn't fire in sync).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting", meta = (ClampMin = "0.1"))
	float Cooldown = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting", meta = (ClampMin = "0"))
	float CooldownRandom = 0.4f;

	// Random aim error in degrees (0 = perfect aim at where the player is).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting", meta = (ClampMin = "0", ClampMax = "20"))
	float AimSpread = 1.5f;

	// Aim this far above the player's centre (0 = chest).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting")
	float AimHeightOffset = 0.f;

	// Shots come from a component or mesh socket with this name. Falls back to chest height in front of the enemy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting")
	FName MuzzleName = TEXT("Muzzle");

	// ---- Standing still punish ----

	// The player counts as standing still below this speed (units per second; walking is ~600).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Punish", meta = (ClampMin = "0"))
	float StillSpeed = 120.f;

	// Seconds of standing still before the next attack becomes a burst.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Punish", meta = (ClampMin = "0"))
	float StillTimeToPunish = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Punish", meta = (ClampMin = "1"))
	int32 BurstCount = 3;

	// Seconds between the shots of a burst.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Punish", meta = (ClampMin = "0.02"))
	float BurstInterval = 0.12f;

	// Wind-up before a burst (shorter than a normal shot: standing still gets less warning).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Punish", meta = (ClampMin = "0"))
	float BurstWindUpTime = 0.2f;

	// Seconds between decisions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.2f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Think();
	void Move(AAIController* AI, APawn* Player, bool bKnowsWhere);
	void StartWindUp(bool bBurst);
	void BeginShots();
	void FireOne();
	void StopShooting();
	bool IsAlive() const;
	FVector MuzzleLocation() const;
	float Now() const;

	UPROPERTY()
	TObjectPtr<UEnemyAwarenessComponent> Awareness;

	FTimerHandle ThinkTimer;
	FTimerHandle ShotTimer;
	bool bWindingUp = false;
	bool bPendingBurst = false;
	bool bReturningHome = false;
	int32 ShotsLeft = 0;
	float NextShotTime = 0.f;
	float StillTime = 0.f;
	float StrafeDir = 1.f;
	float NextStrafeSwitch = 0.f;
};
