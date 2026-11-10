// Fill out your copyright notice in the Description page of Project Settings.

#include "SkillTreeComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "PassiveEffect.h"
#include "demoBillyHill14/Stats/PlayerStatsComponent.h"

USkillTreeComponent::USkillTreeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USkillTreeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!NodeTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s on %s has no NodeTable assigned."), *GetName(), *GetNameSafe(GetOwner()));
		return;
	}

	BuildLinks();
	AllocateStartNodes();
}

int32 USkillTreeComponent::GetSpentPoints() const
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

int32 USkillTreeComponent::GetAvailablePoints() const
{
	return GetTotalPoints() - GetSpentPoints();
}

EPassiveNodeResult USkillTreeComponent::CanAllocateNode(FName NodeId) const
{
	if (!NodeTable) return EPassiveNodeResult::NoNodeTable;
	const FPassiveNodeRow* Node = FindNode(NodeId);
	if (!Node) return EPassiveNodeResult::NodeNotFound;
	if (IsNodeAllocated(NodeId)) return EPassiveNodeResult::AlreadyAllocated;
	if (Node->bSealed) return EPassiveNodeResult::Sealed;
	if (GetAvailablePoints() <= 0) return EPassiveNodeResult::NoPointsAvailable;
	if (!IsLinkedToAllocatedNode(NodeId)) return EPassiveNodeResult::NotConnected;

	return EPassiveNodeResult::Success;
}

EPassiveNodeResult USkillTreeComponent::CanRefundNode(FName NodeId) const
{
	if (!NodeTable) return EPassiveNodeResult::NoNodeTable;
	if (!FindNode(NodeId)) return EPassiveNodeResult::NodeNotFound;
	if (!IsNodeAllocated(NodeId)) return EPassiveNodeResult::NotAllocated;
	if (IsStartNode(NodeId)) return EPassiveNodeResult::CannotRefundStart;
	if (!AllowsRefunds()) return EPassiveNodeResult::RefundsDisabled;
	if (!WouldStayConnectedWithout(NodeId)) return EPassiveNodeResult::WouldDisconnectTree;

	return EPassiveNodeResult::Success;
}

EPassiveNodeResult USkillTreeComponent::AllocateNode(FName NodeId)
{
	const EPassiveNodeResult Result = CanAllocateNode(NodeId);
	if (Result != EPassiveNodeResult::Success) return Result;

	const EPassiveNodeResult AppliedResult = ApplyAllocation(NodeId, *FindNode(NodeId));
	if (AppliedResult == EPassiveNodeResult::Success)
	{
		OnNodeAllocated(NodeId);
		OnPassiveTreeChanged.Broadcast();
	}
	return AppliedResult;
}

EPassiveNodeResult USkillTreeComponent::RefundNode(FName NodeId)
{
	const EPassiveNodeResult Result = CanRefundNode(NodeId);
	if (Result != EPassiveNodeResult::Success) return Result;

	const EPassiveNodeResult AppliedResult = ApplyRefund(NodeId);
	if (AppliedResult == EPassiveNodeResult::Success)
	{
		OnPassiveTreeChanged.Broadcast();
	}
	return AppliedResult;
}

void USkillTreeComponent::RefundAllNodes()
{
	if (!AllowsRefunds()) return;

	RefundAllWithoutBroadcast();
	OnPassiveTreeChanged.Broadcast();
}

void USkillTreeComponent::DebugDeleteSave()
{
	UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
}

bool USkillTreeComponent::IsNodeAllocated(FName NodeId) const
{
	return AllocatedNodes.Contains(NodeId);
}

TArray<FName> USkillTreeComponent::GetNodeIds() const
{
	return NodeTable ? NodeTable->GetRowNames() : TArray<FName>();
}

TArray<FName> USkillTreeComponent::GetLinkedNodes(FName NodeId) const
{
	const TArray<FName>* NodeLinks = Links.Find(NodeId);
	return NodeLinks ? *NodeLinks : TArray<FName>();
}

FName USkillTreeComponent::DebugAllocateNextNode()
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

void USkillTreeComponent::DebugRefundAllNodes()
{
	RefundAllNodes();
}

void USkillTreeComponent::RefundAllWithoutBroadcast()
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

void USkillTreeComponent::ResetAllNodes()
{
	RefundAllWithoutBroadcast();
	if (NodeTable)
	{
		AllocateStartNodes();
	}
}

