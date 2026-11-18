// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "EnemyAwarenessComponent.h"
#include "RangedBrainComponent.generated.h"

class AAIController;
class APawn;
class UAnimSequenceBase;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UDamageType;

/** What a ranged enemy is doing right now. */
UENUM(BlueprintType)
enum class ERangedState : uint8
{
	Asleep,      // has not noticed the player (or gave up and is walking home)
	Reposition,  // running to a new spot to shoot from, or standing there watching until its pause is over
	Aim,         // feet planted, weapon coming up: the tell
	Fire,        // the shot or the burst
	Recover,     // standing open after shooting
	Stagger,     // knocked out of what it was doing
	Alert        // the moment it notices the player
};

/** One shot pattern. A ranged enemy has a list of these, like a melee enemy has a list of attacks. */
USTRUCT(BlueprintType)
struct FRangedAttack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
	FName Name = TEXT("Bolt");

	// It only picks this one when the player is between these distances (100 units = 1 metre).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Choosing", meta = (ClampMin = "0"))
	float MinRange = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Choosing", meta = (ClampMin = "0"))
	float MaxRange = 2500.f;

	// The attacks are tried from the top of the list down. 1 = always picked when it is allowed, 0.3 = 30% of the time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Choosing", meta = (ClampMin = "0", ClampMax = "1"))
	float Chance = 1.f;

	// Seconds before it may pick this one again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Choosing", meta = (ClampMin = "0"))
	float Cooldown = 0.f;

	// The punish: only picked when the player has been standing still for the brain's "Still Time To Punish".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Choosing")
	bool bOnlyWhenPlayerStill = false;

	// The tell: seconds it stands planted with the weapon up before the first shot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0.05"))
	float AimTime = 0.8f;

	// It stops following the player with its aim this long before it fires: the aim line changes colour and freezes.
	// This is the player's window to dodge, so it has to be long enough to react to (about 0.3 s or more).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0"))
	float AimLockTime = 0.35f;

	// How many shots, and the time between them.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "1"))
	int32 ShotCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0.03"))
	float ShotInterval = 0.14f;

	// A burst normally sprays at the spot it locked onto. Turn this on and every shot of the burst aims again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing")
	bool bReaimEachShot = false;

	// Seconds it holds the firing pose after the last shot, before it starts to recover.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0"))
	float FireHoldTime = 0.12f;

	// How long it stands open afterwards.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Timing", meta = (ClampMin = "0"))
	float RecoverTime = 0.6f;

	// What it fires. Empty = the brain's own Projectile Class.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shot")
	TSubclassOf<AActor> ProjectileClass;

	// Projectile speed in units per second. 0 = keep the projectile's own speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shot", meta = (ClampMin = "0"))
	float ProjectileSpeed = 9000.f;

	// Random aim error in degrees (0 = exactly at the locked spot).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Shot", meta = (ClampMin = "0", ClampMax = "20"))
	float AimSpread = 0.5f;

	// Clips. One per phase, like the melee attacks. The fire clip restarts on every shot of a burst.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Animation")
	TObjectPtr<UAnimSequenceBase> AimAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Animation")
	TObjectPtr<UAnimSequenceBase> FireAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Animation")
	TObjectPtr<UAnimSequenceBase> RecoverAnim;

	// A glow that sits on the muzzle for the whole aim (the warning light). Removed when it fires or is staggered.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	TObjectPtr<UNiagaraSystem> AimFX;

	// A trail that rides behind every projectile, so a fast shot still reads.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	TObjectPtr<UNiagaraSystem> TracerFX;

	// A flash at the muzzle on every shot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	TObjectPtr<UNiagaraSystem> MuzzleFX;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	TObjectPtr<USoundBase> AimSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack|Effects")
	TObjectPtr<USoundBase> FireSound;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRangedWindUpSignal, bool, bBurst);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRangedFireSignal, AActor*, Projectile);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRangedNameSignal, FName, AttackName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRangedSimpleSignal);

