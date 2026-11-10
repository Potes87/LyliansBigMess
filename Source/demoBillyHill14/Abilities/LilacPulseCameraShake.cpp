// Fill out your copyright notice in the Description page of Project Settings.

#include "LilacPulseCameraShake.h"

void ULilacPulsePunchPattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(Duration);
}

void ULilacPulsePunchPattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	Elapsed = 0.f;
}

void ULilacPulsePunchPattern::UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult)
{
	Elapsed += Params.DeltaTime;
	const float T = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	// Fast ease-out kick, then a smooth ease back to rest.
	const float Weight = T < AttackFraction
		? FMath::Sin(T / AttackFraction * HALF_PI)
		: FMath::Square(1.f - (T - AttackFraction) / (1.f - AttackFraction));

	OutResult.Rotation = FRotator(PitchKick * Weight, 0.f, 0.f);
	OutResult.FOV = FOVKick * Weight;
	OutResult.Location = FVector(ForwardPush * Weight, 0.f, 0.f);
}

bool ULilacPulsePunchPattern::IsFinishedImpl() const
{
	return Elapsed >= Duration;
}

ULilacPulseCameraShake::ULilacPulseCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	SetRootShakePattern(ObjectInitializer.CreateDefaultSubobject<ULilacPulsePunchPattern>(this, TEXT("PunchPattern")));
}
