// Fill out your copyright notice in the Description page of Project Settings.

#include "LootLibrary.h"
#include "LootTable.h"
#include "demoBillyHill14/Items/ItemDefinition.h"

TSubclassOf<UItemDefinition> ULootLibrary::RollLoot(const ULootTable* Table, float DropChancePercent, bool& bDropped)
{
	bDropped = false;
	if (FMath::FRand() * 100.f >= DropChancePercent)
	{
		return nullptr;
	}

	TSubclassOf<UItemDefinition> Item = PickLootItem(Table);
	bDropped = Item != nullptr;
	return Item;
}

TSubclassOf<UItemDefinition> ULootLibrary::PickLootItem(const ULootTable* Table)
{
	if (!Table)
	{
		return nullptr;
	}

	// Group the usable entries by rarity, and total each group's weight.
	TMap<EItemRarity, TArray<const FLootEntry*>> ByRarity;
	TMap<EItemRarity, float> EntryWeightTotals;
	for (const FLootEntry& Entry : Table->Entries)
	{
		if (!Entry.Item || Entry.Weight <= 0.f)
		{
			continue;
		}
		const EItemRarity Rarity = Entry.Item.GetDefaultObject()->Rarity;
		ByRarity.FindOrAdd(Rarity).Add(&Entry);
		EntryWeightTotals.FindOrAdd(Rarity) += Entry.Weight;
	}

	// Pick a rarity, counting only rarities that have something to drop.
	float RarityTotal = 0.f;
	for (const TPair<EItemRarity, TArray<const FLootEntry*>>& Group : ByRarity)
	{
		RarityTotal += FMath::Max(Table->RarityWeights.FindRef(Group.Key), 0.f);
	}
	if (RarityTotal <= 0.f)
	{
		return nullptr;
	}

	EItemRarity PickedRarity = EItemRarity::None;
	float Roll = FMath::FRand() * RarityTotal;
	for (const TPair<EItemRarity, TArray<const FLootEntry*>>& Group : ByRarity)
	{
		const float Weight = FMath::Max(Table->RarityWeights.FindRef(Group.Key), 0.f);
		if (Weight <= 0.f)
		{
			continue;
		}
		PickedRarity = Group.Key;
		if (Roll < Weight)
		{
			break;
		}
		Roll -= Weight;
	}

	// Pick an item of that rarity by entry weight.
	const TArray<const FLootEntry*>& Candidates = ByRarity[PickedRarity];
	float ItemRoll = FMath::FRand() * EntryWeightTotals[PickedRarity];
	for (const FLootEntry* Entry : Candidates)
	{
		if (ItemRoll < Entry->Weight)
		{
			return Entry->Item;
		}
		ItemRoll -= Entry->Weight;
	}
	return Candidates.Last()->Item;
}
