// Fill out your copyright notice in the Description page of Project Settings.

#include "PassiveTreeWidget.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "PassiveNodeWidget.h"
#include "PassiveTreeComponent.h"
#include "Rendering/DrawElementTypes.h"

#define LOCTEXT_NAMESPACE "PassiveTree"

void UPassiveTreeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// User Widgets ignore the mouse and keyboard by default, and this one needs both.
	SetVisibility(ESlateVisibility::Visible);
	SetIsFocusable(true);
}

void UPassiveTreeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	const APawn* Pawn = GetOwningPlayerPawn();
	Tree = Pawn ? Pawn->FindComponentByClass<UPassiveTreeComponent>() : nullptr;
	if (!Tree)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s couldn't find a PassiveTreeComponent on the player's pawn."), *GetName());
		return;
	}

	// Construct runs again if this widget is reused, and the nodes only need creating once.
	if (NodeWidgets.Num() == 0)
	{
		BuildTree();
	}
	ApplyLayout();
	RefreshTree();
	Tree->OnPassiveTreeChanged.AddUniqueDynamic(this, &UPassiveTreeWidget::RefreshTree);
}

void UPassiveTreeWidget::NativeDestruct()
{
	if (Tree)
	{
		Tree->OnPassiveTreeChanged.RemoveDynamic(this, &UPassiveTreeWidget::RefreshTree);
	}
	Super::NativeDestruct();
}

void UPassiveTreeWidget::CloseTree()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->SetShowMouseCursor(false);
	}
	RemoveFromParent();
}

void UPassiveTreeWidget::AllocateFromScreen(FName NodeId)
{
	if (Tree)
	{
		ShowResult(Tree->AllocateNode(NodeId));
	}
}

void UPassiveTreeWidget::RefundFromScreen(FName NodeId)
{
	if (Tree)
	{
		ShowResult(Tree->RefundNode(NodeId));
	}
}

FReply UPassiveTreeWidget::MakeFocusReply()
{
	return FReply::Handled().SetUserFocus(TakeWidget(), EFocusCause::Mouse);
}

FText UPassiveTreeWidget::GetResultText(EPassiveNodeResult Result)
{
	switch (Result)
	{
	case EPassiveNodeResult::Success: return FText::GetEmpty();
	case EPassiveNodeResult::NoNodeTable: return LOCTEXT("NoNodeTable", "The tree has no node table");
	case EPassiveNodeResult::NodeNotFound: return LOCTEXT("NodeNotFound", "That node doesn't exist");
	case EPassiveNodeResult::AlreadyAllocated: return LOCTEXT("AlreadyAllocated", "Already allocated");
	case EPassiveNodeResult::NotAllocated: return LOCTEXT("NotAllocated", "Not allocated");
	case EPassiveNodeResult::NoPointsAvailable: return LOCTEXT("NoPoints", "No points available");
	case EPassiveNodeResult::NotConnected: return LOCTEXT("NotConnected", "Not connected to a node you own");
	case EPassiveNodeResult::CannotRefundStart: return LOCTEXT("CannotRefundStart", "The starting node can't be refunded");
	case EPassiveNodeResult::WouldDisconnectTree: return LOCTEXT("WouldDisconnect", "Other nodes depend on this one");
	case EPassiveNodeResult::MissingStatsComponent: return LOCTEXT("MissingStats", "The player has no stats component");
	}
	return FText::GetEmpty();
}

void UPassiveTreeWidget::BuildTree()
{
	if (!NodeWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s has no NodeWidgetClass assigned."), *GetName());
		return;
	}

	for (const FName& NodeId : Tree->GetNodeIds())
	{
		const FPassiveNodeRow* Row = Tree->FindNode(NodeId);
		if (!Row) continue;

		UPassiveNodeWidget* NodeWidget = CreateWidget<UPassiveNodeWidget>(this, NodeWidgetClass);
		if (!NodeWidget) continue;

		NodeWidget->InitNode(this, NodeId, *Row);
		NodeWidgets.Add(NodeWidget);

		if (UCanvasPanelSlot* CanvasSlot = TreeCanvas->AddChildToCanvas(NodeWidget))
		{
			// Anchored to the middle, so a Position of (0,0) stays centred at any resolution.
			CanvasSlot->SetAnchors(FAnchors(0.5f));
			CanvasSlot->SetAlignment(FVector2D(0.5, 0.5));
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetZOrder(1);
		}

		FTreeNodeLayout& Layout = NodeLayouts.Add(NodeId);
		Layout.Widget = NodeWidget;
		Layout.Position = Row->Position;
		Layout.Size = GetNodeSize(Row->Type);
	}

	// Links are stored in both directions, so only keep each pair once.
	for (const TPair<FName, FTreeNodeLayout>& Node : NodeLayouts)
	{
		for (const FName& LinkedNode : Tree->GetLinkedNodes(Node.Key))
		{
			const FTreeNodeLayout* LinkedLayout = NodeLayouts.Find(LinkedNode);
			if (!LinkedLayout || !Node.Key.FastLess(LinkedNode)) continue;

			FTreeLine& Line = Lines.AddDefaulted_GetRef();
			Line.NodeA = Node.Key;
			Line.NodeB = LinkedNode;
			Line.PositionA = Node.Value.Position;
			Line.PositionB = LinkedLayout->Position;
			Line.RadiusA = Node.Value.Size * 0.5f;
			Line.RadiusB = LinkedLayout->Size * 0.5f;
		}
	}
}

