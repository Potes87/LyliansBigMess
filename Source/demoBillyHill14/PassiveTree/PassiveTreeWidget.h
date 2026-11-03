// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "PassiveTreeTypes.h"
#include "PassiveTreeWidget.generated.h"

class UButton;
class UCanvasPanel;
class UPassiveNodeWidget;
class UPassiveTreeComponent;
class UTextBlock;

/**
 * The full-screen passive tree. Builds a node for every row in the player's tree, places it using the
 * row's Position, and draws the links between nodes. Drag the background to pan, and scroll to zoom.
 *
 * Make a Widget Blueprint from this class with a Canvas Panel named TreeCanvas as its root.
 * Create it, add it to the viewport and switch to UI input to open it. It closes itself with CloseKey.
 */
UCLASS(Abstract)
class DEMOBILLYHILL14_API UPassiveTreeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Passive Tree")
	void CloseTree();

	// Called by node widgets when they're clicked.
	void AllocateFromScreen(FName NodeId);
	void RefundFromScreen(FName NodeId);

	// A mouse reply that keeps keyboard focus here, so CloseKey still works after clicking.
	FReply MakeFocusReply();

	static FText GetResultText(EPassiveNodeResult Result);

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Passive Tree", meta = (BindWidget))
	TObjectPtr<UCanvasPanel> TreeCanvas;

	// Optional: shows the available points.
	UPROPERTY(BlueprintReadOnly, Category = "Passive Tree", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PointsText;

	// Optional: shows why a click was refused.
	UPROPERTY(BlueprintReadOnly, Category = "Passive Tree", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

	// Optional: refunds every node when clicked.
	UPROPERTY(BlueprintReadOnly, Category = "Passive Tree", meta = (BindWidgetOptional))
	TObjectPtr<UButton> RefundAllButton;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree")
	TSubclassOf<UPassiveNodeWidget> NodeWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree")
	FKey CloseKey = EKeys::N;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Layout")
	float StartNodeSize = 64.f;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Layout")
	float SmallNodeSize = 44.f;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Layout")
	float NotableNodeSize = 64.f;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Layout")
	float KeystoneNodeSize = 88.f;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Layout", meta = (ClampMin = "0.05"))
	float MinZoom = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Layout", meta = (ClampMin = "0.05"))
	float MaxZoom = 2.f;

	// How much one notch of the scroll wheel zooms: 0.1 is 10%.
	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Layout", meta = (ClampMin = "0.01"))
	float ZoomStep = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Lines")
	float LineThickness = 3.f;

	// Both ends are allocated.
	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Lines")
	FLinearColor AllocatedLineColor = FLinearColor(1.f, 0.7f, 0.15f);

	// One end is allocated and the other can be allocated right now.
	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Lines")
	FLinearColor AvailableLineColor = FLinearColor(0.2f, 0.75f, 1.f);

	UPROPERTY(EditDefaultsOnly, Category = "Passive Tree|Lines")
	FLinearColor LockedLineColor = FLinearColor(0.2f, 0.2f, 0.22f);

private:
	struct FTreeNodeLayout
	{
		TObjectPtr<UPassiveNodeWidget> Widget;
		FVector2D Position = FVector2D::ZeroVector;
		float Size = 0.f;
	};

	struct FTreeLine
	{
		FName NodeA;
		FName NodeB;
		FVector2D PositionA = FVector2D::ZeroVector;
		FVector2D PositionB = FVector2D::ZeroVector;
		float RadiusA = 0.f;
		float RadiusB = 0.f;
		FLinearColor Color = FLinearColor::White;
	};

	UFUNCTION()
	void RefreshTree();

	UFUNCTION()
	void HandleRefundAllClicked();

	void BuildTree();
	void ApplyLayout();
	void ShowResult(EPassiveNodeResult Result);
	float GetNodeSize(EPassiveNodeType Type) const;

	UPROPERTY()
	TObjectPtr<UPassiveTreeComponent> Tree;

	// Keeps the node widgets alive. Their positions are in NodeLayouts.
	UPROPERTY()
	TArray<TObjectPtr<UPassiveNodeWidget>> NodeWidgets;

	TMap<FName, FTreeNodeLayout> NodeLayouts;
	TArray<FTreeLine> Lines;

	// How far the tree has been dragged from the middle of the screen, where (0,0) starts.
	FVector2D PanOffset = FVector2D::ZeroVector;
	float Zoom = 1.f;
	bool bIsPanning = false;
};
