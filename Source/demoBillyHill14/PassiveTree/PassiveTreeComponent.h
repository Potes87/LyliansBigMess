// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PassiveTreeTypes.h"
#include "PassiveTreeComponent.generated.h"

class UDataTable;
class UPlayerStatsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPassiveTreeChanged);

/**
 * Tracks which passive nodes the player has allocated, enforces the tree's rules, and sends each
 * allocated node's modifiers to UPlayerStatsComponent.
 *
 * Start nodes are allocated automatically and free. Any other node costs one point and must link to a
 * node you own. A node can only be refunded if everything else you own still connects back to a Start node.
 */
UCLASS(ClassGroup = (Stats), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UPassiveTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPassiveTreeComponent();

	// The table of nodes. Its row structure must be PassiveNodeRow.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive Tree", meta = (RequiredAssetDataTags = "RowStructure=/Script/demoBillyHill14.PassiveNodeRow"))
	TObjectPtr<UDataTable> NodeTable;

	// Points gained for each level above level 1.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive Tree", meta = (ClampMin = "0"))
	int32 PointsPerLevel = 1;

	// Fires when nodes are allocated or refunded, and when the player's level changes the available points.
	UPROPERTY(BlueprintAssignable, Category = "Passive Tree")
	FOnPassiveTreeChanged OnPassiveTreeChanged;

	// Tells the tree the player's current level. Call this at BeginPlay and on every level up.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree")
	void SetPlayerLevel(int32 NewLevel);

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	int32 GetPlayerLevel() const { return PlayerLevel; }

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	int32 GetTotalPoints() const;

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	int32 GetSpentPoints() const;

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	int32 GetAvailablePoints() const;

	// Checks the rules without changing anything. Returns Success if the node could be allocated right now.
	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	EPassiveNodeResult CanAllocateNode(FName NodeId) const;

	// Checks the rules without changing anything. Returns Success if the node could be refunded right now.
	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	EPassiveNodeResult CanRefundNode(FName NodeId) const;

	UFUNCTION(BlueprintCallable, Category = "Passive Tree")
	EPassiveNodeResult AllocateNode(FName NodeId);

	UFUNCTION(BlueprintCallable, Category = "Passive Tree")
	EPassiveNodeResult RefundNode(FName NodeId);

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	bool IsNodeAllocated(FName NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	const TArray<FName>& GetAllocatedNodes() const { return AllocatedNodes; }

	// The row name of every node in the table.
	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	TArray<FName> GetNodeIds() const;

	const FPassiveNodeRow* FindNode(FName NodeId) const;

	// Every node linked to this one, in either direction.
	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	TArray<FName> GetLinkedNodes(FName NodeId) const;

	// Testing helper: allocates the first row the rules allow and returns its name, or None if there isn't one.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Debug")
	FName DebugAllocateNextNode();

	// Testing helper: refunds everything except Start nodes, ignoring the connection rule.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Debug")
	void DebugRefundAllNodes();

protected:
	virtual void BeginPlay() override;

private:
	bool IsStartNode(FName NodeId) const;
	bool IsLinkedToAllocatedNode(FName NodeId) const;
	bool WouldStayConnectedWithout(FName NodeId) const;

	void BuildLinks();
	void AllocateStartNodes();
	EPassiveNodeResult ApplyAllocation(FName NodeId, const FPassiveNodeRow& Node);
	EPassiveNodeResult ApplyRefund(FName NodeId);

	UPlayerStatsComponent* FindStatsComponent() const;

	static FName MakeSourceId(FName NodeId);

	UPROPERTY()
	TArray<FName> AllocatedNodes;

	int32 PlayerLevel = 1;

	// Links in both directions, built from the table at BeginPlay.
	TMap<FName, TArray<FName>> Links;
};
