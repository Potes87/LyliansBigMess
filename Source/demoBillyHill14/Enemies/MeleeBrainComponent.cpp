// Fill out your copyright notice in the Description page of Project Settings.

#include "MeleeBrainComponent.h"

#include "EnemyAwarenessComponent.h"
#include "../Stats/CombatHelpers.h"
#include "AIController.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimationRuntime.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/SkeletalMesh.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

TArray<TWeakObjectPtr<UMeleeBrainComponent>> UMeleeBrainComponent::AllBrains;

namespace
{
	// The strike stops this far short of where the player was, so the enemy ends in front of them, not inside them.
	const float MeleeStopShort = 90.f;

	float MeleeReadNumber(const UObject* Object, const TCHAR* Name, float Default)
	{
		if (!Object) return Default;
		if (const FNumericProperty* Prop = FindFProperty<FNumericProperty>(Object->GetClass(), Name))
		{
			const void* Value = Prop->ContainerPtrToValuePtr<void>(Object);
			return Prop->IsFloatingPoint() ? (float)Prop->GetFloatingPointPropertyValue(Value) : (float)Prop->GetSignedIntPropertyValue(Value);
		}
		return Default;
	}
}

UMeleeBrainComponent::UMeleeBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while attacking or staggered

	// Starting numbers for the Ashling. Change them per enemy in the Blueprint.
	FMeleeAttack Slash;
	Slash.Name = TEXT("Slash");
	Slash.RecoverStepDistance = 45.f;   // the slash animation takes one catch step forward while it recovers

	FMeleeAttack Leap;
	Leap.Name = TEXT("Leap");
	Leap.MinRange = 380.f;
	Leap.MaxRange = 650.f;
	Leap.Chance = 0.6f;
	Leap.Cooldown = 5.f;
	Leap.WindUpTime = 0.6f;
	Leap.AimLockTime = 0.15f;
	Leap.StrikeTime = 0.45f;
	Leap.HitDelay = 0.2f;
	Leap.RecoverTime = 1.2f;
	Leap.LungeDistance = 650.f;
	Leap.bLeap = true;
	Leap.Reach = 220.f;
	Leap.HalfAngle = 60.f;
	Leap.DamageMultiplier = 1.5f;

	Attacks.Add(Slash);
	Attacks.Add(Leap);
}

void UMeleeBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;
	Awareness = Owner->FindComponentByClass<UEnemyAwarenessComponent>();

	AllBrains.RemoveAll([](const TWeakObjectPtr<UMeleeBrainComponent>& B) { return !B.IsValid(); });
	AllBrains.AddUnique(this);

	// Unique: BeginPlay can run twice on an actor when World Partition streams its cell in.
	Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UMeleeBrainComponent::HandleOwnerDamaged);
	if (Awareness)
	{
		Awareness->OnAggroed.AddUniqueDynamic(this, &UMeleeBrainComponent::HandleAggroed);
	}

	if (ACharacter* Char = Cast<ACharacter>(Owner))
	{
		if (Char->GetMesh() && !bHasMeshBase)
		{
			MeshBaseRotation = Char->GetMesh()->GetRelativeRotation().Quaternion();
			MeshBaseLocation = Char->GetMesh()->GetRelativeLocation();
			bHasMeshBase = true;
		}
		// Run before the movement component each frame, so the lunge speed we set is the speed it moves with.
		if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			Move->AddTickPrerequisiteComponent(this);
		}
	}

	ReadyTime.Init(0.f, Attacks.Num());
	NextAttackTime = Now() + FMath::FRandRange(0.2f, 0.5f); // small grace after spawning
	GetWorld()->GetTimerManager().SetTimer(ThinkTimer, this, &UMeleeBrainComponent::Think, ThinkInterval, true,
		FMath::FRandRange(0.f, ThinkInterval));
}

void UMeleeBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
	}
	AllBrains.RemoveAll([this](const TWeakObjectPtr<UMeleeBrainComponent>& B) { return !B.IsValid() || B.Get() == this; });
	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------ helpers

float UMeleeBrainComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

bool UMeleeBrainComponent::IsAlive() const
{
	// Enemy death destroys the controller (the pawn can keep a stale pointer to it, so IsValid, not just non-null)
	// and turns the mesh into a ragdoll.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || !IsValid(Pawn->GetController())) return false;
	if (const ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		if (Char->GetMesh() && Char->GetMesh()->IsSimulatingPhysics()) return false;
	}
	return true;
}

