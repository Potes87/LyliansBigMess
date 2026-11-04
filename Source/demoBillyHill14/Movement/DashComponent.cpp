// Fill out your copyright notice in the Description page of Project Settings.

#include "DashComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "TimerManager.h"

UDashComponent::UDashComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDashComponent::BeginPlay()
{
	Super::BeginPlay();

	ChargesLeft = MaxCharges;
}

bool UDashComponent::TryDash()
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || ChargesLeft <= 0) return false;

	FVector Direction = Movement->GetCurrentAcceleration().GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = Character->GetActorForwardVector().GetSafeNormal2D();
	}

	// Root motion moves the character at a set speed for a set time, and stops at walls like normal movement.
	const TSharedPtr<FRootMotionSource_ConstantForce> Dash = MakeShared<FRootMotionSource_ConstantForce>();
	Dash->InstanceName = TEXT("Dash");
	Dash->AccumulateMode = ERootMotionAccumulateMode::Override;
	Dash->Priority = 500;
	Dash->Force = Direction * (DashDistance / DashDuration);
	Dash->Duration = DashDuration;
	// Without this, the character would keep the dash's speed for a moment when it ends.
	Dash->FinishVelocityParams.Mode = ERootMotionFinishVelocityMode::ClampVelocity;
	Dash->FinishVelocityParams.ClampVelocity = Movement->MaxWalkSpeed;
	Movement->ApplyRootMotionSource(Dash);

	--ChargesLeft;

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	if (!TimerManager.IsTimerActive(CooldownTimer))
	{
		TimerManager.SetTimer(CooldownTimer, this, &UDashComponent::RestoreCharge, Cooldown);
	}
	return true;
}

void UDashComponent::RestoreCharge()
{
	++ChargesLeft;

	if (ChargesLeft < MaxCharges)
	{
		GetWorld()->GetTimerManager().SetTimer(CooldownTimer, this, &UDashComponent::RestoreCharge, Cooldown);
	}
}
