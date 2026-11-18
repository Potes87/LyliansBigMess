// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "EnemyAwarenessComponent.h"
#include "MeleeBrainComponent.generated.h"

class UEnemyAwarenessComponent;
class UAnimSequenceBase;
class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;
class ACharacter;
class APawn;
class UDamageType;
class AController;

/** What a melee enemy is doing right now. */
UENUM(BlueprintType)
enum class EMeleeState : uint8
{
	Idle,     // not attacking: asleep, chasing, or waiting for its turn (Awareness owns the feet)
	WindUp,   // the tell: big pose, turning to face the player. The player should dodge now.
	Strike,   // the attack itself: lunges along its locked direction, the hit shape is live
	Recover,  // the follow through: stands open, cannot attack or move. The player's turn.
	Stagger,  // knocked out of what it was doing by a headshot or heavy damage
	Alert     // it has just noticed the player: a short startled reaction before it comes running
};

/** One attack a melee enemy can do. All distances in units (100 = 1 metre), all times in seconds. */
USTRUCT(BlueprintType)
struct FMeleeAttack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FName Name = TEXT("Slash");

	// The attack can start when the player is between these distances (centre to centre, flat).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Range", meta = (ClampMin = "0"))
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Range", meta = (ClampMin = "0"))
	float MaxRange = 230.f;

	// Chance to use this attack when it is possible (1 = always). A failed roll waits a second before rolling again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Range", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 1.f;

	// Seconds before this same attack can be used again (0 = only the brain's normal pause between attacks).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Range", meta = (ClampMin = "0"))
	float Cooldown = 0.f;

	// ---- Timing ----

	// How long the warning pose lasts. This is the player's reaction time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0.05"))
	float WindUpTime = 0.5f;

	// It stops turning toward the player this long before the strike. After that, side-stepping makes it miss.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0"))
	float AimLockTime = 0.12f;

	// How long the strike lasts (the lunge, or the time in the air for a leap).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0.03"))
	float StrikeTime = 0.15f;

	// The hit shape only turns on this long into the strike (for a leap: so it hits near the landing, not at take-off).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0"))
	float HitDelay = 0.f;

	// A double attack: this long into the strike the hit shape arms again, so the same strike can land a second hit.
	// 0 = a single hit (the normal case). Set it later than Hit Delay and earlier than Strike Time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0"))
	float SecondHitDelay = 0.f;

	// How long it stands open afterwards.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0"))
	float RecoverTime = 1.0f;

	// ---- Movement ----

	// How far it travels during the strike at most. It always stops just short of where the player was at aim lock.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Movement", meta = (ClampMin = "0"))
	float LungeDistance = 120.f;

	// Jumps through the air instead of sliding along the ground.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Movement")
	bool bLeap = false;

	// After the strike it stumbles this much further forward while recovering (0 = stays put). Matches a step in the animation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Movement", meta = (ClampMin = "0"))
	float RecoverStepDistance = 0.f;

	// The stumble starts this long into the recovery...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Movement", meta = (ClampMin = "0"))
	float RecoverStepDelay = 0.1f;

	// ...and takes this long.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Movement", meta = (ClampMin = "0.05"))
	float RecoverStepTime = 0.3f;

	// ---- Hit shape: a fan on the ground in front of the enemy ----

	// How far in front of the enemy the hit reaches.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Hit", meta = (ClampMin = "0"))
	float Reach = 260.f;

	// Half of the fan's width in degrees. 50 = a 100 degree fan. 180 = all the way round.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Hit", meta = (ClampMin = "1", ClampMax = "180"))
	float HalfAngle = 50.f;

	// Damage = the enemy's AttackDamage x this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Hit", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.f;

	// ---- Looks and sound (all optional). Plain animation clips, one per phase, played from their first frame. ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Animation")
	TObjectPtr<UAnimSequenceBase> WindUpAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Animation")
	TObjectPtr<UAnimSequenceBase> StrikeAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Animation")
	TObjectPtr<UAnimSequenceBase> RecoverAnim = nullptr;

	// ---- Effects (all optional) ----

	// Plays on the weapon for the length of the strike (a trail behind the blade).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	TObjectPtr<UNiagaraSystem> StrikeTrailFX = nullptr;

	// The trail rides this bone (the hand that holds the blade)...
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	FName TrailBone = TEXT("hand_r");

	// ...at this point on the model, given where that point is while the enemy stands in its resting stance
	// (mesh space, units: X = its left, Y = forward, Z = up from the feet). Zero = right at the bone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	FVector TrailPoint = FVector::ZeroVector;

	// A second trail for attacks that use both blades. Leave the bone as None for one trail only.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	FName TrailBone2 = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	FVector TrailPoint2 = FVector::ZeroVector;

	// Bursts on the ground where the strike ends (the blade biting the floor, the leap landing).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	TObjectPtr<UNiagaraSystem> ImpactFX = nullptr;

	// How far in front of the enemy the burst appears.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects", meta = (ClampMin = "0"))
	float ImpactForward = 110.f;

	// Passed to the effect as its "Scale" setting, if it has one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects", meta = (ClampMin = "0"))
	float ImpactScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Sound")
	TObjectPtr<USoundBase> WindUpSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Sound")
	TObjectPtr<USoundBase> StrikeSound = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMeleeAttackSignal, FName, AttackName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMeleeHitSignal, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMeleeSignal);

