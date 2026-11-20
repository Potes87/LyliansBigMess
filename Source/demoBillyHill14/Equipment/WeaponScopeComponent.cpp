// Fill out your copyright notice in the Description page of Project Settings.

#include "WeaponScopeComponent.h"

#include "../Movement/CameraFeelComponent.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

TWeakObjectPtr<UWeaponScopeComponent> UWeaponScopeComponent::ActiveScope;

UWeaponScopeComponent::UWeaponScopeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// Before the camera feel component (post physics), so the zoom it applies is this frame's.
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UWeaponScopeComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const AActor* Owner = GetOwner())
	{
		ReloadProp = FindFProperty<FBoolProperty>(Owner->GetClass(), TEXT("bReloading1"));
	}
	ActiveScope = this;
}

void UWeaponScopeComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The gun is going away (swapped, dropped, level change): put everything back.
	APawn* Pawn = CachedPawn.Get();
	if (bScoped)
	{
		ExitScope(Pawn);
	}
	SetGunHidden(false);
	if (Pawn && LastBlinkSent > 0.f)
	{
		SetBlink(Pawn, 0.f);
	}
	if (ScopeWidget)
	{
		ScopeWidget->RemoveFromParent();
		ScopeWidget = nullptr;
	}
	if (ActiveScope.Get() == this)
	{
		ActiveScope = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

APawn* UWeaponScopeComponent::FindPawn() const
{
	const AActor* Owner = GetOwner();
	if (!Owner) return nullptr;
	// The gun is attached to the arms of whoever holds it.
	if (APawn* Holder = Cast<APawn>(Owner->GetAttachParentActor()))
	{
		return Holder;
	}
	return nullptr;
}

bool UWeaponScopeComponent::IsHeldGun() const
{
	const AActor* Owner = GetOwner();
	// A gun that something else has hidden (not us) is not the one in the player's hands.
	return Owner && (!Owner->IsHidden() || bGunHidden);
}

bool UWeaponScopeComponent::IsBlocked() const
{
	const AActor* Owner = GetOwner();
	return bBlockWhileReloading && ReloadProp && Owner && ReloadProp->GetPropertyValue_InContainer(Owner);
}

void UWeaponScopeComponent::SetBlink(APawn* Pawn, float Amount)
{
	Amount = FMath::Clamp(Amount, 0.f, 1.f);
	if (FMath::IsNearlyEqual(Amount, LastBlinkSent, 0.001f)) return;
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetManualCameraFade(Amount, FLinearColor::Black, false);
		LastBlinkSent = Amount;
	}
}

void UWeaponScopeComponent::SetGunHidden(bool bHide)
{
	if (bHide == bGunHidden) return;
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (bHide)
	{
		USceneComponent* Root = Owner->GetRootComponent();
		HiddenArms = Root ? Root->GetAttachParent() : nullptr;

		// Hidden meshes normally stop animating. Keep them going, so the gun really is back at the hip when it reappears.
		SavedAnimTick.Reset();
		TArray<USkeletalMeshComponent*> Meshes;
		Owner->GetComponents<USkeletalMeshComponent>(Meshes);
		if (USkeletalMeshComponent* ArmsMesh = Cast<USkeletalMeshComponent>(HiddenArms.Get()))
		{
			Meshes.Add(ArmsMesh);
		}
		for (USkeletalMeshComponent* M : Meshes)
		{
			if (!M) continue;
			SavedAnimTick.Add(TPair<TWeakObjectPtr<USkeletalMeshComponent>, uint8>(TWeakObjectPtr<USkeletalMeshComponent>(M), (uint8)M->VisibilityBasedAnimTickOption));
			M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		}

		Owner->SetActorHiddenInGame(true);
		if (USceneComponent* A = HiddenArms.Get())
		{
			A->SetHiddenInGame(true, false);
		}
	}
	else
	{
		Owner->SetActorHiddenInGame(false);
		if (USceneComponent* A = HiddenArms.Get())
		{
			A->SetHiddenInGame(false, false);
		}
		for (const TPair<TWeakObjectPtr<USkeletalMeshComponent>, uint8>& Saved : SavedAnimTick)
		{
			if (USkeletalMeshComponent* M = Saved.Key.Get())
			{
				M->VisibilityBasedAnimTickOption = (EVisibilityBasedAnimTickOption)Saved.Value;
			}
		}
		SavedAnimTick.Reset();
		HiddenArms = nullptr;
	}
	bGunHidden = bHide;
}

