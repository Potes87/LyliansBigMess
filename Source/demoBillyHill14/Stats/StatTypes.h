// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StatTypes.generated.h"

/**
 * How a modifier combines with a stat:
 * Final = (Base + all Flat) * (1 + sum of Increased / 100) * (each (1 + More / 100) multiplied together)
 */
UENUM(BlueprintType)
enum class EModifierOp : uint8
{
	Flat,
	Increased,
	More
};

USTRUCT(BlueprintType)
struct DEMOBILLYHILL14_API FStatModifier
{
	GENERATED_BODY()

	// The stat this changes, e.g. Item.Stat.Damage. For Item.Stat.FireRate and Item.Stat.ReloadSpeed, use Increased or More.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	FGameplayTag Stat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	EModifierOp Op = EModifierOp::Flat;

	// Flat: added directly. Increased and More: a percentage, so 30 means 30%. Negative values reduce the stat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	float Value = 0.f;

	// Only applies to items whose ItemType has all of these tags. Leave empty to apply to every item.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
	FGameplayTagContainer RequiredWeaponTags;
};
