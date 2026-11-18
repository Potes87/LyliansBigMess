// Fill out your copyright notice in the Description page of Project Settings.

#include "SHitEdgeFlash.h"

#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

void SHitEdgeFlash::Construct(const FArguments& InArgs)
{
	SetVisibility(EVisibility::HitTestInvisible);
	SetCanTick(false);
}

FVector2D SHitEdgeFlash::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return FVector2D(8.0, 8.0); // it is stretched to fill the viewport
}

int32 SHitEdgeFlash::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
	const int32 N = FMath::Clamp(Bands, 2, 32);
	const double Reach = FMath::Min(Size.X, Size.Y) * FMath::Clamp(Depth, 0.02f, 0.5f);
	const double Step = Reach / N;
	if (!White || Step <= 0.0) return LayerId;

	for (int32 E = 0; E < 4; ++E)
	{
		const float Strength = FMath::Clamp(Edge[E], 0.f, 1.f);
		if (Strength <= 0.004f) continue;
		for (int32 i = 0; i < N; ++i)
		{
			// Band 0 hugs the screen edge and is the strongest; each band inward is fainter (squared falloff).
			const float Fade = 1.f - (float)i / N;
			FLinearColor Tint = Color;
			Tint.A = Color.A * Strength * Fade * Fade;
			if (Tint.A <= 0.003f) continue;

			FVector2D Pos, Box;
			switch (E)
			{
			case 0: Pos = FVector2D(i * Step, 0.0);                    Box = FVector2D(Step, Size.Y); break;   // left
			case 1: Pos = FVector2D(Size.X - (i + 1) * Step, 0.0);     Box = FVector2D(Step, Size.Y); break;   // right
			case 2: Pos = FVector2D(0.0, i * Step);                    Box = FVector2D(Size.X, Step); break;   // top
			default: Pos = FVector2D(0.0, Size.Y - (i + 1) * Step);    Box = FVector2D(Size.X, Step); break;   // bottom
			}
			FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(Box, FSlateLayoutTransform(Pos)),
				White, ESlateDrawEffect::None, Tint);
		}
	}
	return LayerId;
}
