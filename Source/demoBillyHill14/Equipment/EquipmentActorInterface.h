// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "EquipmentActorInterface.generated.h"

class UItemInstance;
class ACharacter;

UINTERFACE(BlueprintType, Blueprintable)
class UEquipmentActorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Implement on any actor used as an EquipmentDefinition's EquipmentActorClass
 * to receive the item it was spawned from.
 */
class DEMOBILLYHILL14_API IEquipmentActorInterface
{
	GENERATED_BODY()

public:
	// Called by UEquipmentInstance right after the equipment actor is spawned and attached.
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Equipment")
	void OnEquipped(UItemInstance* ItemInstance, ACharacter* Character);
};
