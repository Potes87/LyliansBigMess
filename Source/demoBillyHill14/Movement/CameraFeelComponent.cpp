// Fill out your copyright notice in the Description page of Project Settings.

#include "CameraFeelComponent.h"

#include "Camera/CameraComponent.h"
#include "DashComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "UObject/UnrealType.h"

UCameraFeelComponent::UCameraFeelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// After movement, so this frame's velocity and landing are known; before the camera view is taken.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UCameraFeelComponent::FSpring::Step(float TargetValue, float Stiffness, float Damping, float Dt)
{
	// Damped spring, in small steps so it stays stable at any frame rate.
	const float W = FMath::Sqrt(Stiffness);
	const float C = 2.f * Damping * W;
	const int32 N = FMath::Clamp(FMath::CeilToInt(Dt * 240.f), 1, 16);
	const float H = Dt / N;
	for (int32 i = 0; i < N; ++i)
	{
		V += (Stiffness * (TargetValue - X) - C * V) * H;
		X += V * H;
	}
}

void UCameraFeelComponent::BeginPlay()
{
	Super::BeginPlay();

	Character = Cast<ACharacter>(GetOwner());
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	Camera = Owner->FindComponentByClass<UCameraComponent>();

	TArray<USceneComponent*> Scenes;
	Owner->GetComponents<USceneComponent>(Scenes);
	for (USceneComponent* S : Scenes)
	{
		if (S && S->GetFName() == TargetComponentName)
		{
			Target = S;
			break;
		}
	}
	if (!Target.IsValid())
	{
		Target = Camera.Get();
	}
	for (USceneComponent* S : Scenes)
	{
		if (S && S->GetFName() == ArmsComponentName)
		{
			Arms = S;
			ArmsBaseLocation = S->GetRelativeLocation();
			ArmsBaseRotation = S->GetRelativeRotation().Quaternion();
			break;
		}
	}
	if (USceneComponent* T = Target.Get())
	{
		BaseLocation = T->GetRelativeLocation();
		BaseRotation = T->GetRelativeRotation().Quaternion();
	}
	if (const UCameraComponent* Cam = Camera.Get())
	{
		BaseFOV = Cam->FieldOfView;
	}

	AimProp = FindFProperty<FBoolProperty>(Owner->GetClass(), TEXT("bAiming"));
	RunProp = FindFProperty<FBoolProperty>(Owner->GetClass(), TEXT("bRunning"));

	if (ACharacter* Ch = Character.Get())
	{
		Ch->LandedDelegate.AddDynamic(this, &UCameraFeelComponent::HandleLanded);
	}
	if (UDashComponent* DashComp = Owner->FindComponentByClass<UDashComponent>())
	{
		DashComp->OnDashed.AddDynamic(this, &UCameraFeelComponent::HandleDashed);
	}
}

bool UCameraFeelComponent::ReadBool(const FBoolProperty* Prop) const
{
	const AActor* Owner = GetOwner();
	return Prop && Owner && Prop->GetPropertyValue_InContainer(Owner);
}

void UCameraFeelComponent::KickLanding(float Strength)
{
	// Push the spring downward; it dips, then springs back (and slightly past, if damping is low).
	const float W = FMath::Sqrt(LandingStiffness);
	Landing.V -= FMath::Clamp(Strength, 0.f, 1.f) * W * 1.6f;
}

void UCameraFeelComponent::KickDash(FVector Direction)
{
	const ACharacter* Ch = Character.Get();
	const AController* PC = Ch ? Ch->GetController() : nullptr;
	const FRotator Yaw(0.f, PC ? PC->GetControlRotation().Yaw : (Ch ? Ch->GetActorRotation().Yaw : 0.f), 0.f);
	const FVector Dir = Direction.GetSafeNormal2D();
	DashSide = FVector::DotProduct(Dir, FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y));
	DashForward = FVector::DotProduct(Dir, FRotationMatrix(Yaw).GetUnitAxis(EAxis::X));
	Dash.V += FMath::Sqrt(DashStiffness) * 1.6f;
}

void UCameraFeelComponent::KickRecoil(float Strength)
{
	const float S = Strength * FMath::Lerp(1.f, AimRecoilScale, AimAlpha);
	const float W = FMath::Sqrt(RecoilStiffness) * 1.6f;
	RecoilUp.V += S * W;                            // unitless 0..~1, scaled by RecoilPitch in tick
	RecoilSide.V += S * W * FMath::FRandRange(-1.f, 1.f);
	ArmsRecoil.V += S * W;
}

void UCameraFeelComponent::HandleLanded(const FHitResult& Hit)
{
	const float Strength = (LastFallSpeed - LandingMinSpeed) / FMath::Max(LandingHeavySpeed - LandingMinSpeed, 1.f);
	if (Strength > 0.f)
	{
		KickLanding(Strength);
	}
	LastFallSpeed = 0.f;
}

