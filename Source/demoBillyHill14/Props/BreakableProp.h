// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BreakableProp.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;
class UNiagaraSystem;

/**
 * A prop that smashes into pre-cut pieces (crates, barrels, pots, lanterns, carts...).
 *
 * The whole prop is the Mesh. When its Health runs out (any Unreal damage: bullets, melee, Lilac Pulse, explosions)
 * it hides the Mesh and spawns every mesh in Chunks at the same spot with real physics, pushed away from the hit.
 * The chunks share the prop's pivot, so they line up exactly with the whole prop when they appear.
 * Pieces bounce around, rest, then shrink away and the actor removes itself.
 *
 * Pieces only collide with the world and with each other: never with players, enemies, bullets or the camera.
 * Any light components on the actor (lantern glow) switch off when it breaks.
 *
 * Make one Blueprint child per prop: set Mesh, fill Chunks with that prop's _Bk meshes, tune Health.
 */
UCLASS(Blueprintable)
class DEMOBILLYHILL14_API ABreakableProp : public AActor
{
	GENERATED_BODY()

public:
	ABreakableProp();

	// The whole, unbroken prop.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Breakable")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// The broken pieces (SM_K2_<Prop>_Bk01, _Bk02...). All of them are spawned on break.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable")
	TArray<TObjectPtr<UStaticMesh>> Chunks;

	// Damage it takes to break.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable", meta = (ClampMin = "0"))
	float Health = 25.f;

	// ---- Burst ----

	// How fast pieces fly outward from the hit point (cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Burst", meta = (ClampMin = "0"))
	float BurstSpeed = 320.f;

	// Extra push along the shot direction (cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Burst", meta = (ClampMin = "0"))
	float ShotPush = 260.f;

	// Extra upward pop (cm/s).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Burst", meta = (ClampMin = "0"))
	float UpPop = 160.f;

	// Random spin (degrees per second).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Burst", meta = (ClampMin = "0"))
	float Spin = 400.f;

	// Breaks when something slams into it this hard (physics impulse). 0 = only damage breaks it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Burst", meta = (ClampMin = "0"))
	float ImpactBreakImpulse = 0.f;

	// ---- Clean up ----

	// Seconds the pieces stay before shrinking away.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Cleanup", meta = (ClampMin = "0"))
	float PieceLifetime = 7.f;

	// Seconds the shrink takes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Cleanup", meta = (ClampMin = "0.05"))
	float ShrinkTime = 0.6f;

	// ---- Hooks ----

	// Played at the break point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Hooks")
	TObjectPtr<USoundBase> BreakSound;

	// Optional dust / splinter effect at the break point.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breakable|Hooks")
	TObjectPtr<UNiagaraSystem> BreakFX;

	// True once it has broken.
	UPROPERTY(BlueprintReadOnly, Category = "Breakable")
	bool bBroken = false;

	// Smash it now. HitLocation / HitDirection steer the burst (zero = from the centre, no push).
	UFUNCTION(BlueprintCallable, Category = "Breakable")
	void Break(FVector HitLocation, FVector HitDirection);

	// Fires right after it breaks (drop loot, play extra sounds, alert enemies...).
	UFUNCTION(BlueprintImplementableEvent, Category = "Breakable")
	void OnBroken(FVector HitLocation, FVector HitDirection);

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	float BrokenTime = 0.f;
	TArray<FVector> ShrinkCentres;
	TArray<FVector> ShrinkOffsets;
};
