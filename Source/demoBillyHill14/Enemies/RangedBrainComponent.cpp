// Fill out your copyright notice in the Description page of Project Settings.

#include "RangedBrainComponent.h"

#include "../Stats/CombatHelpers.h"
#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

TArray<TWeakObjectPtr<URangedBrainComponent>> URangedBrainComponent::AllBrains;

namespace
{
	float RangedReadNumber(const UObject* Object, const TCHAR* Name, float Default)
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

URangedBrainComponent::URangedBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while it is awake (or settling a flinch)

	static ConstructorHelpers::FObjectFinder<UStaticMesh> LineMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	AimLineMesh = LineMesh.Object;
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LineMat(TEXT("/Game/FX/M_AimLine.M_AimLine"));
	AimLineMaterial = LineMat.Object;
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Tracer(TEXT("/Game/FX/NS_EmberTrail.NS_EmberTrail"));

	// Starting numbers for the gunner. Change them per enemy in the Blueprint.
	// The burst comes first in the list and has a chance; when it is not picked (or is cooling down) he fires the bolt.
	// All three shots go down the locked line: dodge at the lock and every one of them misses.
	FRangedAttack Burst;
	Burst.Name = TEXT("Burst");
	Burst.Chance = 0.3f;
	Burst.Cooldown = 4.f;
	Burst.ShotCount = 3;
	Burst.ShotInterval = 0.12f;
	Burst.AimSpread = 1.2f;
	Burst.RecoverTime = 0.8f;
	Burst.TracerFX = Tracer.Object;

	FRangedAttack Bolt;
	Bolt.Name = TEXT("Bolt");
	Bolt.TracerFX = Tracer.Object;

	Attacks.Add(Burst);
	Attacks.Add(Bolt);
}

void URangedBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;
	Awareness = Owner->FindComponentByClass<UEnemyAwarenessComponent>();

	// Unique: BeginPlay can run twice on an actor when World Partition streams its cell in.
	Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &URangedBrainComponent::HandleOwnerDamaged);
	if (Awareness)
	{
		Awareness->OnAggroed.AddUniqueDynamic(this, &URangedBrainComponent::HandleAggroed);
	}

	if (ACharacter* Char = Cast<ACharacter>(Owner))
	{
		if (Char->GetMesh() && !bHasMeshBase)
		{
			MeshBaseRotation = Char->GetMesh()->GetRelativeRotation().Quaternion();
			MeshBaseLocation = Char->GetMesh()->GetRelativeLocation();
			bHasMeshBase = true;
		}
		// It runs facing where it is going, and this component turns it by hand while it stands and aims.
		Char->bUseControllerRotationYaw = false;
		if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
		{
			Move->bOrientRotationToMovement = true;
			Move->bUseControllerDesiredRotation = false;
		}
	}

	AllBrains.RemoveAll([](const TWeakObjectPtr<URangedBrainComponent>& B) { return !B.IsValid(); });
	AllBrains.AddUnique(this);

	ReadyTime.Init(0.f, Attacks.Num());
	SideSign = FMath::RandBool() ? 1.f : -1.f;
	GetWorld()->GetTimerManager().SetTimer(ThinkTimer, this, &URangedBrainComponent::Think, ThinkInterval, true,
		FMath::FRandRange(0.f, ThinkInterval));
}

void URangedBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
	}
	AllBrains.RemoveAll([this](const TWeakObjectPtr<URangedBrainComponent>& B) { return !B.IsValid() || B.Get() == this; });
	StopAimFX();
	HideAimLine();
	if (AimLine)
	{
		AimLine->DestroyComponent();
		AimLine = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------ helpers

float URangedBrainComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

bool URangedBrainComponent::IsAlive() const
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

const FRangedAttack* URangedBrainComponent::CurrentAttack() const
{
	return Attacks.IsValidIndex(Current) ? &Attacks[Current] : nullptr;
}

float URangedBrainComponent::GetPhaseAlpha() const
{
	if (State == ERangedState::Asleep) return 0.f;
	if (PhaseLength <= KINDA_SMALL_NUMBER) return 1.f;
	return FMath::Clamp((Now() - PhaseStart) / PhaseLength, 0.f, 1.f);
}

FName URangedBrainComponent::GetCurrentAttackName() const
{
	const FRangedAttack* Attack = CurrentAttack();
	const bool bAttacking = State == ERangedState::Aim || State == ERangedState::Fire || State == ERangedState::Recover;
	return (Attack && bAttacking) ? Attack->Name : NAME_None;
}

void URangedBrainComponent::SetOwnerAttacking(bool bValue) const
{
	// The enemy Blueprint's own flag: other systems read it to know the enemy is busy.
	if (AActor* Owner = GetOwner())
	{
		if (FBoolProperty* Flag = FindFProperty<FBoolProperty>(Owner->GetClass(), TEXT("bIsAttacking")))
		{
			Flag->SetPropertyValue_InContainer(Owner, bValue);
		}
	}
}

float URangedBrainComponent::OwnerMaxHealth()
{
	if (CachedMaxHealth > 0.f) return CachedMaxHealth;
	// Find the Blueprint health component (BPC_Health) by its variables, like the level component does.
	if (const AActor* Owner = GetOwner())
	{
		for (const UActorComponent* Comp : Owner->GetComponents())
		{
			if (!Comp) continue;
			float Value = RangedReadNumber(Comp, TEXT("MaxHealth"), 0.f);
			if (Value <= 0.f) Value = RangedReadNumber(Comp, TEXT("BaseMaxHealth"), 0.f);
			if (Value > 0.f)
			{
				CachedMaxHealth = Value;
				return CachedMaxHealth;
			}
		}
	}
	return 100.f; // not cached: try again next time (the health component may not have started yet)
}

void URangedBrainComponent::EnterPhase(ERangedState NewState, float Length)
{
	State = NewState;
	PhaseStart = Now();
	PhaseLength = FMath::Max(Length, 0.f);
}

void URangedBrainComponent::StopMoving() const
{
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	if (!Char) return;
	if (AAIController* AI = Cast<AAIController>(Char->GetController()))
	{
		AI->StopMovement();
	}
	// Plant the feet at once: no sliding into the aim.
	UCharacterMovementComponent* Move = Char->GetCharacterMovement();
	if (Move && Move->IsMovingOnGround())
	{
		Move->Velocity = FVector(0.f, 0.f, Move->Velocity.Z);
	}
}

void URangedBrainComponent::FaceToward(const FVector& Point, float DeltaTime) const
{
	AActor* Owner = GetOwner();
	if (!Owner) return;
	const FVector To = (Point - Owner->GetActorLocation()).GetSafeNormal2D();
	if (To.IsNearlyZero()) return;
	const FRotator Want(0.f, To.Rotation().Yaw, 0.f);
	const FRotator Have(0.f, Owner->GetActorRotation().Yaw, 0.f);
	Owner->SetActorRotation(FMath::RInterpConstantTo(Have, Want, DeltaTime, TurnSpeed));
}

FVector URangedBrainComponent::AimPointNow() const
{
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		FVector Point = Player->GetActorLocation() + FVector(0.f, 0.f, AimHeightOffset);

		// Shoot ahead of a moving player: where they will be when the bullet gets there, scaled by the attack's Aim Lead.
		// Only their speed along the ground counts, so a jump still gets them out of the way.
		const FRangedAttack* Attack = CurrentAttack();
		if (Attack && Attack->AimLead > 0.f)
		{
			float FlightTime = 0.f;
			if (Attack->bLobbed)
			{
				FlightTime = Attack->LobFlightTime;
			}
			else if (Attack->ProjectileSpeed > 1.f)
			{
				FlightTime = (float)FVector::Dist(MuzzleLocation(), Point) / Attack->ProjectileSpeed;
			}
			FVector Velocity = Player->GetVelocity();
			Velocity.Z = 0.f;
			Point += Velocity * FMath::Min(FlightTime, 2.f) * Attack->AimLead;
		}
		return Point;
	}
	return LockedAim;
}

bool URangedBrainComponent::ShootCapFull() const
{
	int32 MaxAllowed = MaxSimultaneousShooters;
	if (MaxAllowed <= 0) return false;
	int32 Count = 0;
	const UWorld* World = GetWorld();
	for (const TWeakObjectPtr<URangedBrainComponent>& Weak : AllBrains)
	{
		const URangedBrainComponent* Other = Weak.Get();
		if (!Other || Other == this || Other->GetWorld() != World) continue;
		if (Other->State == ERangedState::Aim || Other->State == ERangedState::Fire)
		{
			++Count;
			if (Other->MaxSimultaneousShooters <= 0) return false;   // someone in this fight has no limit
			MaxAllowed = FMath::Max(MaxAllowed, Other->MaxSimultaneousShooters);
		}
	}
	return Count >= MaxAllowed;
}

