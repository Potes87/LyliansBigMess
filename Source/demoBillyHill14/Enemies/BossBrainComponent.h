// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "BossBrainComponent.generated.h"

class AAIController;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBossBrainSignal);

/** What the boss is doing with its feet right now. */
UENUM(BlueprintType)
enum class EBossMoveState : uint8
{
	Hold,    // planted, turning to face the player
	Wander,  // waddling to a random nearby spot
	Leash    // walking toward a player who got too far away or out of sight
};

/**
 * A heavy boss's brain. Add it to an enemy pawn that uses a plain AIController.
 * Movement: the boss mostly holds still and faces the player, sometimes wanders to a nearby spot,
 * and only walks toward the player when they get far away or out of sight.
 * Attacks: it does not attack by itself. It fires OnVolley (ask the Blueprint to shoot) and OnSlam
 * (ask the Blueprint to slam) when the time is right. The two can overlap, so a volley can go off mid-slam.
 * Stops on its own when the pawn loses its controller (the enemy death logic destroys it).
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UBossBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBossBrainComponent();

	// Fired about every ThinkInterval while the boss is aggroed, has line of sight and the player is within VolleyRange.
	// Hook it to the shot event. That event keeps its own cooldown, so being asked often is fine.
	UPROPERTY(BlueprintAssignable, Category = "Boss Brain")
	FBossBrainSignal OnVolley;

	// Fired when the player is within SlamRange and the slam cooldown is over. The boss also holds still for SlamHoldTime.
	UPROPERTY(BlueprintAssignable, Category = "Boss Brain")
	FBossBrainSignal OnSlam;

	// Stops walking and pauses wandering for this many seconds (still turns to face the player).
	UFUNCTION(BlueprintCallable, Category = "Boss Brain")
	void HoldStill(float Seconds);

	// Wakes the boss up even if the player is outside AggroRange. Taking damage does this automatically.
	UFUNCTION(BlueprintCallable, Category = "Boss Brain")
	void Aggro() { bAggroed = true; }

	UFUNCTION(BlueprintPure, Category = "Boss Brain")
	EBossMoveState GetMoveState() const { return MoveState; }

	UFUNCTION(BlueprintPure, Category = "Boss Brain")
	bool IsAggroed() const { return bAggroed; }

	// ---- Timing (100 units = 1 metre) ----

	// How often the brain makes a decision, in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Timing", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.25f;

	// The boss wakes up when the player comes this close.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Timing", meta = (ClampMin = "0"))
	float AggroRange = 2500.f;

	// ---- Attacks ----

	// OnVolley only fires when the player is this close and in sight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Attacks", meta = (ClampMin = "0"))
	float VolleyRange = 2200.f;

	// OnSlam fires when the player is this close (flat distance, ignoring height).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Attacks", meta = (ClampMin = "0"))
	float SlamRange = 700.f;

	// Seconds between slams.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Attacks", meta = (ClampMin = "0"))
	float SlamCooldown = 3.f;

	// The first slam can't happen until this many seconds after the boss wakes up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Attacks", meta = (ClampMin = "0"))
	float FirstSlamDelay = 2.f;

	// How long the boss stands planted for a slam (the slam animation's length).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Attacks", meta = (ClampMin = "0"))
	float SlamHoldTime = 1.6f;

	// ---- Wandering ----

	// Shortest and longest wait between wander rolls, in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Wander", meta = (ClampMin = "0"))
	float WanderWaitMin = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Wander", meta = (ClampMin = "0"))
	float WanderWaitMax = 9.f;

	// Chance (0 to 1) that he actually wanders when the wait is over. Otherwise he keeps standing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Wander", meta = (ClampMin = "0", ClampMax = "1"))
	float WanderChance = 0.6f;

	// How far a wander walk goes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Wander", meta = (ClampMin = "0"))
	float WanderDistanceMin = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Wander", meta = (ClampMin = "0"))
	float WanderDistanceMax = 600.f;

	// A wander walk gives up after this many seconds (stuck on something).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Wander", meta = (ClampMin = "0.1"))
	float WanderTimeout = 4.f;

	// ---- Leash ----

	// If the player is farther than this (or out of sight), the boss walks toward them...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Leash", meta = (ClampMin = "0"))
	float LeashRange = 2500.f;

	// ...until they are this close and in sight again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boss Brain|Leash", meta = (ClampMin = "0"))
	float LeashStopDistance = 1600.f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Think();
	void StartWander(AAIController* AI, const FVector& From);
	void SetHold(AAIController* AI);
	float Now() const;

	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	FTimerHandle ThinkTimer;
	EBossMoveState MoveState = EBossMoveState::Hold;
	bool bAggroed = false;
	float HoldUntil = 0.f;
	float NextWanderTime = 0.f;
	float WanderEndTime = 0.f;
	float NextSlamTime = 0.f;
};
