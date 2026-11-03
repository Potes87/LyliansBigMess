// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "StatTypes.h"
#include "PlayerStatsComponent.generated.h"

class UItemInstance;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStatsChanged);

/**
 * Holds every stat modifier affecting the player, grouped by where it came from, and does the maths.
 * It never changes an item's StatsMap: that stays the item's base stats, and modified values are worked out on request.
 */
UCLASS(ClassGroup = (Stats), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UPlayerStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerStatsComponent();

	// Fires whenever modifiers change. Anything caching a modified value should re-read it.
	UPROPERTY(BlueprintAssignable, Category = "Stats")
	FOnStatsChanged OnStatsChanged;

	// Adds a group of modifiers under a unique ID, replacing any group already using that ID.
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void SetModifierSource(FName SourceId, const TArray<FStatModifier>& Modifiers);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void RemoveModifierSource(FName SourceId);

	// The item's base value for this stat with all matching modifiers applied.
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetModifiedStat(UItemInstance* Item, FGameplayTag Stat) const;

	// Applies all matching modifiers to any base value.
	UFUNCTION(BlueprintPure, Category = "Stats")
	float GetModifiedValue(float BaseValue, FGameplayTag Stat, const FGameplayTagContainer& ItemTags) const;

private:
	TMap<FName, TArray<FStatModifier>> ModifierSources;
};