FVector URangedBrainComponent::MuzzleLocation() const
{
	const AActor* Owner = GetOwner();
	if (!MuzzleName.IsNone())
	{
		for (const UActorComponent* Comp : Owner->GetComponents())
		{
			const USceneComponent* Scene = Cast<USceneComponent>(Comp);
			if (Scene && Scene->GetFName() == MuzzleName) return Scene->GetComponentLocation();
		}
		if (const ACharacter* Char = Cast<ACharacter>(Owner))
		{
			const USkeletalMeshComponent* Mesh = Char->GetMesh();
			if (Mesh && Mesh->DoesSocketExist(MuzzleName)) return Mesh->GetSocketLocation(MuzzleName);
		}
	}
	float HalfHeight = 60.f;
	if (const ACharacter* Char = Cast<ACharacter>(Owner))
	{
		HalfHeight = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	return Owner->GetActorLocation() + FVector(0.f, 0.f, HalfHeight * 0.4f) + Owner->GetActorForwardVector() * 60.f;
}

void URangedBrainComponent::PlayAnim(UAnimSequenceBase* Anim, bool bLoop, float Rate, float BlendIn) const
{
	if (!Anim) return;
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	if (!Mesh || Mesh->IsSimulatingPhysics()) return;

	// With an Animation Blueprint on the mesh: play the clip through its Slot node as a one-clip montage, so it
	// cross-fades from whatever was playing. Starting a new one fades the old one out over the same time.
	UAnimInstance* Anims = Mesh->GetAnimationMode() == EAnimationMode::AnimationBlueprint ? Mesh->GetAnimInstance() : nullptr;
	if (Anims && !AnimSlotName.IsNone())
	{
		UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(Anim, AnimSlotName, FMath::Max(0.f, BlendIn), 0.f, 1.f,
			bLoop ? 1000000 : 1);
		if (Montage)
		{
			// Hold the last frame instead of fading out to the bare stance: the brain decides what plays next, and when.
			Montage->bEnableAutoBlendOut = false;
			Anims->Montage_Play(Montage, Rate);
			return;
		}
	}

	// No Animation Blueprint: straight onto the mesh from the first frame (a hard cut).
	Mesh->PlayAnimation(Anim, bLoop);
	Mesh->SetPlayRate(Rate);
}

void URangedBrainComponent::UpdateLocomotion()
{
	if ((State != ERangedState::Asleep && State != ERangedState::Reposition) || (!IdleAnim && !RunAnim)) return;
	const AActor* Owner = GetOwner();
	if (!Owner) return;
	const bool bMoving = Owner->GetVelocity().Size2D() > RunSpeedThreshold;
	UAnimSequenceBase* Want = (bMoving && RunAnim) ? RunAnim.Get() : (IdleAnim ? IdleAnim.Get() : RunAnim.Get());
	if (Want && Want != CurrentLoco)
	{
		CurrentLoco = Want;
		PlayAnim(Want, true, Want == RunAnim ? RunPlayRate : 1.f, BlendLocomotion);
	}
}

void URangedBrainComponent::PlaySound(USoundBase* Sound) const
{
	if (Sound && GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetOwner()->GetActorLocation());
	}
}

void URangedBrainComponent::SpawnAtMuzzle(UNiagaraSystem* System, bool bAttach)
{
	if (!System) return;
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	USkeletalMeshComponent* Mesh = Char ? Char->GetMesh() : nullptr;
	const bool bOnMesh = Mesh && !MuzzleName.IsNone() && Mesh->DoesSocketExist(MuzzleName);

	UNiagaraComponent* Spawned = nullptr;
	if (bAttach && bOnMesh)
	{
		// Rides the muzzle for as long as it lives (the aim glow).
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Mesh, MuzzleName, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true);
	}
	else
	{
		const FVector From = MuzzleLocation();
		FVector Dir = (LockedAim - From).GetSafeNormal();
		if (Dir.IsNearlyZero()) Dir = GetOwner()->GetActorForwardVector();
		Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, From, Dir.Rotation());
	}
	if (bAttach)
	{
		AimGlow = Spawned;
	}
}

void URangedBrainComponent::StopAimFX()
{
	if (AimGlow)
	{
		AimGlow->Deactivate();   // lets what is already in the air fade out, then it removes itself
		AimGlow = nullptr;
	}
}

void URangedBrainComponent::UpdateAimLine()
{
	if (!bAimLine || !AimLineMaterial || !AimLineMesh || !GetOwner()) return;
	UWorld* World = GetWorld();
	if (!World) return;

	// A lobbed shot does not fly along a straight line, so a line would lie to the player.
	// And an attack can simply have its line turned off (common shooters).
	if (const FRangedAttack* Attack = CurrentAttack(); Attack && (Attack->bLobbed || !Attack->bShowAimLine))
	{
		HideAimLine();
		return;
	}

	if (!AimLine)
	{
		// A thin glowing rod, stretched from the muzzle along the aim. It lives in the world, not on the enemy's bones.
		AimLine = NewObject<UStaticMeshComponent>(GetOwner(), NAME_None, RF_Transient);
		AimLine->SetStaticMesh(AimLineMesh);
		AimLine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		AimLine->SetGenerateOverlapEvents(false);
		AimLine->SetCastShadow(false);
		AimLine->SetCanEverAffectNavigation(false);
		AimLine->SetMobility(EComponentMobility::Movable);
		AimLine->RegisterComponent();
		AimLineMID = UMaterialInstanceDynamic::Create(AimLineMaterial, this);
		AimLine->SetMaterial(0, AimLineMID);
	}

	const FVector From = MuzzleLocation();
	FVector Dir = (LockedAim - From).GetSafeNormal();
	if (Dir.IsNearlyZero()) Dir = GetOwner()->GetActorForwardVector();

	// Through the player and on until it hits the world.
	FVector End = From + Dir * AimLineLength;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RangedAimLine), false, GetOwner());
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Params.AddIgnoredActor(Player);
	}
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, From, End, ECC_Visibility, Params))
	{
		End = Hit.ImpactPoint;
	}

	const float Length = (float)FVector::Dist(From, End);
	const float Width = bAimLocked ? AimLineLockWidth : AimLineWidth;
	// The engine cylinder is 100 units tall along Z and 100 wide.
	AimLine->SetWorldLocationAndRotation((From + End) * 0.5f, FRotationMatrix::MakeFromZ(Dir).ToQuat());
	AimLine->SetWorldScale3D(FVector(Width / 100.f, Width / 100.f, FMath::Max(Length, 1.f) / 100.f));
	if (AimLineMID)
	{
		const FLinearColor Color = bAimLocked ? AimLineLockColor * AimLineLockGlow : AimLineColor * AimLineGlow;
		AimLineMID->SetVectorParameterValue(TEXT("Color"), Color);
	}
	AimLine->SetVisibility(true);
}

