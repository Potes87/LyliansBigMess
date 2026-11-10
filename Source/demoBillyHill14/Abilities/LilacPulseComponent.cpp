// Fill out your copyright notice in the Description page of Project Settings.

#include "LilacPulseComponent.h"
#include "LilacPulseCameraShake.h"
#include "demoBillyHill14/Enemies/EnemyTypes.h"
#include "demoBillyHill14/Stats/DamageType_LilacPulse.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

ULilacPulseComponent::ULilacPulseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	PulseShake = ULilacPulseCameraShake::StaticClass();
}

bool ULilacPulseComponent::IsReady() const
{
	const UWorld* World = GetWorld();
	return World && !World->GetTimerManager().IsTimerActive(CooldownTimer);
}

float ULilacPulseComponent::GetCooldownProgress() const
{
	const UWorld* World = GetWorld();
	if (!World || IsReady()) return 1.f;

	const FTimerManager& TimerManager = World->GetTimerManager();
	const float Rate = TimerManager.GetTimerRate(CooldownTimer);
	return Rate > 0.f ? FMath::Clamp(TimerManager.GetTimerElapsed(CooldownTimer) / Rate, 0.f, 1.f) : 1.f;
}

bool ULilacPulseComponent::GetView(FVector& OutLocation, FRotator& OutRotation) const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller) return false;
	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

bool ULilacPulseComponent::TryPulse()
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	FVector ViewLocation;
	FRotator ViewRotation;
	if (!World || !Owner || !IsReady() || !GetView(ViewLocation, ViewRotation)) return false;

	World->GetTimerManager().SetTimer(CooldownTimer, Cooldown, false);
	const FVector Forward = ViewRotation.Vector();

	// Feel: effect, camera punch, then the Blueprint event for sound.
	if (PulseEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, PulseEffect, ViewLocation + Forward * EffectForwardOffset, ViewRotation);
	}
	const APawn* Pawn = Cast<APawn>(Owner);
	if (APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		if (PulseShake) PC->ClientStartCameraShake(PulseShake, ShakeScale);
	}
	OnPulseFired.Broadcast();

	// Find enemies: pawns in range, inside the cone, with no wall in between.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(LilacPulse), false, Owner);
	World->OverlapMultiByObjectType(Overlaps, ViewLocation, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Range), Query);

	const float MinDot = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngle));
	TSet<AActor*> Done;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Enemy = Overlap.GetActor();
		uint8 Tier = 0;
		if (!Enemy || Enemy == Owner || Done.Contains(Enemy) || !ReadTier(Enemy, Tier)) continue;
		Done.Add(Enemy);

		const FVector Target = Enemy->GetActorLocation();
		const FVector ToEnemy = Target - ViewLocation;
		// Enemies right on top of the player always count, whatever the angle.
		if (ToEnemy.Size() > 80.f && FVector::DotProduct(ToEnemy.GetSafeNormal(), Forward) < MinDot) continue;

		FHitResult Wall;
		FCollisionQueryParams LineQuery(SCENE_QUERY_STAT(LilacPulseSight), false, Owner);
		LineQuery.AddIgnoredActor(Enemy);
		if (World->LineTraceSingleByObjectType(Wall, ViewLocation, Target, FCollisionObjectQueryParams(ECC_WorldStatic), LineQuery)) continue;

		HitEnemy(Enemy, Tier, ViewLocation);
	}

	if (bDrawDebug)
	{
		DrawDebugCone(World, ViewLocation, Forward, Range, FMath::DegreesToRadians(ConeHalfAngle), FMath::DegreesToRadians(ConeHalfAngle), 16, FColor(185, 165, 224), false, 2.f);
	}
	return true;
}

