// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "LilacPulseCameraShake.generated.h"

/**
 * One quick "punch": the camera kicks up and the field of view widens a little, then both settle back.
 * Unlike a shake, it doesn't wobble.
 */
UCLASS(meta = (DisplayName = "Lilac Pulse Punch Pattern"))
class DEMOBILLYHILL14_API ULilacPulsePunchPattern : public UCameraShakePattern
{
	GENERATED_BODY()

public:
	// Total length in seconds.
	UPROPERTY(EditAnywhere, Category = "Punch", meta = (ClampMin = "0.01"))
	float Duration = 0.28f;

	// Share of Duration spent kicking out; the rest eases back.
	UPROPERTY(EditAnywhere, Category = "Punch", meta = (ClampMin = "0.01", ClampMax = "0.9"))
	float AttackFraction = 0.15f;

	// Degrees the view tilts up at the peak.
	UPROPERTY(EditAnywhere, Category = "Punch")
	float PitchKick = 2.2f;

	// Degrees the view widens at the peak.
	UPROPERTY(EditAnywhere, Category = "Punch")
	float FOVKick = 3.f;

	// Units the camera pushes forward at the peak.
	UPROPERTY(EditAnywhere, Category = "Punch")
	float ForwardPush = 4.f;

protected:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

private:
	float Elapsed = 0.f;
};

/** The default camera punch for Lilac Pulse. Make a Blueprint child to tune the numbers. */
UCLASS()
class DEMOBILLYHILL14_API ULilacPulseCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	ULilacPulseCameraShake(const FObjectInitializer& ObjectInitializer);
};
