// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "demoBillyHill14/Items/ItemDefinition.h"
#include "LootTable.generated.h"

/** One item that can drop. Its rarity comes from the item definition. */
USTRUCT(BlueprintType)
struct DEMOBILLYHILL14_API FLootEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TSubclassOf<UItemDefinition> Item;

	// How likely this item is compared with other items of the same rarity. 2 drops twice as often as 1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot", meta = (ClampMin = "0"))
	float Weight = 1.f;
};

/**
 * The items an enemy can drop. When a drop happens, a rarity is picked first using RarityWeights,
 * then an item of that rarity is picked using each entry's Weight.
 */
UCLASS(BlueprintType)
class DEMOBILLYHILL14_API ULootTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	ULootTable()
	{
		RarityWeights.Add(EItemRarity::Common, 60.f);
		RarityWeights.Add(EItemRarity::Uncommon, 28.f);
		RarityWeights.Add(EItemRarity::Rare, 10.f);
		RarityWeights.Add(EItemRarity::Exotic, 2.f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TArray<FLootEntry> Entries;

	// The chance of each rarity when a drop happens. Rarities with no entries are skipped, and the rest share their chance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	TMap<EItemRarity, float> RarityWeights;
};