const FMeleeAttack* UMeleeBrainComponent::CurrentAttack() const
{
	return Attacks.IsValidIndex(Current) ? &Attacks[Current] : nullptr;
}

float UMeleeBrainComponent::GetPhaseAlpha() const
{
	if (State == EMeleeState::Idle) return 0.f;
	if (PhaseLength <= KINDA_SMALL_NUMBER) return 1.f;
	return FMath::Clamp((Now() - PhaseStart) / PhaseLength, 0.f, 1.f);
}

FName UMeleeBrainComponent::GetCurrentAttackName() const
{
	const FMeleeAttack* Attack = CurrentAttack();
	return (Attack && State != EMeleeState::Idle && State != EMeleeState::Stagger && State != EMeleeState::Alert) ? Attack->Name : NAME_None;
}

void UMeleeBrainComponent::SetOwnerAttacking(bool bValue) const
{
	// Awareness reads this Blueprint flag and leaves the feet alone while it is true.
	if (AActor* Owner = GetOwner())
	{
		if (FBoolProperty* Flag = FindFProperty<FBoolProperty>(Owner->GetClass(), TEXT("bIsAttacking")))
		{
			Flag->SetPropertyValue_InContainer(Owner, bValue);
		}
	}
}

float UMeleeBrainComponent::OwnerMaxHealth()
{
	if (CachedMaxHealth > 0.f) return CachedMaxHealth;
	// Find the Blueprint health component (BPC_Health) by its variables, like the level component does.
	if (const AActor* Owner = GetOwner())
	{
		for (const UActorComponent* Comp : Owner->GetComponents())
		{
			if (!Comp) continue;
			float Value = MeleeReadNumber(Comp, TEXT("MaxHealth"), 0.f);
			if (Value <= 0.f) Value = MeleeReadNumber(Comp, TEXT("BaseMaxHealth"), 0.f);
			if (Value > 0.f)
			{
				CachedMaxHealth = Value;
				return CachedMaxHealth;
			}
		}
	}
	return 100.f; // not cached: try again next time (the health component may not have started yet)
}

int32 UMeleeBrainComponent::CountOthersAttacking(int32& OutMaxAllowed) const
{
	int32 Count = 0;
	OutMaxAllowed = MaxSimultaneousAttacks;
	const UWorld* World = GetWorld();
	for (const TWeakObjectPtr<UMeleeBrainComponent>& Weak : AllBrains)
	{
		const UMeleeBrainComponent* Other = Weak.Get();
		if (!Other || Other == this || Other->GetWorld() != World) continue;
		if (Other->State == EMeleeState::WindUp || Other->State == EMeleeState::Strike)
		{
			++Count;
			OutMaxAllowed = FMath::Max(OutMaxAllowed, Other->MaxSimultaneousAttacks);
		}
	}
	return Count;
}

void UMeleeBrainComponent::PlayAnim(UAnimSequenceBase* Anim, bool bLoop, float Rate) const
{
	if (!Anim) return;
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Mesh || Mesh->IsSimulatingPhysics()) return;
	// Straight onto the mesh, from the first frame: the snap into the pose is the point.
	Mesh->PlayAnimation(Anim, bLoop);
	Mesh->SetPlayRate(Rate);
}

void UMeleeBrainComponent::UpdateLocomotion()
{
	if (State != EMeleeState::Idle || (!IdleAnim && !RunAnim)) return;
	const AActor* Owner = GetOwner();
	if (!Owner) return;
	const bool bMoving = Owner->GetVelocity().Size2D() > RunSpeedThreshold;
	UAnimSequenceBase* Want = (bMoving && RunAnim) ? RunAnim.Get() : (IdleAnim ? IdleAnim.Get() : RunAnim.Get());
	if (Want && Want != CurrentLoco)
	{
		CurrentLoco = Want;
		PlayAnim(Want, true, Want == RunAnim ? RunPlayRate : 1.f);
	}
}

void UMeleeBrainComponent::PlaySound(USoundBase* Sound) const
{
	if (Sound && GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation());
	}
}

