// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthBarCollapseComponent.generated.h"

class UWidgetComponent;

/**
 * Death animation for an enemy health bar: the bar squashes down into a thin bright line, then the line
 * pinches shut sideways and fades, and finally the widget component is hidden.
 *
 * Use: call CollapseAndHide(HealthBarWidgetComponent) from the death logic instead of hiding the bar.
 * The helper adds this component to the owner by itself; nothing needs to be placed on the Blueprint.
 */
UCLASS(ClassGroup = (Enemies))
class DEMOBILLYHILL14_API UHealthBarCollapseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthBarCollapseComponent();

	// Plays the collapse on Bar and hides it at the end. Safe to call with nothing / an already hidden bar.
	UFUNCTION(BlueprintCallable, Category = "Health Bar")
	static void CollapseAndHide(UWidgetComponent* Bar, float SquashTime = 0.12f, float PinchTime = 0.18f, float LineThickness = 0.12f, float LineBrightness = 2.5f);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	TWeakObjectPtr<UWidgetComponent> Target;
	float Age = 0.f;
	float Squash = 0.12f;
	float Pinch = 0.18f;
	float Line = 0.12f;
	float Bright = 2.5f;
};
