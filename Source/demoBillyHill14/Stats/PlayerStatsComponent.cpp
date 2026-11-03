// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerStatsComponent.h"
#include "demoBillyHill14/Items/ItemDefinition.h"
#include "demoBillyHill14/Items/ItemInstance.h"

UPlayerStatsComponent::UPlayerStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPlayerStatsComponent::SetModifierSource(FName SourceId, const TArray<FStatModifier>& Modifiers)
{
	ModifierSources.Add(SourceId, Modifiers);
	OnStatsChanged.Broadcast();
}

void UPlayerStatsComponent::RemoveModifierSource(FName SourceId)
{
	if (ModifierSources.Remove(SourceId) > 0)
	{
		OnStatsChanged.Broadcast();
	}
}

float UPlayerStatsComponent::GetModifiedStat(UItemInstance* Item, FGameplayTag Stat) const
{
	if (!Item) return 0.f;

	FGameplayTagContainer ItemTags;
	if (const UItemDefinition* Definition = Item->ItemDefinition.GetDefaultObject())
	{
		ItemTags = Definition->ItemType;
	}

	return GetModifiedValue(Item->GetStatValue(Stat), Stat, ItemTags);
}

float UPlayerStatsComponent::GetModifiedValue(float BaseValue, FGameplayTag Stat, const FGameplayTagContainer& ItemTags) const
{
	float Flat = 0.f;
	float IncreasedPercent = 0.f;
	float MoreMultiplier = 1.f;

	for (const TPair<FName, TArray<FStatModifier>>& Source : ModifierSources)
	{
		for (const FStatModifier& Modifier : Source.Value)
		{
			if (Modifier.Stat != Stat || !ItemTags.HasAll(Modifier.RequiredWeaponTags))
			{
				continue;
			}

			switch (Modifier.Op)
			{
			case EModifierOp::Flat:
				Flat += Modifier.Value;
				break;
			case EModifierOp::Increased:
				IncreasedPercent += Modifier.Value;
				break;
			case EModifierOp::More:
				MoreMultiplier *= FMath::Max(0.f, 1.f + Modifier.Value / 100.f);
				break;
			}
		}
	}

	const float IncreasedMultiplier = FMath::Max(0.f, 1.f + IncreasedPercent / 100.f);
	return (BaseValue + Flat) * IncreasedMultiplier * MoreMultiplier;
}
