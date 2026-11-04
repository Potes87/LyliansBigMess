// Fill out your copyright notice in the Description page of Project Settings.

#include "ItemSaveLibrary.h"
#include "ItemDefinition.h"
#include "ItemInstance.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* ItemSaveSlot = TEXT("PlayerItems");

	UPlayerItemsSaveGame* LoadSave()
	{
		return Cast<UPlayerItemsSaveGame>(UGameplayStatics::LoadGameFromSlot(ItemSaveSlot, 0));
	}

	UPlayerItemsSaveGame* LoadOrCreateSave()
	{
		if (UPlayerItemsSaveGame* Save = LoadSave())
		{
			return Save;
		}
		return Cast<UPlayerItemsSaveGame>(UGameplayStatics::CreateSaveGameObject(UPlayerItemsSaveGame::StaticClass()));
	}
}

FSavedItem UItemSaveLibrary::MakeSavedItem(UItemInstance* Item, int32 Count, bool bEquipped)
{
	FSavedItem SavedItem;
	if (Item)
	{
		SavedItem.ItemDefinition = Item->ItemDefinition;
		SavedItem.Count = Count;
		SavedItem.StatsMap = Item->StatsMap;
		SavedItem.bEquipped = bEquipped;
	}
	return SavedItem;
}

UItemInstance* UItemSaveLibrary::CreateItemFromSave(UObject* Outer, const FSavedItem& SavedItem)
{
	if (!Outer || !SavedItem.ItemDefinition) return nullptr;

	UItemInstance* Item = NewObject<UItemInstance>(Outer);
	Item->Initialize(SavedItem.ItemDefinition, SavedItem.StatsMap);
	return Item;
}

bool UItemSaveLibrary::SavePlayerItems(const TArray<FSavedItem>& Items)
{
	UPlayerItemsSaveGame* Save = LoadOrCreateSave();
	if (!Save) return false;

	Save->PlayerItems = Items;
	Save->bHasPlayerItems = true;
	return UGameplayStatics::SaveGameToSlot(Save, ItemSaveSlot, 0);
}

bool UItemSaveLibrary::LoadPlayerItems(TArray<FSavedItem>& Items)
{
	Items.Reset();

	const UPlayerItemsSaveGame* Save = LoadSave();
	if (!Save || !Save->bHasPlayerItems) return false;

	Items = Save->PlayerItems;
	return true;
}

bool UItemSaveLibrary::SaveChestItems(const TArray<FSavedItem>& Items)
{
	UPlayerItemsSaveGame* Save = LoadOrCreateSave();
	if (!Save) return false;

	Save->ChestItems = Items;
	Save->bHasChestItems = true;
	return UGameplayStatics::SaveGameToSlot(Save, ItemSaveSlot, 0);
}

bool UItemSaveLibrary::LoadChestItems(TArray<FSavedItem>& Items)
{
	Items.Reset();

	const UPlayerItemsSaveGame* Save = LoadSave();
	if (!Save || !Save->bHasChestItems) return false;

	Items = Save->ChestItems;
	return true;
}

void UItemSaveLibrary::DebugDeleteSavedItems()
{
	UGameplayStatics::DeleteGameInSlot(ItemSaveSlot, 0);
}
