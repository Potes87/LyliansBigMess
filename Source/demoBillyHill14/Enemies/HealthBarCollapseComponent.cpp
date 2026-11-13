// Fill out your copyright notice in the Description page of Project Settings.

#include "HealthBarCollapseComponent.h"

#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"

UHealthBarCollapseComponent::UHealthBarCollapseComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UHealthBarCollapseComponent::CollapseAndHide(UWidgetComponent* Bar, float SquashTime, float PinchTime, float LineThickness, float LineBrightness)
{
	if (!Bar)
	{
		return;
	}
	AActor* Owner = Bar->GetOwner();
	UUserWidget* W = Bar->GetUserWidgetObject();
	if (!Owner || !W || !Bar->IsVisible())
	{
		Bar->SetVisibility(false); // nothing to animate (bar never showed): just make sure it is hidden
		return;
	}

	UHealthBarCollapseComponent* C = NewObject<UHealthBarCollapseComponent>(Owner);
	C->Target = Bar;
	C->Squash = FMath::Max(SquashTime, 0.01f);
	C->Pinch = FMath::Max(PinchTime, 0.01f);
	C->Line = FMath::Clamp(LineThickness, 0.01f, 1.f);
	C->Bright = FMath::Max(LineBrightness, 1.f);
	C->RegisterComponent();

	W->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

void UHealthBarCollapseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWidgetComponent* Bar = Target.Get();
	UUserWidget* W = Bar ? Bar->GetUserWidgetObject() : nullptr;
	if (!W)
	{
		DestroyComponent();
		return;
	}

	Age += DeltaTime;
	float ScaleX = 1.f;
	float ScaleY = 1.f;
	float Opacity = 1.f;
	float Glow = 1.f;

	if (Age < Squash)
	{
		// Phase 1: squash down into a thin line, brightening as it thins.
		const float T = FMath::InterpEaseIn(0.f, 1.f, Age / Squash, 2.f);
		ScaleY = FMath::Lerp(1.f, Line, T);
		Glow = FMath::Lerp(1.f, Bright, T);
	}
	else
	{
		// Phase 2: the line pinches shut toward its middle and fades.
		const float T = FMath::Clamp((Age - Squash) / Pinch, 0.f, 1.f);
		const float E = FMath::InterpEaseIn(0.f, 1.f, T, 2.f);
		ScaleY = Line;
		ScaleX = 1.f - E;
		Opacity = 1.f - E * E;
		Glow = Bright;
		if (T >= 1.f)
		{
			// Stay collapsed and invisible. Do NOT reset scale/opacity here: a screen-space widget component
			// only disappears on the next frame, so a reset would flash the full bar for one frame.
			W->SetRenderScale(FVector2D(0.001f, Line));
			W->SetRenderOpacity(0.f);
			W->SetVisibility(ESlateVisibility::Collapsed); // hides the widget itself immediately
			Bar->SetVisibility(false);
			DestroyComponent();
			return;
		}
	}

	W->SetRenderScale(FVector2D(FMath::Max(ScaleX, 0.001f), ScaleY));
	W->SetRenderOpacity(Opacity);
	W->SetColorAndOpacity(FLinearColor(Glow, Glow, Glow, 1.f));
}