void UMeleeBrainComponent::StartTrails(const FMeleeAttack& Attack)
{
	StopTrails();
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Attack.StrikeTrailFX || !Mesh || !Mesh->GetSkeletalMeshAsset()) return;
	const FReferenceSkeleton& Ref = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();

	auto Spawn = [&](FName Bone, const FVector& Point)
	{
		const int32 Index = Bone.IsNone() ? INDEX_NONE : Ref.FindBoneIndex(Bone);
		if (Index == INDEX_NONE) return;
		// TrailPoint is given in the resting stance. Work out where that is relative to the bone, so the trail
		// stays on that spot of the blade however the arm moves.
		const FVector Offset = Point.IsNearlyZero() ? FVector::ZeroVector
			: FAnimationRuntime::GetComponentSpaceTransformRefPose(Ref, Index).InverseTransformPosition(Point);
		if (UNiagaraComponent* Trail = UNiagaraFunctionLibrary::SpawnSystemAttached(Attack.StrikeTrailFX, Mesh, Bone, Offset,
			FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true))
		{
			ActiveTrails.Add(Trail);
		}
	};
	Spawn(Attack.TrailBone, Attack.TrailPoint);
	Spawn(Attack.TrailBone2, Attack.TrailPoint2);
}

void UMeleeBrainComponent::StopTrails()
{
	for (UNiagaraComponent* Trail : ActiveTrails)
	{
		if (IsValid(Trail))
		{
			Trail->Deactivate(); // stops making new particles; the ones already out fade by themselves, then it removes itself
		}
	}
	ActiveTrails.Reset();
}

void UMeleeBrainComponent::SpawnImpact(const FMeleeAttack& Attack)
{
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	UWorld* World = GetWorld();
	if (!Attack.ImpactFX || !Char || !World) return;

	// On the ground, a little in front of it along the strike. Falls back to foot height if nothing is under it.
	const float HalfHeight = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Above = Char->GetActorLocation() + LockedDir * Attack.ImpactForward;
	FVector Where = Above - FVector(0.f, 0.f, HalfHeight);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeImpactFX), false, Char);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	if (World->LineTraceSingleByObjectType(Hit, Above, Above - FVector(0.f, 0.f, HalfHeight + 150.f), Objects, Params))
	{
		Where = Hit.ImpactPoint;
	}
	// These burst effects fire along their own +X, so point that up out of the ground.
	const FRotator Up = FRotationMatrix::MakeFromX(FVector::UpVector).Rotator();
	if (UNiagaraComponent* Burst = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Attack.ImpactFX, Where, Up))
	{
		Burst->SetVariableFloat(TEXT("Scale"), Attack.ImpactScale);
	}
}

void UMeleeBrainComponent::TakeTheFeet()
{
	SetOwnerAttacking(true);
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
		{
			AI->StopMovement();
		}
	}
	SetComponentTickEnabled(true);
}

void UMeleeBrainComponent::StopSliding()
{
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		UCharacterMovementComponent* Move = Char->GetCharacterMovement();
		if (Move && Move->IsMovingOnGround())
		{
			Move->Velocity = FVector(0.f, 0.f, Move->Velocity.Z);
		}
	}
	LungeSpeed = 0.f;
}

void UMeleeBrainComponent::EnterPhase(EMeleeState NewState, float Length)
{
	State = NewState;
	PhaseStart = Now();
	PhaseLength = FMath::Max(Length, 0.f);
}

// ------------------------------------------------------------------ deciding