void URangedBrainComponent::HideAimLine()
{
	if (AimLine)
	{
		AimLine->SetVisibility(false);
	}
}

// ------------------------------------------------------------------ deciding (asleep, waking, giving up)

void URangedBrainComponent::Think()
{
	if (!IsAlive())
	{
		StopAimFX();
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	AAIController* AI = Cast<AAIController>(Pawn->GetController());
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!AI || !Player) return;

	// Track how long the player has been standing still (always, so it is ready the moment it wakes).
	if (Player->GetVelocity().Size2D() < StillSpeed)
	{
		StillTime += ThinkInterval;
	}
	else
	{
		StillTime = 0.f;
	}

	// Without an Awareness component: awake on plain line of sight within shooting range.
	bool bAggroed = false;
	if (Awareness)
	{
		bAggroed = Awareness->IsAggroed();
	}
	else
	{
		bAggroed = FVector::Dist(Pawn->GetActorLocation(), Player->GetActorLocation()) <= MaxShootRange && AI->LineOfSightTo(Player);
	}

	if (!bAggroed)
	{
		if (State != ERangedState::Asleep && State != ERangedState::Stagger)
		{
			GoToSleep();
		}
		if (bReturningHome && Awareness && FVector::Dist2D(Pawn->GetActorLocation(), Awareness->GetHomeLocation()) < 150.f)
		{
			bReturningHome = false;
		}
		UpdateLocomotion(); // idle, or the run clip while it walks home
		return;
	}
	bReturningHome = false;

	// Give up after not seeing the player for a while, and walk home.
	if (Awareness && Awareness->GiveUpTime > 0.f && Awareness->GetTimeSinceSeen() > Awareness->GiveUpTime
		&& State != ERangedState::Aim && State != ERangedState::Fire && State != ERangedState::Stagger)
	{
		GoToSleep();
		Awareness->Calm(false);
		AI->MoveToLocation(Awareness->GetHomeLocation(), 50.f, true, true, true, true, nullptr, true);
		bReturningHome = true;
		return;
	}

	if (State == ERangedState::Asleep)
	{
		// Just woke up and did not do the alert reaction: straight to work.
		BeginReposition(true);
	}
}

void URangedBrainComponent::GoToSleep()
{
	StopAimFX();
	HideAimLine();
	State = ERangedState::Asleep;
	Current = INDEX_NONE;
	bMoveDone = false;
	bGoingToLastSeen = false;
	SetOwnerAttacking(false);
	SetComponentTickEnabled(IsFlinching());
}

void URangedBrainComponent::HandleAggroed(EAggroReason Reason)
{
	// The moment it notices the player: stop, react, then get to work.
	if (!bAlertReaction || State != ERangedState::Asleep || !IsAlive()) return;
	if (Reason == EAggroReason::Damage && !bAlertWhenShot) return;
	if (AlertChance < 1.f && FMath::FRand() > AlertChance) return;
	bReturningHome = false;
	StopMoving();
	CurrentLoco = nullptr;
	EnterPhase(ERangedState::Alert, AlertTime);
	PlayAnim(AlertAnim, false, 1.f, BlendAlert);
	PlaySound(AlertSound);
	SetComponentTickEnabled(true);
	OnAlerted.Broadcast();
}

// ------------------------------------------------------------------ reposition

