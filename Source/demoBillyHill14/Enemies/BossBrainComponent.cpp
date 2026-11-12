// Fill out your copyright notice in the Description page of Project Settings.

#include "BossBrainComponent.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

UBossBrainComponent::UBossBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBossBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UBossBrainComponent::HandleOwnerDamaged);
	}

	NextWanderTime = Now() + FMath::FRandRange(WanderWaitMin, FMath::Max(WanderWaitMin, WanderWaitMax));
	GetWorld()->GetTimerManager().SetTimer(ThinkTimer, this, &UBossBrainComponent::Think, ThinkInterval, true,
		FMath::FRandRange(0.f, ThinkInterval)); // spread out several bosses' thinking
}

void UBossBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ThinkTimer);
	}
	Super::EndPlay(EndPlayReason);
}

float UBossBrainComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

void UBossBrainComponent::HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                             AController* InstigatedBy, AActor* DamageCauser)
{
	if (!bAggroed)
	{
		bAggroed = true;
		NextSlamTime = Now() + FirstSlamDelay;
	}
}

void UBossBrainComponent::HoldStill(float Seconds)
{
	HoldUntil = FMath::Max(HoldUntil, Now() + Seconds);
	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
		{
			SetHold(AI);
		}
	}
}

void UBossBrainComponent::SetHold(AAIController* AI)
{
	if (MoveState != EBossMoveState::Hold)
	{
		AI->StopMovement();
		MoveState = EBossMoveState::Hold;
	}
}

void UBossBrainComponent::StartWander(AAIController* AI, const FVector& From)
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav)
	{
		return;
	}

	// A few tries to find a reachable spot that is far enough away to be worth the walk.
	for (int32 Try = 0; Try < 6; ++Try)
	{
		FNavLocation Spot;
		if (Nav->GetRandomReachablePointInRadius(From, WanderDistanceMax, Spot)
			&& FVector::Dist2D(Spot.Location, From) >= WanderDistanceMin)
		{
			AI->MoveToLocation(Spot.Location, 50.f, /*bStopOnOverlap*/ true, /*bUsePathfinding*/ true);
			MoveState = EBossMoveState::Wander;
			WanderEndTime = Now() + WanderTimeout;
			return;
		}
	}
}

void UBossBrainComponent::Think()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	// IsValid also catches a controller that was destroyed but not cleaned up yet (the death logic
	// destroys it, and the pawn's pointer only clears later). Without this he kept "attacking" as a corpse.
	// The enemy death logic also ragdolls the mesh, so a simulating body means dead too.
	const USkeletalMeshComponent* Body = Pawn ? Pawn->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	const bool bRagdolled = Body && Body->IsSimulatingPhysics();
	if (bRagdolled || !IsValid(AI) || !IsValid(Pawn) || AI->GetPawn() != Pawn)
	{
		// Only stop for good when he is surely dead (ragdoll); a missing controller can also just mean
		// "not possessed yet" right after spawning.
		UWorld* World = GetWorld();
		if (bRagdolled && World)
		{
			World->GetTimerManager().ClearTimer(ThinkTimer);
		}
		if (IsValid(AI))
		{
			AI->StopMovement();
			AI->ClearFocus(EAIFocusPriority::Gameplay);
		}
		// No live controller: dead or not possessed yet.
		// Fade out any sound attached to him (attack cries etc.) so nothing keeps playing on the corpse.
		// Note: Spawn Sound Attached makes the sound belong to the world, not to him, so GetComponents
		// alone misses it. Also check everything attached to each of his parts (the mesh in particular).
		if (AActor* Owner = GetOwner())
		{
			TArray<UAudioComponent*> Sounds;
			Owner->GetComponents<UAudioComponent>(Sounds);
			TArray<USceneComponent*> Parts;
			Owner->GetComponents<USceneComponent>(Parts);
			for (USceneComponent* Part : Parts)
			{
				if (!Part)
				{
					continue;
				}
				for (USceneComponent* Child : Part->GetAttachChildren())
				{
					if (UAudioComponent* S = Cast<UAudioComponent>(Child))
					{
						Sounds.AddUnique(S);
					}
				}
			}
			for (UAudioComponent* S : Sounds)
			{
				if (IsValid(S) && S->IsPlaying())
				{
					S->FadeOut(0.15f, 0.f);
				}
			}
		}
		return;
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	const float T = Now();
	const FVector Me = Pawn->GetActorLocation();
	const float Dist = FVector::Dist(Me, Player->GetActorLocation());
	const float FlatDist = FVector::Dist2D(Me, Player->GetActorLocation());

	if (!bAggroed)
	{
		if (Dist > AggroRange)
		{
			return;
		}
		bAggroed = true;
		NextSlamTime = T + FirstSlamDelay;
	}

	AI->SetFocus(Player);
	const bool bSeesPlayer = AI->LineOfSightTo(Player);

	// ---- attacks (independent of each other, so they can overlap) ----
	if (FlatDist <= SlamRange && T >= NextSlamTime)
	{
		NextSlamTime = T + SlamCooldown;
		HoldStill(SlamHoldTime);
		OnSlam.Broadcast();
	}
	if (bSeesPlayer && Dist <= VolleyRange)
	{
		OnVolley.Broadcast();
	}

	// ---- feet ----
	if (T < HoldUntil)
	{
		SetHold(AI);
		return;
	}

	const bool bMoving = AI->GetMoveStatus() == EPathFollowingStatus::Moving;

	switch (MoveState)
	{
	case EBossMoveState::Leash:
		if (Dist <= LeashStopDistance && bSeesPlayer)
		{
			SetHold(AI);
		}
		else if (!bMoving)
		{
			AI->MoveToActor(Player, bSeesPlayer ? LeashStopDistance * 0.8f : 300.f);
		}
		return;

	case EBossMoveState::Wander:
		if (!bMoving || T >= WanderEndTime)
		{
			SetHold(AI);
			NextWanderTime = T + FMath::FRandRange(WanderWaitMin, FMath::Max(WanderWaitMin, WanderWaitMax));
		}
		break;

	case EBossMoveState::Hold:
	default:
		break;
	}

	// Player far away or hidden: go after them.
	if (Dist > LeashRange || !bSeesPlayer)
	{
		AI->MoveToActor(Player, bSeesPlayer ? LeashStopDistance * 0.8f : 300.f);
		MoveState = EBossMoveState::Leash;
		return;
	}

	// Standing around: sometimes take a little walk.
	if (MoveState == EBossMoveState::Hold && T >= NextWanderTime)
	{
		NextWanderTime = T + FMath::FRandRange(WanderWaitMin, FMath::Max(WanderWaitMin, WanderWaitMax));
		if (FMath::FRand() < WanderChance)
		{
			StartWander(AI, Me);
		}
	}
}
