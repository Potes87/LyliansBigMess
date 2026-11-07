// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "CombatHelpers.generated.h"

class UDamageType;
class USkeletalMeshComponent;
struct FHitResult;

/**
 * Hit helpers for bullets and enemies: headshot detection, and packing the crit and headshot flags into one damage type.
 */
UCLASS()
class DEMOBILLYHILL14_API UCombatHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * True when the hit counts as a headshot. If the hit landed on Mesh with a bone, it must be HeadBone or one of its children.
	 * Otherwise, for example when the capsule blocked the bullet first, it's a headshot when the impact point is at least
	 * as high as HeadBone plus HeadZOffset. Lower HeadZOffset to make headshots easier.
	 */
	UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "Is Headshot"))
	static bool IsHeadshot(const FHitResult& Hit, const USkeletalMeshComponent* Mesh, FName HeadBone = TEXT("head"), float HeadZOffset = -6.f);

	/** Returns the damage type to pass to Apply Damage for this combination of flags. */
	UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "Make Hit Damage Type"))
	static TSubclassOf<UDamageType> MakeHitDamageType(bool bIsCrit, bool bIsHeadshot);

	/** Reads the crit and headshot flags back out of a damage type, for example in Event Any Damage. */
	UFUNCTION(BlueprintPure, Category = "Combat", meta = (DisplayName = "Break Hit Damage Type"))
	static void BreakHitDamageType(const UDamageType* DamageType, bool& bIsCrit, bool& bIsHeadshot);
};
