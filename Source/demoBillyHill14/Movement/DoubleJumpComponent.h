// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DoubleJumpComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;

/**
 * Handles a character's jumping: one jump from the ground and a set number of jumps in the air.
 * Every press gives the same strong jump, even while falling, and falling is faster than rising so jumps feel snappy.
 * Add this to a Character and call TryJump once per press, from the Started pin of the jump input event.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UDoubleJumpComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDoubleJumpComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// Jumps if possible. If not, remembers the press for a moment and jumps as soon as it can. Returns true if it jumped now.
	UFUNCTION(BlueprintCallable, Category = "Jump")
	bool TryJump();

	// Upward speed of the jump from the ground.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0"))
	float JumpVelocity = 700.f;

	// Upward speed of a jump in the air. It replaces the current upward or downward speed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0"))
	float AirJumpVelocity = 650.f;

	// How many jumps the character gets in the air. 1 makes a double jump.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0"))
	int32 MaxAirJumps = 1;

	// If true, a jump in the air sends the character's current sideways speed in the direction being held.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump")
	bool bAirJumpRedirects = true;

	// Gravity is multiplied by this while moving up in the air.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0.01"))
	float RiseGravityMultiplier = 1.5f;

	// Gravity is multiplied by this while moving down in the air.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0.01"))
	float FallGravityMultiplier = 2.f;

	// How long after walking off a ledge a press still counts as a jump from the ground, in seconds.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0"))
	float CoyoteTime = 0.1f;

	// How long a press that couldn't jump is remembered, in seconds. It covers pressing just before landing or during a dash.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Jump", meta = (ClampMin = "0"))
	float JumpBufferTime = 0.2f;

protected:
	virtual void BeginPlay() override;

private:
	bool PerformJump();

	UPROPERTY()
	TObjectPtr<ACharacter> Character;

	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> Movement;

	float BaseGravityScale = 1.f;
	int32 AirJumpsUsed = 0;
	bool bUsedGroundJump = false;
	double LastGroundedTime = -1000.0;
	double BufferedJumpExpiry = -1000.0;
};
