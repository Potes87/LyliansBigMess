// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Templates/SubclassOf.h"
#include "ItemSaveLibrary.generated.h"

class UItemDefinition;
class UItemInstance;

/**
 * One inventory stack as it's written to disk. StatsMap keeps the gun's rolled stats, so loading never rerolls them.
 */
USTRUCT(BlueprintType)
struct DEMOBILLYHILL14_API FSavedItem
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Item Saving")
	TSubclassOf<UItemDefinition> ItemDefinition;

	UPROPERTY(BlueprintReadWrite, Category = "Item Saving")
	int32 Count = 1;

	UPROPERTY(BlueprintReadWrite, Category = "Item Saving")
	TMap<FGameplayTag, float> StatsMap;

	UPROPERTY(BlueprintReadWrite, Category = "Item Saving")
	bool bEquipped = false;
};

/**
 * The player's items and the chest's items. They share one file so an item moved between them is never lost or doubled.
 */
UCLASS()
class DEMOBILLYHILL14_API UPlayerItemsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FSavedItem> PlayerItems;

	UPROPERTY()
	TArray<FSavedItem> ChestItems;

	// These tell an empty list that was saved apart from a list that has never been saved.
	UPROPERTY()
	bool bHasPlayerItems = false;

	UPROPERTY()
	bool bHasChestItems = false;
};

/**
 * Saves and restores inventories. BPC_Inventory is a Blueprint, so it builds and reads the lists, and this writes them.
 */
UCLASS()
class DEMOBILLYHILL14_API UItemSaveLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Item Saving")
	static FSavedItem MakeSavedItem(UItemInstance* Item, int32 Count, bool bEquipped);

	// Rebuilds an item with its saved stats. Returns null if that item no longer exists in the game.
	UFUNCTION(BlueprintCallable, Category = "Item Saving", meta = (DefaultToSelf = "Outer"))
	static UItemInstance* CreateItemFromSave(UObject* Outer, const FSavedItem& SavedItem);

	UFUNCTION(BlueprintCallable, Category = "Item Saving")
	static bool SavePlayerItems(const TArray<FSavedItem>& Items);

	// Returns false if the player's items have never been saved, leaving Items empty.
	UFUNCTION(BlueprintCallable, Category = "Item Saving")
	static bool LoadPlayerItems(TArray<FSavedItem>& Items);

	UFUNCTION(BlueprintCallable, Category = "Item Saving")
	static bool SaveChestItems(const TArray<FSavedItem>& Items);

	// Returns false if the chest's items have never been saved, leaving Items empty.
	UFUNCTION(BlueprintCallable, Category = "Item Saving")
	static bool LoadChestItems(TArray<FSavedItem>& Items);

	// Testing helper: deletes the saved items so the next Play starts with the default ones.
	UFUNCTION(BlueprintCallable, Category = "Item Saving|Debug")
	static void DebugDeleteSavedItems();
};
