// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SubclassOf.h"
#include "WalletSubsystem.generated.h"

class UItemDefinition;

/** What's written to disk. Kept apart from items so currency can't be lost when inventories change. */
UCLASS()
class DEMOBILLYHILL14_API UWalletSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TMap<FGameplayTag, int32> Balances;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnWalletChanged, FGameplayTag, Currency, int32, NewBalance);

/**
 * The player's currency (divine orbs, runes of seeking, ...). Lives on the game instance, so it survives level travel,
 * and saves itself after every change, so it survives restarts. Never takes inventory or stash slots.
 * An item counts as currency when its ItemType has a Currency.* tag, e.g. Currency.DivineOrb.
 */
UCLASS()
class DEMOBILLYHILL14_API UWalletSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// The most of one currency the wallet holds.
	UPROPERTY(BlueprintReadOnly, Category = "Wallet")
	int32 MaxPerCurrency = 5000;

	// Fires whenever a balance changes. Good place for UI refreshes and the pickup or spend sound.
	UPROPERTY(BlueprintAssignable, Category = "Wallet")
	FOnWalletChanged OnWalletChanged;

	UFUNCTION(BlueprintPure, Category = "Wallet", meta = (Categories = "Currency"))
	int32 GetBalance(FGameplayTag Currency) const;

	UFUNCTION(BlueprintPure, Category = "Wallet", meta = (Categories = "Currency"))
	bool CanAfford(FGameplayTag Currency, int32 Amount) const;

	// Adds up to the cap. Returns how many were actually added.
	UFUNCTION(BlueprintCallable, Category = "Wallet", meta = (Categories = "Currency"))
	int32 AddCurrency(FGameplayTag Currency, int32 Amount);

	// Removes Amount if the player has enough. Returns false and changes nothing otherwise.
	UFUNCTION(BlueprintCallable, Category = "Wallet", meta = (Categories = "Currency"))
	bool SpendCurrency(FGameplayTag Currency, int32 Amount);

	// Finds the Currency.* tag in an item's ItemType. Returns false if the item isn't currency.
	UFUNCTION(BlueprintPure, Category = "Wallet")
	static bool GetCurrencyTag(TSubclassOf<UItemDefinition> Item, FGameplayTag& Currency);

	/**
	 * Call when the player picks something up. bIsCurrency is false for normal items, which then go to the inventory as usual.
	 * For currency, bCollected is true if anything fit in the wallet; if the wallet is full, leave the pickup on the ground.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wallet")
	void TryCollectItem(TSubclassOf<UItemDefinition> Item, int32 Count, bool& bIsCurrency, bool& bCollected);

	// Testing only: empties the wallet and its save.
	UFUNCTION(BlueprintCallable, Category = "Wallet|Debug")
	void DebugClearWallet();

private:
	void Save() const;

	UPROPERTY()
	TMap<FGameplayTag, int32> Balances;

	static const FString SlotName;
};
