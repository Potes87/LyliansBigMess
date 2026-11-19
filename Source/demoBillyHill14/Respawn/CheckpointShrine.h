// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CheckpointShrine.generated.h"

class UArrowComponent;
class USphereComponent;

/**
 * A shrine the player touches to set where they wake up. Touching it also heals the player and brings every
 * enemy back (see URespawnSubsystem).
 *
 * Make a Blueprint child (BP_Shrine), give it a mesh and a light, and place it. Nothing else to wire.
 *   - TouchRange is the sphere the player walks into.
 *   - WakePoint (the arrow) is where the player stands after dying, and which way they face. Move it clear of the mesh.
 *   - The OnTouched event in the Blueprint is for a sound, a flash, or a glow change.
 *   - To use a button press instead of a walk-in, untick ActivateOnTouch and call Activate from your interact logic.
 */
UCLASS()
class DEMOBILLYHILL14_API ACheckpointShrine : public AActor
{
	GENERATED_BODY()

public:
	ACheckpointShrine();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shrine")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shrine")
	TObjectPtr<USphereComponent> TouchRange;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shrine")
	TObjectPtr<UArrowComponent> WakePoint;

	// Walking into TouchRange activates the shrine. Untick to activate it yourself (Activate).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shrine")
	bool bActivateOnTouch = true;

	// Name saved to disk as "the last shrine". Leave empty to use the placed actor's own name.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shrine")
	FName ShrineId;

	// After activating, the shrine ignores the player for this long (so waking next to it does not fire it again).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shrine", meta = (ClampMin = "0"))
	float RearmSeconds = 2.f;

	/** Set this shrine as the wake-up point, heal the player, bring every enemy back. */
	UFUNCTION(BlueprintCallable, Category = "Shrine")
	void Activate(APawn* Player);

	/** True while this is the shrine the player would wake at. */
	UFUNCTION(BlueprintPure, Category = "Shrine")
	bool IsCurrent() const;

	UFUNCTION(BlueprintPure, Category = "Shrine")
	FName GetShrineId() const;

	/** Fired when the shrine is activated. Sound, flash, glow. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Shrine", meta = (DisplayName = "On Touched"))
	void OnTouched(APawn* Player);

protected:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	virtual void BeginPlay() override;

private:
	double LastActivated = -1000.0;
};
