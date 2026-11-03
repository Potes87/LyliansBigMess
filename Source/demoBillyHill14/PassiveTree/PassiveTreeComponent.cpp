// Fill out your copyright notice in the Description page of Project Settings.

#include "PassiveTreeComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "demoBillyHill14/Stats/PlayerStatsComponent.h"

UPassiveTreeComponent::UPassiveTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPassiveTreeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!NodeTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("PassiveTreeComponent on %s has no NodeTable assigned."), *GetNameSafe(GetOwner()));
		return;
	}

	BuildLinks();
	AllocateStartNodes();
}

void UPassiveTreeComponent::SetPlayerLevel(int32 NewLevel)
{
	if (NewLevel == PlayerLevel) return;

	PlayerLevel = NewLevel;
	OnPassiveTreeChanged.Broadcast();
}

int32 UPassiveTreeComponent::GetTotalPoints() const
{
	return FMath::Max(0, PlayerLevel - 1) * PointsPerLevel;
}

int32 UPassiveTreeComponent::GetSpentPoints() const
{
	int32 SpentPoints = 0;
	for (const FName& NodeId : AllocatedNodes)
	{
		if (!IsStartNode(NodeId))
		{
			++SpentPoints;
		}
	}
	return SpentPoints;
}

int32 UPassiveTreeComponent::GetAvailablePoints() const
{
	return GetTotalPoints() - GetSpentPoints();
}

EPassiveNodeResult UPassiveTreeComponent::CanAllocateNode(FName NodeId) const
{
	if (!NodeTable) return EPassiveNodeResult::NoNodeTable;
	if (!FindNode(NodeId)) return EPassiveNodeResult::NodeNotFound;
	if (IsNodeAllocated(NodeId)) return EPassiveNodeResult::AlreadyAllocated;
	if (GetAvailablePoints() <= 0) return EPassiveNodeResult::NoPointsAvailable;
	if (!IsLinkedToAllocatedNode(NodeId)) return EPassiveNodeResult::NotConnected;

	return EPassiveNodeResult::Success;
}

EPassiveNodeResult UPassiveTreeComponent::CanRefundNode(FName NodeId) const
{
	if (!NodeTable) return EPassiveNodeResult::NoNodeTable;
	if (!FindNode(NodeId)) return EPassiveNodeResult::NodeNotFound;
	if (!IsNodeAllocated(NodeId)) return EPassiveNodeResult::NotAllocated;
	if (IsStartNode(NodeId)) return EPassiveNodeResult::CannotRefundStart;
	if (!WouldStayConnectedWithout(NodeId)) return EPassiveNodeResult::WouldDisconnectTree;

	return EPassiveNodeResult::Success;
}

EPassiveNodeResult UPassiveTreeComponent::AllocateNode(FName NodeId)
{
	const EPassiveNodeResult Result = CanAllocateNode(NodeId);
	if (Result != EPassiveNodeResult::Success) return Result;

	return ApplyAllocation(NodeId, *FindNode(NodeId));
}

EPassiveNodeResult UPassiveTreeComponent::RefundNode(FName NodeId)
{
	const EPassiveNodeResult Result = CanRefundNode(NodeId);
	if (Result != EPassiveNodeResult::Success) return Result;

	return ApplyRefund(NodeId);
}

bool UPassiveTreeComponent::IsNodeAllocated(FName NodeId) const
{
	return AllocatedNodes.Contains(NodeId);
}

TArray<FName> UPassiveTreeComponent::GetLinkedNodes(FName NodeId) const
{
	const TArray<FName>* NodeLinks = Links.Find(NodeId);
	return NodeLinks ? *NodeLinks : TArray<FName>();
}

FName UPassiveTreeComponent::DebugAllocateNextNode()
{
	if (!NodeTable) return NAME_None;

	for (const FName& RowName : NodeTable->GetRowNames())
	{
		if (AllocateNode(RowName) == EPassiveNodeResult::Success)
		{
			return RowName;
		}
	}
	return NAME_None;
}

void UPassiveTreeComponent::DebugRefundAllNodes()
{
	const TArray<FName> NodesToCheck = AllocatedNodes;
	for (const FName& NodeId : NodesToCheck)
	{
		if (!IsStartNode(NodeId))
		{
			ApplyRefund(NodeId);
		}
	}
}

