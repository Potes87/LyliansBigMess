// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "EnemyAwarenessComponent.generated.h"

class APawn;

/** Why an enemy woke up. */
UENUM(BlueprintType)
enum class EAggroReason : uint8
{
	Sight,    // it saw the player itself
	Damage,   // it got hit
	Alerted,  // a nearby enemy woke it
	Forced    // something called Aggro() directly (a script, a trigger, a boss)
};

/** What the enemy's feet are doing (only used when bDriveMovement is on). */
UENUM(BlueprintType)
enum class EChaseState : uint8
{
	Idle,    // asleep, or standing at home
	Chase,   // walking to the player along the navmesh
	Direct,  // walking straight at the player (no path: player jumping, on a ledge, off the navmesh)
	Search,  // lost sight: going to where the player was last seen and waiting there
	Return,  // gave up: walking back to where it was placed
	Flank,   // close to the player: walking to its own spot in the ring around them, then attacking
	Wait     // too many enemies already on the player: circling at a short distance until a spot opens
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnemyAggroSignal, EAggroReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEnemyAwarenessSignal);

/**
 * What an enemy knows about the player. Add it to an enemy pawn (BP_Enemy_Base has it).
 *
 * Sight: every SightInterval it looks for the player. The player has to be within SightRange, inside the
 * view cone, and visible (a trace from the enemy's eyes to the player's head or chest that only world
 * geometry can block: walls, floors, landscape, props. Other enemies and the player don't block it).
 * So enemies behind walls or further into the level stay asleep until the player actually shows up.
 *
 * Aggro: seeing the player or taking damage wakes the enemy. Once awake it stays awake (memory and giving
 * up come later). Waking also sets the Blueprint bool "bIsAggroed" on the owner if it has one, so older
 * Blueprint logic keeps working.
 *
 * Group alert: when an enemy wakes up it alerts the other enemies within AlertRadius. Each alerted enemy
 * reacts after a short random delay. By default an alert needs line of sight between the two enemies, so
 * a room behind a wall does not wake up. Alerted enemies do not pass the alert on unless bAlertsChain is on.
 *
 * Chasing (bDriveMovement): while awake the component walks the enemy toward the player, every ChaseInterval.
 *  - Sees the player (or lost them less than LoseSightGrace ago): follows the player along the navmesh.
 *  - No path to the player (jumping, standing on a ledge or off the navmesh) but close and in sight:
 *    walks straight at them instead of freezing, and goes back to pathing as soon as a path works again.
 *  - Lost sight for longer: walks to where the player was last seen and waits there. Getting shot tells it
 *    where the player is again.
 *  - Not seen for GiveUpTime: gives up, goes back to sleep and (bReturnHome) walks back to where it was placed.
 *    Sleeping enemies wake up again normally (sight, damage, alerts).
 *
 * Spreading out (bSpreadOut): once inside EngageRadius, enemies that know where the player is share them out.
 * The MaxAttackers closest ones get an "attack spot" each, spaced around the player (at least SlotSpacing
 * degrees apart), so a group surrounds the player instead of queueing in one line. Everyone else waits on
 * a ring at WaitRadius, slowly circling, and steps in when an attacker dies or falls behind.
 * It never moves while the owner's "bIsAttacking" is true or while the player is inside the owner's "AttackRange";
 * the attack itself is still started by the AI controller. Turn bDriveMovement off for enemies that have their
 * own movement brain (ranged enemies, bosses).
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UEnemyAwarenessComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyAwarenessComponent();

	// Fired once, the moment this enemy wakes up. Good for a growl, a glow flash, an "!" popup.
	UPROPERTY(BlueprintAssignable, Category = "Awareness")
	FEnemyAggroSignal OnAggroed;

	// Wake this enemy up now. Does nothing if it is already awake.
	UFUNCTION(BlueprintCallable, Category = "Awareness")
	void Aggro(EAggroReason Reason = EAggroReason::Forced);

	UFUNCTION(BlueprintPure, Category = "Awareness")
	bool IsAggroed() const { return bAggroed; }

	// True if the player was visible at the last sight check (only kept up to date while awake or looking).
	UFUNCTION(BlueprintPure, Category = "Awareness")
	bool CanSeePlayer() const { return bSeesPlayer; }

	// Where the player was the last time this enemy saw them (zero if never).
	UFUNCTION(BlueprintPure, Category = "Awareness")
	FVector GetLastSeenLocation() const { return LastSeenLocation; }

	// Line-of-sight test from this enemy's eyes to any actor's head/chest, using the same rules as sight.
	UFUNCTION(BlueprintCallable, Category = "Awareness")
	bool HasLineOfSightTo(const AActor* Target) const;

	// Fired when the enemy gives up looking for the player and goes back to sleep.
	UPROPERTY(BlueprintAssignable, Category = "Awareness")
	FEnemyAwarenessSignal OnGaveUp;

	// Put the enemy back to sleep right away (it can be woken again normally). Optionally walks it home.
	UFUNCTION(BlueprintCallable, Category = "Awareness")
	void Calm(bool bGoHome = true);

	UFUNCTION(BlueprintPure, Category = "Awareness")
	EChaseState GetChaseState() const { return ChaseState; }

	// Seconds since the player was last seen (a large number if never).
	UFUNCTION(BlueprintPure, Category = "Awareness")
	float GetTimeSinceSeen() const;

	// Where the enemy was placed (its spawn spot), used by bReturnHome.
	UFUNCTION(BlueprintPure, Category = "Awareness")
	FVector GetHomeLocation() const { return HomeLocation; }

	// ---- Sight (100 units = 1 metre) ----

	// Seconds between sight checks. Each enemy starts at a random offset so they don't all trace on one frame.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Sight", meta = (ClampMin = "0.05"))
	float SightInterval = 0.2f;

	// How far the enemy can see.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Sight", meta = (ClampMin = "0"))
	float SightRange = 3500.f;

	// Half of the view cone in degrees, measured from where the enemy faces. 180 = sees all the way round.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Sight", meta = (ClampMin = "0", ClampMax = "180"))
	float SightHalfAngle = 180.f;

	// The player is noticed inside this distance even without line of sight (heard or smelled). 0 = off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Sight", meta = (ClampMin = "0"))
	float CloseSenseRange = 250.f;

	// Eye height as a fraction of the capsule half height, above the capsule centre (0.7 = near the top).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Sight", meta = (ClampMin = "0", ClampMax = "1"))
	float EyeHeightFraction = 0.7f;

	// Draw the sight traces in the world (green = sees, red = blocked). For testing only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Sight")
	bool bDebugDraw = false;

	// ---- Group alert ----

	// Waking up alerts other enemies within this distance. 0 = never alert anyone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Alert", meta = (ClampMin = "0"))
	float AlertRadius = 1500.f;

	// Only alert enemies this enemy can see (a wall between them stops the alert).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Alert")
	bool bAlertNeedsSight = true;

	// Alert others when this enemy gets hit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Alert")
	bool bAlertOnDamage = true;

	// Alert others when this enemy spots the player itself.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Alert")
	bool bAlertOnSight = true;

	// Enemies woken by an alert pass it on to their own neighbours. Off by default so one shot can't wake a whole level.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Alert")
	bool bAlertsChain = false;

	// Alerted enemies react after a random delay between these (seconds), so a group wakes up in a ripple.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Alert", meta = (ClampMin = "0"))
	float AlertDelayMin = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Alert", meta = (ClampMin = "0"))
	float AlertDelayMax = 0.45f;

	// This enemy ignores alerts from others (it only wakes from sight or damage). For sentries and ambushers.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Alert")
	bool bIgnoreAlerts = false;

	// ---- Chase ----

	// The component walks the enemy while it is awake. Turn off for enemies with their own movement brain.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Chase")
	bool bDriveMovement = true;

	// Seconds between movement decisions.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Chase", meta = (ClampMin = "0.05"))
	float ChaseInterval = 0.2f;

	// How close to the player a chase stops (on top of both capsules' widths). The attack range takes over from there.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Chase", meta = (ClampMin = "0"))
	float ChaseAcceptance = 20.f;

	// After losing sight it still follows the player's real position for this many seconds (rounds corners naturally).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Chase", meta = (ClampMin = "0"))
	float LoseSightGrace = 1.0f;

	// Walks straight at the player when there is no path, but only within this distance and with line of sight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Chase", meta = (ClampMin = "0"))
	float DirectChaseRange = 900.f;

	// Gives up and goes back to sleep after not seeing the player for this many seconds. 0 = never gives up.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Chase", meta = (ClampMin = "0"))
	float GiveUpTime = 8.f;

	// After giving up, walk back to where it was placed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Chase")
	bool bReturnHome = true;

	// ---- Spread out ----

	// Take a spot around the player instead of walking straight at them. Needs bDriveMovement.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Spread")
	bool bSpreadOut = true;

	// Starts picking a spot once this close to the player. Further out it just chases.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Spread", meta = (ClampMin = "0"))
	float EngageRadius = 1200.f;

	// How many enemies may be on the player at once. The rest wait on the ring. (The largest value in a group wins.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Spread", meta = (ClampMin = "1"))
	int32 MaxAttackers = 3;

	// Smallest angle between two attackers' spots, in degrees (fewer attackers spread wider, up to a full circle).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Spread", meta = (ClampMin = "0", ClampMax = "180"))
	float SlotSpacing = 70.f;

	// Distance from the player where waiting enemies circle. Always kept outside the enemy's attack range.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Spread", meta = (ClampMin = "0"))
	float WaitRadius = 450.f;

	// How far ahead on the ring a waiting enemy aims, in degrees. Bigger = circles faster. 0 = stands still.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Awareness|Spread", meta = (ClampMin = "0", ClampMax = "90"))
	float CircleLead = 20.f;

	UFUNCTION(BlueprintPure, Category = "Awareness")
	bool HasAttackSpot() const { return bHasToken; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Look();
	void AlertNeighbours();
	void ReceiveAlert();
	FVector EyeLocation() const;
	bool TraceClear(const FVector& From, const FVector& To, const AActor* Target) const;
	bool IsAlive() const;

	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser);

	void Chase();
	void SetFlagOnOwner(bool bValue) const;
	bool OwnerIsAttacking() const;
	float OwnerAttackRange() const;
	float Now() const;
	void NoteSeen(const FVector& Where);

	FTimerHandle LookTimer;
	FTimerHandle AlertTimer;
	FTimerHandle ChaseTimer;
	bool bAggroed = false;
	bool bSeesPlayer = false;
	bool bAlertPending = false;
	FVector LastSeenLocation = FVector::ZeroVector;
	float LastSeenTime = -1000.f;
	FVector HomeLocation = FVector::ZeroVector;
	EChaseState ChaseState = EChaseState::Idle;
	bool bSearchArrived = false;

	// Spread out: worked out for the whole group once per frame by whichever enemy asks first.
	static void RebuildSlots(UWorld* World, const APawn* Player);
	bool bHasToken = false;   // has one of the MaxAttackers attack spots
	bool bInSpread = false;   // took part in this frame's sharing out
	float SlotAngle = 0.f;    // world yaw from the player to this enemy's spot
	float CircleDir = 1.f;    // which way it circles while waiting (+1 or -1, random per enemy)
	static uint64 SlotFrame;
	static TWeakObjectPtr<UWorld> SlotWorld;

	// Every awareness component in play, so alerts don't have to search the whole world.
	static TArray<TWeakObjectPtr<UEnemyAwarenessComponent>> AllAware;
};
