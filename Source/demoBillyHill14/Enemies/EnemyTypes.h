// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EnemyTypes.generated.h"

/**
 * How dangerous an enemy is. Shown as the colour of its health bar frame.
 * Normal = white, Rare = gold, Elite and Unique = brown.
 * Lilac Pulse kills Normal enemies outright and only knocks back Normal enemies.
 */
UENUM(BlueprintType)
enum class EEnemyTier : uint8
{
	Normal,
	Rare,
	Elite,
	Unique
};

UCLASS()
class DEMOBILLYHILL14_API UEnemyTierLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// The tier's colour from the Dusk Hollow palette: Pale, Gold, or a Peach-into-Shadow brown.
	UFUNCTION(BlueprintPure, Category = "Enemy Tier")
	static FLinearColor GetTierColor(EEnemyTier Tier);

	// True for Normal (white) enemies only. These can be one-shot and knocked back by Lilac Pulse.
	UFUNCTION(BlueprintPure, Category = "Enemy Tier")
	static bool IsBasicTier(EEnemyTier Tier) { return Tier == EEnemyTier::Normal; }
};
