// Fill out your copyright notice in the Description page of Project Settings.

#include "PassiveTreeComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "PassiveEffect.h"
#include "PlayerProgressSaveGame.h"
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

	const EPassiveNodeResult AppliedResult = ApplyAllocation(NodeId, *FindNode(NodeId));
	if (AppliedResult == EPassiveNodeResult::Success)
	{
		OnPassiveTreeChanged.Broadcast();
	}
	return AppliedResult;
}

EPassiveNodeResult UPassiveTreeComponent::RefundNode(FName NodeId)
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

void UPassiveTreeComponent::RefundAllNodes()
{
	RefundAllWithoutBroadcast();
	OnPassiveTreeChanged.Broadcast();
}

bool UPassiveTreeComponent::SaveProgress(float CurrentXP)
{
	UPlayerProgressSaveGame* SaveGame = Cast<UPlayerProgressSaveGame>(UGameplayStatics::CreateSaveGameObject(UPlayerProgressSaveGame::StaticClass()));
	if (!SaveGame) return false;

	SaveGame->PlayerLevel = PlayerLevel;
	SaveGame->CurrentXP = CurrentXP;
	SaveGame->AllocatedNodes = AllocatedNodes;

	const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlotName, 0);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Warning, TEXT("PassiveTreeComponent couldn't write save slot %s."), *SaveSlotName);
	}
	return bSaved;
}

bool UPassiveTreeComponent::LoadProgress(int32& Level, float& CurrentXP)
{
	Level = PlayerLevel;
	CurrentXP = 0.f;

	const UPlayerProgressSaveGame* SaveGame = Cast<UPlayerProgressSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!SaveGame) return false;

	Level = FMath::Max(1, SaveGame->PlayerLevel);
	CurrentXP = SaveGame->CurrentXP;
	PlayerLevel = Level;

	RefundAllWithoutBroadcast();
	RestoreNodes(SaveGame->AllocatedNodes);
	OnPassiveTreeChanged.Broadcast();
	return true;
}

void UPassiveTreeComponent::DebugDeleteSave()
{
	UGameplayStatics::DeleteGameInSlot(SaveSlotName, 0);
}

bool UPassiveTreeComponent::IsNodeAllocated(FName NodeId) const
{
	return AllocatedNodes.Contains(NodeId);
}

TArray<FName> UPassiveTreeComponent::GetNodeIds() const
{
	return NodeTable ? NodeTable->GetRowNames() : TArray<FName>();
}

TArray<FName> UPassiveTreeComponent::GetLinkedNodes(FName NodeId) const
{
	const TArray<FName>* NodeLinks = Links.Find(NodeId);
	return NodeLinks ? *NodeLinks : TArray<FName>();
}

float UPassiveTreeComponent::NotifyShotFired(float Damage)
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		Damage = ActiveEffect.Value->OnShotFired(Damage);
	}
	return Damage;
}

float UPassiveTreeComponent::NotifyEnemyHit(AActor* Enemy, float Damage)
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		Damage = ActiveEffect.Value->OnEnemyHit(Enemy, Damage);
	}
	return Damage;
}

void UPassiveTreeComponent::NotifyReloaded()
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		ActiveEffect.Value->OnReloaded();
	}
}

bool UPassiveTreeComponent::CanFireShot() const
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		if (!ActiveEffect.Value->CanFireShot()) return false;
	}
	return true;
}

int32 UPassiveTreeComponent::GetAmmoCost() const
{
	int32 Cost = 1;
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		Cost = ActiveEffect.Value->GetAmmoCost(Cost);
	}
	return FMath::Max(0, Cost);
}

bool UPassiveTreeComponent::CanReload() const
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		if (!ActiveEffect.Value->CanReload()) return false;
	}
	return true;
}

int32 UPassiveTreeComponent::TakeRegeneratedAmmo(float DeltaSeconds, int32 MagazineSize)
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		PendingAmmo += ActiveEffect.Value->GetAmmoRegen(DeltaSeconds, MagazineSize);
	}

	const int32 WholeRounds = FMath::FloorToInt(PendingAmmo);
	PendingAmmo -= WholeRounds;
	return WholeRounds;
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
	RefundAllNodes();
}

void UPassiveTreeComponent::RefundAllWithoutBroadcast()
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

void UPassiveTreeComponent::RestoreNodes(const TArray<FName>& SavedNodes)
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
			UE_LOG(LogTemp, Warning, TEXT("Saved passive node %s couldn't be restored."), *NodeId.ToString());
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
	OnPassiveTreeChanged.Broadcast();
}

EPassiveNodeResult UPassiveTreeComponent::ApplyAllocation(FName NodeId, const FPassiveNodeRow& Node)
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

EPassiveNodeResult UPassiveTreeComponent::ApplyRefund(FName NodeId)
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