void USkillTreeComponent::RestoreNodes(const TArray<FName>& SavedNodes)
{
	// A refund through a loop can save a node before the one connecting it, so keep passing over the list until
	// nothing more can be allocated. The normal rules apply, so an edited table can't produce an invalid tree.
	TArray<FName> PendingNodes = SavedNodes;
	int32 NumRemoved;
	do
	{
		NumRemoved = PendingNodes.RemoveAll([this](const FName& NodeId)
		{
			if (CanAllocateNode(NodeId) != EPassiveNodeResult::Success) return false;

			ApplyAllocation(NodeId, *FindNode(NodeId));
			return true;
		});
	}
	while (NumRemoved > 0);

	for (const FName& NodeId : PendingNodes)
	{
		if (!IsNodeAllocated(NodeId))
		{
			UE_LOG(LogTemp, Warning, TEXT("Saved node %s on %s couldn't be restored."), *NodeId.ToString(), *GetName());
		}
	}
}

const FPassiveNodeRow* USkillTreeComponent::FindNode(FName NodeId) const
{
	return NodeTable ? NodeTable->FindRow<FPassiveNodeRow>(NodeId, TEXT("SkillTreeComponent"), false) : nullptr;
}

bool USkillTreeComponent::IsStartNode(FName NodeId) const
{
	const FPassiveNodeRow* Node = FindNode(NodeId);
	return Node && Node->Type == EPassiveNodeType::Start;
}

bool USkillTreeComponent::IsLinkedToAllocatedNode(FName NodeId) const
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

bool USkillTreeComponent::WouldStayConnectedWithout(FName NodeId) const
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

void USkillTreeComponent::BuildLinks()
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
				UE_LOG(LogTemp, Warning, TEXT("Node %s links to %s, which isn't in the table."), *RowName.ToString(), *LinkedNode.ToString());
				continue;
			}

			Links.FindOrAdd(RowName).AddUnique(LinkedNode);
			Links.FindOrAdd(LinkedNode).AddUnique(RowName);
		}
	}
}

void USkillTreeComponent::AllocateStartNodes()
{
	for (const FName& RowName : NodeTable->GetRowNames())
	{
		const FPassiveNodeRow* Node = FindNode(RowName);
		if (Node && Node->Type == EPassiveNodeType::Start && !IsNodeAllocated(RowName))
		{
			ApplyAllocation(RowName, *Node);
		}
	}
	OnPassiveTreeChanged.Broadcast();
}

EPassiveNodeResult USkillTreeComponent::ApplyAllocation(FName NodeId, const FPassiveNodeRow& Node)
{
	UPlayerStatsComponent* Stats = FindStatsComponent();
	if (!Stats) return EPassiveNodeResult::MissingStatsComponent;

	AllocatedNodes.Add(NodeId);
	Stats->SetModifierSource(MakeSourceId(NodeId), Node.Modifiers);

	if (Node.Effect)
	{
		UPassiveEffect* Effect = NewObject<UPassiveEffect>(this, Node.Effect);
		ActiveEffects.Add(NodeId, Effect);
		Effect->OnActivated();
	}
	return EPassiveNodeResult::Success;
}

EPassiveNodeResult USkillTreeComponent::ApplyRefund(FName NodeId)
{
	UPlayerStatsComponent* Stats = FindStatsComponent();
	if (!Stats) return EPassiveNodeResult::MissingStatsComponent;

	AllocatedNodes.Remove(NodeId);
	Stats->RemoveModifierSource(MakeSourceId(NodeId));

	TObjectPtr<UPassiveEffect> Effect;
	if (ActiveEffects.RemoveAndCopyValue(NodeId, Effect))
	{
		Effect->OnDeactivated();
	}
	return EPassiveNodeResult::Success;
}

UPlayerStatsComponent* USkillTreeComponent::FindStatsComponent() const
{
	const AActor* Owner = GetOwner();
	UPlayerStatsComponent* Stats = Owner ? Owner->FindComponentByClass<UPlayerStatsComponent>() : nullptr;
	if (!Stats)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s on %s needs a PlayerStatsComponent on the same actor."), *GetName(), *GetNameSafe(Owner));
	}
	return Stats;
}

FName USkillTreeComponent::MakeSourceId(FName NodeId) const
{
	return FName(*FString::Printf(TEXT("%s.%s"), *GetName(), *NodeId.ToString()));
}
