// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArmorComponent.generated.h"

class UNiagaraSystem;
class UStaticMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArmorChangedSignature, float, CurrentArmor, float, MaxArmor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArmorBrokenSignature);

/**
 * A layer of armor that soaks up damage before health does. It does not come back.
 * Damage left over after the armor runs out carries through to health.
 *
 * Armor pieces: every static mesh component on the owner tagged ArmorPieceTag is a piece. Each time the armor
 * drops by (1 / number of pieces), the piece facing the player pops off, tumbles to the ground and shrinks away.
 *
 * Bar: if the owner's health bar widget has a ProgressBar named BarWidgetName, it shows the armor and is
 * hidden once the armor is gone. Enemies without this component keep that bar hidden.
 *
 * Hook-up: enemy damage goes through UArmorComponent::AbsorbDamage before reaching health.
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UArmorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArmorComponent();

	// Takes Damage out of Target's armor (if it has an Armor component) and returns what is left for health.
	// Safe to call on anything: no armor component = the full damage comes back.
	UFUNCTION(BlueprintCallable, Category = "Armor")
	static float AbsorbDamage(AActor* Target, float Damage);

	// Damage multiplier for a hit on Target: CoreDamageMultiplier once its armor is all gone, otherwise 1.
	// Use it for the damage number so the popup shows the real (bonus) damage.
	UFUNCTION(BlueprintPure, Category = "Armor")
	static float GetDamageMultiplier(AActor* Target);

	// True when Target has an Armor component and all of its armor is gone (core exposed).
	UFUNCTION(BlueprintPure, Category = "Armor")
	static bool IsCoreExposed(AActor* Target);

	// Once the armor is gone, hits on the exposed core do this much more damage.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor", meta = (ClampMin = "0"))
	float CoreDamageMultiplier = 1.5f;

	UFUNCTION(BlueprintPure, Category = "Armor")
	float GetArmor() const { return Armor; }

	UFUNCTION(BlueprintPure, Category = "Armor")
	float GetArmorPercent() const { return MaxArmor > 0.f ? Armor / MaxArmor : 0.f; }

	UFUNCTION(BlueprintPure, Category = "Armor")
	bool HasArmor() const { return Armor > 0.f; }

	UPROPERTY(BlueprintAssignable, Category = "Armor")
	FArmorChangedSignature OnArmorChanged;

	// Fired once, when the last of the armor is gone.
	UPROPERTY(BlueprintAssignable, Category = "Armor")
	FArmorBrokenSignature OnArmorBroken;

	// How much damage the armor soaks up in total.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor", meta = (ClampMin = "0"))
	float MaxArmor = 300.f;

	// Static mesh components with this tag are the pieces that pop off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FName ArmorPieceTag = TEXT("BWArmor");

	// Name of the ProgressBar inside the health bar widget that shows armor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	FName BarWidgetName = TEXT("ArmorBar");

	// Effect spawned where a piece breaks off. Optional.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor|Pieces")
	TObjectPtr<UNiagaraSystem> BreakFX;

	// How hard a piece is thrown outward and upward (100 units = 1 metre).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor|Pieces", meta = (ClampMin = "0"))
	float PopOutSpeed = 420.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor|Pieces", meta = (ClampMin = "0"))
	float PopUpSpeed = 380.f;

	// Seconds a broken piece lies on the ground before shrinking away.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor|Pieces", meta = (ClampMin = "0"))
	float DebrisLifetime = 3.f;

protected:
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float Absorb(float Damage);
	void BreakOnePiece();
	void RefreshBar();

	struct FDebris
	{
		TWeakObjectPtr<UStaticMeshComponent> Mesh;
		FVector Velocity = FVector::ZeroVector;
		FVector SpinAxis = FVector::UpVector;
		float SpinDegPerSec = 0.f;
		float Age = 0.f;
		bool bLanded = false;
		FVector StartScale = FVector::OneVector;
	};

	float Armor = 0.f;
	int32 TotalPieces = 0;
	int32 BrokenPieces = 0;
	bool bBrokenFired = false;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Pieces;

	TArray<FDebris> Debris;
};
