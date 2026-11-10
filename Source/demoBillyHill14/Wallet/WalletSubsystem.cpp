// Fill out your copyright notice in the Description page of Project Settings.

#include "WalletSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "demoBillyHill14/Items/ItemDefinition.h"

const FString UWalletSubsystem::SlotName = TEXT("PlayerWallet");

void UWalletSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		if (const UWalletSaveGame* SaveGame = Cast<UWalletSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0)))
		{
			Balances = SaveGame->Balances;
		}
	}
}

int32 UWalletSubsystem::GetBalance(FGameplayTag Currency) const
{
	return Balances.FindRef(Currency);
}

bool UWalletSubsystem::CanAfford(FGameplayTag Currency, int32 Amount) const
{
	return Amount <= 0 || GetBalance(Currency) >= Amount;
}

int32 UWalletSubsystem::AddCurrency(FGameplayTag Currency, int32 Amount)
{
	if (!Currency.IsValid() || Amount <= 0)
	{
		return 0;
	}
	int32& Balance = Balances.FindOrAdd(Currency);
	const int32 Added = FMath::Clamp(MaxPerCurrency - Balance, 0, Amount);
	if (Added > 0)
	{
		Balance += Added;
		const int32 NewBalance = Balance;
		Save();
		OnWalletChanged.Broadcast(Currency, NewBalance);
	}
	return Added;
}

bool UWalletSubsystem::SpendCurrency(FGameplayTag Currency, int32 Amount)
{
	if (Amount <= 0)
	{
		return true;
	}
	if (!CanAfford(Currency, Amount))
	{
		return false;
	}
	int32& Balance = Balances.FindOrAdd(Currency);
	Balance -= Amount;
	const int32 NewBalance = Balance;
	Save();
	OnWalletChanged.Broadcast(Currency, NewBalance);
	return true;
}

bool UWalletSubsystem::GetCurrencyTag(TSubclassOf<UItemDefinition> Item, FGameplayTag& Currency)
{
	Currency = FGameplayTag();
	if (!Item)
	{
		return false;
	}
	const FGameplayTag Root = FGameplayTag::RequestGameplayTag(TEXT("Currency"), false);
	if (!Root.IsValid())
	{
		return false;
	}
	for (const FGameplayTag& Tag : Item.GetDefaultObject()->ItemType)
	{
		if (Tag.MatchesTag(Root) && Tag != Root)
		{
			Currency = Tag;
			return true;
		}
	}
	return false;
}

void UWalletSubsystem::TryCollectItem(TSubclassOf<UItemDefinition> Item, int32 Count, bool& bIsCurrency, bool& bCollected)
{
	FGameplayTag Currency;
	bIsCurrency = GetCurrencyTag(Item, Currency);
	bCollected = bIsCurrency && AddCurrency(Currency, FMath::Max(Count, 1)) > 0;
}

void UWalletSubsystem::DebugClearWallet()
{
	TArray<FGameplayTag> Keys;
	Balances.GetKeys(Keys);
	Balances.Reset();
	Save();
	for (const FGameplayTag& Tag : Keys)
	{
		OnWalletChanged.Broadcast(Tag, 0);
	}
}

void UWalletSubsystem::Save() const
{
	UWalletSaveGame* SaveGame = Cast<UWalletSaveGame>(UGameplayStatics::CreateSaveGameObject(UWalletSaveGame::StaticClass()));
	SaveGame->Balances = Balances;
	UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, 0);
}
