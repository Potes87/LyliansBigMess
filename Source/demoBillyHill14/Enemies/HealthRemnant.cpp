// Fill out your copyright notice in the Description page of Project Settings.

#include "HealthRemnant.h"

#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

namespace
{
	double RemnantReadNumber(const UObject* Object, const TCHAR* Name, double Default)
	{
		if (!Object) return Default;
		if (const FNumericProperty* Prop = FindFProperty<FNumericProperty>(Object->GetClass(), Name))
		{
			const void* Value = Prop->ContainerPtrToValuePtr<void>(Object);
			return Prop->IsFloatingPoint() ? Prop->GetFloatingPointPropertyValue(Value) : (double)Prop->GetSignedIntPropertyValue(Value);
		}
		return Default;
	}

	// Calls the Blueprint health component's own Heal(Amount), so the health bar updates the normal way.
	bool RemnantCallHeal(UActorComponent* Health, double Amount)
	{
		UFunction* Heal = Health ? Health->FindFunction(TEXT("Heal")) : nullptr;
		if (!Heal) return false;

		const int32 Size = FMath::Max<int32>(Heal->ParmsSize, 1);
		uint8* Params = (uint8*)FMemory_Alloca(Size);
		FMemory::Memzero(Params, Size);
		bool bSet = false;
		for (TFieldIterator<FProperty> It(Heal); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
			const FNumericProperty* Number = CastField<FNumericProperty>(*It);
			if (!bSet && Number && !It->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				void* Value = Number->ContainerPtrToValuePtr<void>(Params);
				if (Number->IsFloatingPoint()) Number->SetFloatingPointPropertyValue(Value, Amount);
				else Number->SetIntPropertyValue(Value, (int64)FMath::RoundToInt(Amount));
				bSet = true;
			}
		}
		Health->ProcessEvent(Heal, Params);
		for (TFieldIterator<FProperty> It(Heal); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		return bSet;
	}
}

AHealthRemnant::AHealthRemnant()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/FX/M_ShatterPiece.M_ShatterPiece"));
	if (Cube.Succeeded()) Mesh->SetStaticMesh(Cube.Object);
	if (Mat.Succeeded()) Material = Mat.Object;
}

void AHealthRemnant::BeginPlay()
{
	Super::BeginPlay();

	// The engine cube is 100 units wide. Stood on a corner, so it reads as a gem and not as rubble.
	Mesh->SetWorldScale3D(FVector(Size / 100.f));
	Mesh->SetWorldRotation(FRotator(45.f, FMath::FRandRange(0.f, 360.f), 35.26f));
	if (Material)
	{
		MID = UMaterialInstanceDynamic::Create(Material, this);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		MID->SetScalarParameterValue(TEXT("Glow"), Glow);
		Mesh->SetMaterial(0, MID);
	}
	BobPhase = FMath::FRandRange(0.f, 6.28f);
	RestPoint = GetActorLocation();
	if (FlyVelocity.IsNearlyZero())
	{
		FlyVelocity = FVector(0.f, 0.f, 150.f);   // placed by hand or spawned without a launch: a small hop, then it settles
	}
}

void AHealthRemnant::Launch(FVector Velocity)
{
	FlyVelocity = Velocity;
	Phase = EPhase::Flying;
}

UActorComponent* AHealthRemnant::FindHealth(const APawn* Player)
{
	if (!Player) return nullptr;
	for (UActorComponent* Comp : Player->GetComponents())
	{
		if (Comp && Comp->FindFunction(TEXT("Heal")) && FindFProperty<FNumericProperty>(Comp->GetClass(), TEXT("CurrentHealth")))
		{
			return Comp;
		}
	}
	return nullptr;
}

bool AHealthRemnant::IsAtFullHealth(const UActorComponent* Health)
{
	if (!Health) return false;
	const double Current = RemnantReadNumber(Health, TEXT("CurrentHealth"), 0.0);
	const double Max = RemnantReadNumber(Health, TEXT("MaxHealth"), 0.0);
	return Max > 0.0 && Current >= Max - 0.5;
}

