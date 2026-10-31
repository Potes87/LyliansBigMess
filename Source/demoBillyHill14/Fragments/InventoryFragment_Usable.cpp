// Fill out your copyright notice in the Description page of Project Settings.


#include "InventoryFragment_Usable.h"

#include "demoBillyHill14/ItemActions/ItemAction.h"

bool UInventoryFragment_Usable::Use(AActor* ItemOwner)
{
	bool bAnySucceeded = false; 
	
	for (const TObjectPtr<UItemAction>& Action : OnUseActions)
	{
		if (Action && Action->Execute(ItemOwner))
		{
			bAnySucceeded = true;
		}
	}
	return bAnySucceeded;	
}
