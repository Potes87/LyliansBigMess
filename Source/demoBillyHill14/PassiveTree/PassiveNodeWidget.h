// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "PassiveTreeTypes.h"
#include "PassiveNodeWidget.generated.h"

class UImage;
class UPassiveTreeComponent;
class UPassiveTreeWidget;

/**
 * One clickable node on the tree screen. Left click allocates, right click refunds.
 * Make a Widget Blueprint from this class containing an Image named NodeImage, and optionally an Image named
 * StateRing that shows on allocated nodes.
 *
 * A node's color comes from its type and modifiers. Penalties are ignored and a gun-only bonus beats the stat it
 * changes, so new nodes color themselves. Brightness shows whether the node is allocated, available or locked.
 */
UCLASS(Abstract)
class DEMOBILLYHILL14_API UPassiveNodeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitNode(UPassiveTreeWidget* InOwnerTree, FName InNodeId, const FPassiveNodeRow& InRow);

	// Updates the colour and tooltip to match the tree's current state.
	void RefreshState(const UPassiveTreeComponent& Tree);

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Passive Node", meta = (BindWidget))
	TObjectPtr<UImage> NodeImage;

	// Shown on top of the node while it's allocated.
	UPROPERTY(BlueprintReadOnly, Category = "Passive Node", meta = (BindWidgetOptional))
	TObjectPtr<UImage> StateRing;

	// The color for each stat and gun tag, such as Item.Stat.Damage or Weapon.Type.SMG.
	UPROPERTY(EditDefaultsOnly, Category = "Passive Node|Colors", meta = (ForceInlineRow))
	TMap<FGameplayTag, FLinearColor> TagColors;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Node|Colors")
	FLinearColor StartColor = FLinearColor(0.8f, 0.8f, 0.8f);

	UPROPERTY(EditDefaultsOnly, Category = "Passive Node|Colors")
	FLinearColor KeystoneColor = FLinearColor(1.f, 0.65f, 0.1f);

	// For nodes that give bonuses to more than one stat.
	UPROPERTY(EditDefaultsOnly, Category = "Passive Node|Colors")
	FLinearColor MixedColor = FLinearColor(1.f, 0.35f, 0.7f);

	// For nodes whose bonuses have no entry in TagColors.
	UPROPERTY(EditDefaultsOnly, Category = "Passive Node|Colors")
	FLinearColor FallbackColor = FLinearColor(0.8f, 0.8f, 0.8f);

	// How much of the node's color shows when it can be allocated right now.
	UPROPERTY(EditDefaultsOnly, Category = "Passive Node|Colors", meta = (ClampMin = "0", ClampMax = "1"))
	float AvailableBrightness = 0.6f;

	// How much of the node's color shows when it can't be allocated yet.
	UPROPERTY(EditDefaultsOnly, Category = "Passive Node|Colors", meta = (ClampMin = "0", ClampMax = "1"))
	float LockedBrightness = 0.2f;

private:
	FLinearColor FindNodeColor() const;

	UPROPERTY()
	TObjectPtr<UPassiveTreeWidget> OwnerTree;

	FName NodeId;

	FLinearColor NodeColor = FLinearColor::White;

	// Points into the DataTable, which doesn't change during play.
	const FPassiveNodeRow* Row = nullptr;
};
