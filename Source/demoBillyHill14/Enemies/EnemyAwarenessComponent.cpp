// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyAwarenessComponent.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationData.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

TArray<TWeakObjectPtr<UEnemyAwarenessComponent>> UEnemyAwarenessComponent::AllAware;
uint64 UEnemyAwarenessComponent::SlotFrame = 0;
TWeakObjectPtr<UWorld> UEnemyAwarenessComponent::SlotWorld;

UEnemyAwarenessComponent::UEnemyAwarenessComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyAwarenessComponent::BeginPlay()
{
	Super::BeginPlay();

	AllAware.RemoveAll([](const TWeakObjectPtr<UEnemyAwarenessComponent>& C) { return !C.IsValid(); });
	AllAware.AddUnique(this);

	if (AActor* Owner = GetOwner())
	{
		// Unique: BeginPlay can run twice on an actor when World Partition streams its cell in.
		Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UEnemyAwarenessComponent::HandleOwnerDamaged);
		HomeLocation = Owner->GetActorLocation();
	}
	CircleDir = FMath::RandBool() ? 1.f : -1.f;

	FTimerManager& Timers = GetWorld()->GetTimerManager();
	// Random start offsets spread many enemies' traces and path requests over several frames.
	Timers.SetTimer(LookTimer, this, &UEnemyAwarenessComponent::Look, SightInterval, true, FMath::FRandRange(0.f, SightInterval));
	Timers.SetTimer(ChaseTimer, this, &UEnemyAwarenessComponent::Chase, ChaseInterval, true, FMath::FRandRange(0.f, ChaseInterval));
}

void UEnemyAwarenessComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LookTimer);
		World->GetTimerManager().ClearTimer(AlertTimer);
		World->GetTimerManager().ClearTimer(ChaseTimer);
	}
	AllAware.RemoveAll([this](const TWeakObjectPtr<UEnemyAwarenessComponent>& C) { return !C.IsValid() || C.Get() == this; });
	Super::EndPlay(EndPlayReason);
}

// ------------------------------------------------------------------ helpers

float UEnemyAwarenessComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.f;
}

float UEnemyAwarenessComponent::GetTimeSinceSeen() const
{
	return Now() - LastSeenTime;
}

void UEnemyAwarenessComponent::NoteSeen(const FVector& Where)
{
	LastSeenLocation = Where;
	LastSeenTime = Now();
	bSearchArrived = false;
}

bool UEnemyAwarenessComponent::IsAlive() const
{
	// The enemy death logic destroys the controller (the pawn can keep a stale pointer to it, so IsValid, not just
	// non-null) and turns the mesh into a ragdoll. Either means dead.
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!IsValid(Pawn) || Pawn->IsActorBeingDestroyed() || !IsValid(Pawn->GetController())) return false;
	if (const ACharacter* Char = Cast<ACharacter>(Pawn))
	{
		if (Char->GetMesh() && Char->GetMesh()->IsSimulatingPhysics()) return false;
	}
	return true;
}

void UEnemyAwarenessComponent::SetFlagOnOwner(bool bValue) const
{
	// Keep the Blueprint flag "bIsAggroed" in sync so the AI controller and older Blueprint logic agree with us.
	if (AActor* Owner = GetOwner())
	{
		if (FBoolProperty* Flag = FindFProperty<FBoolProperty>(Owner->GetClass(), TEXT("bIsAggroed")))
		{
			Flag->SetPropertyValue_InContainer(Owner, bValue);
		}
	}
}

bool UEnemyAwarenessComponent::OwnerIsAttacking() const
{
	const AActor* Owner = GetOwner();
	if (!Owner) return false;
	if (const FBoolProperty* Flag = FindFProperty<FBoolProperty>(Owner->GetClass(), TEXT("bIsAttacking")))
	{
		return Flag->GetPropertyValue_InContainer(Owner);
	}
	return false;
}

