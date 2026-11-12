// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeathShatterComponent.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Death burst: when the enemy dies, a spray of small low-poly pieces (cubes, cones...) bursts out of its body
 * with real physics while the ragdoll falls. Pieces bounce around, rest a moment, then shrink away.
 *
 * Put it on the enemy base Blueprint so every enemy gets it, and call Shatter (or ShatterActor) from the death logic.
 * Each enemy can tune piece count, size, force and colours. Bigger enemies throw more pieces automatically.
 * Pieces only collide with the world (ground, walls), never with players, enemies, bullets or each other.
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UDeathShatterComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDeathShatterComponent();

	// Bursts Target's Death Shatter component, if it has one. Safe to call on anything.
	UFUNCTION(BlueprintCallable, Category = "Death Shatter")
	static void ShatterActor(AActor* Target);

	// Throws the pieces now (after BurstDelay). Only works once.
	UFUNCTION(BlueprintCallable, Category = "Death Shatter")
	void Shatter();

	// ---- Amount ----

	// Pieces for a normal-sized enemy. Bigger enemies get more (see ReferenceRadius).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter", meta = (ClampMin = "0"))
	int32 PieceCount = 36;

	// Size of a "normal" enemy (bounding sphere radius, cm). An enemy twice this size throws twice the pieces.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter", meta = (ClampMin = "1"))
	float ReferenceRadius = 100.f;

	// Never more than this many pieces from one enemy.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter", meta = (ClampMin = "0"))
	int32 MaxPieces = 110;

	// Seconds after death before the burst (0 = same moment the ragdoll starts).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter", meta = (ClampMin = "0"))
	float BurstDelay = 0.03f;

	// ---- Look ----

	// Piece edge length range in cm.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Look", meta = (ClampMin = "0.5"))
	float PieceSizeMin = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Look", meta = (ClampMin = "0.5"))
	float PieceSizeMax = 17.f;

	// Shapes to pick from at random. Defaults: engine cube and cone.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Look")
	TArray<TObjectPtr<UStaticMesh>> PieceMeshes;

	// Needs a vector parameter "Color" and a scalar "Glow". Default: /Game/FX/M_ShatterPiece.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Look")
	TObjectPtr<UMaterialInterface> PieceMaterial;

	// Body colours (picked at random). Defaults: palette cloth darks.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Look")
	TArray<FLinearColor> PieceColors;

	// Share of pieces that glow (0 to 1), and their colour and brightness.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Look", meta = (ClampMin = "0", ClampMax = "1"))
	float GlowFraction = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Look")
	FLinearColor GlowColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Look", meta = (ClampMin = "0"))
	float GlowStrength = 8.f;

	// ---- Force (100 units = 1 metre) ----

	// Outward throw speed, and extra upward speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Force", meta = (ClampMin = "0"))
	float BurstSpeed = 650.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Force", meta = (ClampMin = "0"))
	float UpSpeed = 380.f;

	// Max spin in degrees per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Force", meta = (ClampMin = "0"))
	float SpinSpeed = 720.f;

	// How much of the falling body's own motion the pieces keep (0 to 1).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Force", meta = (ClampMin = "0", ClampMax = "1"))
	float InheritBodyVelocity = 0.5f;

	// ---- Cleanup ----

	// Seconds a piece lives (random in this range) before shrinking away.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Cleanup", meta = (ClampMin = "0"))
	float LifetimeMin = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Cleanup", meta = (ClampMin = "0"))
	float LifetimeMax = 4.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Shatter|Cleanup", meta = (ClampMin = "0.01"))
	float ShrinkTime = 0.4f;

	// Pieces alive in the whole level at once, across all enemies. New bursts get fewer pieces past this.
	static constexpr int32 MaxLivePieces = 450;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void DoBurst();
	UMaterialInstanceDynamic* GetMaterialFor(bool bGlow, int32 ColorIndex);

	struct FPiece
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		float Age = 0.f;
		float Lifetime = 3.f;
		FVector StartScale = FVector::OneVector;
		bool bFrozen = false;
	};

	TArray<FPiece> Pieces;
	bool bShattered = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterial;

	static int32 LivePieces;
};