void UMeleeBrainComponent::Think()
{
	if (State != EMeleeState::Idle) return; // the tick owns it while it is busy
	if (!IsAlive()) return;
	UpdateLocomotion();

	APawn* Pawn = Cast<APawn>(GetOwner());
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn || !Player) return;

	if (!bWarnedController && Pawn->GetController()->GetClass()->GetName().StartsWith(TEXT("AIC_Enemy")))
	{
		bWarnedController = true;
		const FString Msg = FString::Printf(TEXT("%s has a Melee Brain but still uses %s. Set its AI Controller Class to plain AIController, or it will also do the old instant attack."),
			*Pawn->GetName(), *Pawn->GetController()->GetClass()->GetName());
		UE_LOG(LogTemp, Warning, TEXT("%s"), *Msg);
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Orange, Msg);
	}

	if (Now() < NextAttackTime) return;

	const FVector Me = Pawn->GetActorLocation();
	const FVector Them = Player->GetActorLocation();
	const float Dist = (float)FVector::Dist2D(Me, Them);

	if (Awareness)
	{
		if (!Awareness->IsAggroed() || !Awareness->CanSeePlayer()) return;
		// In a pack, only the enemies holding an attack spot may swing. The rest are circling.
		if (bNeedsAttackSpot && Awareness->bSpreadOut && Awareness->bDriveMovement && !Awareness->HasAttackSpot()) return;
	}
	else if (Dist > 1500.f)
	{
		return;
	}

	if (FMath::Abs(Them.Z - Me.Z) > HeightTolerance) return;

	int32 MaxAllowed = MaxSimultaneousAttacks;
	if (CountOthersAttacking(MaxAllowed) >= MaxAllowed) return;

	if (ReadyTime.Num() != Attacks.Num())
	{
		ReadyTime.Init(0.f, Attacks.Num()); // the list was changed while playing
	}

	for (int32 i = 0; i < Attacks.Num(); ++i)
	{
		const FMeleeAttack& Attack = Attacks[i];
		if (Now() < ReadyTime[i]) continue;
		if (Dist < Attack.MinRange || Dist > Attack.MaxRange) continue;
		if (Attack.Chance < 1.f && FMath::FRand() > Attack.Chance)
		{
			ReadyTime[i] = Now() + 1.f;
			continue;
		}
		StartAttack(i);
		return;
	}
}

// ------------------------------------------------------------------ the attack

void UMeleeBrainComponent::StartAttack(int32 Index)
{
	if (!Attacks.IsValidIndex(Index)) return;
	Current = Index;
	const FMeleeAttack& Attack = Attacks[Index];

	bHitDone = false;
	bSecondHitArmed = false;
	LungeSpeed = 0.f;
	if (const AActor* Owner = GetOwner())
	{
		LockedDir = Owner->GetActorForwardVector().GetSafeNormal2D();
		LockedTarget = Owner->GetActorLocation() + LockedDir * Attack.MaxRange;
	}
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		LockedTarget = Player->GetActorLocation();
	}

	TakeTheFeet();
	StopSliding();
	CurrentLoco = nullptr;
	EnterPhase(EMeleeState::WindUp, Attack.WindUpTime);
	PlayAnim(Attack.WindUpAnim);
	PlaySound(Attack.WindUpSound);
	OnWindUp.Broadcast(Attack.Name);
}

void UMeleeBrainComponent::BeginStrike()
{
	const FMeleeAttack* Attack = CurrentAttack();
	ACharacter* Char = Cast<ACharacter>(GetOwner());
	if (!Attack || !Char)
	{
		EndAttack();
		return;
	}

	// Travel toward where the player was at aim lock, stopping just short, and never further than LungeDistance.
	const FVector Me = Char->GetActorLocation();
	FVector ToTarget = LockedTarget - Me;
	ToTarget.Z = 0.f;
	const float Gap = (float)ToTarget.Size();
	const float Travel = FMath::Clamp(Gap - MeleeStopShort, 0.f, Attack->LungeDistance);
	const float Time = FMath::Max(Attack->StrikeTime, 0.03f);

	Char->SetActorRotation(FRotator(0.f, LockedDir.Rotation().Yaw, 0.f));

	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	if (Attack->bLeap && Move)
	{
		// Thrown like a ball so it lands after StrikeTime: up speed = half of gravity x air time.
		const float UpSpeed = 0.5f * FMath::Abs(Move->GetGravityZ()) * Time;
		Char->LaunchCharacter(LockedDir * (Travel / Time) + FVector(0.f, 0.f, UpSpeed), true, true);
		LungeSpeed = 0.f;
	}
	else
	{
		LungeSpeed = Travel / Time;
	}

	EnterPhase(EMeleeState::Strike, Time);
	PlayAnim(Attack->StrikeAnim);
	PlaySound(Attack->StrikeSound);
	StartTrails(*Attack);
	OnStrike.Broadcast(Attack->Name);
}

