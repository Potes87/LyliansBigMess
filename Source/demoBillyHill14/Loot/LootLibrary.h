// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "CurrencyDropTable.h"
#include "LootLibrary.generated.h"

class ULootTable;
class UItemDefinition;

/**
 * Rolls enemy drops. It only picks which item drops; the Blueprint spawns the pickup,
 * and the item's stats roll as usual when it's picked up.
 * The player's rune tree bonuses apply automatically: Item Rarity raises the weight of every rarity above
 * Common, and Currency Find raises currency chances (both read from player 0's UPlayerStatsComponent).
 */
UCLASS()
class DEMOBILLYHILL14_API ULootLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Rolls DropChancePercent (0 to 100). If it hits, picks an item from Table and sets bDropped.
	 * Each call is a new roll, so call it once per death.
	 */
	UFUNCTION(BlueprintCallable, Category = "Loot", meta = (WorldContext = "WorldContextObject"))
	static TSubclassOf<UItemDefinition> RollLoot(const UObject* WorldContextObject, const ULootTable* Table, float DropChancePercent, bool& bDropped);

	// Picks an item from Table with no drop chance roll. Returns null if the table has nothing that can drop.
	UFUNCTION(BlueprintCallable, Category = "Loot", meta = (WorldContext = "WorldContextObject"))
	static TSubclassOf<UItemDefinition> PickLootItem(const UObject* WorldContextObject, const ULootTable* Table);

	// Rolls every entry of a currency table for one kill. Returns one result per pickup to spawn (often none).
	UFUNCTION(BlueprintCallable, Category = "Loot", meta = (WorldContext = "WorldContextObject"))
	static TArray<FCurrencyDropResult> RollCurrencyDrops(const UObject* WorldContextObject, const UCurrencyDropTable* Table, EEnemyTier Tier);

private:
	// Player 0's stats component, or null.
	static const class UPlayerStatsComponent* FindPlayerStats(const UObject* WorldContextObject);
};