/**
 * Attack brain for a melee enemy. Add it to the enemy Blueprint next to the Awareness component and give the enemy
 * a plain AIController (not AIC_Enemy: that one starts the old instant attack).
 *
 * Awareness still decides when the enemy is awake and walks it to the player (leave its "Drive Movement" ON).
 * This component takes over once the player is in range of one of its Attacks:
 *
 *   WIND-UP  -> the tell. It stops, snaps to a big pose and keeps turning to face the player. Shortly before the
 *               strike (AimLockTime) it stops turning: from then on the strike goes where it is facing.
 *   STRIKE   -> lunges (or leaps) along that locked direction. The hit is a fan in front of the enemy, checked
 *               while it moves. The player takes damage once if they are inside it. Side-step or dash = miss.
 *   RECOVER  -> stands open for RecoverTime. No moving, no attacking. This is the player's window.
 *
 * Fighting back:
 *   - A headshot during the wind-up staggers it and cancels the attack.
 *   - Taking StaggerHealthFraction of its max health in quick succession staggers it in any state except the strike.
 *   - After a stagger it cannot be staggered again for StaggerImmunityTime (no stun-locking).
 *
 * Every shot that lands also makes it FLINCH: the whole body jolts away from the shot for a moment. That is only
 * a visual reaction, it never interrupts what the enemy is doing.
 *
 * Crowds: only MaxSimultaneousAttacks enemies may be winding up or striking at the same moment, and (with Awareness
 * "Spread Out") only enemies holding an attack spot may start. So a pack takes turns and every tell can be read.
 *
 * Animation: give it clips (Idle, Run, one per attack phase, Stagger) and it plays them itself on the mesh, each from
 * its first frame, with no Animation Blueprint. Leave them empty and the enemy keeps whatever animation setup it has.
 *
 * It sets the Blueprint bool "bIsAttacking" on the owner while it is busy, so Awareness leaves the feet alone.
 * Damage is the owner's Blueprint "AttackDamage" (already scaled by the enemy's level) x the attack's multiplier.
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UMeleeBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMeleeBrainComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ---- Signals (for glow, sounds, FX, Blueprint logic) ----

	UPROPERTY(BlueprintAssignable, Category = "Melee Brain")
	FMeleeAttackSignal OnWindUp;

	UPROPERTY(BlueprintAssignable, Category = "Melee Brain")
	FMeleeAttackSignal OnStrike;

	UPROPERTY(BlueprintAssignable, Category = "Melee Brain")
	FMeleeAttackSignal OnRecover;

	// The strike connected with the player.
	UPROPERTY(BlueprintAssignable, Category = "Melee Brain")
	FMeleeHitSignal OnHitPlayer;

	UPROPERTY(BlueprintAssignable, Category = "Melee Brain")
	FMeleeSignal OnStaggered;

	// It has just noticed the player and started its alert reaction. Good for the alerted sound.
	UPROPERTY(BlueprintAssignable, Category = "Melee Brain")
	FMeleeSignal OnAlerted;

	// It was hit and flinched (every damaging hit). Good for a grunt or a spark.
	UPROPERTY(BlueprintAssignable, Category = "Melee Brain")
	FMeleeSignal OnFlinched;

	// Back to normal after an attack or a stagger.
	UPROPERTY(BlueprintAssignable, Category = "Melee Brain")
	FMeleeSignal OnAttackEnded;

	UFUNCTION(BlueprintPure, Category = "Melee Brain")
	EMeleeState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Melee Brain")
	bool IsWindingUp() const { return State == EMeleeState::WindUp; }

	UFUNCTION(BlueprintPure, Category = "Melee Brain")
	bool IsStriking() const { return State == EMeleeState::Strike; }

	UFUNCTION(BlueprintPure, Category = "Melee Brain")
	bool IsRecovering() const { return State == EMeleeState::Recover; }

	UFUNCTION(BlueprintPure, Category = "Melee Brain")
	bool IsStaggered() const { return State == EMeleeState::Stagger; }

	UFUNCTION(BlueprintPure, Category = "Melee Brain")
	bool IsAlerting() const { return State == EMeleeState::Alert; }

	// How far through the current phase it is, 0 to 1 (0 while idle).
	UFUNCTION(BlueprintPure, Category = "Melee Brain")
	float GetPhaseAlpha() const;

	// Name of the attack in progress (None while idle).
	UFUNCTION(BlueprintPure, Category = "Melee Brain")
	FName GetCurrentAttackName() const;

	// Knock it out of whatever it is doing right now (ignores immunity).
	UFUNCTION(BlueprintCallable, Category = "Melee Brain")
	void ForceStagger();

	// ---- Attacks ----

	// Every attack this enemy knows. Each time it may attack, it uses the first one in the list that is in range and ready.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain")
	TArray<FMeleeAttack> Attacks;

	// Pause after an attack before the next one may start: a random time between these.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain", meta = (ClampMin = "0"))
	float AttackPauseMin = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain", meta = (ClampMin = "0"))
	float AttackPauseMax = 0.8f;

	// How fast it turns to face the player during the wind-up, in degrees per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain", meta = (ClampMin = "0"))
	float TurnSpeed = 720.f;

	// It will not start an attack, and its hits miss, when the player is more than this far above or below it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain", meta = (ClampMin = "0"))
	float HeightTolerance = 150.f;

	// ---- Crowds ----

	// How many melee enemies may be winding up or striking at the same moment. (The largest value in play wins.)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Crowd", meta = (ClampMin = "1"))
	int32 MaxSimultaneousAttacks = 2;

	// Only attack while holding one of Awareness' attack spots (when Awareness "Spread Out" is on).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Crowd")
	bool bNeedsAttackSpot = true;

	// ---- Stagger ----

	// A headshot during the wind-up cancels the attack.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Stagger")
	bool bHeadshotStaggersWindUp = true;

	// Taking this much of its max health in quick succession staggers it. 0.3 = 30%. 0 = off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Stagger", meta = (ClampMin = "0", ClampMax = "1"))
	float StaggerHealthFraction = 0.3f;

	// The damage count starts over after this long without being hit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Stagger", meta = (ClampMin = "0"))
	float StaggerResetTime = 2.f;

	// How long a stagger lasts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Stagger", meta = (ClampMin = "0.05"))
	float StaggerTime = 0.9f;

	// After a stagger ends it cannot be staggered again for this long.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Stagger", meta = (ClampMin = "0"))
	float StaggerImmunityTime = 2.5f;

	// Heavy damage can also interrupt the strike itself. Off = once it swings, it swings.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Stagger")
	bool bStaggerDuringStrike = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Stagger")
	TObjectPtr<UAnimSequenceBase> StaggerAnim = nullptr;

	// More stagger clips. Each stagger picks one at random from Stagger Anim and these, so it does not always look the same.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Stagger")
	TArray<TObjectPtr<UAnimSequenceBase>> StaggerAnimVariants;

	// ---- Alert: the moment it notices the player ----

	// Stop for a moment and react when it first notices the player, before it comes running.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Alert")
	bool bAlertReaction = true;

	// How long the reaction lasts. It turns to face the player during it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Alert", meta = (ClampMin = "0.05"))
	float AlertTime = 0.8f;

	// Chance that it reacts at all when it notices the player (1 = always, 0.5 = half the time). The rest just charge,
	// so a group does not all do the same thing at once.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Alert", meta = (ClampMin = "0", ClampMax = "1"))
	float AlertChance = 0.5f;

	// Also react when it is woken by being shot. Off = a shot enemy skips the reaction and just comes for the player.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Alert")
	bool bAlertWhenShot = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Alert")
	TObjectPtr<UAnimSequenceBase> AlertAnim = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Alert")
	TObjectPtr<USoundBase> AlertSound = nullptr;

	// ---- Standing and running (played by the brain whenever it is not attacking or staggered) ----

	// Looping clip while it stands still.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnim = nullptr;

	// Looping clip while it moves.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Animation")
	TObjectPtr<UAnimSequenceBase> RunAnim = nullptr;

	// Faster than this (units per second) counts as moving.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Animation", meta = (ClampMin = "0"))
	float RunSpeedThreshold = 40.f;

	// Speed of the run clip. Raise it if the feet look like they slide, lower it if they spin.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Animation", meta = (ClampMin = "0.1"))
	float RunPlayRate = 1.f;

	// ---- Blending between clips ----
	// Blending needs the mesh to run an Animation Blueprint whose graph is just one Slot node (named below) wired to
	// the output. Without one (mesh set to "Use Animation Asset") the clips cut from one to the next, as before.

	// The Slot node in the enemy's Animation Blueprint that the clips are played through.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Animation|Blending")
	FName AnimSlotName = TEXT("DefaultSlot");

	// Idle <-> run, and from the end of a recovery, stagger or alert back into idle or run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Animation|Blending", meta = (ClampMin = "0"))
	float BlendLocomotion = 0.2f;

	// Into a wind-up. Keep it short: the big first pose is the tell.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Animation|Blending", meta = (ClampMin = "0"))
	float BlendWindUp = 0.06f;

	// Into a stagger.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Animation|Blending", meta = (ClampMin = "0"))
	float BlendStagger = 0.05f;

	// Into the alert reaction.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Animation|Blending", meta = (ClampMin = "0"))
	float BlendAlert = 0.1f;

	// ---- Flinch: a quick jolt away from every shot that lands (it does not interrupt anything) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Flinch")
	bool bFlinch = true;

	// How far the body is shoved away from the shot, in units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Flinch", meta = (ClampMin = "0"))
	float FlinchDistance = 5.f;

	// How far it tips away from the shot, in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Flinch", meta = (ClampMin = "0", ClampMax = "30"))
	float FlinchAngle = 5.f;

	// How long one flinch lasts. It snaps out almost at once and settles back over the rest.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Flinch", meta = (ClampMin = "0.05"))
	float FlinchTime = 0.18f;

	// Headshots flinch this many times harder.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Flinch", meta = (ClampMin = "0"))
	float HeadshotFlinchScale = 1.7f;

	// ---- Testing aids (turn both off once the real animations are in) ----

	// Leans the whole mesh so the phases can be read without animations: back = wind-up, forward = strike, slumped = recover.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Testing")
	bool bPlaceholderLean = true;

	// Draws the hit fan on the ground: yellow = winding up (still turning), orange = aim locked, red = hit is live,
	// green ball = recovering (punish now), white ball = staggered.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain|Testing")
	bool bDebugDraw = true;

	// Seconds between decisions while not attacking.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee Brain", meta = (ClampMin = "0.03"))
	float ThinkInterval = 0.1f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Think();
	void StartAttack(int32 Index);
	void BeginStrike();
	void BeginRecover();
	void BeginStagger();
	void EndAttack();
	void EnterPhase(EMeleeState NewState, float Length);
	void TryHit(const FMeleeAttack& Attack);
	void TakeTheFeet();
	void StopSliding();
	void UpdateLean();
	void StartFlinch(const FVector& WorldDirection, float Strength);
	void ApplyMeshPose();
	bool IsFlinching() const;
	void DrawDebug() const;
	// BlendIn = seconds to cross-fade from whatever is playing (only with an Animation Blueprint, see Blending above).
	void PlayAnim(UAnimSequenceBase* Anim, bool bLoop = false, float Rate = 1.f, float BlendIn = 0.f) const;
	void UpdateLocomotion();
	void PlaySound(USoundBase* Sound) const;
	void StartTrails(const FMeleeAttack& Attack);
	void StopTrails();
	void SpawnImpact(const FMeleeAttack& Attack);
	void SetOwnerAttacking(bool bValue) const;
	float OwnerMaxHealth();
	int32 CountOthersAttacking(int32& OutMaxAllowed) const;
	const FMeleeAttack* CurrentAttack() const;
	bool IsAlive() const;
	float Now() const;

	UFUNCTION()
	void HandleAggroed(EAggroReason Reason);

	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	UPROPERTY()
	TObjectPtr<UEnemyAwarenessComponent> Awareness;

	FTimerHandle ThinkTimer;
	EMeleeState State = EMeleeState::Idle;
	int32 Current = INDEX_NONE;       // index into Attacks of the attack in progress
	float PhaseStart = 0.f;
	float PhaseLength = 0.f;
	float NextAttackTime = 0.f;
	TArray<float> ReadyTime;          // per attack: when it may be used again

	FVector LockedDir = FVector::ForwardVector;   // flat direction the strike travels and hits along
	FVector LockedTarget = FVector::ZeroVector;   // where the player was at aim lock
	float LungeSpeed = 0.f;
	bool bHitDone = false;
	bool bSecondHitArmed = false;     // the second hit of a double attack has been switched on

	float StaggerDamage = 0.f;        // damage taken recently, counted toward a stagger
	float LastDamageTime = -1000.f;
	float StaggerImmuneUntil = 0.f;
	float CachedMaxHealth = 0.f;

	// Strike trails that are playing right now.
	UPROPERTY()
	TArray<TObjectPtr<UNiagaraComponent>> ActiveTrails;

	// The idle / run clip playing right now (null while an attack or stagger clip has the mesh).
	UPROPERTY()
	TObjectPtr<UAnimSequenceBase> CurrentLoco;

	FQuat MeshBaseRotation = FQuat::Identity;
	FVector MeshBaseLocation = FVector::ZeroVector;
	bool bHasMeshBase = false;
	bool bMeshPosed = false;                        // the mesh is currently moved off its base pose by us
	float LeanDegrees = 0.f;                        // placeholder lean, forward is positive
	FVector FlinchDirLocal = FVector::ZeroVector;   // away from the shot, in the capsule's space
	float FlinchStart = -1000.f;
	float FlinchStrength = 0.f;
	bool bWarnedController = false;

	// Every melee brain in play, to count how many are attacking at once.
	static TArray<TWeakObjectPtr<UMeleeBrainComponent>> AllBrains;
};