void URangedBrainComponent::BeginReposition(bool bFirst)
{
	EnterPhase(ERangedState::Reposition, RepositionTimeMax);
	bMoveDone = false;
	bGoingToLastSeen = false;
	Current = INDEX_NONE;
	SetOwnerAttacking(false);
	SetComponentTickEnabled(true);
	if (bFirst)
	{
		NextAttackTime = Now() + FMath::FRandRange(0.3f, 0.7f); // small grace after waking before the first aim
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!AI || !Player)
	{
		bMoveDone = true;
		return;
	}

	const bool bSees = Awareness ? Awareness->CanSeePlayer() : AI->LineOfSightTo(Player);
	const bool bKnowsWhere = bSees || !Awareness || Awareness->GetTimeSinceSeen() <= Awareness->LoseSightGrace;
	if (!bKnowsWhere)
	{
		// Lost sight: go to where the player was last seen to find an angle again.
		AI->MoveToLocation(Awareness->GetLastSeenLocation(), 80.f, true, true, true, true, nullptr, true);
		bGoingToLastSeen = true;
		return;
	}

	const FVector Me = Pawn->GetActorLocation();
	const FVector P = Player->GetActorLocation();
	FVector Away = (Me - P).GetSafeNormal2D();
	if (Away.IsNearlyZero()) Away = -Pawn->GetActorForwardVector().GetSafeNormal2D();

	// Keep the distance it has if that is inside its band; otherwise head for the preferred one (back off, or close in).
	const float D = (float)FVector::Dist2D(Me, P);
	const float R = (D < MinRange || D > MaxRange) ? PreferredRange : D;

	SideSign = FMath::RandBool() ? 1.f : -1.f;
	const float Angle = FMath::FRandRange(SideStepAngleMin, FMath::Max(SideStepAngleMin, SideStepAngleMax));
	for (int32 Try = 0; Try < 2; ++Try)
	{
		FVector Goal = P + Away.RotateAngleAxis(Angle * SideSign, FVector::UpVector) * R;
		Goal.Z = Me.Z;
		if (AI->MoveToLocation(Goal, 60.f, false, true, true, true, nullptr, true) != EPathFollowingRequestResult::Failed)
		{
			return;
		}
		SideSign = -SideSign; // blocked that way (wall, ledge): try the other side
	}
	bMoveDone = true; // nowhere to go: shoot from here
}

void URangedBrainComponent::TickReposition(float DeltaTime, APawn* Pawn, AAIController* AI, APawn* Player)
{
	UpdateLocomotion();
	if (!AI || !Player) return;

	const float T = Now() - PhaseStart;
	const bool bSees = Awareness ? Awareness->CanSeePlayer() : AI->LineOfSightTo(Player);
	const float Dist = (float)FVector::Dist(Pawn->GetActorLocation(), Player->GetActorLocation());
	const bool bCanShootFromHere = bSees && Dist <= MaxShootRange;

	if (!bMoveDone)
	{
		const bool bArrived = AI->GetMoveStatus() == EPathFollowingStatus::Idle;
		// Its pause is over and it has a shot: stop where it is, do not finish the run.
		const bool bTimeToShoot = T >= 0.4f && Now() >= NextAttackTime && bCanShootFromHere && Dist >= MinRange * 0.6f;
		if (bArrived || bTimeToShoot || T >= PhaseLength)
		{
			if (!bArrived) StopMoving();
			bMoveDone = true;
		}
		return;
	}

	// Standing at its spot: watch the player.
	if (bSees)
	{
		FaceToward(Player->GetActorLocation(), DeltaTime);
	}
	if (Now() < NextAttackTime) return;

	if (bCanShootFromHere && ShootCapFull())
	{
		// Enough of the others are shooting already: keep moving and look again in a moment.
		NextAttackTime = Now() + FMath::FRandRange(0.3f, 0.7f);
		BeginReposition(false);
		return;
	}
	if (bCanShootFromHere)
	{
		const int32 Index = PickAttack(Dist);
		if (Index != INDEX_NONE)
		{
			BeginAim(Index);
			return;
		}
	}
	// No shot from here (no line of sight, too far, or everything on cooldown): move again, but not every frame.
	if (T >= 0.5f)
	{
		BeginReposition(false);
	}
}

int32 URangedBrainComponent::PickAttack(float Distance) const
{
	const bool bPlayerStill = StillTime >= StillTimeToPunish;
	int32 Fallback = INDEX_NONE;
	for (int32 i = 0; i < Attacks.Num(); ++i)
	{
		const FRangedAttack& Attack = Attacks[i];
		if (ReadyTime.IsValidIndex(i) && Now() < ReadyTime[i]) continue;
		if (Distance < Attack.MinRange || Distance > Attack.MaxRange) continue;
		if (Attack.bOnlyWhenPlayerStill && !bPlayerStill) continue;
		if (!Attack.ProjectileClass && !ProjectileClass) continue;
		Fallback = i;
		if (Attack.Chance >= 1.f || FMath::FRand() <= Attack.Chance)
		{
			return i;
		}
	}
	return Fallback;
}

// ------------------------------------------------------------------ the attack

