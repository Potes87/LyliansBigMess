// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InventoryItemFragment.h"
#include "InventoryFragment_Usable.generated.h"

class UItemAction;
/**
 * 
 */
UCLASS()
class DEMOBILLYHILL14_API UInventoryFragment_Usable : public UInventoryItemFragment
{
	GENERATED_BODY()
public:
	
	UFUNCTION(BlueprintCallable)
	bool Use(AActor* ItemOwner);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Options")
	bool bConsumeOnUse = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category = "Actions")
	TArray<TObjectPtr<UItemAction>> OnUseActions;
};