void UMeleeBrainComponent::TryHit(const FMeleeAttack& Attack)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Pawn || !Player) return;

	const FVector To = Player->GetActorLocation() - Pawn->GetActorLocation();
	if (FMath::Abs(To.Z) > HeightTolerance) return;

	float PlayerRadius = 0.f;
	if (const ACharacter* PlayerChar = Cast<ACharacter>(Player))
	{
		PlayerRadius = PlayerChar->GetCapsuleComponent()->GetScaledCapsuleRadius();
	}
	const float Flat = (float)To.Size2D();
	if (Flat - PlayerRadius > Attack.Reach) return;

	// Inside the fan? (Standing right on top of the enemy always counts.)
	if (Flat > 60.f)
	{
		const float Dot = (float)FVector::DotProduct(To.GetSafeNormal2D(), LockedDir);
		if (Dot < FMath::Cos(FMath::DegreesToRadians(Attack.HalfAngle))) return;
	}

	// A wall or pillar between them blocks the hit.
	if (Awareness && !Awareness->HasLineOfSightTo(Player)) return;

	bHitDone = true;
	const float Damage = MeleeReadNumber(Pawn, TEXT("AttackDamage"), 10.f) * Attack.DamageMultiplier;
	UGameplayStatics::ApplyDamage(Player, Damage, Pawn->GetController(), Pawn, UDamageType::StaticClass());
	OnHitPlayer.Broadcast(Damage);
}

void UMeleeBrainComponent::BeginRecover()
{
	const FMeleeAttack* Attack = CurrentAttack();
	if (!Attack)
	{
		EndAttack();
		return;
	}
	StopSliding();
	StopTrails();
	SpawnImpact(*Attack);
	EnterPhase(EMeleeState::Recover, Attack->RecoverTime);
	PlayAnim(Attack->RecoverAnim);
	OnRecover.Broadcast(Attack->Name);
}

void UMeleeBrainComponent::EndAttack()
{
	if (Attacks.IsValidIndex(Current) && ReadyTime.IsValidIndex(Current) && State != EMeleeState::Stagger)
	{
		ReadyTime[Current] = Now() + Attacks[Current].Cooldown;
	}
	const bool bWasBusy = State != EMeleeState::Idle;
	StopTrails();
	State = EMeleeState::Idle;
	Current = INDEX_NONE;
	LungeSpeed = 0.f;
	NextAttackTime = Now() + FMath::FRandRange(AttackPauseMin, FMath::Max(AttackPauseMin, AttackPauseMax));
	SetOwnerAttacking(false);
	SetComponentTickEnabled(IsFlinching()); // a flinch still needs the tick to settle

	// Stand the mesh back up (unless it has become a ragdoll).
	LeanDegrees = 0.f;
	ApplyMeshPose();
	UpdateLocomotion(); // back to idle / run straight away, so it never sits on the last frame of an attack clip
	if (bWasBusy)
	{
		OnAttackEnded.Broadcast();
	}
}

// ------------------------------------------------------------------ alert

void UMeleeBrainComponent::HandleAggroed(EAggroReason Reason)
{
	// The moment it notices the player: stop, snap into the alert pose facing them, then come running.
	if (!bAlertReaction || State != EMeleeState::Idle || !IsAlive()) return;
	if (Reason == EAggroReason::Damage && !bAlertWhenShot) return;
	if (AlertChance < 1.f && FMath::FRand() > AlertChance) return;   // only some of them react; the rest just charge
	TakeTheFeet();
	StopSliding();
	CurrentLoco = nullptr;
	EnterPhase(EMeleeState::Alert, AlertTime);
	PlayAnim(AlertAnim);
	PlaySound(AlertSound);
	OnAlerted.Broadcast();
}

// ------------------------------------------------------------------ stagger

