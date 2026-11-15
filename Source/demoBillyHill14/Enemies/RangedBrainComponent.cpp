// Fill out your copyright notice in the Description page of Project Settings.

#include "RangedBrainComponent.h"

#include "EnemyAwarenessComponent.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

URangedBrainComponent::URangedBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URangedBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner) return;
	Awareness = Owner->FindComponentByClass<UEnemyAwarenessComponent>();

	if (bFacePlayer)
	{
		// Turn with the controller's focus (the player) instead of toward where it is walking: strafing looks right.
		if (ACharacter* Char = Cast<ACharacter>(Owner))
		{
			Char->bUseControllerRotationYaw = false;
			if (UCharacterMovementComponent* Move = Char->GetCharacterMovement())
			{
				Move->bOrientRotationToMovement = false;
				Move->bUseControllerDesiredRotation = true;
			}
		}
	}

	StrafeDir = FMath::RandBool() ? 1.f : -1.f;
	NextShotTime = Now() + FMath::FRandRange(0.3f, 0.8f); // small grace after waking before the first shot
	GetWorld()->GetTimerManager().SetTimer(ThinkTimer, this, &URangedBrainComponent::Think, ThinkInterval, true,
		FMath::FRandRange(0.f, ThinkInterval));
}

void URangedBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
		World->GetTimerManager().ClearTimer(ShotTimer);
	}
	Super::EndPlay(EndPlayReason);
}

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

void URangedBrainComponent::StopShooting()
{
	bWindingUp = false;
	ShotsLeft = 0;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ShotTimer);
	}
}

void URangedBrainComponent::Think()
{
	if (!IsAlive())
	{
		StopShooting();
		return;
	}
	APawn* Pawn = Cast<APawn>(GetOwner());
	AAIController* AI = Cast<AAIController>(Pawn->GetController());
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!AI || !Player) return;

	// Track how long the player has been standing still (always, so it is ready the moment we wake).
	if (Player->GetVelocity().Size2D() < StillSpeed)
	{
		StillTime += ThinkInterval;
	}
	else
	{
		StillTime = 0.f;
	}

	const float Dist = FVector::Dist(Pawn->GetActorLocation(), Player->GetActorLocation());

	// Asleep: without an Awareness component, wake on plain line of sight within shooting range.
	bool bAggroed = false;
	bool bSees = false;
	if (Awareness)
	{
		bAggroed = Awareness->IsAggroed();
		bSees = Awareness->CanSeePlayer();
	}
	else
	{
		bSees = Dist <= MaxShootRange && AI->LineOfSightTo(Player);
		bAggroed = bSees;
	}

	if (!bAggroed)
	{
		AI->ClearFocus(EAIFocusPriority::Gameplay);
		StopShooting();
		if (bReturningHome && Awareness && FVector::Dist2D(Pawn->GetActorLocation(), Awareness->GetHomeLocation()) < 150.f)
		{
			bReturningHome = false;
		}
		return;
	}
	bReturningHome = false;

	// Give up after not seeing the player for a while (Awareness only does this itself for enemies it walks).
	if (Awareness && Awareness->GiveUpTime > 0.f && Awareness->GetTimeSinceSeen() > Awareness->GiveUpTime)
	{
		StopShooting();
		AI->ClearFocus(EAIFocusPriority::Gameplay);
		Awareness->Calm(false);
		AI->MoveToLocation(Awareness->GetHomeLocation(), 50.f, true, true, true, true, nullptr, true);
		bReturningHome = true;
		return;
	}

	const bool bKnowsWhere = bSees || !Awareness || Awareness->GetTimeSinceSeen() <= Awareness->LoseSightGrace;
	if (bSees)
	{
		AI->SetFocus(Player);
	}
	else
	{
		AI->ClearFocus(EAIFocusPriority::Gameplay);
	}

	Move(AI, Player, bKnowsWhere);

	if (bSees && !IsShooting() && Now() >= NextShotTime && Dist <= MaxShootRange && ProjectileClass)
	{
		StartWindUp(StillTime >= StillTimeToPunish);
	}
}