float UEnemyAwarenessComponent::OwnerAttackRange() const
{
	const AActor* Owner = GetOwner();
	if (Owner)
	{
		if (const FNumericProperty* Prop = FindFProperty<FNumericProperty>(Owner->GetClass(), TEXT("AttackRange")))
		{
			const void* Value = Prop->ContainerPtrToValuePtr<void>(Owner);
			return Prop->IsFloatingPoint() ? (float)Prop->GetFloatingPointPropertyValue(Value) : (float)Prop->GetSignedIntPropertyValue(Value);
		}
	}
	return 150.f;
}

FVector UEnemyAwarenessComponent::EyeLocation() const
{
	const AActor* Owner = GetOwner();
	if (!Owner) return FVector::ZeroVector;
	float HalfHeight = 60.f;
	if (const ACharacter* Char = Cast<ACharacter>(Owner))
	{
		HalfHeight = Char->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	return Owner->GetActorLocation() + FVector(0.f, 0.f, HalfHeight * EyeHeightFraction);
}

bool UEnemyAwarenessComponent::TraceClear(const FVector& From, const FVector& To, const AActor* Target) const
{
	// Only world geometry blocks sight. Pawns (the player, other enemies) are not in the query at all.
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySight), false, GetOwner());
	if (Target) Params.AddIgnoredActor(Target);

	FHitResult Hit;
	const bool bBlocked = GetWorld()->LineTraceSingleByObjectType(Hit, From, To, Objects, Params);
	if (bDebugDraw)
	{
		DrawDebugLine(GetWorld(), From, bBlocked ? Hit.ImpactPoint : To, bBlocked ? FColor::Red : FColor::Green, false, SightInterval * 1.1f, 0, 1.5f);
	}
	return !bBlocked;
}

bool UEnemyAwarenessComponent::HasLineOfSightTo(const AActor* Target) const
{
	if (!Target || !GetWorld()) return false;
	const FVector Eye = EyeLocation();

	// Try the target's eyes (pawns) and its middle, so a low wall or a ledge doesn't hide the whole body.
	FVector Head = Target->GetActorLocation();
	if (const APawn* Pawn = Cast<APawn>(Target)) Head = Pawn->GetPawnViewLocation();
	if (TraceClear(Eye, Head, Target)) return true;
	return TraceClear(Eye, Target->GetActorLocation(), Target);
}

// ------------------------------------------------------------------ sight

void UEnemyAwarenessComponent::Look()
{
	if (!IsAlive())
	{
		bSeesPlayer = false;
		return;
	}
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const AActor* Owner = GetOwner();
	if (!Player || !Owner) return;

	const FVector ToPlayer = Player->GetActorLocation() - Owner->GetActorLocation();
	const float Dist = ToPlayer.Size();

	bool bSees = false;
	if (Dist <= SightRange)
	{
		bool bInCone = true;
		// Once awake it is looking for the player, so the cone only matters while asleep.
		if (!bAggroed && SightHalfAngle < 180.f && Dist > KINDA_SMALL_NUMBER)
		{
			const float CosHalf = FMath::Cos(FMath::DegreesToRadians(SightHalfAngle));
			bInCone = FVector::DotProduct(Owner->GetActorForwardVector(), ToPlayer / Dist) >= CosHalf;
		}
		bSees = bInCone && HasLineOfSightTo(Player);
	}
	bSeesPlayer = bSees;

	const bool bSensed = CloseSenseRange > 0.f && Dist <= CloseSenseRange;
	if (bSees || bSensed)
	{
		NoteSeen(Player->GetActorLocation());
	}

	if (!bAggroed && (bSees || bSensed))
	{
		Aggro(EAggroReason::Sight);
	}
}

// ------------------------------------------------------------------ aggro + alerts

void UEnemyAwarenessComponent::Aggro(EAggroReason Reason)
{
	if (bAggroed) return;
	bAggroed = true;
	bAlertPending = false;
	SetFlagOnOwner(true);

	// Damage, alerts and scripts tell the enemy where the player is right now (sight already noted it).
	if (Reason != EAggroReason::Sight)
	{
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			NoteSeen(Player->GetActorLocation());
		}
	}

	OnAggroed.Broadcast(Reason);

	const bool bPassOn =
		(Reason == EAggroReason::Damage && bAlertOnDamage) ||
		(Reason == EAggroReason::Sight && bAlertOnSight) ||
		(Reason == EAggroReason::Forced) ||
		(Reason == EAggroReason::Alerted && bAlertsChain);
	if (bPassOn)
	{
		AlertNeighbours();
	}
}

