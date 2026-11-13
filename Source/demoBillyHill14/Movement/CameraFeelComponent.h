// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CameraFeelComponent.generated.h"

class ACharacter;
class UCameraComponent;
class USceneComponent;
class FBoolProperty;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFootstepSignature, bool, bLeftFoot);

/**
 * First-person camera feel, all procedural (code, not animations), so every number can be tuned live.
 *
 * Camera (kept steady and snappy, Borderlands style):
 *   step bob tied to distance walked (not time), side sway and roll per step, more when sprinting, much less while aiming,
 *   idle breathing, strafe lean, landing dip (scaled by fall speed), dash kick (FOV punch + lean), sprint FOV.
 * Arms (where the weight lives) - this component only calculates them, the arms animation Blueprint applies them:
 *   ArmsOffset / ArmsRotation = the same step rhythm a little later and bigger, lag behind the mouse with overshoot,
 *   lag against movement, extra landing dip.
 *
 * Moves the scene component named TargetComponentName (a camera pivot), or the first camera if there is none.
 * Offsets are added on top of the target's starting relative transform, in the target's own axes.
 * Reads the owner's Blueprint bools "bAiming" and "bRunning" if they exist.
 */
UCLASS(ClassGroup = (Movement), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UCameraFeelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCameraFeelComponent();

	// Fired on every footfall while walking or running on the ground (for footstep sounds later).
	UPROPERTY(BlueprintAssignable, Category = "Camera Feel")
	FFootstepSignature OnFootstep;

	// ---- General ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel")
	bool bEnabled = true;

	// Overall strength of all camera motion, 0 to 2 (for a "camera bob" setting). Arms are not affected.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel", meta = (ClampMin = "0", ClampMax = "2"))
	float Intensity = 1.f;

	// Scene component to move. If none has this name, the first camera component is moved instead.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel")
	FName TargetComponentName = TEXT("CameraPivot");

	// Let this component add sprint FOV and dash kick to the camera's field of view.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel")
	bool bDriveFOV = true;

	// Speeds that count as walking and sprinting (the character's walk and run speeds).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel", meta = (ClampMin = "1"))
	float WalkSpeed = 620.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel", meta = (ClampMin = "1"))
	float SprintSpeed = 900.f;

	// How quickly the bob blends in and out (starting, stopping, walk <-> sprint, aiming).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel", meta = (ClampMin = "0.1"))
	float BlendSpeed = 8.f;

	// ---- Step bob (cm and degrees). Height is the total up-down travel per step. ----

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "1"))
	float WalkStepLength = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "1"))
	float SprintStepLength = 190.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float WalkBobHeight = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float SprintBobHeight = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float WalkBobSide = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float SprintBobSide = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float WalkBobRoll = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float SprintBobRoll = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float WalkBobPitch = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float SprintBobPitch = 0.35f;

	// Bob strength while aiming down sights (0.2 = 20%).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0", ClampMax = "1"))
	float AimBobScale = 0.2f;

	// Idle breathing: up-down travel in cm and breaths per second.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float BreathHeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Bob", meta = (ClampMin = "0"))
	float BreathRate = 0.3f;

	// ---- Lean, landing, dash, FOV ----

	// Roll in degrees when strafing at sprint speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion")
	float StrafeRoll = 1.f;

	// Landing: falls slower than this (cm/s) don't dip; at LandingHeavySpeed and above the dip is full.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion", meta = (ClampMin = "0"))
	float LandingMinSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion", meta = (ClampMin = "1"))
	float LandingHeavySpeed = 1400.f;

	// Full landing dip in cm and nod in degrees.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion", meta = (ClampMin = "0"))
	float LandingDip = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion", meta = (ClampMin = "0"))
	float LandingPitch = 1.5f;

	// Landing spring: higher stiffness = faster; lower damping = bouncier (1 = no bounce).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion", meta = (ClampMin = "1"))
	float LandingStiffness = 140.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion", meta = (ClampMin = "0.05", ClampMax = "2"))
	float LandingDamping = 0.55f;

	// Dash kick: FOV punch (degrees), lean toward a sideways dash, pitch on a forward dash.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion")
	float DashFOVKick = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion")
	float DashRoll = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion")
	float DashPitch = -0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion", meta = (ClampMin = "1"))
	float DashStiffness = 90.f;

	// Extra FOV at full sprint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Motion")
	float SprintFOV = 4.f;

	// ---- Arms (calculated here, applied by the arms animation Blueprint) ----

	// Arms bob = camera bob x this, a little behind it in the step (radians).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "0"))
	float ArmsBobScale = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms")
	float ArmsPhaseLag = 0.35f;

	// Mouse sway: degrees (rotation) and cm (offset) per 100 deg/s of turning, and the caps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "0"))
	float MouseSwayRotation = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "0"))
	float MouseSwayOffset = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "0"))
	float MouseSwayMaxRotation = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "0"))
	float MouseSwayMaxOffset = 2.5f;

	// Arms lag against movement: cm at sprint speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "0"))
	float MoveLag = 2.5f;

	// Extra arms dip on landing, as a share of the camera dip.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "0"))
	float ArmsLandingScale = 1.5f;

	// Arms sway spring: stiffness (speed) and damping (lower = more overshoot).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "1"))
	float ArmsStiffness = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "0.05", ClampMax = "2"))
	float ArmsDamping = 0.5f;

	// While aiming, arms sway is scaled by this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera Feel|Arms", meta = (ClampMin = "0", ClampMax = "1"))
	float AimArmsScale = 0.25f;

	// ---- Output (read these in the arms animation Blueprint) ----

	// Arms offset in camera space: X forward, Y right, Z up (cm).
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera Feel|Output")
	FVector ArmsOffset = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera Feel|Output")
	FRotator ArmsRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera Feel|Output")
	FVector CameraOffset = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Camera Feel|Output")
	FRotator CameraRotation = FRotator::ZeroRotator;

	// Kicks the landing dip by hand (0 to 1 strength). Landing does this automatically.
	UFUNCTION(BlueprintCallable, Category = "Camera Feel")
	void KickLanding(float Strength);

	// Kicks the dash response by hand (world direction). Dash does this automatically.
	UFUNCTION(BlueprintCallable, Category = "Camera Feel")
	void KickDash(FVector Direction);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;

private:
	struct FSpring
	{
		float X = 0.f;
		float V = 0.f;
		void Step(float Target, float Stiffness, float Damping, float Dt);
	};

	UFUNCTION()
	void HandleLanded(const FHitResult& Hit);

	UFUNCTION()
	void HandleDashed(FVector Direction);

	bool ReadBool(const FBoolProperty* Prop) const;

	TWeakObjectPtr<ACharacter> Character;
	TWeakObjectPtr<USceneComponent> Target;
	TWeakObjectPtr<UCameraComponent> Camera;

	FVector BaseLocation = FVector::ZeroVector;
	FQuat BaseRotation = FQuat::Identity;
	float BaseFOV = 90.f;

	const FBoolProperty* AimProp = nullptr;
	const FBoolProperty* RunProp = nullptr;

	float Phase = 0.f;
	int32 LastStep = 0;
	float MoveAlpha = 0.f;
	float SprintAlpha = 0.f;
	float AimAlpha = 0.f;
	float Time = 0.f;
	float LastFallSpeed = 0.f;
	float LastYaw = 0.f;
	float LastPitch = 0.f;
	bool bHaveLastRotation = false;

	FSpring Lean;
	FSpring Landing;
	FSpring Dash;
	float DashSide = 0.f;
	float DashForward = 0.f;

	FSpring ArmsLoc[3];
	FSpring ArmsRot[3];
};
