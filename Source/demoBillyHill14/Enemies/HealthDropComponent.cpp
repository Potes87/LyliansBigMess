// Fill out your copyright notice in the Description page of Project Settings.

#include "HealthDropComponent.h"

#include "HealthRemnant.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

UHealthDropComponent::UHealthDropComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UHealthDropComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UHealthDropComponent::HandleOwnerDamaged);   // unique: BeginPlay can run twice when a cell streams back in
	}
}

bool UHealthDropComponent::IsOwnerDead() const
{
	const AActor* Owner = GetOwner();
	if (!Owner) return false;
	// The Blueprint health component (BPC_Health) is found by its CurrentHealth variable.
	for (const UActorComponent* Comp : Owner->GetComponents())
	{
		if (!Comp || Comp == this) continue;
		if (const FNumericProperty* Prop = FindFProperty<FNumericProperty>(Comp->GetClass(), TEXT("CurrentHealth")))
		{
			const void* Value = Prop->ContainerPtrToValuePtr<void>(Comp);
			const double Current = Prop->IsFloatingPoint() ? Prop->GetFloatingPointPropertyValue(Value) : (double)Prop->GetSignedIntPropertyValue(Value);
			return Current <= 0.0;
		}
	}
	return false;
}

void UHealthDropComponent::HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                              AController* InstigatedBy, AActor* DamageCauser)
{
	// The enemy Blueprint has already taken the health off by the time this runs.
	if (bDone || Damage <= 0.f || !IsOwnerDead()) return;
	bDone = true;
	if (DropChance >= 1.f || FMath::FRand() <= DropChance)
	{
		DropNow();
	}
}

void UHealthDropComponent::DropNow()
{
	const AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) return;

	UClass* Class = RemnantClass ? RemnantClass.Get() : AHealthRemnant::StaticClass();
	const int32 Count = FMath::RandRange(FMath::Min(CountMin, CountMax), FMath::Max(CountMin, CountMax));
	const FVector From = Owner->GetActorLocation() + FVector(0.f, 0.f, 30.f);

	for (int32 i = 0; i < Count; ++i)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AHealthRemnant* Remnant = World->SpawnActor<AHealthRemnant>(Class, From, FRotator::ZeroRotator, Params))
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			const float Side = ThrowSide * FMath::FRandRange(0.5f, 1.f);
			Remnant->Launch(FVector(FMath::Cos(Angle) * Side, FMath::Sin(Angle) * Side, ThrowUp * FMath::FRandRange(0.8f, 1.1f)));
		}
	}
}