void UWeaponScopeComponent::EnterScope(APawn* Pawn)
{
	if (bScoped || !Pawn) return;
	bScoped = true;
	SetGunHidden(true);

	if (UCameraFeelComponent* Feel = Pawn->FindComponentByClass<UCameraFeelComponent>())
	{
		NormalFOV = Feel->GetNormalFOV();
		Feel->SetScopeFOV(ScopeFOV);
	}
	else if (UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>())
	{
		NormalFOV = Cam->FieldOfView;
		Cam->SetFieldOfView(ScopeFOV);
	}

	if (!ScopeWidget && ScopeWidgetClass)
	{
		if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
		{
			ScopeWidget = CreateWidget<UUserWidget>(PC, ScopeWidgetClass);
			if (ScopeWidget)
			{
				ScopeWidget->AddToViewport(-5);   // under the HUD, so ammo and health stay readable
			}
		}
	}
	if (ScopeWidget)
	{
		ScopeWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	OnScopeChanged.Broadcast(true);
}

void UWeaponScopeComponent::ExitScope(APawn* Pawn)
{
	if (!bScoped) return;
	bScoped = false;

	if (Pawn)
	{
		if (UCameraFeelComponent* Feel = Pawn->FindComponentByClass<UCameraFeelComponent>())
		{
			Feel->SetScopeFOV(0.f);
		}
		else if (UCameraComponent* Cam = Pawn->FindComponentByClass<UCameraComponent>())
		{
			Cam->SetFieldOfView(NormalFOV);
		}
	}
	if (ScopeWidget)
	{
		ScopeWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	ShowTimer = ShowArmsDelay;   // the gun stays hidden a moment longer (see tick)
	OnScopeChanged.Broadcast(false);
}

void UWeaponScopeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* Pawn = CachedPawn.Get();
	if (!Pawn)
	{
		Pawn = FindPawn();
		if (!Pawn) return;
		CachedPawn = Pawn;
		AimProp = FindFProperty<FBoolProperty>(Pawn->GetClass(), TEXT("bAiming"));
	}
	if (!Pawn->IsLocallyControlled()) return;

	const float Dt = FMath::Clamp(DeltaTime, 0.0001f, 0.05f);
	const bool bAiming = AimProp && AimProp->GetPropertyValue_InContainer(Pawn);
	const bool bWant = bEnabled && bAiming && IsHeldGun() && !IsBlocked();

	if (bWant && !bScoped)
	{
		// Blink to black; at full black swap to the scope view.
		Blink = FMath::Min(1.f, Blink + Dt / FMath::Max(BlinkInTime, 0.01f));
		if (Blink >= 1.f)
		{
			EnterScope(Pawn);
		}
	}
	else if (!bWant && bScoped)
	{
		ExitScope(Pawn);
		Blink = FMath::Max(Blink, ExitBlink);
	}
	else
	{
		Blink = FMath::Max(0.f, Blink - Dt / FMath::Max(BlinkOutTime, 0.01f));
	}

	// After leaving the scope: bring the arms back once the gun has had time to drop.
	if (!bScoped && bGunHidden && !bWant)
	{
		ShowTimer -= Dt;
		if (ShowTimer <= 0.f)
		{
			SetGunHidden(false);
		}
	}

	SetBlink(Pawn, Blink);
}

bool UWeaponScopeComponent::IsScoped()
{
	const UWeaponScopeComponent* S = ActiveScope.Get();
	return S && S->bScoped;
}

bool UWeaponScopeComponent::ScopeAllowsAim()
{
	const UWeaponScopeComponent* S = ActiveScope.Get();
	return !S || !S->bEnabled || !S->IsBlocked();
}

float UWeaponScopeComponent::ScopeLookScale()
{
	const UWeaponScopeComponent* S = ActiveScope.Get();
	if (!S || !S->bScoped) return 1.f;
	const float Zoomed = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(S->ScopeFOV, 5.f, 170.f) * 0.5f));
	const float Normal = FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(S->NormalFOV, 5.f, 170.f) * 0.5f));
	return FMath::Clamp(Zoomed / FMath::Max(Normal, 0.01f), 0.05f, 1.f) * S->LookScaleMultiplier;
}