void ULilacPulseComponent::HitEnemy(AActor* Enemy, uint8 Tier, const FVector& From)
{
	const bool bBasic = UEnemyTierLibrary::IsBasicTier(static_cast<EEnemyTier>(Tier));
	float Health = 0.f;
	const bool bKnowsHealth = ReadCurrentHealth(Enemy, Health);
	if (bKnowsHealth && Health <= 0.f) return;   // already dead

	const float Damage = bBasic ? (bKnowsHealth ? FMath::CeilToFloat(Health) : KillDamage) : FixedDamage;

	const APawn* Pawn = Cast<APawn>(GetOwner());
	UGameplayStatics::ApplyDamage(Enemy, Damage, Pawn ? Pawn->GetController() : nullptr, GetOwner(), UDamageType_LilacPulse::StaticClass());

	bool bKilled = false;
	if (bBasic)
	{
		float HealthAfter = 0.f;
		bKilled = !ReadCurrentHealth(Enemy, HealthAfter) || HealthAfter <= 0.f;

		FVector Away = (Enemy->GetActorLocation() - From).GetSafeNormal2D();
		if (Away.IsNearlyZero()) Away = GetOwner()->GetActorForwardVector();
		const FVector Velocity = (Away * (1.f - KnockbackLift) + FVector::UpVector * KnockbackLift).GetSafeNormal() * KnockbackSpeed;
		FlingRagdoll(Enemy, Velocity, true);
	}

	if (bDrawDebug)
	{
		DrawDebugSphere(GetWorld(), Enemy->GetActorLocation(), 40.f, 8, bBasic ? FColor::White : FColor(235, 213, 146), false, 2.f);
	}
	OnPulseHit.Broadcast(Enemy, Damage, bKilled);
}

void ULilacPulseComponent::FlingRagdoll(TWeakObjectPtr<AActor> Enemy, FVector Velocity, bool bRetry)
{
	ACharacter* Character = Cast<ACharacter>(Enemy.Get());
	if (!Character) return;

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	if (Mesh && Mesh->IsSimulatingPhysics())
	{
		Mesh->SetAllPhysicsLinearVelocity(Velocity, true);
		return;
	}
	// The ragdoll may switch on a moment after death, so try once more next frame before falling back.
	if (bRetry)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Enemy, Velocity]()
		{
			FlingRagdoll(Enemy, Velocity, false);
		}));
		return;
	}
	Character->LaunchCharacter(Velocity, true, true);
}

bool ULilacPulseComponent::ReadTier(const AActor* Actor, uint8& OutTier)
{
	const FEnumProperty* Property = FindFProperty<FEnumProperty>(Actor->GetClass(), TEXT("Tier"));
	if (!Property || Property->GetEnum() != StaticEnum<EEnemyTier>()) return false;
	OutTier = static_cast<uint8>(Property->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Actor)));
	return true;
}

bool ULilacPulseComponent::ReadCurrentHealth(const AActor* Actor, float& OutHealth)
{
	// Enemy health lives in a Blueprint component (BPC_Health) with a GetCurrentHealth function.
	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);
	for (UActorComponent* Component : Components)
	{
		UFunction* Function = Component ? Component->FindFunction(TEXT("GetCurrentHealth")) : nullptr;
		if (!Function || Function->NumParms == 0) continue;

		uint8* Params = static_cast<uint8*>(FMemory_Alloca(Function->ParmsSize));
		FMemory::Memzero(Params, Function->ParmsSize);
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It) It->InitializeValue_InContainer(Params);
		Component->ProcessEvent(Function, Params);

		bool bFound = false;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			if (!It->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm) || bFound) continue;
			if (const FDoubleProperty* D = CastField<FDoubleProperty>(*It)) { OutHealth = static_cast<float>(D->GetPropertyValue_InContainer(Params)); bFound = true; }
			else if (const FFloatProperty* F = CastField<FFloatProperty>(*It)) { OutHealth = F->GetPropertyValue_InContainer(Params); bFound = true; }
		}
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It) It->DestroyValue_InContainer(Params);
		if (bFound) return true;
	}
	return false;
}