const FPassiveNodeRow* UPassiveTreeComponent::FindNode(FName NodeId) const
{
	return NodeTable ? NodeTable->FindRow<FPassiveNodeRow>(NodeId, TEXT("PassiveTreeComponent"), false) : nullptr;
}

bool UPassiveTreeComponent::IsStartNode(FName NodeId) const
{
	const FPassiveNodeRow* Node = FindNode(NodeId);
	return Node && Node->Type == EPassiveNodeType::Start;
}

bool UPassiveTreeComponent::IsLinkedToAllocatedNode(FName NodeId) const
{
	const TArray<FName>* NodeLinks = Links.Find(NodeId);
	if (!NodeLinks) return false;

	for (const FName& LinkedNode : *NodeLinks)
	{
		if (IsNodeAllocated(LinkedNode))
		{
			return true;
		}
	}
	return false;
}

bool UPassiveTreeComponent::WouldStayConnectedWithout(FName NodeId) const
{
	// Walk outward from the Start nodes through allocated nodes, skipping the one being refunded.
	TSet<FName> Reached;
	TArray<FName> ToVisit;
	for (const FName& AllocatedNode : AllocatedNodes)
	{
		if (IsStartNode(AllocatedNode))
		{
			Reached.Add(AllocatedNode);
			ToVisit.Add(AllocatedNode);
		}
	}

	while (ToVisit.Num() > 0)
	{
		const FName Current = ToVisit.Pop();
		const TArray<FName>* NodeLinks = Links.Find(Current);
		if (!NodeLinks) continue;

		for (const FName& LinkedNode : *NodeLinks)
		{
			if (LinkedNode != NodeId && IsNodeAllocated(LinkedNode) && !Reached.Contains(LinkedNode))
			{
				Reached.Add(LinkedNode);
				ToVisit.Add(LinkedNode);
			}
		}
	}

	// Everything allocated except the refunded node must have been reached.
	return Reached.Num() == AllocatedNodes.Num() - 1;
}

void UPassiveTreeComponent::BuildLinks()
{
	Links.Reset();

	for (const FName& RowName : NodeTable->GetRowNames())
	{
		const FPassiveNodeRow* Node = FindNode(RowName);
		if (!Node) continue;

		for (const FName& LinkedNode : Node->LinkedNodes)
		{
			if (!FindNode(LinkedNode))
			{
				UE_LOG(LogTemp, Warning, TEXT("Passive node %s links to %s, which isn't in the table."), *RowName.ToString(), *LinkedNode.ToString());
				continue;
			}

			Links.FindOrAdd(RowName).AddUnique(LinkedNode);
			Links.FindOrAdd(LinkedNode).AddUnique(RowName);
		}
	}
}

void UPassiveTreeComponent::AllocateStartNodes()
{
	for (const FName& RowName : NodeTable->GetRowNames())
	{
		const FPassiveNodeRow* Node = FindNode(RowName);
		if (Node && Node->Type == EPassiveNodeType::Start && !IsNodeAllocated(RowName))
		{
			ApplyAllocation(RowName, *Node);
		}
	}
}

EPassiveNodeResult UPassiveTreeComponent::ApplyAllocation(FName NodeId, const FPassiveNodeRow& Node)
{
	UPlayerStatsComponent* Stats = FindStatsComponent();
	if (!Stats) return EPassiveNodeResult::MissingStatsComponent;

	AllocatedNodes.Add(NodeId);
	Stats->SetModifierSource(MakeSourceId(NodeId), Node.Modifiers);
	OnPassiveTreeChanged.Broadcast();
	return EPassiveNodeResult::Success;
}

EPassiveNodeResult UPassiveTreeComponent::ApplyRefund(FName NodeId)
{
	UPlayerStatsComponent* Stats = FindStatsComponent();
	if (!Stats) return EPassiveNodeResult::MissingStatsComponent;

	AllocatedNodes.Remove(NodeId);
	Stats->RemoveModifierSource(MakeSourceId(NodeId));
	OnPassiveTreeChanged.Broadcast();
	return EPassiveNodeResult::Success;
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
