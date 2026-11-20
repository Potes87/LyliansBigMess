// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HealthRemnant.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class USoundBase;
class APawn;

/**
 * A small glowing piece of health that a dead enemy leaves behind.
 *
 * It pops out of the enemy, lands, and floats there. When the player comes near it flies to them and heals them.
 * By default it waits on the ground while the player is at full health, so none are wasted.
 *
 * Works as it is (the Health Drop component spawns this class when no other is picked). Make a Blueprint child
 * only if you want a different look, sound or numbers.
 */
UCLASS()
class DEMOBILLYHILL14_API AHealthRemnant : public AActor
{
	GENERATED_BODY()

public:
	AHealthRemnant();

	// ---- Healing ----

	// Heals this share of the player's max health (0.06 = 6%).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Heal", meta = (ClampMin = "0", ClampMax = "1"))
	float HealPercent = 0.06f;

	// Plus this many health points.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Heal", meta = (ClampMin = "0"))
	float HealFlat = 0.f;

	// Off: it stays on the ground while the player is at full health and comes when they are hurt.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Heal")
	bool bPickUpAtFullHealth = false;

	// ---- Pick up ----

	// It starts flying to the player when they are this close (units; 400 = 4 m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Pick Up", meta = (ClampMin = "0"))
	float MagnetRadius = 400.f;

	// It is collected when it gets this close.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Pick Up", meta = (ClampMin = "1"))
	float PickupRadius = 70.f;

	// Speed when it starts flying to the player, and how fast that speed grows.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Pick Up", meta = (ClampMin = "0"))
	float MagnetSpeed = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Pick Up", meta = (ClampMin = "0"))
	float MagnetAcceleration = 5000.f;

	// It cannot be collected for this long after it appears, so the pop out of the enemy is seen.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Pick Up", meta = (ClampMin = "0"))
	float PickupDelay = 0.35f;

	// Seconds before it fades away if nobody takes it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Pick Up", meta = (ClampMin = "1"))
	float Lifetime = 20.f;

	// ---- Look ----

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Remnant")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// Size of the piece in units.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Look", meta = (ClampMin = "1"))
	float Size = 16.f;

	// Needs the parameters "Color" and "Glow" (the shatter piece material has them).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Look")
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Look")
	FLinearColor Color = FLinearColor(0.80f, 0.09f, 0.13f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Look", meta = (ClampMin = "0"))
	float Glow = 8.f;

	// How high above the ground it floats, how far it bobs, and how fast it spins (degrees per second).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Look", meta = (ClampMin = "0"))
	float FloatHeight = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Look", meta = (ClampMin = "0"))
	float BobHeight = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Look")
	float SpinSpeed = 180.f;

	// ---- Effects ----

	// Played "in your ears" when it heals you.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Effects")
	TObjectPtr<USoundBase> PickupSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Effects", meta = (ClampMin = "0"))
	float PickupVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Remnant|Effects")
	TObjectPtr<UNiagaraSystem> PickupFX;

	// Throws it out with this speed (called by the Health Drop component right after spawning).
	UFUNCTION(BlueprintCallable, Category = "Remnant")
	void Launch(FVector Velocity);

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	enum class EPhase : uint8 { Flying, Resting, Homing };

	void Collect(APawn* Player);
	static UActorComponent* FindHealth(const APawn* Player);
	static bool IsAtFullHealth(const UActorComponent* Health);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MID;

	EPhase Phase = EPhase::Flying;
	FVector FlyVelocity = FVector::ZeroVector;
	FVector RestPoint = FVector::ZeroVector;
	float Age = 0.f;
	float HomingSpeed = 0.f;
	float BobPhase = 0.f;
};
