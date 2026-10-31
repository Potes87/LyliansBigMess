// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemFragment.h"
#include "InventoryFragment_Equippable.generated.h"

class UEquipmentDefinition;
/**
 * 
 */
UCLASS()
class DEMOBILLYHILL14_API UInventoryFragment_Equippable : public UInventoryItemFragment
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<UEquipmentDefinition> EquipmentDefinition;
};
