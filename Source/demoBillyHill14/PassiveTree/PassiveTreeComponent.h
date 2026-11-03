// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PassiveTreeComponent.generated.h"

class UDataTable;
class UPlayerStatsComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPassiveTreeChanged);

/**
 * Tracks which passive nodes the player has allocated and sends their modifiers to UPlayerStatsComponent.
 * Points and connection rules aren't enforced yet: they arrive in phase 2.
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

	UPROPERTY(BlueprintAssignable, Category = "Passive Tree")
	FOnPassiveTreeChanged OnPassiveTreeChanged;

	// Returns false if the node doesn't exist or is already allocated.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree")
	bool AllocateNode(FName NodeId);

	// Returns false if the node isn't allocated.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree")
	bool RefundNode(FName NodeId);

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	bool IsNodeAllocated(FName NodeId) const;

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	const TArray<FName>& GetAllocatedNodes() const { return AllocatedNodes; }

	// Testing helper: allocates the first unallocated row and returns its name, or None if all are allocated.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Debug")
	FName DebugAllocateNextNode();

	// Testing helper: refunds every allocated node.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Debug")
	void DebugRefundAllNodes();

private:
	UPlayerStatsComponent* FindStatsComponent() const;

	static FName MakeSourceId(FName NodeId);

	UPROPERTY()
	TArray<FName> AllocatedNodes;
};