void UMeleeBrainComponent::HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                              AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.f || !IsAlive()) return;

	if (bFlinch)
	{
		// Jolt away from whoever shot it. Bigger hits and headshots jolt harder.
		const AActor* Owner = GetOwner();
		const AActor* Source = DamageCauser ? DamageCauser : (InstigatedBy ? InstigatedBy->GetPawn() : nullptr);
		FVector Away = Source ? (Owner->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		if (Away.IsNearlyZero()) Away = -Owner->GetActorForwardVector().GetSafeNormal2D();
		bool bFlinchCrit = false;
		bool bFlinchHead = false;
		UCombatHelpers::BreakHitDamageType(DamageType, bFlinchCrit, bFlinchHead);
		float Strength = FMath::Clamp(Damage / FMath::Max(OwnerMaxHealth() * 0.1f, 1.f), 0.6f, 1.4f);
		if (bFlinchHead) Strength *= HeadshotFlinchScale;
		StartFlinch(Away, Strength);
	}

	if (Now() - LastDamageTime > StaggerResetTime)
	{
		StaggerDamage = 0.f;
	}
	StaggerDamage += Damage;
	LastDamageTime = Now();

	if (State == EMeleeState::Stagger || Now() < StaggerImmuneUntil) return;
	if (State == EMeleeState::Strike && !bStaggerDuringStrike) return;

	bool bCrit = false;
	bool bHeadshot = false;
	UCombatHelpers::BreakHitDamageType(DamageType, bCrit, bHeadshot);

	const bool bHeadshotCancel = bHeadshotStaggersWindUp && bHeadshot && State == EMeleeState::WindUp;
	const bool bHeavyDamage = StaggerHealthFraction > 0.f && StaggerDamage >= OwnerMaxHealth() * StaggerHealthFraction;
	if (bHeadshotCancel || bHeavyDamage)
	{
		BeginStagger();
	}
}

void UMeleeBrainComponent::ForceStagger()
{
	if (IsAlive() && State != EMeleeState::Stagger)
	{
		BeginStagger();
	}
}

void UMeleeBrainComponent::BeginStagger()
{
	// The attack it was doing is cancelled; it may try the same one again right after.
	StopTrails();
	TakeTheFeet();
	StopSliding();
	EnterPhase(EMeleeState::Stagger, StaggerTime);
	StaggerDamage = 0.f;
	StaggerImmuneUntil = Now() + StaggerTime + StaggerImmunityTime;

	CurrentLoco = nullptr;
	// One of the stagger clips at random.
	TArray<UAnimSequenceBase*> Choices;
	if (StaggerAnim) Choices.Add(StaggerAnim);
	for (UAnimSequenceBase* Variant : StaggerAnimVariants)
	{
		if (Variant) Choices.Add(Variant);
	}
	if (Choices.Num() > 0)
	{
		PlayAnim(Choices[FMath::RandRange(0, Choices.Num() - 1)]);
	}
	OnStaggered.Broadcast();
}

// ------------------------------------------------------------------ every frame while busy

void UMeleeBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsAlive())
	{
		// Died (maybe mid-attack): let go without touching the ragdoll.
		StopTrails();
		State = EMeleeState::Idle;
		Current = INDEX_NONE;
		LungeSpeed = 0.f;
		FlinchStart = -1000.f;
		SetComponentTickEnabled(false);
		return;
	}
	if (State == EMeleeState::Idle)
	{
		// Not attacking: the only reason to be ticking is a flinch that has not settled yet.
		ApplyMeshPose();
		if (!IsFlinching())
		{
			SetComponentTickEnabled(false);
		}
		return;
	}

	ACharacter* Char = Cast<ACharacter>(GetOwner());
	const FMeleeAttack* Attack = CurrentAttack();
	const float T = Now() - PhaseStart;

	switch (State)
	{
	case EMeleeState::WindUp:
	{
		if (!Attack || !Char)
		{
			EndAttack();
			return;
		}
		// Keep turning to face the player until the aim locks.
		if (T < PhaseLength - Attack->AimLockTime)
		{
			if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				const FVector To = (Player->GetActorLocation() - Char->GetActorLocation()).GetSafeNormal2D();
				if (!To.IsNearlyZero())
				{
					const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
					const FRotator Have(0.f, Char->GetActorRotation().Yaw, 0.f);
					Char->SetActorRotation(FMath::RInterpConstantTo(Have, Want, DeltaTime, TurnSpeed));
				}
				LockedTarget = Player->GetActorLocation();
			}
			LockedDir = Char->GetActorForwardVector().GetSafeNormal2D();
		}
		if (T >= PhaseLength)
		{
			BeginStrike();
		}
		break;
	}
	case EMeleeState::Strike:
	{
		if (!Attack || !Char)
		{
			EndAttack();
			return;
		}
		if (LungeSpeed > 0.f)
		{
			UCharacterMovementComponent* Move = Char->GetCharacterMovement();
			if (Move && Move->IsMovingOnGround())
			{
				Move->Velocity = FVector(LockedDir.X * LungeSpeed, LockedDir.Y * LungeSpeed, Move->Velocity.Z);
			}
		}
		// Double attack: part way through the strike the hit shape arms once more.
		if (Attack->SecondHitDelay > 0.f && !bSecondHitArmed && T >= Attack->SecondHitDelay)
		{
			bSecondHitArmed = true;
			bHitDone = false;
		}
		if (!bHitDone && T >= Attack->HitDelay)
		{
			TryHit(*Attack);
		}
		if (T >= PhaseLength)
		{
			BeginRecover();
		}
		break;
	}
	case EMeleeState::Recover:
		// A short stumble forward after its own swing (speed ramps up then down, so it covers RecoverStepDistance).
		if (Attack && Char && Attack->RecoverStepDistance > 0.f)
		{
			const float U = (T - Attack->RecoverStepDelay) / FMath::Max(Attack->RecoverStepTime, 0.05f);
			UCharacterMovementComponent* Move = Char->GetCharacterMovement();
			if (U >= 0.f && U <= 1.f && Move && Move->IsMovingOnGround())
			{
				const float Speed = Attack->RecoverStepDistance / FMath::Max(Attack->RecoverStepTime, 0.05f) * 1.5708f * FMath::Sin(U * PI);
				Move->Velocity = FVector(LockedDir.X * Speed, LockedDir.Y * Speed, Move->Velocity.Z);
			}
		}
		if (T >= PhaseLength)
		{
			EndAttack();
			return;
		}
		break;
	case EMeleeState::Stagger:
		if (T >= PhaseLength)
		{
			EndAttack();
			return;
		}
		break;
	case EMeleeState::Alert:
		// Turn to face the player while it reacts.
		if (Char)
		{
			if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
			{
				const FVector To = (Player->GetActorLocation() - Char->GetActorLocation()).GetSafeNormal2D();
				if (!To.IsNearlyZero())
				{
					const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
					const FRotator Have(0.f, Char->GetActorRotation().Yaw, 0.f);
					Char->SetActorRotation(FMath::RInterpConstantTo(Have, Want, DeltaTime, TurnSpeed));
				}
			}
		}
		if (T >= PhaseLength)
		{
			EndAttack();
			NextAttackTime = Now(); // the reaction was the pause: it may attack as soon as it is in range
			return;
		}
		break;
	default:
		break;
	}

	UpdateLean();
	ApplyMeshPose();
	if (bDebugDraw)
	{
		DrawDebug();
	}
}

// ------------------------------------------------------------------ flinch + mesh pose

bool UMeleeBrainComponent::IsFlinching() const
{
	return bFlinch && (Now() - FlinchStart) < FlinchTime;
}

void UMeleeBrainComponent::StartFlinch(const FVector& WorldDirection, float Strength)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !bHasMeshBase) return;
	FlinchDirLocal = Owner->GetActorTransform().InverseTransformVectorNoScale(WorldDirection).GetSafeNormal2D();
	FlinchStrength = Strength;
	FlinchStart = Now();           // a new hit restarts it, so rapid fire keeps it shuddering
	SetComponentTickEnabled(true);
	OnFlinched.Broadcast();
}

void UMeleeBrainComponent::ApplyMeshPose()
{
	// Everything that moves the whole mesh inside its capsule goes through here: the flinch and the placeholder lean.
	if (!bHasMeshBase) return;
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Mesh || Mesh->IsSimulatingPhysics()) return;

	float Flinch = 0.f;
	if (IsFlinching())
	{
		// Snaps out in 0.03 s, then eases back.
		const float T = Now() - FlinchStart;
		const float In = FMath::Min(0.03f, FlinchTime * 0.5f);
		Flinch = (T < In ? T / In : 1.f - FMath::SmoothStep(0.f, 1.f, (T - In) / FMath::Max(FlinchTime - In, 0.01f))) * FlinchStrength;
	}
	const bool bWantPose = Flinch > 0.f || !FMath::IsNearlyZero(LeanDegrees);
	if (!bWantPose && !bMeshPosed) return;   // already standing on its base pose: leave the mesh alone

	// Pitch up is positive in Unreal, so leaning forward is a negative pitch (in the capsule's space).
	const FQuat Lean = FRotator(-LeanDegrees, 0.f, 0.f).Quaternion();
	FQuat Tip = FQuat::Identity;
	FVector Shove = FVector::ZeroVector;
	if (Flinch > 0.f && !FlinchDirLocal.IsNearlyZero())
	{
		// Tip the top of the body away from the shot (it pivots at the feet) and shove it the same way.
		const FVector Tilted = (FVector::UpVector + FlinchDirLocal * FMath::Tan(FMath::DegreesToRadians(FlinchAngle * Flinch))).GetSafeNormal();
		Tip = FQuat::FindBetweenNormals(FVector::UpVector, Tilted);
		Shove = FlinchDirLocal * FlinchDistance * Flinch;
	}
	Mesh->SetRelativeLocationAndRotation(MeshBaseLocation + Shove, Tip * Lean * MeshBaseRotation);
	bMeshPosed = bWantPose;
}