void AHealthRemnant::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float Dt = FMath::Clamp(DeltaTime, 0.0001f, 0.05f);
	Age += Dt;

	UWorld* World = GetWorld();
	if (!World) return;

	// Out of time: shrink away over the last second.
	if (Age >= Lifetime)
	{
		Destroy();
		return;
	}
	const float Fade = FMath::Clamp(Lifetime - Age, 0.f, 1.f);
	Mesh->SetWorldScale3D(FVector(Size / 100.f * FMath::Max(Fade, 0.01f)));
	Mesh->AddLocalRotation(FRotator(0.f, SpinSpeed * Dt, 0.f));

	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Here = GetActorLocation();

	// Should it go to the player now?
	if (Phase != EPhase::Homing && Player && Age >= PickupDelay)
	{
		const float Dist = (float)FVector::Dist(Here, Player->GetActorLocation());
		if (Dist <= MagnetRadius)
		{
			const UActorComponent* Health = FindHealth(Player);
			if (Health && (bPickUpAtFullHealth || !IsAtFullHealth(Health)))
			{
				Phase = EPhase::Homing;
				HomingSpeed = MagnetSpeed;
			}
		}
	}

	switch (Phase)
	{
	case EPhase::Flying:
	{
		FlyVelocity.Z += World->GetGravityZ() * Dt;
		const FVector Next = Here + FlyVelocity * Dt;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(HealthRemnantFly), false, this);
		if (World->LineTraceSingleByChannel(Hit, Here, Next - FVector(0.f, 0.f, FloatHeight * 0.5f), ECC_WorldStatic, Params))
		{
			if (Hit.ImpactNormal.Z > 0.5f || Age > 3.f)
			{
				// Landed: float just above that spot.
				RestPoint = Hit.ImpactPoint + FVector(0.f, 0.f, FloatHeight);
				Phase = EPhase::Resting;
			}
			else
			{
				// A wall: drop straight down from here.
				FlyVelocity = FVector(0.f, 0.f, FMath::Min(FlyVelocity.Z, 0.f));
			}
		}
		else if (Age > 3.f)
		{
			RestPoint = Here;   // never found ground (thrown off a ledge): hang where it is
			Phase = EPhase::Resting;
		}
		else
		{
			SetActorLocation(Next);
		}
		break;
	}
	case EPhase::Resting:
	{
		const FVector Want = RestPoint + FVector(0.f, 0.f, FMath::Sin(Age * 3.f + BobPhase) * BobHeight);
		SetActorLocation(FMath::VInterpTo(Here, Want, Dt, 10.f));
		break;
	}
	case EPhase::Homing:
	{
		if (!Player)
		{
			RestPoint = Here;
			Phase = EPhase::Resting;
			break;
		}
		HomingSpeed += MagnetAcceleration * Dt;
		const FVector Target = Player->GetActorLocation();
		const FVector To = Target - Here;
		const float Dist = (float)To.Size();
		const float Step = HomingSpeed * Dt;
		if (Dist <= PickupRadius || Step >= Dist)
		{
			Collect(Player);
			return;
		}
		SetActorLocation(Here + To / Dist * Step);
		break;
	}
	}
}

void AHealthRemnant::Collect(APawn* Player)
{
	if (UActorComponent* Health = FindHealth(Player))
	{
		const double Max = RemnantReadNumber(Health, TEXT("MaxHealth"), 0.0);
		const double Amount = Max * HealPercent + HealFlat;
		if (Amount > 0.0)
		{
			RemnantCallHeal(Health, Amount);
		}
	}
	if (PickupSound)
	{
		UGameplayStatics::PlaySound2D(this, PickupSound, PickupVolume);
	}
	if (PickupFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), PickupFX, GetActorLocation());
	}
	Destroy();
}
