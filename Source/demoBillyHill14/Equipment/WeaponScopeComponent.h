// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponScopeComponent.generated.h"

class APawn;
class UUserWidget;
class USceneComponent;
class USkeletalMeshComponent;
class FBoolProperty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FScopeChangedSignature, bool, bScoped);

/**
 * Add this to a gun Blueprint to give that gun a scope. Guns without it are not affected in any way.
 *
 * A scoped gun only has two looks: at the hip, or the full-screen scope view. The raised "aiming" pose is never shown
 * (the scope model would sit inside the camera).
 *
 * Aim pressed:  the screen blinks to black, the arms and gun are hidden, the view zooms and the scope overlay shows.
 * Aim released: the overlay and zoom drop at once, and the arms come back once the gun is nearly down again.
 * Reloading:    scoping is blocked, so the reload is always watched from the hip. Still holding aim when it ends = scope back in.
 *
 * Reads the Blueprint bools "bAiming" (on the character) and "bReloading1" (on the gun) by name.
 * Three small Blueprint hooks use the static nodes at the bottom: the aim input, the look input and the crosshair.
 */
UCLASS(ClassGroup = (Equipment), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UWeaponScopeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponScopeComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope")
	bool bEnabled = true;

	// Field of view while scoped. The normal view is 90; 55 is a little under 2x zoom. Smaller = more zoom.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope", meta = (ClampMin = "5", ClampMax = "120"))
	float ScopeFOV = 55.f;

	// The full-screen overlay (black ring + reticle). May be empty: then there is only the zoom.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope")
	TSubclassOf<UUserWidget> ScopeWidgetClass;

	// Seconds for the screen to blink to black when aim is pressed. The gun is hidden at full black. Smaller = snappier.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope|Timing", meta = (ClampMin = "0.01"))
	float BlinkInTime = 0.06f;

	// Seconds for the black to clear again (after scoping in, and after scoping out).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope|Timing", meta = (ClampMin = "0.01"))
	float BlinkOutTime = 0.1f;

	// How black the blink is when leaving the scope (0 = no blink, a plain cut).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope|Timing", meta = (ClampMin = "0", ClampMax = "1"))
	float ExitBlink = 0.8f;

	// After leaving the scope the arms and gun stay hidden this long, so the gun is nearly back at the hip when it appears.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope|Timing", meta = (ClampMin = "0"))
	float ShowArmsDelay = 0.12f;

	// Scoping is blocked while the gun reloads (the reload is watched from the hip).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope")
	bool bBlockWhileReloading = true;

	// Extra mouse slow-down on top of the automatic one (the automatic one matches the zoom). 1 = just the automatic one.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scope", meta = (ClampMin = "0.05", ClampMax = "3"))
	float LookScaleMultiplier = 1.f;

	// Fired when the scope view opens (true) and closes (false). For sounds.
	UPROPERTY(BlueprintAssignable, Category = "Scope")
	FScopeChangedSignature OnScopeChanged;

	// True while the scope view is on screen.
	UFUNCTION(BlueprintPure, Category = "Scope")
	bool IsScopedNow() const { return bScoped; }

	// ---- Nodes for the three Blueprint hooks. They work for whatever gun is in the player's hands;
	// ---- with no scoped gun equipped they answer "nothing special" (false / true / 1). ----

	// Crosshair: hide it while this is true.
	UFUNCTION(BlueprintPure, Category = "Scope")
	static bool IsScoped();

	// Aim input: only start aiming while this is true. False while a scoped gun is reloading.
	UFUNCTION(BlueprintPure, Category = "Scope")
	static bool ScopeAllowsAim();

	// Look input: multiply the mouse input by this. 1 normally, smaller while scoped.
	UFUNCTION(BlueprintPure, Category = "Scope")
	static float ScopeLookScale();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	APawn* FindPawn() const;
	bool IsBlocked() const;
	bool IsHeldGun() const;
	void EnterScope(APawn* Pawn);
	void ExitScope(APawn* Pawn);
	void SetGunHidden(bool bHide);
	void SetBlink(APawn* Pawn, float Amount);

	static TWeakObjectPtr<UWeaponScopeComponent> ActiveScope;

	TWeakObjectPtr<APawn> CachedPawn;
	const FBoolProperty* AimProp = nullptr;
	const FBoolProperty* ReloadProp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ScopeWidget;

	// The arms mesh the gun is attached to, and what we changed on the meshes we hid (to put it back exactly).
	TWeakObjectPtr<USceneComponent> HiddenArms;
	TArray<TPair<TWeakObjectPtr<USkeletalMeshComponent>, uint8>> SavedAnimTick;

	bool bScoped = false;
	bool bGunHidden = false;
	float Blink = 0.f;
	float LastBlinkSent = 0.f;
	float ShowTimer = 0.f;
	float NormalFOV = 90.f;
};
