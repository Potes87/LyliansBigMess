// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PassiveTreeTypes.h"
#include "SkillTreeComponent.generated.h"

class UDataTable;
class UPassiveEffect;
class UPlayerStatsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPassiveTreeChanged);

/**
 * The shared rules of every node tree: which nodes are allocated, links, start nodes and sealed nodes. Each
 * allocated node's modifiers go to UPlayerStatsComponent. Subclasses decide where points come from:
 * UPassiveTreeComponent from the player's level, URuneTreeComponent from runes bought at the shrine.
 *
 * Start nodes are allocated automatically and free. Any other node costs one point and must link to a node
 * you own. A node can only be refunded if the tree allows refunds and everything else you own still connects
 * back to a Start node.
 */
UCLASS(Abstract, ClassGroup = (Stats))
class DEMOBILLYHILL14_API USkillTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USkillTreeComponent();

	// The table of nodes. Its row structure must be PassiveNodeRow.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive Tree", meta = (RequiredAssetDataTags = "RowStructure=/Script/demoBillyHill14.PassiveNodeRow"))
	TObjectPtr<UDataTable> NodeTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive Tree|Saving")
	FString SaveSlotName = TEXT("PlayerProgress");

	// Fires when nodes are allocated or refunded, and whenever the available points change.
	UPROPERTY(BlueprintAssignable, Category = "Passive Tree")
	FOnPassiveTreeChanged OnPassiveTreeChanged;

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	virtual int32 GetTotalPoints() const { return 0; }

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	int32 GetSpentPoints() const;

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	int32 GetAvailablePoints() const;

	// Whether allocated nodes can ever be refunded on this tree.
	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	virtual bool AllowsRefunds() const { return true; }

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

	// Refunds everything except Start nodes. Does nothing on trees that don't allow refunds.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree")
	void RefundAllNodes();

	// Testing helper: deletes the save so the next Play starts fresh.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Debug")
	void DebugDeleteSave();

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

	// Testing helper: the same as RefundAllNodes.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Debug")
	void DebugRefundAllNodes();

protected:
	virtual void BeginPlay() override;

	// Called after a player allocates a node, before OnPassiveTreeChanged fires.
	virtual void OnNodeAllocated(FName NodeId) {}

	bool IsStartNode(FName NodeId) const;
	void RefundAllWithoutBroadcast();
	void RestoreNodes(const TArray<FName>& SavedNodes);

	// Removes every node except Start nodes, ignoring the refund rules. Doesn't broadcast.
	void ResetAllNodes();

	UPROPERTY()
	TArray<FName> AllocatedNodes;

	// The running effect of each allocated node that has one.
	UPROPERTY()
	TMap<FName, TObjectPtr<UPassiveEffect>> ActiveEffects;

private:
	bool IsLinkedToAllocatedNode(FName NodeId) const;
	bool WouldStayConnectedWithout(FName NodeId) const;

	void BuildLinks();
	void AllocateStartNodes();
	EPassiveNodeResult ApplyAllocation(FName NodeId, const FPassiveNodeRow& Node);
	EPassiveNodeResult ApplyRefund(FName NodeId);

	UPlayerStatsComponent* FindStatsComponent() const;

	// Unique per tree component, so two trees can use the same row names.
	FName MakeSourceId(FName NodeId) const;

	// Links in both directions, built from the table at BeginPlay.
	TMap<FName, TArray<FName>> Links;
};