void UCameraFeelComponent::HandleDashed(FVector Direction)
{
	KickDash(Direction);
}

void UCameraFeelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ACharacter* Ch = Character.Get();
	USceneComponent* T = Target.Get();
	if (!bEnabled || !Ch || !T)
	{
		return;
	}
	const float Dt = FMath::Clamp(DeltaTime, 0.0001f, 0.05f);
	Time += Dt;

	const UCharacterMovementComponent* Move = Ch->GetCharacterMovement();
	const FVector Vel = Ch->GetVelocity();
	const FVector Vel2D(Vel.X, Vel.Y, 0.f);
	const float Speed = Vel2D.Size();
	const bool bGrounded = Move && Move->IsMovingOnGround();
	if (Move && Move->IsFalling())
	{
		LastFallSpeed = FMath::Max(0.f, -Vel.Z);
	}

	// Look direction (yaw only) for strafe / movement lag, and turning speed for mouse sway.
	const AController* PC = Ch->GetController();
	const FRotator Control = PC ? PC->GetControlRotation() : Ch->GetActorRotation();
	const FRotator YawOnly(0.f, Control.Yaw, 0.f);
	const FVector Fwd = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);
	const float LocalFwd = FVector::DotProduct(Vel2D, Fwd);
	const float LocalRight = FVector::DotProduct(Vel2D, Right);

	float YawRate = 0.f;
	float PitchRate = 0.f;
	if (bHaveLastRotation)
	{
		YawRate = FMath::Clamp(FMath::FindDeltaAngleDegrees(LastYaw, Control.Yaw) / Dt, -900.f, 900.f);
		PitchRate = FMath::Clamp(FMath::FindDeltaAngleDegrees(LastPitch, Control.Pitch) / Dt, -900.f, 900.f);
	}
	LastYaw = Control.Yaw;
	LastPitch = Control.Pitch;
	bHaveLastRotation = true;

	// Smooth blends: moving, sprinting, aiming.
	const float Blend = 1.f - FMath::Exp(-BlendSpeed * Dt);
	const float MoveTarget = bGrounded ? FMath::Clamp(Speed / (WalkSpeed * 0.5f), 0.f, 1.f) : 0.f;
	const float SprintFromSpeed = FMath::Clamp((Speed - WalkSpeed * 0.95f) / FMath::Max(SprintSpeed - WalkSpeed * 0.95f, 1.f), 0.f, 1.f);
	const float SprintTarget = (RunProp && !ReadBool(RunProp)) ? 0.f : SprintFromSpeed;
	MoveAlpha += (MoveTarget - MoveAlpha) * Blend;
	SprintAlpha += (SprintTarget - SprintAlpha) * Blend;
	AimAlpha += ((ReadBool(AimProp) ? 1.f : 0.f) - AimAlpha) * Blend;

	// Step phase follows distance walked: one footfall every PI.
	if (bGrounded)
	{
		const float StepLength = FMath::Lerp(WalkStepLength, SprintStepLength, SprintAlpha);
		Phase += Speed * Dt / StepLength * PI;
	}
	const int32 Step = FMath::FloorToInt(Phase / PI);
	if (Step != LastStep)
	{
		if (bGrounded && MoveAlpha > 0.5f)
		{
			OnFootstep.Broadcast((Step & 1) == 0);
		}
		LastStep = Step;
	}
	if (Phase > 2000.f * PI)
	{
		Phase -= 2000.f * PI; // keep the number small (2000 PI is a whole number of strides)
		LastStep = FMath::FloorToInt(Phase / PI);
	}

	const float Height = FMath::Lerp(WalkBobHeight, SprintBobHeight, SprintAlpha);
	const float Side = FMath::Lerp(WalkBobSide, SprintBobSide, SprintAlpha);
	const float Roll = FMath::Lerp(WalkBobRoll, SprintBobRoll, SprintAlpha);
	const float Nod = FMath::Lerp(WalkBobPitch, SprintBobPitch, SprintAlpha);
	const float BobScale = MoveAlpha * FMath::Lerp(1.f, AimBobScale, AimAlpha);

	// Lowest at each footfall, highest mid-stride; sway and roll toward the planted foot.
	const float Down = -0.5f * FMath::Cos(2.f * Phase);
	const float Sway = FMath::Sin(Phase);

	// Springs: strafe lean, landing, dash.
	Lean.Step(-LocalRight / SprintSpeed * StrafeRoll, 60.f, 1.f, Dt);
	Landing.Step(0.f, LandingStiffness, LandingDamping, Dt);
	Dash.Step(0.f, DashStiffness, 0.7f, Dt);
	RecoilUp.Step(0.f, RecoilStiffness, RecoilDamping, Dt);
	RecoilSide.Step(0.f, RecoilStiffness, RecoilDamping, Dt);
	ArmsRecoil.Step(0.f, RecoilStiffness * 0.8f, RecoilDamping, Dt);

	const float Breath = (1.f - MoveAlpha) * BreathHeight * 0.5f * FMath::Sin(2.f * PI * BreathRate * Time);

	// ---- Camera ----
	FVector CamLoc(0.f, Side * Sway * BobScale, Height * Down * BobScale + Breath + Landing.X * LandingDip);
	FRotator CamRot(
		Nod * Down * BobScale + Landing.X * LandingPitch + Dash.X * DashPitch * DashForward,
		0.f,
		Roll * Sway * BobScale + Lean.X + Dash.X * DashRoll * DashSide);
	CamLoc *= Intensity;
	CamRot *= Intensity;
	// Recoil is gameplay feedback, not bob, so the Intensity setting doesn't remove it.
	CamRot.Pitch += RecoilUp.X * RecoilPitch;
	CamRot.Yaw += RecoilSide.X * RecoilYaw;
	CameraOffset = CamLoc;
	CameraRotation = CamRot;

	T->SetRelativeLocationAndRotation(BaseLocation + BaseRotation.RotateVector(CamLoc), BaseRotation * CamRot.Quaternion());

	if (bDriveFOV)
	{
		if (UCameraComponent* Cam = Camera.Get())
		{
			const float FOV = BaseFOV + (SprintFOV * SprintAlpha * MoveAlpha * (1.f - AimAlpha) + DashFOVKick * Dash.X) * FMath::Min(Intensity, 1.f);
			Cam->SetFieldOfView(FOV);
		}
	}

	// ---- Arms ----
	const float ArmsPhase = Phase - ArmsPhaseLag;
	const float ArmsDown = -0.5f * FMath::Cos(2.f * ArmsPhase);
	const float ArmsSway = FMath::Sin(ArmsPhase);
	const float ArmsScale = FMath::Lerp(1.f, AimArmsScale, AimAlpha);

	const float MaxOff = MouseSwayMaxOffset;
	const float MaxRot = MouseSwayMaxRotation;
	const FVector LocTarget = FVector(
		-LocalFwd / SprintSpeed * MoveLag,
		-LocalRight / SprintSpeed * MoveLag + FMath::Clamp(-YawRate * 0.01f * MouseSwayOffset, -MaxOff, MaxOff),
		FMath::Clamp(-PitchRate * 0.01f * MouseSwayOffset, -MaxOff, MaxOff)) * ArmsScale;
	const FVector RotTarget = FVector(
		FMath::Clamp(-PitchRate * 0.01f * MouseSwayRotation, -MaxRot, MaxRot),   // pitch
		FMath::Clamp(-YawRate * 0.01f * MouseSwayRotation, -MaxRot, MaxRot),     // yaw
		FMath::Clamp(-YawRate * 0.006f * MouseSwayRotation, -MaxRot, MaxRot)) * ArmsScale; // roll

	for (int32 i = 0; i < 3; ++i)
	{
		ArmsLoc[i].Step(LocTarget[i], ArmsStiffness, ArmsDamping, Dt);
		ArmsRot[i].Step(RotTarget[i], ArmsStiffness, ArmsDamping, Dt);
	}

	const float ArmsBob = BobScale * ArmsBobScale * ArmsScale;
	ArmsOffset = FVector(
		ArmsLoc[0].X,
		ArmsLoc[1].X + Side * ArmsSway * ArmsBob,
		ArmsLoc[2].X + Height * ArmsDown * ArmsBob + Landing.X * LandingDip * (ArmsLandingScale - 1.f));
	ArmsRotation = FRotator(
		ArmsRot[0].X + Nod * ArmsDown * ArmsBob,
		ArmsRot[1].X,
		ArmsRot[2].X + Roll * ArmsSway * ArmsBob);

	ArmsOffset.X -= ArmsRecoil.X * ArmsRecoilBack;
	ArmsRotation.Pitch += ArmsRecoil.X * ArmsRecoilPitch;

	// Move the arms mesh (child of the camera, so its relative space is camera space), rotating around ArmsPivot.
	if (bDriveArms)
	{
		if (USceneComponent* A = Arms.Get())
		{
			const FQuat R = ArmsRotation.Quaternion();
			const FVector Loc = ArmsPivot + R.RotateVector(ArmsBaseLocation - ArmsPivot) + ArmsOffset;
			A->SetRelativeLocationAndRotation(Loc, R * ArmsBaseRotation);
		}
	}
}
