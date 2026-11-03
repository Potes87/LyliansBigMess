// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PassiveTreeTypes.h"
#include "PassiveNodeWidget.generated.h"

class UImage;
class UPassiveTreeComponent;
class UPassiveTreeWidget;

/**
 * One clickable node on the tree screen. Left click allocates, right click refunds.
 * Make a Widget Blueprint from this class containing an Image named NodeImage.
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

	UPROPERTY(EditDefaultsOnly, Category = "Passive Node")
	FLinearColor AllocatedColor = FLinearColor(1.f, 0.7f, 0.15f);

	// The node can be allocated right now.
	UPROPERTY(EditDefaultsOnly, Category = "Passive Node")
	FLinearColor AvailableColor = FLinearColor(0.2f, 0.75f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Passive Node")
	FLinearColor LockedColor = FLinearColor(0.2f, 0.2f, 0.22f);

private:
	UPROPERTY()
	TObjectPtr<UPassiveTreeWidget> OwnerTree;

	FName NodeId;

	// Points into the DataTable, which doesn't change during play.
	const FPassiveNodeRow* Row = nullptr;
};