void UEnemyAwarenessComponent::Calm(bool bGoHome)
{
	const bool bWasAggroed = bAggroed;
	bAggroed = false;
	bSeesPlayer = false;
	bSearchArrived = false;
	SetFlagOnOwner(false);
	ChaseState = EChaseState::Idle;

	if (const APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
		{
			AI->StopMovement();
			if (bGoHome && bDriveMovement && FVector::Dist2D(Pawn->GetActorLocation(), HomeLocation) > 150.f)
			{
				AI->MoveToLocation(HomeLocation, 50.f, true, true, true, true, nullptr, true);
				ChaseState = EChaseState::Return;
			}
		}
	}
	if (bWasAggroed)
	{
		OnGaveUp.Broadcast();
	}
}

void UEnemyAwarenessComponent::AlertNeighbours()
{
	if (AlertRadius <= 0.f) return;
	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World) return;
	const FVector Here = Owner->GetActorLocation();
	const float R2 = AlertRadius * AlertRadius;

	for (const TWeakObjectPtr<UEnemyAwarenessComponent>& Weak : AllAware)
	{
		UEnemyAwarenessComponent* Other = Weak.Get();
		if (!Other || Other == this || Other->GetWorld() != World) continue;
		if (Other->bAggroed || Other->bAlertPending || Other->bIgnoreAlerts || !Other->IsAlive()) continue;
		const AActor* OtherOwner = Other->GetOwner();
		if (!OtherOwner || FVector::DistSquared(Here, OtherOwner->GetActorLocation()) > R2) continue;
		if (bAlertNeedsSight && !TraceClear(EyeLocation(), Other->EyeLocation(), OtherOwner)) continue;

		Other->bAlertPending = true;
		const float Delay = FMath::FRandRange(Other->AlertDelayMin, FMath::Max(Other->AlertDelayMin, Other->AlertDelayMax));
		if (Delay <= KINDA_SMALL_NUMBER)
		{
			Other->ReceiveAlert();
		}
		else
		{
			Other->GetWorld()->GetTimerManager().SetTimer(Other->AlertTimer, Other, &UEnemyAwarenessComponent::ReceiveAlert, Delay, false);
		}
	}
}

void UEnemyAwarenessComponent::ReceiveAlert()
{
	bAlertPending = false;
	if (IsAlive())
	{
		Aggro(EAggroReason::Alerted);
	}
}

void UEnemyAwarenessComponent::HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                                  AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.f) return;
	if (bAggroed)
	{
		// Already awake: getting shot tells it where the player is now (cuts a search short, resets give-up).
		if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			NoteSeen(Player->GetActorLocation());
		}
		return;
	}
	// Runs even on the killing blow, so a one-shot kill still alerts the group (they heard it).
	Aggro(EAggroReason::Damage);
}

// ------------------------------------------------------------------ chase