void URangedBrainComponent::BeginAim(int32 Index)
{
	if (!Attacks.IsValidIndex(Index)) return;
	Current = Index;
	const FRangedAttack& Attack = Attacks[Index];

	StopMoving();
	SetOwnerAttacking(true);
	EnterPhase(ERangedState::Aim, Attack.AimTime);
	bAimLocked = false;
	ShotsFired = 0;
	LockedAim = AimPointNow();

	CurrentLoco = nullptr;
	PlayAnim(Attack.AimAnim, false, 1.f, BlendAim);
	PlaySound(Attack.AimSound);
	StopAimFX();
	SpawnAtMuzzle(Attack.AimFX, true);
	OnWindUp.Broadcast(Attack.ShotCount > 1);
	OnAim.Broadcast(Attack.Name);
}

void URangedBrainComponent::BeginFire()
{
	const FRangedAttack* Attack = CurrentAttack();
	if (!Attack)
	{
		EndAttack();
		BeginReposition(false);
		return;
	}
	StopAimFX();
	const int32 Count = FMath::Max(1, Attack->ShotCount);
	EnterPhase(ERangedState::Fire, (Count - 1) * Attack->ShotInterval + Attack->FireHoldTime);
	FireOne();
}

void URangedBrainComponent::FireOne()
{
	const FRangedAttack* Attack = CurrentAttack();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Attack || !Pawn) return;
	++ShotsFired;

	if (Attack->bReaimEachShot)
	{
		LockedAim = AimPointNow();
	}
	const FVector From = MuzzleLocation();

	PlayAnim(Attack->FireAnim);   // restarts on every shot of a burst
	PlaySound(Attack->FireSound);
	SpawnAtMuzzle(Attack->MuzzleFX, false);

	UClass* Class = Attack->ProjectileClass ? Attack->ProjectileClass.Get() : ProjectileClass.Get();
	if (!Class) return;

	// A lobbed shot: the launch velocity that lands it on the target after Lob Flight Time.
	const float Gravity = GetWorld()->GetGravityZ() * Attack->LobGravityScale;
	const float FlightTime = FMath::Max(Attack->LobFlightTime, 0.2f);
	FVector LobVelocity = FVector::ZeroVector;
	if (Attack->bLobbed)
	{
		LobVelocity = (LobLandingPoint() - From) / FlightTime;
		LobVelocity.Z -= 0.5f * Gravity * FlightTime;
	}

	FVector AimDir = (LockedAim - From).GetSafeNormal();
	if (AimDir.IsNearlyZero()) AimDir = Pawn->GetActorForwardVector();

	// A moving player is harder to hit.
	float Spread = Attack->AimSpread;
	if (Attack->MovingSpread > 0.f)
	{
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Spread += Attack->MovingSpread * FMath::Clamp((float)Player->GetVelocity().Size2D() / FMath::Max(PlayerFullSpeed, 1.f), 0.f, 1.f);
		}
	}

	// One bullet, or several at once spread side to side (a fan).
	const int32 Pellets = FMath::Clamp(Attack->PelletCount, 1, 24);
	for (int32 i = 0; i < Pellets; ++i)
	{
		const float Yaw = Pellets > 1 ? Attack->FanAngle * ((float)i / (float)(Pellets - 1) - 0.5f) : 0.f;

		FVector Velocity;
		if (Attack->bLobbed)
		{
			Velocity = LobVelocity.RotateAngleAxis(Yaw, FVector::UpVector);
			if (Spread > 0.f)
			{
				Velocity = FMath::VRandCone(Velocity.GetSafeNormal(), FMath::DegreesToRadians(Spread)) * Velocity.Size();
			}
		}
		else
		{
			FVector Dir = AimDir.RotateAngleAxis(Yaw, FVector::UpVector);
			if (Spread > 0.f)
			{
				Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(Spread));
			}
			Velocity = Dir * Attack->ProjectileSpeed;   // zero when the projectile keeps its own speed
			if (Velocity.IsNearlyZero()) Velocity = Dir;
		}

		FActorSpawnParameters Params;
		Params.Owner = Pawn;
		Params.Instigator = Pawn; // enemy level damage scaling reads this
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Projectile = GetWorld()->SpawnActor<AActor>(Class, From, Velocity.Rotation(), Params);
		if (!Projectile) continue;

		if (UProjectileMovementComponent* PM = Projectile->FindComponentByClass<UProjectileMovementComponent>())
		{
			if (Attack->bLobbed)
			{
				PM->ProjectileGravityScale = Attack->LobGravityScale;
				PM->MaxSpeed = 0.f;   // no limit: it speeds up on the way down
				PM->Velocity = Velocity;
			}
			else if (Attack->ProjectileSpeed > 0.f)
			{
				PM->MaxSpeed = FMath::Max(PM->MaxSpeed, Attack->ProjectileSpeed);
				PM->Velocity = Velocity;
			}
		}
		if (Attack->TracerFX && Projectile->GetRootComponent())
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(Attack->TracerFX, Projectile->GetRootComponent(), NAME_None, FVector::ZeroVector,
				FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
		}
		OnFire.Broadcast(Projectile);
	}
}

