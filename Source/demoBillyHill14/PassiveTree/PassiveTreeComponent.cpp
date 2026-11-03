// Fill out your copyright notice in the Description page of Project Settings.

#include "PassiveTreeComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "PassiveTreeTypes.h"
#include "demoBillyHill14/Stats/PlayerStatsComponent.h"

UPassiveTreeComponent::UPassiveTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UPassiveTreeComponent::AllocateNode(FName NodeId)
{
	if (!NodeTable || IsNodeAllocated(NodeId)) return false;

	const FPassiveNodeRow* Node = NodeTable->FindRow<FPassiveNodeRow>(NodeId, TEXT("AllocateNode"));
	if (!Node) return false;

	UPlayerStatsComponent* Stats = FindStatsComponent();
	if (!Stats) return false;

	AllocatedNodes.Add(NodeId);
	Stats->SetModifierSource(MakeSourceId(NodeId), Node->Modifiers);
	OnPassiveTreeChanged.Broadcast();
	return true;
}

bool UPassiveTreeComponent::RefundNode(FName NodeId)
{
	if (!IsNodeAllocated(NodeId)) return false;

	UPlayerStatsComponent* Stats = FindStatsComponent();
	if (!Stats) return false;

	AllocatedNodes.Remove(NodeId);
	Stats->RemoveModifierSource(MakeSourceId(NodeId));
	OnPassiveTreeChanged.Broadcast();
	return true;
}

bool UPassiveTreeComponent::IsNodeAllocated(FName NodeId) const
{
	return AllocatedNodes.Contains(NodeId);
}

FName UPassiveTreeComponent::DebugAllocateNextNode()
{
	if (!NodeTable) return NAME_None;

	for (const FName& RowName : NodeTable->GetRowNames())
	{
		if (!IsNodeAllocated(RowName))
		{
			return AllocateNode(RowName) ? RowName : FName(NAME_None);
		}
	}
	return NAME_None;
}

void UPassiveTreeComponent::DebugRefundAllNodes()
{
	const TArray<FName> NodesToRefund = AllocatedNodes;
	for (const FName& NodeId : NodesToRefund)
	{
		RefundNode(NodeId);
	}
}

UPlayerStatsComponent* UPassiveTreeComponent::FindStatsComponent() const
{
	const AActor* Owner = GetOwner();
	UPlayerStatsComponent* Stats = Owner ? Owner->FindComponentByClass<UPlayerStatsComponent>() : nullptr;
	if (!Stats)
	{
		UE_LOG(LogTemp, Warning, TEXT("PassiveTreeComponent on %s needs a PlayerStatsComponent on the same actor."), *GetNameSafe(Owner));
	}
	return Stats;
}

FName UPassiveTreeComponent::MakeSourceId(FName NodeId)
{
	return FName(*FString::Printf(TEXT("Passive.%s"), *NodeId.ToString()));
}
