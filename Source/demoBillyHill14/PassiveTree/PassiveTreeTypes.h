// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "demoBillyHill14/Stats/StatTypes.h"
#include "PassiveTreeTypes.generated.h"

UENUM(BlueprintType)
enum class EPassiveNodeType : uint8
{
	Start,
	Small,
	Notable,
	Keystone
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

	// Row names of the nodes this one connects to.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	TArray<FName> LinkedNodes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Node")
	TArray<FStatModifier> Modifiers;
};
