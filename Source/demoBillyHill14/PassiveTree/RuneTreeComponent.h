// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SkillTreeComponent.h"
#include "RuneTreeComponent.generated.h"

class UWalletSubsystem;

/** The rune tree's save: how many points were bought and which nodes they went into. */
UCLASS()
class DEMOBILLYHILL14_API URuneTreeSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	int32 BoughtPoints = 0;

	UPROPERTY()
	TArray<FName> AllocatedNodes;
};

/**
 * The Seeking: the rune tree. Points are bought with Runes of Seeking from the wallet, usually at the rune shrine.
 * Points and nodes are permanent: nothing is ever refunded. Every upgrade is meant to be good, so the only choice
 * is the order you take them in.
 *
 * Point N (counting from 0) costs BasePointCost + PointCostStep * N, capped at MaxPointCost.
 * It loads itself at BeginPlay and saves after every purchase and allocation, so it needs no Blueprint save wiring.
 */
UCLASS(ClassGroup = (Stats), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API URuneTreeComponent : public USkillTreeComponent
{
	GENERATED_BODY()

public:
	URuneTreeComponent();

	// What the first point costs.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune Tree", meta = (ClampMin = "1"))
	int32 BasePointCost = 10;

	// How much each point costs more than the last.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune Tree", meta = (ClampMin = "0"))
	int32 PointCostStep = 2;

	// No point ever costs more than this.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rune Tree", meta = (ClampMin = "1"))
	int32 MaxPointCost = 20;

	virtual int32 GetTotalPoints() const override { return BoughtPoints; }
	virtual bool AllowsRefunds() const override { return false; }

	UFUNCTION(BlueprintPure, Category = "Rune Tree")
	int32 GetBoughtPoints() const { return BoughtPoints; }

	// The rune cost of the next point.
	UFUNCTION(BlueprintPure, Category = "Rune Tree")
	int32 GetNextPointCost() const;

	// How many Runes of Seeking the wallet holds.
	UFUNCTION(BlueprintPure, Category = "Rune Tree")
	int32 GetRuneBalance() const;

	// How many nodes the tree has that a bought point could still go into. Buying stops when this runs out.
	UFUNCTION(BlueprintPure, Category = "Rune Tree")
	int32 GetUnclaimedNodeCount() const;

	// True if the wallet can pay for the next point and there's still a node left to spend it on.
	UFUNCTION(BlueprintPure, Category = "Rune Tree")
	bool CanBuyPoint() const;

	// Spends the runes and adds one point. Returns false and changes nothing if CanBuyPoint is false.
	UFUNCTION(BlueprintCallable, Category = "Rune Tree")
	bool BuyPoint();

	// Testing only: wipes bought points and nodes, and deletes the save. The runes aren't given back.
	UFUNCTION(BlueprintCallable, Category = "Rune Tree|Debug")
	void DebugResetRuneTree();

protected:
	virtual void BeginPlay() override;
	virtual void OnNodeAllocated(FName NodeId) override;

private:
	UWalletSubsystem* GetWallet() const;
	void LoadRuneTree();
	void SaveRuneTree() const;

	int32 BoughtPoints = 0;
};