void UEnemyAwarenessComponent::Chase()
{
	if (!bDriveMovement || !IsAlive()) return;
	APawn* Pawn = Cast<APawn>(GetOwner());
	AAIController* AI = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!AI || !Player) return;

	const FVector Here = Pawn->GetActorLocation();

	// Asleep: only thing to do is notice we got home.
	if (!bAggroed)
	{
		if (ChaseState == EChaseState::Return && FVector::Dist2D(Here, HomeLocation) < 150.f)
		{
			ChaseState = EChaseState::Idle;
		}
		return;
	}

	// The attack (started by the AI controller) owns the feet while it plays.
	if (OwnerIsAttacking()) return;

	const float SinceSeen = GetTimeSinceSeen();
	if (GiveUpTime > 0.f && SinceSeen > GiveUpTime)
	{
		Calm(bReturnHome);
		return;
	}

	const float Dist = FVector::Dist(Here, Player->GetActorLocation());
	const bool bKnowsWhere = bSeesPlayer || SinceSeen <= LoseSightGrace || (CloseSenseRange > 0.f && Dist <= CloseSenseRange);

	if (bKnowsWhere)
	{
		bSearchArrived = false;
		if (!bSeesPlayer)
		{
			LastSeenLocation = Player->GetActorLocation(); // rounding a corner: keep tracking for the grace time
		}
		if (Dist <= OwnerAttackRange())
		{
			ChaseState = EChaseState::Chase; // close enough: the controller's attack check takes over
			return;
		}

		// Player in the air (jumping) can't be put on the navmesh, so don't even ask for a path then.
		bool bPlayerAirborne = false;
		if (const ACharacter* PlayerChar = Cast<ACharacter>(Player))
		{
			bPlayerAirborne = PlayerChar->GetCharacterMovement() && PlayerChar->GetCharacterMovement()->IsFalling();
		}

		// Close in: walk to this enemy's own spot around the player (or circle on the waiting ring).
		if (bSpreadOut && !bPlayerAirborne && Dist <= EngageRadius)
		{
			RebuildSlots(GetWorld(), Player);
			if (bInSpread)
			{
				const float Range = OwnerAttackRange();
				float Angle = SlotAngle;
				float Radius = Range * 0.6f; // inside attack range, so the controller attacks on arrival
				if (!bHasToken)
				{
					Radius = FMath::Max(WaitRadius, Range + 100.f);
					Angle += CircleDir * CircleLead; // aim a bit ahead on the ring = keeps circling
				}
				const FVector Goal = Player->GetActorLocation() + FRotator(0.f, Angle, 0.f).Vector() * Radius;
				const EPathFollowingRequestResult::Type SpotResult =
					AI->MoveToLocation(Goal, 25.f, false, true, true, true, nullptr, true);
				if (SpotResult != EPathFollowingRequestResult::Failed)
				{
					ChaseState = bHasToken ? EChaseState::Flank : EChaseState::Wait;
					if (bDebugDraw)
					{
						DrawDebugSphere(GetWorld(), Goal, 25.f, 8, bHasToken ? FColor::Yellow : FColor::Cyan, false, ChaseInterval * 1.1f);
					}
					return;
				}
				// Spot not reachable (wall, ledge): fall through and chase the player directly.
			}
		}

		bool bNoGoodPath = bPlayerAirborne;
		if (!bPlayerAirborne)
		{
			const EPathFollowingRequestResult::Type Result =
				AI->MoveToActor(Player, ChaseAcceptance, true, true, true, nullptr, true);
			if (Result == EPathFollowingRequestResult::Failed)
			{
				bNoGoodPath = true;
			}
			else if (const UPathFollowingComponent* Follow = AI->GetPathFollowingComponent())
			{
				// A partial path that ends well short of the player (ledge, off the navmesh) counts as no path.
				const FNavPathSharedPtr Path = Follow->GetPath();
				if (Path.IsValid() && Path->IsPartial() &&
					FVector::Dist2D(Path->GetEndLocation(), Player->GetActorLocation()) > OwnerAttackRange())
				{
					bNoGoodPath = true;
				}
			}
		}

		if (bNoGoodPath && Dist <= DirectChaseRange && (bSeesPlayer || HasLineOfSightTo(Player)))
		{
			// Straight line, no navmesh: keeps the pressure on a jumping or ledge-standing player.
			AI->MoveToActor(Player, ChaseAcceptance, true, false, true, nullptr, false);
			ChaseState = EChaseState::Direct;
		}
		else
		{
			ChaseState = EChaseState::Chase;
		}
		return;
	}

	// Lost the player: go to where they were last seen, then wait there until the give-up time runs out.
	ChaseState = EChaseState::Search;
	if (bDebugDraw)
	{
		DrawDebugSphere(GetWorld(), LastSeenLocation, 40.f, 8, FColor::Orange, false, ChaseInterval * 1.1f);
	}
	if (bSearchArrived) return;

	if (FVector::Dist2D(Here, LastSeenLocation) < 120.f)
	{
		bSearchArrived = true;
		AI->StopMovement();
		return;
	}
	const EPathFollowingRequestResult::Type Result =
		AI->MoveToLocation(LastSeenLocation, 50.f, true, true, true, true, nullptr, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		bSearchArrived = true; // can't get there: wait where we are
	}
	else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		bSearchArrived = true;
	}
}