/**
 * Movement + shooting brain for a ranged enemy. Add it to the enemy Blueprint next to the Awareness component,
 * turn Awareness "Drive Movement" OFF, and give the enemy a plain AIController.
 *
 * It fights in beats, so the player can read it:
 *   Reposition  it runs to a new spot (to the side, further away if rushed, closer if the player backs off), then
 *               stands there watching the player until its pause is over.
 *   Aim         feet planted, weapon up. This is the tell. It follows the player with its aim until Aim Lock Time
 *               before the shot, then freezes: side-step or dash after that and it misses.
 *   Fire        one shot or a burst, at the spot it locked onto.
 *   Recover     it stands open for a moment. This is when to shoot it.
 * Then it repositions again. It never shoots while moving.
 *
 * Attacks is a list of shot patterns (see FRangedAttack). An attack marked "Only When Player Still" is the punish for
 * standing still: by default a three-shot burst.
 *
 * Awareness decides WHEN it is awake and what it can see. Lost sight: it walks to where it last saw the player; after
 * Awareness' Give Up Time it walks home.
 *
 * Reactions, clips and blending work exactly like the Melee Brain: stagger (a headshot during the aim cancels the
 * shot), flinch, the alert reaction, one clip per phase, and cross-fades when the mesh runs an Animation Blueprint
 * whose graph is one Slot node.
 *
 * The enemy is passed to the projectile as its Owner and Instigator, so enemy level damage scaling works.
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API URangedBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	URangedBrainComponent();

	// ---- Signals (for glows, sounds and anything else the Blueprint wants to add) ----

	// The aim started (bBurst = more than one shot is coming).
	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedWindUpSignal OnWindUp;

	// A projectile was just fired (once per shot, so 3 times in a burst).
	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedFireSignal OnFire;

	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedNameSignal OnAim;

	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedNameSignal OnRecover;

	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedSimpleSignal OnStaggered;

	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedSimpleSignal OnAlerted;

	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedSimpleSignal OnFlinched;

	UPROPERTY(BlueprintAssignable, Category = "Ranged Brain")
	FRangedSimpleSignal OnAttackEnded;

	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	ERangedState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	bool IsWindingUp() const { return State == ERangedState::Aim; }

	// True while aiming or firing.
	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	bool IsShooting() const { return State == ERangedState::Aim || State == ERangedState::Fire; }

	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	bool IsRecovering() const { return State == ERangedState::Recover; }

	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	bool IsStaggered() const { return State == ERangedState::Stagger; }

	// 0 at the start of the current phase, 1 at its end.
	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	float GetPhaseAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	FName GetCurrentAttackName() const;

	// How long the player has been (nearly) standing still, in seconds.
	UFUNCTION(BlueprintPure, Category = "Ranged Brain")
	float GetPlayerStillTime() const { return StillTime; }

	// Knock it out of what it is doing, whatever the damage was.
	UFUNCTION(BlueprintCallable, Category = "Ranged Brain")
	void ForceStagger();

	// ---- Attacks ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain")
	TArray<FRangedAttack> Attacks;

	// What it fires, unless an attack names its own. Make a child or copy of BP_EnemyProjectile.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting")
	TSubclassOf<AActor> ProjectileClass;

	// Shots come from a component, mesh socket or bone with this name. Falls back to chest height in front of it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting")
	FName MuzzleName = TEXT("Muzzle");

	// Aim this far above the player's centre (0 = chest).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting")
	float AimHeightOffset = 0.f;

	// It only starts an attack when the player is within this distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting", meta = (ClampMin = "0"))
	float MaxShootRange = 2500.f;

	// How fast it turns to keep its aim on the player (degrees per second).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Shooting", meta = (ClampMin = "0"))
	float TurnSpeed = 540.f;

	// ---- Aim line: the line from the muzzle that shows the player they are being aimed at, and when it has locked ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line")
	bool bAimLine = true;

	// An unlit glowing material with a colour parameter called "Color". Without one the line is not drawn.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line")
	TObjectPtr<UMaterialInterface> AimLineMaterial;

	// While it is still following the player: thin and faint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line")
	FLinearColor AimLineColor = FLinearColor(0.92f, 0.66f, 0.28f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line", meta = (ClampMin = "0"))
	float AimLineGlow = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line", meta = (ClampMin = "0.1"))
	float AimLineWidth = 1.2f;

	// Once it has locked: bright, thicker, and frozen where the player was. Dodge now.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line")
	FLinearColor AimLineLockColor = FLinearColor(0.95f, 0.40f, 0.20f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line", meta = (ClampMin = "0"))
	float AimLineLockGlow = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line", meta = (ClampMin = "0.1"))
	float AimLineLockWidth = 3.5f;

	// The line runs through the player and on until it hits something, up to this far.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line", meta = (ClampMin = "100"))
	float AimLineLength = 6000.f;

	// Played, and spawned at the muzzle, at the moment the aim locks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line")
	TObjectPtr<USoundBase> LockSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Aim Line")
	TObjectPtr<UNiagaraSystem> LockFX;

	// ---- Range (100 units = 1 metre) ----

	// Closer than this and its next move is to back away.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Range", meta = (ClampMin = "0"))
	float MinRange = 700.f;

	// The distance it goes to when it is too close or too far.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Range", meta = (ClampMin = "0"))
	float PreferredRange = 1000.f;

	// Further than this and its next move is to close in.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Range", meta = (ClampMin = "0"))
	float MaxRange = 1400.f;

	// ---- Reposition (what it does between attacks) ----

	// Each move takes it this far round the player, in degrees, to a random side. Bigger = longer runs.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Reposition", meta = (ClampMin = "0", ClampMax = "120"))
	float SideStepAngleMin = 15.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Reposition", meta = (ClampMin = "0", ClampMax = "120"))
	float SideStepAngleMax = 40.f;

	// Seconds between the end of one attack and the start of the next aim (random between the two). It moves first,
	// then stands and watches for whatever is left.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Reposition", meta = (ClampMin = "0"))
	float AttackPauseMin = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Reposition", meta = (ClampMin = "0"))
	float AttackPauseMax = 1.6f;

	// If the move takes longer than this it stops where it is and shoots from there.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Reposition", meta = (ClampMin = "0.2"))
	float RepositionTimeMax = 2.2f;

	// ---- Standing still punish ----

	// The player counts as standing still below this speed (units per second; walking is ~600).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Punish", meta = (ClampMin = "0"))
	float StillSpeed = 120.f;

	// Seconds of standing still before attacks marked "Only When Player Still" may be picked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Punish", meta = (ClampMin = "0"))
	float StillTimeToPunish = 1.0f;

	// ---- Stagger ----

	// A headshot during the aim cancels the shot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Stagger")
	bool bHeadshotStaggersAim = true;

	// Taking this much of its max health within Stagger Reset Time staggers it. 0 = never from damage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Stagger", meta = (ClampMin = "0", ClampMax = "1"))
	float StaggerHealthFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Stagger", meta = (ClampMin = "0.1"))
	float StaggerResetTime = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Stagger", meta = (ClampMin = "0.1"))
	float StaggerTime = 0.8f;

	// After a stagger it cannot be staggered again for this long.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Stagger", meta = (ClampMin = "0"))
	float StaggerImmunityTime = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Stagger")
	TObjectPtr<UAnimSequenceBase> StaggerAnim;

	// More stagger clips: one of them (or the one above) is picked at random each time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Stagger")
	TArray<TObjectPtr<UAnimSequenceBase>> StaggerAnimVariants;

	// ---- Alert: the moment it notices the player ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Alert")
	bool bAlertReaction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Alert", meta = (ClampMin = "0.1"))
	float AlertTime = 0.8f;

	// Only this share of them react; the rest go straight to work.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Alert", meta = (ClampMin = "0", ClampMax = "1"))
	float AlertChance = 0.5f;

	// Also react when it was woken by being shot.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Alert")
	bool bAlertWhenShot = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Alert")
	TObjectPtr<UAnimSequenceBase> AlertAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Alert")
	TObjectPtr<USoundBase> AlertSound;

	// ---- Animation ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Animation")
	TObjectPtr<UAnimSequenceBase> IdleAnim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Animation")
	TObjectPtr<UAnimSequenceBase> RunAnim;

	// Faster than this and it plays the run clip, slower and it plays the idle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Animation", meta = (ClampMin = "0"))
	float RunSpeedThreshold = 40.f;

	// Speed of the run clip. Raise it if the feet look like they slide, lower it if they spin.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Animation", meta = (ClampMin = "0.1"))
	float RunPlayRate = 1.f;

	// Blending needs the mesh to run an Animation Blueprint whose graph is just one Slot node (named here) wired to the
	// output. Without one the clips cut from one to the next.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Animation|Blending")
	FName AnimSlotName = TEXT("DefaultSlot");

	// Idle <-> run, and from the end of a recovery, stagger or alert back into idle or run.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Animation|Blending", meta = (ClampMin = "0"))
	float BlendLocomotion = 0.2f;

	// Into the aim. A little longer than a melee wind-up: raising a weapon reads well as a motion.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Animation|Blending", meta = (ClampMin = "0"))
	float BlendAim = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Animation|Blending", meta = (ClampMin = "0"))
	float BlendStagger = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Animation|Blending", meta = (ClampMin = "0"))
	float BlendAlert = 0.1f;

	// ---- Flinch: a quick jolt away from every shot that lands (it does not interrupt anything) ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Flinch")
	bool bFlinch = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Flinch", meta = (ClampMin = "0"))
	float FlinchDistance = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Flinch", meta = (ClampMin = "0"))
	float FlinchAngle = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Flinch", meta = (ClampMin = "0.05"))
	float FlinchTime = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Flinch", meta = (ClampMin = "1"))
	float HeadshotFlinchScale = 1.7f;

	// ---- Testing ----

	// Draws a plain debug aim line too: yellow while it follows the player, red once it has locked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain|Testing")
	bool bDebugDraw = false;

	// Seconds between decisions while it is asleep or walking home.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged Brain", meta = (ClampMin = "0.05"))
	float ThinkInterval = 0.2f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Think();
	void EnterPhase(ERangedState NewState, float Length);
	void BeginReposition(bool bFirst);
	void TickReposition(float DeltaTime, APawn* Pawn, AAIController* AI, APawn* Player);
	int32 PickAttack(float Distance) const;
	void BeginAim(int32 Index);
	void BeginFire();
	void FireOne();
	void BeginRecover();
	void EndAttack();
	void BeginStagger();
	void GoToSleep();
	void StopMoving() const;
	void FaceToward(const FVector& Point, float DeltaTime) const;
	void StopAimFX();
	void UpdateAimLine();
	void HideAimLine();
	void SpawnAtMuzzle(UNiagaraSystem* System, bool bAttach);
	const FRangedAttack* CurrentAttack() const;
	FVector AimPointNow() const;

	bool IsAlive() const;
	FVector MuzzleLocation() const;
	float Now() const;
	float OwnerMaxHealth();
	void SetOwnerAttacking(bool bValue) const;

	// BlendIn = seconds to cross-fade from whatever is playing (only with an Animation Blueprint).
	void PlayAnim(UAnimSequenceBase* Anim, bool bLoop = false, float Rate = 1.f, float BlendIn = 0.f) const;
	void UpdateLocomotion();
	void PlaySound(USoundBase* Sound) const;

	bool IsFlinching() const;
	void StartFlinch(const FVector& WorldDirection, float Strength);
	void ApplyMeshPose();
	void DrawDebug() const;

	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandleAggroed(EAggroReason Reason);

	UPROPERTY()
	TObjectPtr<UEnemyAwarenessComponent> Awareness;

	UPROPERTY()
	TObjectPtr<UAnimSequenceBase> CurrentLoco;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> AimGlow;

	UPROPERTY()
	TObjectPtr<UStaticMesh> AimLineMesh;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> AimLine;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> AimLineMID;

	FTimerHandle ThinkTimer;

	ERangedState State = ERangedState::Asleep;
	float PhaseStart = 0.f;
	float PhaseLength = 0.f;
	int32 Current = INDEX_NONE;
	TArray<float> ReadyTime;          // per attack: when it may be picked again
	float NextAttackTime = 0.f;       // earliest moment the next aim may start
	int32 ShotsFired = 0;
	bool bAimLocked = false;
	FVector LockedAim = FVector::ZeroVector;   // the spot in the world it will shoot at
	bool bMoveDone = false;           // reached the reposition spot (or gave up on it)
	bool bGoingToLastSeen = false;
	bool bReturningHome = false;
	float SideSign = 1.f;
	float StillTime = 0.f;

	float StaggerDamage = 0.f;
	float LastDamageTime = -1000.f;
	float StaggerImmuneUntil = 0.f;
	float CachedMaxHealth = 0.f;

	bool bHasMeshBase = false;
	bool bMeshPosed = false;
	FQuat MeshBaseRotation = FQuat::Identity;
	FVector MeshBaseLocation = FVector::ZeroVector;
	FVector FlinchDirLocal = FVector::ZeroVector;
	float FlinchStrength = 0.f;
	float FlinchStart = -1000.f;
};