FVector URangedBrainComponent::LobLandingPoint() const
{
	// The ground under the spot it locked onto (the locked spot itself is at the player's chest).
	const UWorld* World = GetWorld();
	if (!World) return LockedAim;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RangedLobGround), false, GetOwner());
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		Params.AddIgnoredActor(Player);
	}
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, LockedAim, LockedAim - FVector(0.f, 0.f, 1500.f), ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}
	return LockedAim;
}

void URangedBrainComponent::BeginRecover()
{
	const FRangedAttack* Attack = CurrentAttack();
	if (!Attack)
	{
		EndAttack();
		BeginReposition(false);
		return;
	}
	HideAimLine();
	EnterPhase(ERangedState::Recover, Attack->RecoverTime);
	PlayAnim(Attack->RecoverAnim);
	OnRecover.Broadcast(Attack->Name);
}

void URangedBrainComponent::EndAttack()
{
	if (Attacks.IsValidIndex(Current) && ReadyTime.IsValidIndex(Current))
	{
		ReadyTime[Current] = Now() + Attacks[Current].Cooldown;
	}
	StopAimFX();
	HideAimLine();
	Current = INDEX_NONE;
	NextAttackTime = Now() + FMath::FRandRange(AttackPauseMin, FMath::Max(AttackPauseMin, AttackPauseMax));
	SetOwnerAttacking(false);
	OnAttackEnded.Broadcast();
}

// ------------------------------------------------------------------ stagger

void URangedBrainComponent::HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                               AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.f || !IsAlive()) return;

	bool bCrit = false;
	bool bHeadshot = false;
	UCombatHelpers::BreakHitDamageType(DamageType, bCrit, bHeadshot);

	if (bFlinch)
	{
		// Jolt away from whoever shot it. Bigger hits and headshots jolt harder.
		const AActor* Owner = GetOwner();
		const AActor* Source = DamageCauser ? DamageCauser : (InstigatedBy ? InstigatedBy->GetPawn() : nullptr);
		FVector Away = Source ? (Owner->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D() : FVector::ZeroVector;
		if (Away.IsNearlyZero()) Away = -Owner->GetActorForwardVector().GetSafeNormal2D();
		float Strength = FMath::Clamp(Damage / FMath::Max(OwnerMaxHealth() * 0.1f, 1.f), 0.6f, 1.4f);
		if (bHeadshot) Strength *= HeadshotFlinchScale;
		StartFlinch(Away, Strength);
	}

	if (Now() - LastDamageTime > StaggerResetTime)
	{
		StaggerDamage = 0.f;
	}
	StaggerDamage += Damage;
	LastDamageTime = Now();

	if (State == ERangedState::Stagger || Now() < StaggerImmuneUntil) return;

	const bool bHeadshotCancel = bHeadshotStaggersAim && bHeadshot && State == ERangedState::Aim;
	const bool bHeavyDamage = StaggerHealthFraction > 0.f && StaggerDamage >= OwnerMaxHealth() * StaggerHealthFraction;
	if (bHeadshotCancel || bHeavyDamage)
	{
		BeginStagger();
	}
}

void URangedBrainComponent::ForceStagger()
{
	if (IsAlive() && State != ERangedState::Stagger)
	{
		BeginStagger();
	}
}

void URangedBrainComponent::BeginStagger()
{
	// Whatever it was doing is cancelled, including a shot it was about to take.
	StopAimFX();
	HideAimLine();
	StopMoving();
	SetOwnerAttacking(true);
	Current = INDEX_NONE;
	EnterPhase(ERangedState::Stagger, StaggerTime);
	StaggerDamage = 0.f;
	StaggerImmuneUntil = Now() + StaggerTime + StaggerImmunityTime;
	NextAttackTime = Now() + StaggerTime + FMath::FRandRange(0.3f, 0.6f);
	SetComponentTickEnabled(true);

	CurrentLoco = nullptr;
	TArray<UAnimSequenceBase*> Choices;
	if (StaggerAnim) Choices.Add(StaggerAnim);
	for (UAnimSequenceBase* Variant : StaggerAnimVariants)
	{
		if (Variant) Choices.Add(Variant);
	}
	if (Choices.Num() > 0)
	{
		PlayAnim(Choices[FMath::RandRange(0, Choices.Num() - 1)], false, 1.f, BlendStagger);
	}
	OnStaggered.Broadcast();
}

// ------------------------------------------------------------------ every frame while awake

void URangedBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsAlive())
	{
		// Died (maybe mid-aim): let go without touching the ragdoll.
		StopAimFX();
		HideAimLine();
		State = ERangedState::Asleep;
		Current = INDEX_NONE;
		FlinchStart = -1000.f;
		SetComponentTickEnabled(false);
		return;
	}
	if (State == ERangedState::Asleep)
	{
		// Asleep: the only reason to be ticking is a flinch that has not settled yet.
		ApplyMeshPose();
		if (!IsFlinching())
		{
			SetComponentTickEnabled(false);
		}
		return;
	}

	APawn* Pawn = Cast<APawn>(GetOwner());
	AAIController* AI = Cast<AAIController>(Pawn->GetController());
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FRangedAttack* Attack = CurrentAttack();
	const float T = Now() - PhaseStart;

	switch (State)
	{
	case ERangedState::Reposition:
		TickReposition(DeltaTime, Pawn, AI, Player);
		break;
	case ERangedState::Aim:
	{
		if (!Attack)
		{
			EndAttack();
			BeginReposition(false);
			break;
		}
		if (!bAimLocked)
		{
			if (T < PhaseLength - Attack->AimLockTime)
			{
				// Follow the player with the aim. The spot it will shoot at moves with them.
				LockedAim = AimPointNow();
				FaceToward(LockedAim, DeltaTime);
			}
			else
			{
				bAimLocked = true; // from here it is committed: move and it misses
				PlaySound(LockSound);
				SpawnAtMuzzle(LockFX, false);
				if (Attack->bLobbed && Attack->LandingFX)
				{
					UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Attack->LandingFX, LobLandingPoint());
				}
			}
		}
		UpdateAimLine();
		if (T >= PhaseLength)
		{
			BeginFire();
		}
		break;
	}
	case ERangedState::Fire:
	{
		if (!Attack)
		{
			EndAttack();
			BeginReposition(false);
			break;
		}
		const int32 Count = FMath::Max(1, Attack->ShotCount);
		if (Attack->bReaimEachShot && ShotsFired < Count)
		{
			FaceToward(AimPointNow(), DeltaTime);
		}
		while (ShotsFired < Count && T >= ShotsFired * Attack->ShotInterval)
		{
			FireOne();
		}
		if (ShotsFired < Count)
		{
			UpdateAimLine(); // stays up until the last shot of a burst has gone
		}
		else
		{
			HideAimLine();
		}
		if (ShotsFired >= Count && T >= PhaseLength)
		{
			BeginRecover();
		}
		break;
	}
	case ERangedState::Recover:
		if (T >= PhaseLength)
		{
			EndAttack();
			BeginReposition(false);
		}
		break;
	case ERangedState::Stagger:
		if (T >= PhaseLength)
		{
			SetOwnerAttacking(false);
			BeginReposition(false); // Think puts it back to sleep if nothing woke it
		}
		break;
	case ERangedState::Alert:
		if (Player)
		{
			FaceToward(Player->GetActorLocation(), DeltaTime);
		}
		if (T >= PhaseLength)
		{
			BeginReposition(true);
		}
		break;
	default:
		break;
	}

	ApplyMeshPose();
	if (bDebugDraw)
	{
		DrawDebug();
	}
}

// ------------------------------------------------------------------ flinch + mesh pose

bool URangedBrainComponent::IsFlinching() const
{
	return bFlinch && (Now() - FlinchStart) < FlinchTime;
}

void URangedBrainComponent::StartFlinch(const FVector& WorldDirection, float Strength)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !bHasMeshBase) return;
	FlinchDirLocal = Owner->GetActorTransform().InverseTransformVectorNoScale(WorldDirection).GetSafeNormal2D();
	FlinchStrength = Strength;
	FlinchStart = Now();           // a new hit restarts it, so rapid fire keeps it shuddering
	SetComponentTickEnabled(true);
	OnFlinched.Broadcast();
}

void URangedBrainComponent::ApplyMeshPose()
{
	// The flinch moves the whole mesh inside its capsule: a quick tip and shove away from the shot.
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
	const bool bWantPose = Flinch > 0.f;
	if (!bWantPose && !bMeshPosed) return;   // already standing on its base pose: leave the mesh alone

	FQuat Tip = FQuat::Identity;
	FVector Shove = FVector::ZeroVector;
	if (Flinch > 0.f && !FlinchDirLocal.IsNearlyZero())
	{
		const FVector Tilted = (FVector::UpVector + FlinchDirLocal * FMath::Tan(FMath::DegreesToRadians(FlinchAngle * Flinch))).GetSafeNormal();
		Tip = FQuat::FindBetweenNormals(FVector::UpVector, Tilted);
		Shove = FlinchDirLocal * FlinchDistance * Flinch;
	}
	Mesh->SetRelativeLocationAndRotation(MeshBaseLocation + Shove, Tip * MeshBaseRotation);
	bMeshPosed = bWantPose;
}

// ------------------------------------------------------------------ testing aids

void URangedBrainComponent::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World) return;
	if (State == ERangedState::Aim || State == ERangedState::Fire)
	{
		const FColor Color = (State == ERangedState::Fire || bAimLocked) ? FColor::Red : FColor::Yellow;
		DrawDebugLine(World, MuzzleLocation(), LockedAim, Color, false, -1.f, 0, bAimLocked ? 4.f : 2.f);
		DrawDebugSphere(World, LockedAim, 18.f, 8, Color, false, -1.f, 0, 2.f);
	}
#endif
}