void URangedBrainComponent::Move(AAIController* AI, APawn* Player, bool bKnowsWhere)
{
	const AActor* Owner = GetOwner();
	const FVector Me = Owner->GetActorLocation();

	if (!bKnowsWhere)
	{
		// Lost sight: go to where the player was last seen to find an angle again.
		if (Awareness)
		{
			AI->MoveToLocation(Awareness->GetLastSeenLocation(), 80.f, true, true, true, true, nullptr, true);
		}
		return;
	}

	if (Now() >= NextStrafeSwitch)
	{
		StrafeDir = -StrafeDir;
		NextStrafeSwitch = Now() + FMath::FRandRange(StrafeSwitchMin, FMath::Max(StrafeSwitchMin, StrafeSwitchMax));
	}

	const FVector P = Player->GetActorLocation();
	FVector Away = (Me - P).GetSafeNormal2D();
	if (Away.IsNearlyZero()) Away = -Owner->GetActorForwardVector().GetSafeNormal2D();

	const float D = (float)FVector::Dist2D(Me, P);
	float R = D;
	if (D < MinRange || D > MaxRange) R = PreferredRange;

	const FVector Side = FVector::CrossProduct(FVector::UpVector, Away) * StrafeDir;
	FVector Goal = P + Away * R + Side * StrafeStep;
	Goal.Z = Me.Z;

	const EPathFollowingRequestResult::Type Result =
		AI->MoveToLocation(Goal, 60.f, false, true, true, true, nullptr, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		// Blocked that way (wall, ledge): strafe the other way next time.
		StrafeDir = -StrafeDir;
		NextStrafeSwitch = Now() + FMath::FRandRange(StrafeSwitchMin, FMath::Max(StrafeSwitchMin, StrafeSwitchMax));
	}
}

void URangedBrainComponent::StartWindUp(bool bBurst)
{
	bWindingUp = true;
	bPendingBurst = bBurst;
	OnWindUp.Broadcast(bBurst);
	const float Delay = bBurst ? BurstWindUpTime : WindUpTime;
	if (Delay <= KINDA_SMALL_NUMBER)
	{
		BeginShots();
	}
	else
	{
		GetWorld()->GetTimerManager().SetTimer(ShotTimer, this, &URangedBrainComponent::BeginShots, Delay, false);
	}
}

void URangedBrainComponent::BeginShots()
{
	bWindingUp = false;
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const bool bCanSee = Player && (Awareness ? Awareness->HasLineOfSightTo(Player) : true);
	if (!IsAlive() || !bCanSee)
	{
		// Player ducked behind cover during the wind-up: no shot, try again soon.
		NextShotTime = Now() + Cooldown * 0.5f;
		return;
	}
	ShotsLeft = bPendingBurst ? FMath::Max(1, BurstCount) : 1;
	FireOne();
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

void URangedBrainComponent::FireOne()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!IsAlive() || !Player || !ProjectileClass)
	{
		StopShooting();
		return;
	}

	// Aim at where the player is right now: moving sideways dodges it.
	const FVector From = MuzzleLocation();
	const FVector Target = Player->GetActorLocation() + FVector(0.f, 0.f, AimHeightOffset);
	FVector Dir = (Target - From).GetSafeNormal();
	if (AimSpread > 0.f)
	{
		Dir = FMath::VRandCone(Dir, FMath::DegreesToRadians(AimSpread));
	}

	FActorSpawnParameters Params;
	Params.Owner = Pawn;
	Params.Instigator = Pawn; // enemy level damage scaling reads this
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Projectile = GetWorld()->SpawnActor<AActor>(ProjectileClass, From, Dir.Rotation(), Params);
	if (Projectile)
	{
		if (ProjectileSpeed > 0.f)
		{
			if (UProjectileMovementComponent* PM = Projectile->FindComponentByClass<UProjectileMovementComponent>())
			{
				PM->MaxSpeed = FMath::Max(PM->MaxSpeed, ProjectileSpeed);
				PM->Velocity = Dir * ProjectileSpeed;
			}
		}
		OnFire.Broadcast(Projectile);
	}

	--ShotsLeft;
	if (ShotsLeft > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(ShotTimer, this, &URangedBrainComponent::FireOne, BurstInterval, false);
	}
	else
	{
		ShotsLeft = 0;
		NextShotTime = Now() + Cooldown + FMath::FRandRange(0.f, CooldownRandom);
	}
}
