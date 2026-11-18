// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

/**
 * Full-screen overlay that flashes the edges of the screen when the player is hit. Each edge has its own strength,
 * so the flash shows which side the hit came from. Drawn as stepped bands (no smooth gradient) to sit with the
 * game's pixel look. Never takes mouse input. Owned and driven by UCameraFeelComponent.
 */
class SHitEdgeFlash : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SHitEdgeFlash) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// 0 = left, 1 = right, 2 = top, 3 = bottom. Each 0 to 1.
	float Edge[4] = { 0.f, 0.f, 0.f, 0.f };
	FLinearColor Color = FLinearColor::Red;
	// How far each edge reaches into the screen, as a share of the screen's shorter side.
	float Depth = 0.2f;
	int32 Bands = 10;

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
};
