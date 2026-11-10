// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "demoBillyHill14/Enemies/EnemyTypes.h"
#include "demoBillyHill14/Items/ItemDefinition.h"
#include "CurrencyDropTable.generated.h"

/** One thing that can drop from the currency table: currency, keys or other quest items. */
USTRUCT(BlueprintType)
struct DEMOBILLYHILL14_API FCurrencyDropEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency Drop")
	TSubclassOf<UItemDefinition> Item;

	// Chance per kill of a Normal enemy, 0 to 100. Every entry rolls on its own, so one kill can drop several.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency Drop", meta = (ClampMin = "0", ClampMax = "100"))
	float ChancePercent = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency Drop", meta = (ClampMin = "1"))
	int32 MinCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency Drop", meta = (ClampMin = "1"))
	int32 MaxCount = 1;
};

/** A result of RollCurrencyDrops: spawn one pickup of Item with this Count. */
USTRUCT(BlueprintType)
struct DEMOBILLYHILL14_API FCurrencyDropResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Currency Drop")
	TSubclassOf<UItemDefinition> Item;

	UPROPERTY(BlueprintReadOnly, Category = "Currency Drop")
	int32 Count = 1;
};

/**
 * Drops that roll separately from guns, so currency never takes a gun's place.
 * Each entry rolls ChancePercent times the enemy tier's multiplier.
 */
UCLASS(BlueprintType)
class DEMOBILLYHILL14_API UCurrencyDropTable : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UCurrencyDropTable()
	{
		TierChanceMultiplier.Add(EEnemyTier::Normal, 1.f);
		TierChanceMultiplier.Add(EEnemyTier::Rare, 3.f);
		TierChanceMultiplier.Add(EEnemyTier::Elite, 6.f);
		TierChanceMultiplier.Add(EEnemyTier::Unique, 10.f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency Drop")
	TArray<FCurrencyDropEntry> Entries;

	// Multiplies every entry's chance for enemies of this tier. Missing tiers count as 1.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Currency Drop")
	TMap<EEnemyTier, float> TierChanceMultiplier;
};
