// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyLevelComponent.h"

#include "EnemyLevelZone.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "../Respawn/RespawnSubsystem.h"
#include "UObject/UnrealType.h"

namespace
{
	// Multiply a number variable (float, double or int) on any object, found by name. Does nothing if it isn't there.
	void ScaleNumber(UObject* Obj, const TCHAR* Name, double Mult)
	{
		if (!Obj) return;
		FNumericProperty* Prop = FindFProperty<FNumericProperty>(Obj->GetClass(), Name);
		if (!Prop) return;
		void* Value = Prop->ContainerPtrToValuePtr<void>(Obj);
		if (Prop->IsFloatingPoint())
		{
			Prop->SetFloatingPointPropertyValue(Value, Prop->GetFloatingPointPropertyValue(Value) * Mult);
		}
		else if (Prop->IsInteger())
		{
			Prop->SetIntPropertyValue(Value, (int64)FMath::RoundToDouble((double)Prop->GetSignedIntPropertyValue(Value) * Mult));
		}
	}
}

UEnemyLevelComponent::UEnemyLevelComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UEnemyLevelComponent::GetDamageMultiplierFor(const AActor* Enemy)
{
	const UEnemyLevelComponent* C = Enemy ? Enemy->FindComponentByClass<UEnemyLevelComponent>() : nullptr;
	return C ? C->DamageMult : 1.f;
}

int32 UEnemyLevelComponent::GetLevelFor(const AActor* Enemy)
{
	const UEnemyLevelComponent* C = Enemy ? Enemy->FindComponentByClass<UEnemyLevelComponent>() : nullptr;
	return C ? C->Level : 1;
}

int32 UEnemyLevelComponent::DecideLevel() const
{
	if (LevelOverride > 0) return LevelOverride;

	const AActor* Owner = GetOwner();
	const UWorld* World = GetWorld();
	if (!Owner || !World) return DefaultLevel;

	const FVector Here = Owner->GetActorLocation();
	const AEnemyLevelZone* Best = nullptr;
	for (TActorIterator<AEnemyLevelZone> It(World); It; ++It)
	{
		const AEnemyLevelZone* Zone = *It;
		if (!Zone || !Zone->ContainsPoint(Here)) continue;
		if (!Best || Zone->Priority > Best->Priority || (Zone->Priority == Best->Priority && Zone->ZoneLevel > Best->ZoneLevel))
		{
			Best = Zone;
		}
	}
	if (!Best) return DefaultLevel;
	return Best->ZoneLevel + (Best->LevelVariance > 0 ? FMath::RandRange(0, Best->LevelVariance) : 0);
}

void UEnemyLevelComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bApplied) return;
	bApplied = true;

	if (URespawnSubsystem* Respawn = URespawnSubsystem::Get(this))
	{
		Respawn->EnemyStarted(GetOwner(), LevelOverride);
	}

	Level = FMath::Clamp(DecideLevel(), 1, FMath::Max(1, MaxLevel));
	const int32 Above = Level - 1;
	HealthMult = (float)FMath::Pow((double)HealthGrowth, (double)Above);
	DamageMult = 1.f + DamagePerLevel * Above;
	XPMult = 1.f + XPPerLevel * Above;

	if (Above > 0)
	{
		AActor* Owner = GetOwner();
		ScaleNumber(Owner, TEXT("AttackDamage"), DamageMult);
		ScaleNumber(Owner, TEXT("XPReward"), XPMult);
		ApplyHealth();
	}
	OnLevelApplied.Broadcast(Level);
}

void UEnemyLevelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URespawnSubsystem* Respawn = URespawnSubsystem::Get(this))
	{
		Respawn->EnemyEnded(GetOwner(), EndPlayReason);
	}
	Super::EndPlay(EndPlayReason);
}

void UEnemyLevelComponent::ApplyHealth()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Find the Blueprint health component (BPC_Health) by its variables, so C++ doesn't need to know the Blueprint class.
	for (UActorComponent* Comp : Owner->GetComponents())
	{
		if (!Comp || !FindFProperty<FNumericProperty>(Comp->GetClass(), TEXT("BaseMaxHealth"))) continue;

		ScaleNumber(Comp, TEXT("BaseMaxHealth"), HealthMult);

		// BPC_Health's own SetLevel(0) recalculates MaxHealth from BaseMaxHealth, tops CurrentHealth up by the
		// difference and fires OnHealthChanged so the health bar shows the new numbers. Works whether or not the
		// health component has already run its BeginPlay (if not, its BeginPlay just fills health to the new max).
		if (UFunction* SetLevel = Comp->FindFunction(TEXT("SetLevel")))
		{
			uint8* Params = (uint8*)FMemory_Alloca(FMath::Max<int32>(SetLevel->ParmsSize, 1));
			FMemory::Memzero(Params, FMath::Max<int32>(SetLevel->ParmsSize, 1));
			for (TFieldIterator<FProperty> It(SetLevel); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->InitializeValue_InContainer(Params); // NewLevel stays 0, which means "level 1" inside BPC_Health
			}
			Comp->ProcessEvent(SetLevel, Params);
			for (TFieldIterator<FProperty> It(SetLevel); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->DestroyValue_InContainer(Params);
			}
		}
		break;
	}
}
