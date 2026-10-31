// Fill out your copyright notice in the Description page of Project Settings.


#include "demoBillyHill14/Items/ItemDefinition.h"
#include "demoBillyHill14/Fragments/InventoryItemFragment.h"

const UInventoryItemFragment* UItemDefinition::FindFragmentByClass(const TSubclassOf<UItemDefinition> ItemDefinition, const TSubclassOf<UInventoryItemFragment> FragmentClass)
{
	if (ItemDefinition && FragmentClass)
	{
		UItemDefinition* ItemCDO = ItemDefinition.GetDefaultObject();
		for (const TObjectPtr<UInventoryItemFragment>& Fragment : ItemCDO->Fragments)
		{
			if (Fragment->IsA(FragmentClass))
			{
				return Fragment;
			}
		}
	}
	return nullptr; 	
}
