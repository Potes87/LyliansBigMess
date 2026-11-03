// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PassiveTreeTypes.h"
#include "PassiveTreeComponent.generated.h"

class UDataTable;
class UPassiveEffect;
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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive Tree|Saving")
	FString SaveSlotName = TEXT("PlayerProgress");

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

	// Refunds everything except Start nodes.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree")
	void RefundAllNodes();

	// Writes the level, the XP passed in and the allocated nodes to the save slot. Returns false if that failed.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Saving")
	bool SaveProgress(float CurrentXP);

	// Restores the level and allocated nodes from the save slot, and outputs the saved level and XP for the
	// leveling component. Saved nodes that no longer exist, connect or fit the available points are dropped.
	// Returns false if there's no save, leaving the tree unchanged.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Saving")
	bool LoadProgress(int32& Level, float& CurrentXP);

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

	// The gun calls this before spawning a bullet. Returns the damage the bullet should carry.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Events")
	float NotifyShotFired(float Damage);

	// The bullet calls this when it hits an enemy. Returns the damage to deal.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Events")
	float NotifyEnemyHit(AActor* Enemy, float Damage);

	// The gun calls this when a reload finishes.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Events")
	void NotifyReloaded();

	// Whether the gun may fire right now, as far as the allocated nodes are concerned.
	UFUNCTION(BlueprintPure, Category = "Passive Tree|Events")
	bool CanFireShot() const;

	// How many rounds one shot takes from the magazine.
	UFUNCTION(BlueprintPure, Category = "Passive Tree|Events")
	int32 GetAmmoCost() const;

	// Whether the gun may reload, as far as the allocated nodes are concerned.
	UFUNCTION(BlueprintPure, Category = "Passive Tree|Events")
	bool CanReload() const;

	// The gun calls this every frame. Returns the whole rounds to add to the magazine, usually 0.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Events")
	int32 TakeRegeneratedAmmo(float DeltaSeconds, int32 MagazineSize);

	// Testing helper: allocates the first row the rules allow and returns its name, or None if there isn't one.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Debug")
	FName DebugAllocateNextNode();

	// Testing helper: the same as RefundAllNodes.
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
	void RefundAllWithoutBroadcast();
	void RestoreNodes(const TArray<FName>& SavedNodes);

	UPlayerStatsComponent* FindStatsComponent() const;

	static FName MakeSourceId(FName NodeId);

	UPROPERTY()
	TArray<FName> AllocatedNodes;

	// The running effect of each allocated node that has one.
	UPROPERTY()
	TMap<FName, TObjectPtr<UPassiveEffect>> ActiveEffects;

	int32 PlayerLevel = 1;

	// Regenerated ammo that hasn't added up to a whole round yet.
	float PendingAmmo = 0.f;

	// Links in both directions, built from the table at BeginPlay.
	TMap<FName, TArray<FName>> Links;
};
