// Fill out your copyright notice in the Description page of Project Settings.

#include "CameraFeelComponent.h"

#include "Camera/CameraComponent.h"
#include "DashComponent.h"
#include "SHitEdgeFlash.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
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
	Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UCameraFeelComponent::HandleDamaged);
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

void UCameraFeelComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (FlashWidget.IsValid())
	{
		if (GEngine && GEngine->GameViewport)
		{
			GEngine->GameViewport->RemoveViewportWidgetContent(FlashWidget.ToSharedRef());
		}
		FlashWidget.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UCameraFeelComponent::StartFlash(float Strength)
{
	const ACharacter* Ch = Character.Get();
	if (!bHitFlash || !Ch || !Ch->IsLocallyControlled() || !GEngine || !GEngine->GameViewport) return;
	if (!FlashWidget.IsValid())
	{
		FlashWidget = SNew(SHitEdgeFlash);
		GEngine->GameViewport->AddViewportWidgetContent(FlashWidget.ToSharedRef(), 50);
	}
	// The hit came from the side OPPOSITE to the way it pushes the player. Front = top of the screen, behind = bottom.
	const float Peak = FMath::Clamp(Strength, 0.5f, 1.f);
	const float Dir[4] = { FMath::Max(0.f, HitSide), FMath::Max(0.f, -HitSide), FMath::Max(0.f, -HitForward), FMath::Max(0.f, HitForward) };
	for (int32 i = 0; i < 4; ++i)
	{
		// A fresh hit never dims an edge that is still lit from the last one.
		FlashEdges[i] = FMath::Max(FlashEdges[i] * FlashAlpha, Peak * FMath::Lerp(HitFlashAllEdges, 1.f, Dir[i]));
	}
	FlashAlpha = 1.f;
}

void UCameraFeelComponent::UpdateFlash(float Dt)
{
	if (!FlashWidget.IsValid()) return;
	FlashAlpha = FMath::Max(0.f, FlashAlpha - Dt / FMath::Max(HitFlashTime, 0.05f));
	const float Fade = FlashAlpha * FlashAlpha;   // quick at first, then a soft tail
	FLinearColor Col = HitFlashColor;
	Col.A = HitFlashOpacity;
	FlashWidget->Color = Col;
	FlashWidget->Depth = HitFlashDepth;
	FlashWidget->Bands = HitFlashBands;
	for (int32 i = 0; i < 4; ++i)
	{
		FlashWidget->Edge[i] = bHitFlash ? FlashEdges[i] * Fade : 0.f;
	}
}

void UCameraFeelComponent::KickHit(FVector AwayDirection, float Strength, bool bShove)
{
	ACharacter* Ch = Character.Get();
	if (!Ch || Strength <= 0.f) return;
	const AController* PC = Ch->GetController();
	const FRotator Yaw(0.f, PC ? PC->GetControlRotation().Yaw : Ch->GetActorRotation().Yaw, 0.f);
	FVector Dir = AwayDirection.GetSafeNormal2D();
	if (Dir.IsNearlyZero()) Dir = -FRotationMatrix(Yaw).GetUnitAxis(EAxis::X);   // unknown direction: treat it as a hit from the front
	HitSide = FVector::DotProduct(Dir, FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y));
	HitForward = FVector::DotProduct(Dir, FRotationMatrix(Yaw).GetUnitAxis(EAxis::X));
	HitSpring.V += Strength * FMath::Sqrt(HitStiffness) * 1.6f;
	StartFlash(Strength);

	if (bShove && HitShove > 0.f && Strength >= HitShoveMinStrength)
	{
		// Added to whatever the player is already doing, so it never cancels a dash or a jump.
		Ch->LaunchCharacter(Dir * HitShove * Strength + FVector(0.f, 0.f, HitShoveLift * Strength), false, false);
	}
}

float UCameraFeelComponent::OwnerMaxHealth() const
{
	// Find the Blueprint health component (BPC_Health) by its variables, so C++ doesn't need to know the Blueprint class.
	if (const AActor* Owner = GetOwner())
	{
		for (const UActorComponent* Comp : Owner->GetComponents())
		{
			if (!Comp) continue;
			static const TCHAR* const Names[] = { TEXT("MaxHealth"), TEXT("BaseMaxHealth") };
			for (const TCHAR* Name : Names)
			{
				if (const FNumericProperty* Prop = FindFProperty<FNumericProperty>(Comp->GetClass(), Name))
				{
					const void* Value = Prop->ContainerPtrToValuePtr<void>(Comp);
					const float Health = Prop->IsFloatingPoint() ? (float)Prop->GetFloatingPointPropertyValue(Value) : (float)Prop->GetSignedIntPropertyValue(Value);
					if (Health > 0.f) return Health;
				}
			}
		}
	}
	return 100.f;
}

void UCameraFeelComponent::HandleDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                         AController* InstigatedBy, AActor* DamageCauser)
{
	if (!bHitKick || Damage <= 0.f) return;
	const AActor* Owner = GetOwner();
	if (!Owner) return;
	// Where did it come from? The thing that hit us, or failing that whoever is behind it.
	const AActor* Source = DamageCauser ? DamageCauser : (InstigatedBy ? InstigatedBy->GetPawn() : nullptr);
	if (!Source || Source == Owner) return;   // damage with no attacker (burning, falling): no jolt, no shove
	const FVector Away = Owner->GetActorLocation() - Source->GetActorLocation();
	const float Strength = FMath::Clamp(Damage / FMath::Max(OwnerMaxHealth() * HitFullStrengthHealth, 1.f), HitMinStrength, HitMaxStrength);
	KickHit(Away, Strength, true);
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
	HitSpring.Step(0.f, HitStiffness, HitDamping, Dt);
	UpdateFlash(Dt);

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
	// Getting hit: the head is knocked the way the hit pushes. From the front it snaps back (looks up a little),
	// from the side it tips and turns that way. Also gameplay feedback, so Intensity doesn't remove it.
	CamRot.Pitch -= HitSpring.X * HitPitch * HitForward;
	CamRot.Roll += HitSpring.X * HitRoll * HitSide;
	CamRot.Yaw += HitSpring.X * HitYaw * HitSide;
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
