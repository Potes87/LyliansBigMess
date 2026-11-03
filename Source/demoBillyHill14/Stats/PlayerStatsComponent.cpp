// Fill out your copyright notice in the Description page of Project Settings.

#include "PlayerStatsComponent.h"
#include "demoBillyHill14/Items/ItemDefinition.h"
#include "demoBillyHill14/Items/ItemInstance.h"
#include "NativeGameplayTags.h"

namespace
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Stat_MagazineSize, "Item.Stat.MagazineSize");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Stat_FireDelay, "Item.Stat.FireDelay");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Stat_FireRate, "Item.Stat.FireRate");
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Stat_ReloadSpeed, "Item.Stat.ReloadSpeed");

	// Stops a large penalty from making a shot or reload take forever.
	constexpr float MinSpeedMultiplier = 0.1f;
}

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

	return GetModifiedValue(Item->GetStatValue(Stat), Stat, GetItemTags(Item));
}

int32 UPlayerStatsComponent::GetMagazineSize(UItemInstance* Item) const
{
	return FMath::Max(1, FMath::RoundToInt(GetModifiedStat(Item, TAG_Stat_MagazineSize)));
}

float UPlayerStatsComponent::GetFireDelay(UItemInstance* Item) const
{
	if (!Item) return MinFireDelay;

	return ApplySpeedBonus(Item->GetStatValue(TAG_Stat_FireDelay), TAG_Stat_FireRate, Item, MinFireDelay);
}

float UPlayerStatsComponent::GetReloadTime(UItemInstance* Item) const
{
	if (!Item) return BaseReloadTime;

	const float ItemReloadTime = Item->GetStatValue(TAG_Stat_ReloadSpeed);
	const float BaseTime = ItemReloadTime > 0.f ? ItemReloadTime : BaseReloadTime;
	return ApplySpeedBonus(BaseTime, TAG_Stat_ReloadSpeed, Item, MinReloadTime);
}

float UPlayerStatsComponent::GetReloadPlayRate(UItemInstance* Item) const
{
	return BaseReloadTime / GetReloadTime(Item);
}

FGameplayTagContainer UPlayerStatsComponent::GetItemTags(const UItemInstance* Item)
{
	const UItemDefinition* Definition = Item ? Item->ItemDefinition.GetDefaultObject() : nullptr;
	return Definition ? Definition->ItemType : FGameplayTagContainer();
}

float UPlayerStatsComponent::ApplySpeedBonus(float BaseTime, FGameplayTag SpeedStat, const UItemInstance* Item, float MinTime) const
{
	const float SpeedMultiplier = FMath::Max(MinSpeedMultiplier, GetModifiedValue(1.f, SpeedStat, GetItemTags(Item)));
	return FMath::Max(MinTime, BaseTime / SpeedMultiplier);
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
