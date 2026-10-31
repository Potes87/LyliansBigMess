// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemInstance.h"
#include "demoBillyHill14/Items/ItemDefinition.h"
#include "demoBillyHill14/Fragments/InventoryItemFragment.h"

float UItemInstance::GetStatValue(FGameplayTag StatTag)
{
	return StatsMap.FindRef(StatTag); 
}

void UItemInstance::SetStatValue(FGameplayTag StatTag, float StatValue)
{
	StatsMap.Add(StatTag, StatValue);
}

void UItemInstance::Initialize(TSubclassOf<UItemDefinition> ItemDef, const TMap<FGameplayTag, float>& InitialStats)
{
	if (!ItemDef) return;
	
	ItemDefinition = ItemDef;
	
	UItemDefinition* ItemCDO = ItemDefinition.GetDefaultObject();
	
	for (const TObjectPtr<UInventoryItemFragment>& Fragment : ItemCDO->Fragments)
	{
		Fragment->OnInstanceCreated(this);
	}
	
	if (!InitialStats.IsEmpty())
	{
		StatsMap = InitialStats;
	}
	
}
