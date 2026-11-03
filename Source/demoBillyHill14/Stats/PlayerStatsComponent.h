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

	// The gun's magazine with all modifiers applied, rounded to a whole number. Always at least 1.
	UFUNCTION(BlueprintPure, Category = "Stats|Weapon")
	int32 GetMagazineSize(UItemInstance* Item) const;

	// Seconds between shots: the gun's FireDelay divided by its Item.Stat.FireRate bonuses, down to MinFireDelay.
	UFUNCTION(BlueprintPure, Category = "Stats|Weapon")
	float GetFireDelay(UItemInstance* Item) const;

	// Seconds per reload: the gun's rolled ReloadSpeed divided by its ReloadSpeed bonuses, down to MinReloadTime.
	UFUNCTION(BlueprintPure, Category = "Stats|Weapon")
	float GetReloadTime(UItemInstance* Item) const;

	// The play rate that makes the reload animations fit GetReloadTime.
	UFUNCTION(BlueprintPure, Category = "Stats|Weapon")
	float GetReloadPlayRate(UItemInstance* Item) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Weapon", meta = (ClampMin = "0.01"))
	float MinFireDelay = 0.03f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Weapon", meta = (ClampMin = "0.01"))
	float MinReloadTime = 0.3f;

	// How long a reload looks right at play rate 1. Also used if a gun has no ReloadSpeed stat.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stats|Weapon", meta = (ClampMin = "0.01"))
	float BaseReloadTime = 2.1f;

private:
	static FGameplayTagContainer GetItemTags(const UItemInstance* Item);

	// A base time divided by the speed bonuses on SpeedStat, which start from 1 instead of an item value.
	float ApplySpeedBonus(float BaseTime, FGameplayTag SpeedStat, const UItemInstance* Item, float MinTime) const;

	TMap<FName, TArray<FStatModifier>> ModifierSources;
};
