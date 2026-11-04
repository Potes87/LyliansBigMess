// Fill out your copyright notice in the Description page of Project Settings.

#include "DoubleJumpComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UDoubleJumpComponent::UDoubleJumpComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDoubleJumpComponent::BeginPlay()
{
	Super::BeginPlay();

	Character = Cast<ACharacter>(GetOwner());
	Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement)
	{
		BaseGravityScale = Movement->GravityScale;
	}
}

void UDoubleJumpComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Movement) return;

	const double Now = GetWorld()->GetTimeSeconds();

	// A launch only takes effect on the next movement update. Until then the character still counts as on the ground,
	// and resetting the jumps here would give back the one just used.
	if (Movement->IsMovingOnGround() && Movement->PendingLaunchVelocity.IsZero())
	{
		LastGroundedTime = Now;
		bUsedGroundJump = false;
		AirJumpsUsed = 0;
	}

	float GravityMultiplier = 1.f;
	if (Movement->IsFalling())
	{
		GravityMultiplier = Movement->Velocity.Z > 0.f ? RiseGravityMultiplier : FallGravityMultiplier;
	}
	Movement->GravityScale = BaseGravityScale * GravityMultiplier;

	if (Now <= BufferedJumpExpiry && PerformJump())
	{
		BufferedJumpExpiry = -1000.0;
	}
}

bool UDoubleJumpComponent::TryJump()
{
	if (PerformJump())
	{
		BufferedJumpExpiry = -1000.0;
		return true;
	}

	BufferedJumpExpiry = GetWorld()->GetTimeSeconds() + JumpBufferTime;
	return false;
}

bool UDoubleJumpComponent::PerformJump()
{
	if (!Movement || !Movement->CanAttemptJump()) return false;

	// A dash's root motion overrides velocity and would cancel the jump, so the press waits in the buffer instead.
	if (!Movement->PendingLaunchVelocity.IsZero() || Movement->HasRootMotionSources()) return false;

	const double Now = GetWorld()->GetTimeSeconds();
	const bool bWithinCoyoteTime = !bUsedGroundJump && Now - LastGroundedTime <= CoyoteTime;
	if (Movement->IsMovingOnGround() || bWithinCoyoteTime)
	{
		bUsedGroundJump = true;
		Character->LaunchCharacter(FVector(0.f, 0.f, JumpVelocity), false, true);
		return true;
	}

	if (AirJumpsUsed >= MaxAirJumps) return false;

	++AirJumpsUsed;

	FVector Launch(0.f, 0.f, AirJumpVelocity);
	const FVector InputDirection = Movement->GetCurrentAcceleration().GetSafeNormal2D();
	const bool bRedirect = bAirJumpRedirects && !InputDirection.IsNearlyZero();
	if (bRedirect)
	{
		const FVector Sideways = InputDirection * Movement->Velocity.Size2D();
		Launch.X = Sideways.X;
		Launch.Y = Sideways.Y;
	}
	Character->LaunchCharacter(Launch, bRedirect, true);
	return true;
}
