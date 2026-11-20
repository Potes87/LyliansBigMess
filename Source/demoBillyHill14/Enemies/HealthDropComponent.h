// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthDropComponent.generated.h"

class AHealthRemnant;
class UDamageType;
class AController;

/**
 * Makes an enemy leave health remnants behind when it dies. Add it to an enemy Blueprint (or the enemy parent).
 * It notices the death by itself: there is nothing to wire.
 *
 * A child Blueprint can change the numbers on its own copy: an elite can drop more.
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UHealthDropComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthDropComponent();

	// Chance that this enemy drops anything when it dies. 1 = always.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Drop", meta = (ClampMin = "0", ClampMax = "1"))
	float DropChance = 0.5f;

	// How many remnants it drops when it does (random between the two).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Drop", meta = (ClampMin = "1", ClampMax = "20"))
	int32 CountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Drop", meta = (ClampMin = "1", ClampMax = "20"))
	int32 CountMax = 1;

	// What it drops. Empty = the plain red remnant.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Drop")
	TSubclassOf<AHealthRemnant> RemnantClass;

	// How hard they are thrown out: sideways and upwards (units per second).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Drop", meta = (ClampMin = "0"))
	float ThrowSide = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health Drop", meta = (ClampMin = "0"))
	float ThrowUp = 380.f;

	// Drops now, ignoring the chance. For special cases in Blueprint.
	UFUNCTION(BlueprintCallable, Category = "Health Drop")
	void DropNow();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	bool IsOwnerDead() const;

	bool bDone = false;
};
