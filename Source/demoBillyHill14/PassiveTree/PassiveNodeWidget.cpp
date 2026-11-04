// Fill out your copyright notice in the Description page of Project Settings.

#include "PassiveNodeWidget.h"
#include "Components/Image.h"
#include "PassiveTreeComponent.h"
#include "PassiveTreeWidget.h"

#define LOCTEXT_NAMESPACE "PassiveTree"

namespace
{
	// "Item.Stat.ReloadSpeed" becomes "Reload Speed".
	FString GetTagDisplayName(const FGameplayTag& Tag)
	{
		FString Name = Tag.ToString();
		int32 LastDot;
		if (Name.FindLastChar(TEXT('.'), LastDot))
		{
			Name.RightChopInline(LastDot + 1);
		}
		return FName::NameToDisplayString(Name, false);
	}

	FString DescribeModifier(const FStatModifier& Modifier)
	{
		const FString Amount = FText::AsNumber(FMath::Abs(Modifier.Value)).ToString();
		const FString StatName = GetTagDisplayName(Modifier.Stat);
		const bool bIsNegative = Modifier.Value < 0.f;

		FString Description;
		switch (Modifier.Op)
		{
		case EModifierOp::Flat:
			Description = FString::Printf(TEXT("%s%s %s"), bIsNegative ? TEXT("-") : TEXT("+"), *Amount, *StatName);
			break;
		case EModifierOp::Increased:
			Description = FString::Printf(TEXT("%s%% %s %s"), *Amount, bIsNegative ? TEXT("reduced") : TEXT("increased"), *StatName);
			break;
		case EModifierOp::More:
			Description = FString::Printf(TEXT("%s%% %s %s"), *Amount, bIsNegative ? TEXT("less") : TEXT("more"), *StatName);
			break;
		}

		for (const FGameplayTag& RequiredTag : Modifier.RequiredWeaponTags)
		{
			Description += FString::Printf(TEXT(" (%s only)"), *GetTagDisplayName(RequiredTag));
		}
		return Description;
	}
}

void UPassiveNodeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// User Widgets ignore the mouse by default, and nodes need to be clicked.
	SetVisibility(ESlateVisibility::Visible);
}

void UPassiveNodeWidget::InitNode(UPassiveTreeWidget* InOwnerTree, FName InNodeId, const FPassiveNodeRow& InRow)
{
	OwnerTree = InOwnerTree;
	NodeId = InNodeId;
	Row = &InRow;
	NodeColor = FindNodeColor();
}

FLinearColor UPassiveNodeWidget::FindNodeColor() const
{
	if (Row->Type == EPassiveNodeType::Start) return StartColor;
	if (Row->Type == EPassiveNodeType::Keystone) return KeystoneColor;

	TArray<FGameplayTag> BoostedStats;
	for (const FStatModifier& Modifier : Row->Modifiers)
	{
		if (Modifier.Value <= 0.f) continue;

		for (const FGameplayTag& RequiredTag : Modifier.RequiredWeaponTags)
		{
			if (const FLinearColor* GunColor = TagColors.Find(RequiredTag)) return *GunColor;
		}
		BoostedStats.AddUnique(Modifier.Stat);
	}

	if (BoostedStats.Num() > 1) return MixedColor;
	if (BoostedStats.Num() == 1)
	{
		if (const FLinearColor* StatColor = TagColors.Find(BoostedStats[0])) return *StatColor;
	}
	return FallbackColor;
}

void UPassiveNodeWidget::RefreshState(const UPassiveTreeComponent& Tree)
{
	if (!Row) return;

	const bool bIsAllocated = Tree.IsNodeAllocated(NodeId);
	const EPassiveNodeResult AllocateResult = Tree.CanAllocateNode(NodeId);

	const float Brightness = bIsAllocated ? 1.f : AllocateResult == EPassiveNodeResult::Success ? AvailableBrightness : LockedBrightness;
	NodeImage->SetColorAndOpacity(FLinearColor(NodeColor.R * Brightness, NodeColor.G * Brightness, NodeColor.B * Brightness, NodeColor.A));

	if (StateRing)
	{
		StateRing->SetVisibility(bIsAllocated ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	FString Tooltip = Row->DisplayName.ToString();
	if (Row->Type == EPassiveNodeType::Notable || Row->Type == EPassiveNodeType::Keystone)
	{
		Tooltip += FString::Printf(TEXT(" (%s)"), *UEnum::GetDisplayValueAsText(Row->Type).ToString());
	}
	for (const FStatModifier& Modifier : Row->Modifiers)
	{
		Tooltip += TEXT("\n") + DescribeModifier(Modifier);
	}
	if (!Row->Description.IsEmpty())
	{
		Tooltip += TEXT("\n") + Row->Description.ToString();
	}

	Tooltip += TEXT("\n\n");
	if (Row->Type == EPassiveNodeType::Start)
	{
		Tooltip += LOCTEXT("StartHint", "Your starting point").ToString();
	}
	else if (bIsAllocated)
	{
		Tooltip += LOCTEXT("RefundHint", "Right-click to refund").ToString();
	}
	else if (AllocateResult == EPassiveNodeResult::Success)
	{
		Tooltip += LOCTEXT("AllocateHint", "Left-click to allocate").ToString();
	}
	else
	{
		Tooltip += UPassiveTreeWidget::GetResultText(AllocateResult).ToString();
	}
	SetToolTipText(FText::FromString(Tooltip));
}

FReply UPassiveNodeWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!OwnerTree) return FReply::Unhandled();

	const FKey Button = InMouseEvent.GetEffectingButton();
	if (Button == EKeys::LeftMouseButton)
	{
		OwnerTree->AllocateFromScreen(NodeId);
	}
	else if (Button == EKeys::RightMouseButton)
	{
		OwnerTree->RefundFromScreen(NodeId);
	}
	else
	{
		return FReply::Unhandled();
	}
	return OwnerTree->MakeFocusReply();
}

#undef LOCTEXT_NAMESPACE