void UPassiveTreeWidget::ApplyLayout()
{
	for (const TPair<FName, FTreeNodeLayout>& Node : NodeLayouts)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Node.Value.Widget->Slot))
		{
			CanvasSlot->SetPosition(PanOffset + Node.Value.Position * Zoom);
			CanvasSlot->SetSize(FVector2D(Node.Value.Size * Zoom));
		}
	}
	InvalidateLayoutAndVolatility();
}

void UPassiveTreeWidget::RefreshTree()
{
	if (!Tree) return;

	for (UPassiveNodeWidget* NodeWidget : NodeWidgets)
	{
		NodeWidget->RefreshState(*Tree);
	}

	for (FTreeLine& Line : Lines)
	{
		const bool bOwnsA = Tree->IsNodeAllocated(Line.NodeA);
		const bool bOwnsB = Tree->IsNodeAllocated(Line.NodeB);
		const bool bCanExtend = (bOwnsA && Tree->CanAllocateNode(Line.NodeB) == EPassiveNodeResult::Success)
			|| (bOwnsB && Tree->CanAllocateNode(Line.NodeA) == EPassiveNodeResult::Success);

		Line.Color = bOwnsA && bOwnsB ? AllocatedLineColor : bCanExtend ? AvailableLineColor : LockedLineColor;
	}

	if (PointsText)
	{
		PointsText->SetText(FText::Format(LOCTEXT("Points", "Passive points: {0}"), Tree->GetAvailablePoints()));
	}
	InvalidateLayoutAndVolatility();
}

void UPassiveTreeWidget::ShowResult(EPassiveNodeResult Result)
{
	if (StatusText)
	{
		StatusText->SetText(GetResultText(Result));
	}
}

float UPassiveTreeWidget::GetNodeSize(EPassiveNodeType Type) const
{
	switch (Type)
	{
	case EPassiveNodeType::Start: return StartNodeSize;
	case EPassiveNodeType::Notable: return NotableNodeSize;
	case EPassiveNodeType::Keystone: return KeystoneNodeSize;
	default: return SmallNodeSize;
	}
}

int32 UPassiveTreeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	const int32 LineLayer = MaxLayer + 1;

	const FVector2D Centre = FVector2D(AllottedGeometry.GetLocalSize()) * 0.5 + PanOffset;
	const FPaintGeometry PaintGeometry = AllottedGeometry.ToPaintGeometry();

	for (const FTreeLine& Line : Lines)
	{
		const FVector2D A = Centre + Line.PositionA * Zoom;
		const FVector2D B = Centre + Line.PositionB * Zoom;
		const FVector2D Delta = B - A;
		const double Length = Delta.Size();
		const double RadiusA = Line.RadiusA * Zoom;
		const double RadiusB = Line.RadiusB * Zoom;

		// These paint above the nodes, so stop each line at the edge of its nodes.
		if (Length <= RadiusA + RadiusB) continue;

		const FVector2D Direction = Delta / Length;
		TArray<FVector2f> Points;
		Points.Add(FVector2f(A + Direction * RadiusA));
		Points.Add(FVector2f(B - Direction * RadiusB));

		FSlateDrawElement::MakeLines(OutDrawElements, LineLayer, PaintGeometry, MoveTemp(Points), ESlateDrawEffect::None, Line.Color, true, LineThickness);
	}
	return LineLayer;
}

FReply UPassiveTreeWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Holding the key that opened the screen shouldn't close it again.
	if (InKeyEvent.GetKey() == CloseKey && !InKeyEvent.IsRepeat())
	{
		CloseTree();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UPassiveTreeWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FKey Button = InMouseEvent.GetEffectingButton();
	if (Button != EKeys::LeftMouseButton && Button != EKeys::RightMouseButton)
	{
		return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
	}

	bIsPanning = true;
	return MakeFocusReply().CaptureMouse(TakeWidget());
}

FReply UPassiveTreeWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bIsPanning)
	{
		return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
	}

	bIsPanning = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply UPassiveTreeWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bIsPanning)
	{
		return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
	}

	// The cursor delta is in screen pixels, and the layout uses DPI-scaled units.
	PanOffset += FVector2D(InMouseEvent.GetCursorDelta()) / InGeometry.GetAccumulatedLayoutTransform().GetScale();
	ApplyLayout();
	return FReply::Handled();
}

FReply UPassiveTreeWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const float NewZoom = FMath::Clamp(Zoom * (1.f + ZoomStep * InMouseEvent.GetWheelDelta()), MinZoom, MaxZoom);
	if (FMath::IsNearlyEqual(NewZoom, Zoom)) return FReply::Handled();

	// Keep whatever is under the cursor in the same place on screen.
	const FVector2D CursorFromCentre = FVector2D(InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition())) - FVector2D(InGeometry.GetLocalSize()) * 0.5;
	PanOffset = CursorFromCentre - (CursorFromCentre - PanOffset) * (NewZoom / Zoom);
	Zoom = NewZoom;

	ApplyLayout();
	return FReply::Handled();
}

void UPassiveTreeWidget::NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent)
{
	bIsPanning = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

#undef LOCTEXT_NAMESPACE
