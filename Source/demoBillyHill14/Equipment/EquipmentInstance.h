// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EquipmentInstance.generated.h"

class UItemInstance;
class UEquipmentDefinition;
/**
 * 
 */
UCLASS(BlueprintType)
class DEMOBILLYHILL14_API UEquipmentInstance : public UObject
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UEquipmentDefinition> EquipmentDefinition;
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UItemInstance> SourceItemInstance;
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<AActor> SpawnedEquipmentActor;
	
	UFUNCTION(BlueprintCallable)
	void Initialize(UItemInstance* ItemInstance, ACharacter* Character);
	
	UFUNCTION(BlueprintCallable)
	void HandleEquipItem(ACharacter* Character);
	
	UFUNCTION(BlueprintCallable)
	void HandleUnequipItem(ACharacter* Character);
};
