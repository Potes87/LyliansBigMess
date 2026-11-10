// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GunRerollLibrary.generated.h"

class UItemInstance;
class UInventoryItemFragment;

/** One random stat on a gun: its range and where the current value sits in it. */
USTRUCT(BlueprintType)
struct DEMOBILLYHILL14_API FStatRollInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	FGameplayTag Stat;

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	float Min = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	float Max = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	float Value = 0.f;

	// 0 = worst possible roll, 1 = best. Stats where lower is better (fire delay, reload time) are already flipped.
	UPROPERTY(BlueprintReadOnly, Category = "Reroll")
	float Quality = 0.f;
};

/**
 * Divine orb rerolls. A gun's random stats come from its BP_ItemFragment_RandomSetStats fragment
 * (the RandomStatValues array); a reroll runs that fragment's roll again on the existing gun.
 * The equipped check lives in BPC_Inventory, so do it in Blueprint before calling TryReroll.
 */
UCLASS()
class DEMOBILLYHILL14_API UGunRerollLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// True if the item has random stats that can be rerolled.
	UFUNCTION(BlueprintPure, Category = "Reroll")
	static bool HasRandomStats(UItemInstance* Item);

	// Every random stat with its range and roll quality. Empty for items without random stats.
	UFUNCTION(BlueprintPure, Category = "Reroll")
	static TArray<FStatRollInfo> GetStatRolls(UItemInstance* Item);

	// Average quality of all random stats, 0 to 1. Returns 0 for items without random stats.
	UFUNCTION(BlueprintPure, Category = "Reroll")
	static float GetRollQuality(UItemInstance* Item);

	// True if the item has random stats and the player has a divine orb. Doesn't check equipped.
	UFUNCTION(BlueprintPure, Category = "Reroll", meta = (WorldContext = "WorldContextObject"))
	static bool CanReroll(const UObject* WorldContextObject, UItemInstance* Item);

	// Spends one divine orb and rerolls every random stat. Returns false (and spends nothing) if it can't.
	// Save the inventory afterwards.
	UFUNCTION(BlueprintCallable, Category = "Reroll", meta = (WorldContext = "WorldContextObject"))
	static bool TryReroll(const UObject* WorldContextObject, UItemInstance* Item);

private:
	static const UInventoryItemFragment* FindRandomStatsFragment(UItemInstance* Item);
	static bool IsLowerBetter(const FGameplayTag& Stat);
};
