// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ItemDefinition.generated.h"

class UInventoryItemFragment; 
/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, Abstract, Const)
class DEMOBILLYHILL14_API UItemDefinition : public UObject
{
	GENERATED_BODY()
public:
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText ItemName; 
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	FText ItemDescription;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Display")
	TObjectPtr<UTexture2D> ItemIcon; 
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Instanced, Category = "FragmentsArray")
	TArray<TObjectPtr<UInventoryItemFragment>> Fragments;
	
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (DeterminesOutputType = "FragmentClass"))
	static const UInventoryItemFragment* FindFragmentByClass(const TSubclassOf<UItemDefinition> ItemDefinition, const TSubclassOf<UInventoryItemFragment> FragmentClass);
};
