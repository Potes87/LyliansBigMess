// Fill out your copyright notice in the Description page of Project Settings.

#include "LootLibrary.h"
#include "LootTable.h"
#include "demoBillyHill14/Items/ItemDefinition.h"
#include "demoBillyHill14/Stats/PlayerStatsComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

const UPlayerStatsComponent* ULootLibrary::FindPlayerStats(const UObject* WorldContextObject)
{
	const APawn* Player = UGameplayStatics::GetPlayerPawn(WorldContextObject, 0);
	return Player ? Player->FindComponentByClass<UPlayerStatsComponent>() : nullptr;
}

TSubclassOf<UItemDefinition> ULootLibrary::RollLoot(const UObject* WorldContextObject, const ULootTable* Table, float DropChancePercent, bool& bDropped)
{
	bDropped = false;
	if (FMath::FRand() * 100.f >= DropChancePercent)
	{
		return nullptr;
	}

	TSubclassOf<UItemDefinition> Item = PickLootItem(WorldContextObject, Table);
	bDropped = Item != nullptr;
	return Item;
}

TSubclassOf<UItemDefinition> ULootLibrary::PickLootItem(const UObject* WorldContextObject, const ULootTable* Table)
{
	if (!Table)
	{
		return nullptr;
	}

	// Item Rarity multiplies the weight of every rarity better than Common.
	const UPlayerStatsComponent* Stats = FindPlayerStats(WorldContextObject);
	const float RarityMultiplier = Stats ? Stats->GetItemRarityMultiplier() : 1.f;
	auto GetRarityWeight = [Table, RarityMultiplier](EItemRarity Rarity)
	{
		const float Weight = FMath::Max(Table->RarityWeights.FindRef(Rarity), 0.f);
		return Rarity > EItemRarity::Common && Rarity != EItemRarity::Quest ? Weight * RarityMultiplier : Weight;
	};

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
		RarityTotal += GetRarityWeight(Group.Key);
	}
	if (RarityTotal <= 0.f)
	{
		return nullptr;
	}

	EItemRarity PickedRarity = EItemRarity::None;
	float Roll = FMath::FRand() * RarityTotal;
	for (const TPair<EItemRarity, TArray<const FLootEntry*>>& Group : ByRarity)
	{
		const float Weight = GetRarityWeight(Group.Key);
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

TArray<FCurrencyDropResult> ULootLibrary::RollCurrencyDrops(const UObject* WorldContextObject, const UCurrencyDropTable* Table, EEnemyTier Tier)
{
	TArray<FCurrencyDropResult> Results;
	if (!Table)
	{
		return Results;
	}
	const float* FoundMultiplier = Table->TierChanceMultiplier.Find(Tier);
	const UPlayerStatsComponent* Stats = FindPlayerStats(WorldContextObject);
	const float FindMultiplier = Stats ? Stats->GetCurrencyFindMultiplier() : 1.f;
	const float Multiplier = (FoundMultiplier ? FMath::Max(*FoundMultiplier, 0.f) : 1.f) * FindMultiplier;
	for (const FCurrencyDropEntry& Entry : Table->Entries)
	{
		if (!Entry.Item || FMath::FRand() * 100.f >= Entry.ChancePercent * Multiplier)
		{
			continue;
		}
		FCurrencyDropResult& Result = Results.AddDefaulted_GetRef();
		Result.Item = Entry.Item;
		Result.Count = FMath::RandRange(Entry.MinCount, FMath::Max(Entry.MinCount, Entry.MaxCount));
	}
	return Results;
}
