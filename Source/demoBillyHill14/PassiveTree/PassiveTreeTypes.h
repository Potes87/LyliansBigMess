// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Templates/SubclassOf.h"
#include "demoBillyHill14/Stats/StatTypes.h"
#include "PassiveTreeTypes.generated.h"

class UPassiveEffect;

UENUM(BlueprintType)
enum class EPassiveNodeType : uint8
{
	Start,
	Small,
	Notable,
	Keystone
};

/**
 * Why allocating or refunding a node did or didn't work.
 */
UENUM(BlueprintType)
enum class EPassiveNodeResult : uint8
{
	Success,
	NoNodeTable,
	NodeNotFound,
	AlreadyAllocated,
	NotAllocated,
	NoPointsAvailable,
	NotConnected,
	CannotRefundStart,
	WouldDisconnectTree,
	MissingStatsComponent
};

/**
 * One node on the passive tree. The row name is the node's permanent ID, so don't rename rows once saves exist.
 */
USTRUCT(BlueprintType)
struct DEMOBILLYHILL14_API FPassiveNodeRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	EPassiveNodeType Type = EPassiveNodeType::Small;

	// Where the node sits in the tree UI.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	FVector2D Position = FVector2D::ZeroVector;

	// Row names of the nodes this one connects to. Links work in both directions, even if only one row lists them.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	TArray<FName> LinkedNodes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	TArray<FStatModifier> Modifiers;

	// Optional behaviour that stat modifiers can't express, such as a keystone.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	TSubclassOf<UPassiveEffect> Effect;

	// Optional tooltip lines explaining the Effect, shown after the modifiers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node", meta = (MultiLine = true))
	FText Description;
};