// ------------------------------------------------------------------ testing aids

void UMeleeBrainComponent::UpdateLean()
{
	LeanDegrees = 0.f;
	if (!bPlaceholderLean || !bHasMeshBase) return;

	const float T = Now() - PhaseStart;
	const float A = GetPhaseAlpha();
	float Lean = 0.f; // degrees, forward is positive
	switch (State)
	{
	case EMeleeState::WindUp:
		Lean = -22.f * FMath::Clamp(T / 0.06f, 0.f, 1.f); // snaps back almost at once: the "big first frame"
		break;
	case EMeleeState::Strike:
		Lean = FMath::Lerp(-22.f, 42.f, FMath::Clamp(A * 2.f, 0.f, 1.f));
		break;
	case EMeleeState::Recover:
		Lean = 42.f * (1.f - FMath::SmoothStep(0.6f, 1.f, A)); // stays slumped, stands up at the end
		break;
	case EMeleeState::Stagger:
		Lean = -30.f * FMath::Clamp(T / 0.05f, 0.f, 1.f) * (1.f - FMath::SmoothStep(0.5f, 1.f, A));
		break;
	default:
		break;
	}
	LeanDegrees = Lean;
}

void UMeleeBrainComponent::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	const UWorld* World = GetWorld();
	if (!Char || !World) return;

	const float HalfHeight = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = Char->GetActorLocation() - FVector(0.f, 0.f, HalfHeight - 8.f);
	const FVector Head = Char->GetActorLocation() + FVector(0.f, 0.f, HalfHeight + 40.f);

	if (State == EMeleeState::Recover)
	{
		DrawDebugSphere(World, Head, 18.f, 8, FColor::Green);
		return;
	}
	if (State == EMeleeState::Stagger)
	{
		DrawDebugSphere(World, Head, 18.f, 8, FColor::White);
		return;
	}
	if (State == EMeleeState::Alert)
	{
		DrawDebugSphere(World, Head, 18.f, 8, FColor::Magenta);
		return;
	}

	const FMeleeAttack* Attack = CurrentAttack();
	if (!Attack) return;

	const float T = Now() - PhaseStart;
	FColor Color = FColor::Yellow;
	float Thickness = 2.f;
	if (State == EMeleeState::WindUp && T >= PhaseLength - Attack->AimLockTime)
	{
		Color = FColor::Orange;
	}
	else if (State == EMeleeState::Strike)
	{
		const bool bLive = !bHitDone && T >= Attack->HitDelay;
		Color = bLive ? FColor::Red : FColor(120, 0, 0);
		Thickness = 4.f;
	}

	// The fan: two edges and an arc.
	const float Yaw = LockedDir.Rotation().Yaw;
	const int32 Steps = 12;
	FVector Prev = Feet;
	for (int32 i = 0; i <= Steps; ++i)
	{
		const float Angle = Yaw + FMath::Lerp(-Attack->HalfAngle, Attack->HalfAngle, (float)i / Steps);
		const FVector Point = Feet + FRotator(0.f, Angle, 0.f).Vector() * Attack->Reach;
		DrawDebugLine(World, Prev, Point, Color, false, -1.f, 0, Thickness);
		Prev = Point;
	}
	DrawDebugLine(World, Prev, Feet, Color, false, -1.f, 0, Thickness);
#endif
}
