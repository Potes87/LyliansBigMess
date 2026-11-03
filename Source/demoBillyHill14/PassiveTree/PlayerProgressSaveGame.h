// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "PlayerProgressSaveGame.generated.h"

/**
 * The player's saved progress. Available points aren't stored, because they come from the level and allocated nodes.
 */
UCLASS()
class DEMOBILLYHILL14_API UPlayerProgressSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 PlayerLevel = 1;

	UPROPERTY()
	float CurrentXP = 0.f;

	// Row names from the passive tree table.
	UPROPERTY()
	TArray<FName> AllocatedNodes;
};
