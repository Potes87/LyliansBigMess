// Fill out your copyright notice in the Description page of Project Settings.

#include "RuneTreeComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NativeGameplayTags.h"
#include "demoBillyHill14/Wallet/WalletSubsystem.h"

namespace
{
	UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Currency_Rune, "Currency.RuneOfSeeking");
}

URuneTreeComponent::URuneTreeComponent()
{
	SaveSlotName = TEXT("RuneTree");
}

void URuneTreeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (NodeTable)
	{
		LoadRuneTree();
	}
}

int32 URuneTreeComponent::GetNextPointCost() const
{
	return FMath::Min(MaxPointCost, BasePointCost + PointCostStep * BoughtPoints);
}

int32 URuneTreeComponent::GetRuneBalance() const
{
	const UWalletSubsystem* Wallet = GetWallet();
	return Wallet ? Wallet->GetBalance(TAG_Currency_Rune) : 0;
}

int32 URuneTreeComponent::GetUnclaimedNodeCount() const
{
	int32 Count = 0;
	for (const FName& NodeId : GetNodeIds())
	{
		const FPassiveNodeRow* Node = FindNode(NodeId);
		if (Node && Node->Type != EPassiveNodeType::Start && !Node->bSealed && !IsNodeAllocated(NodeId))
		{
			++Count;
		}
	}
	return Count;
}

bool URuneTreeComponent::CanBuyPoint() const
{
	const UWalletSubsystem* Wallet = GetWallet();
	return Wallet && GetAvailablePoints() < GetUnclaimedNodeCount() && Wallet->CanAfford(TAG_Currency_Rune, GetNextPointCost());
}

bool URuneTreeComponent::BuyPoint()
{
	UWalletSubsystem* Wallet = GetWallet();
	if (!CanBuyPoint() || !Wallet->SpendCurrency(TAG_Currency_Rune, GetNextPointCost())) return false;

	++BoughtPoints;
	SaveRuneTree();
	OnPassiveTreeChanged.Broadcast();
	return true;
}

void URuneTreeComponent::DebugResetRuneTree()
{
	BoughtPoints = 0;
	ResetAllNodes();
	DebugDeleteSave();
	OnPassiveTreeChanged.Broadcast();
}

void URuneTreeComponent::OnNodeAllocated(FName NodeId)
{
	SaveRuneTree();
}

UWalletSubsystem* URuneTreeComponent::GetWallet() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UWalletSubsystem>() : nullptr;
}

void URuneTreeComponent::LoadRuneTree()
{
	const URuneTreeSaveGame* SaveGame = Cast<URuneTreeSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!SaveGame) return;

	BoughtPoints = FMath::Max(0, SaveGame->BoughtPoints);
	RestoreNodes(SaveGame->AllocatedNodes);
	OnPassiveTreeChanged.Broadcast();
}

void URuneTreeComponent::SaveRuneTree() const
{
	URuneTreeSaveGame* SaveGame = Cast<URuneTreeSaveGame>(UGameplayStatics::CreateSaveGameObject(URuneTreeSaveGame::StaticClass()));
	if (!SaveGame) return;

	SaveGame->BoughtPoints = BoughtPoints;
	SaveGame->AllocatedNodes = AllocatedNodes;
	if (!UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlotName, 0))
	{
		UE_LOG(LogTemp, Warning, TEXT("RuneTreeComponent couldn't write save slot %s."), *SaveSlotName);
	}
}
