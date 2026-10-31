// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "ItemInstance.generated.h"

class UItemDefinition;
/**
 * 
 */
UCLASS(Blueprintable)
class DEMOBILLYHILL14_API UItemInstance : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UItemDefinition> ItemDefinition;
	
	UPROPERTY(BlueprintReadOnly)
	TMap<FGameplayTag, float> StatsMap;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Item Stats")
	float GetStatValue(FGameplayTag StatTag);
	
	UFUNCTION(BlueprintCallable, Category = "Item Stats")
	void SetStatValue(FGameplayTag StatTag, float StatValue);
	
	UFUNCTION(BlueprintCallable, Category = "Item Instances")
	void Initialize(TSubclassOf<UItemDefinition> ItemDef);
};