// ------------------------------------------------------------------ spread out

void UEnemyAwarenessComponent::RebuildSlots(UWorld* World, const APawn* Player)
{
	// Once per frame for everyone: the first enemy to ask does the work, the rest read their result.
	if (!World || !Player) return;
	if (SlotFrame == GFrameCounter && SlotWorld.Get() == World) return;
	SlotFrame = GFrameCounter;
	SlotWorld = World;

	struct FEntry
	{
		UEnemyAwarenessComponent* C;
		float Dist;
		float Bearing; // world yaw from the player to the enemy
	};

	const FVector P = Player->GetActorLocation();
	TArray<FEntry> In;
	int32 MaxTokens = 1;
	for (const TWeakObjectPtr<UEnemyAwarenessComponent>& Weak : AllAware)
	{
		UEnemyAwarenessComponent* C = Weak.Get();
		if (!C || C->GetWorld() != World) continue;
		C->bInSpread = false;
		const bool bTakesPart = C->bSpreadOut && C->bDriveMovement && C->bAggroed && C->IsAlive()
			&& C->GetTimeSinceSeen() <= C->LoseSightGrace;
		const AActor* Owner = C->GetOwner();
		if (!bTakesPart || !Owner)
		{
			C->bHasToken = false;
			continue;
		}
		const FVector D = Owner->GetActorLocation() - P;
		const float Dist = D.Size2D();
		if (Dist > C->EngageRadius)
		{
			C->bHasToken = false;
			continue;
		}
		In.Add({ C, Dist, (float)FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)) });
		MaxTokens = FMath::Max(MaxTokens, C->MaxAttackers);
	}

	// Closest enemies get the attack spots. Current holders get a 1.5 m head start so spots don't flicker back and forth.
	In.Sort([](const FEntry& A, const FEntry& B)
	{
		return (A.Dist - (A.C->bHasToken ? 150.f : 0.f)) < (B.Dist - (B.C->bHasToken ? 150.f : 0.f));
	});

	TArray<FEntry> Attackers, Waiters;
	for (int32 i = 0; i < In.Num(); ++i)
	{
		In[i].C->bInSpread = true;
		In[i].C->bHasToken = i < MaxTokens;
		(i < MaxTokens ? Attackers : Waiters).Add(In[i]);
	}

	// Spread a group evenly around where it already is (keeps each enemy on its own side, no crossing over).
	auto Assign = [](TArray<FEntry>& Group)
	{
		if (Group.Num() == 0) return;
		FVector2D Sum = FVector2D::ZeroVector;
		for (const FEntry& E : Group)
		{
			const float R = FMath::DegreesToRadians(E.Bearing);
			Sum += FVector2D(FMath::Cos(R), FMath::Sin(R));
		}
		const float Mean = Sum.IsNearlyZero() ? Group[0].Bearing : (float)FMath::RadiansToDegrees(FMath::Atan2(Sum.Y, Sum.X));
		Group.Sort([Mean](const FEntry& A, const FEntry& B)
		{
			return FMath::FindDeltaAngleDegrees(Mean, A.Bearing) < FMath::FindDeltaAngleDegrees(Mean, B.Bearing);
		});
		const int32 N = Group.Num();
		const float Step = N > 1 ? FMath::Min(Group[0].C->SlotSpacing, 360.f / N) : 0.f;
		for (int32 i = 0; i < N; ++i)
		{
			Group[i].C->SlotAngle = Mean + (i - (N - 1) * 0.5f) * Step;
		}
	};
	Assign(Attackers);
	Assign(Waiters);
}
