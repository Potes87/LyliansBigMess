// Fill out your copyright notice in the Description page of Project Settings.

#include "GunRerollLibrary.h"
#include "ItemDefinition.h"
#include "ItemInstance.h"
#include "demoBillyHill14/Fragments/InventoryItemFragment.h"
#include "demoBillyHill14/Wallet/WalletSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"

namespace
{
	const FName RandomStatsArrayName(TEXT("RandomStatValues"));

	// Blueprint struct fields carry a GUID suffix (MinValue_4_ABC...), so match on the start of the name.
	float ReadNumber(const UStruct* Struct, const void* Data, const TCHAR* Prefix)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			if (!It->GetName().StartsWith(Prefix)) continue;
			if (const FDoubleProperty* D = CastField<FDoubleProperty>(*It)) return static_cast<float>(D->GetPropertyValue_InContainer(Data));
			if (const FFloatProperty* F = CastField<FFloatProperty>(*It)) return F->GetPropertyValue_InContainer(Data);
		}
		return 0.f;
	}

	FGameplayTag ReadTag(const UStruct* Struct, const void* Data)
	{
		for (TFieldIterator<FStructProperty> It(Struct); It; ++It)
		{
			if (It->Struct == FGameplayTag::StaticStruct())
			{
				return *It->ContainerPtrToValuePtr<FGameplayTag>(Data);
			}
		}
		return FGameplayTag();
	}

	UWalletSubsystem* GetWallet(const UObject* WorldContextObject)
	{
		const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		return GameInstance ? GameInstance->GetSubsystem<UWalletSubsystem>() : nullptr;
	}

	FGameplayTag DivineOrbTag()
	{
		return FGameplayTag::RequestGameplayTag(TEXT("Currency.DivineOrb"), false);
	}
}

const UInventoryItemFragment* UGunRerollLibrary::FindRandomStatsFragment(UItemInstance* Item)
{
	if (!Item || !Item->ItemDefinition) return nullptr;
	for (const UInventoryItemFragment* Fragment : Item->ItemDefinition.GetDefaultObject()->Fragments)
	{
		if (Fragment && FindFProperty<FArrayProperty>(Fragment->GetClass(), RandomStatsArrayName))
		{
			return Fragment;
		}
	}
	return nullptr;
}

bool UGunRerollLibrary::IsLowerBetter(const FGameplayTag& Stat)
{
	// Fire delay and reload time are in seconds, so smaller numbers are better.
	static const FName FireDelay(TEXT("Item.Stat.FireDelay"));
	static const FName ReloadSpeed(TEXT("Item.Stat.ReloadSpeed"));
	return Stat.GetTagName() == FireDelay || Stat.GetTagName() == ReloadSpeed;
}

bool UGunRerollLibrary::HasRandomStats(UItemInstance* Item)
{
	return GetStatRolls(Item).Num() > 0;
}

TArray<FStatRollInfo> UGunRerollLibrary::GetStatRolls(UItemInstance* Item)
{
	TArray<FStatRollInfo> Rolls;
	const UInventoryItemFragment* Fragment = FindRandomStatsFragment(Item);
	if (!Fragment) return Rolls;

	const FArrayProperty* ArrayProp = FindFProperty<FArrayProperty>(Fragment->GetClass(), RandomStatsArrayName);
	const FStructProperty* Inner = CastField<FStructProperty>(ArrayProp->Inner);
	if (!Inner) return Rolls;

	FScriptArrayHelper Array(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(Fragment));
	for (int32 i = 0; i < Array.Num(); ++i)
	{
		const void* Element = Array.GetRawPtr(i);
		FStatRollInfo Info;
		Info.Stat = ReadTag(Inner->Struct, Element);
		Info.Min = ReadNumber(Inner->Struct, Element, TEXT("MinValue"));
		Info.Max = ReadNumber(Inner->Struct, Element, TEXT("MaxValue"));
		if (!Info.Stat.IsValid()) continue;

		Info.Value = Item->GetStatValue(Info.Stat);
		const float Span = Info.Max - Info.Min;
		float Q = FMath::IsNearlyZero(Span) ? 1.f : FMath::Clamp((Info.Value - Info.Min) / Span, 0.f, 1.f);
		if (IsLowerBetter(Info.Stat)) Q = 1.f - Q;
		Info.Quality = Q;
		Rolls.Add(Info);
	}
	return Rolls;
}

float UGunRerollLibrary::GetRollQuality(UItemInstance* Item)
{
	const TArray<FStatRollInfo> Rolls = GetStatRolls(Item);
	if (Rolls.Num() == 0) return 0.f;
	float Sum = 0.f;
	for (const FStatRollInfo& Roll : Rolls) Sum += Roll.Quality;
	return Sum / Rolls.Num();
}

bool UGunRerollLibrary::CanReroll(const UObject* WorldContextObject, UItemInstance* Item)
{
	const UWalletSubsystem* Wallet = GetWallet(WorldContextObject);
	return Wallet && Wallet->CanAfford(DivineOrbTag(), 1) && HasRandomStats(Item);
}

bool UGunRerollLibrary::TryReroll(const UObject* WorldContextObject, UItemInstance* Item)
{
	UWalletSubsystem* Wallet = GetWallet(WorldContextObject);
	const UInventoryItemFragment* Fragment = FindRandomStatsFragment(Item);
	if (!Wallet || !Fragment || !CanReroll(WorldContextObject, Item)) return false;
	if (!Wallet->SpendCurrency(DivineOrbTag(), 1)) return false;

	// Run the same roll the gun got when it was first created.
	const_cast<UInventoryItemFragment*>(Fragment)->OnInstanceCreated(Item);
	return true;
}
