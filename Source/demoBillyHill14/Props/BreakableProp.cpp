// Fill out your copyright notice in the Description page of Project Settings.

#include "BreakableProp.h"

#include "Components/StaticMeshComponent.h"
#include "Components/LightComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Sound/SoundBase.h"

ABreakableProp::ABreakableProp()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetCanEverAffectNavigation(true);
}

void ABreakableProp::BeginPlay()
{
	Super::BeginPlay();
	if (ImpactBreakImpulse > 0.f)
	{
		Mesh->OnComponentHit.AddDynamic(this, &ABreakableProp::HandleMeshHit);
	}
}

float ABreakableProp::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const float Taken = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
	if (bBroken || DamageAmount <= 0.f)
	{
		return Taken;
	}
	Health -= DamageAmount;
	if (Health > 0.f)
	{
		return DamageAmount;
	}

	// Work out where it was hit and which way the hit was travelling.
	FVector HitLoc = FVector::ZeroVector;
	FVector HitDir = FVector::ZeroVector;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& P = static_cast<const FPointDamageEvent&>(DamageEvent);
		HitLoc = P.HitInfo.ImpactPoint;
		HitDir = P.ShotDirection;
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent& R = static_cast<const FRadialDamageEvent&>(DamageEvent);
		HitLoc = R.Origin;
		HitDir = (GetActorLocation() - R.Origin).GetSafeNormal();
	}
	else if (DamageCauser)
	{
		// Bullets / projectiles: the causer is right at the hit point and its velocity is the shot direction.
		HitLoc = DamageCauser->GetActorLocation();
		HitDir = DamageCauser->GetVelocity().GetSafeNormal();
		if (HitDir.IsNearlyZero())
		{
			HitDir = (GetActorLocation() - HitLoc).GetSafeNormal();
		}
	}
	Break(HitLoc, HitDir);
	return DamageAmount;
}

void ABreakableProp::HandleMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bBroken && ImpactBreakImpulse > 0.f && NormalImpulse.Size() >= ImpactBreakImpulse)
	{
		Break(Hit.ImpactPoint, -Hit.ImpactNormal);
	}
}

void ABreakableProp::Break(FVector HitLocation, FVector HitDirection)
{
	if (bBroken)
	{
		return;
	}
	bBroken = true;

	const FTransform Xf = Mesh->GetComponentTransform();
	const FVector Centre = Mesh->Bounds.Origin;
	if (HitLocation.IsNearlyZero())
	{
		HitLocation = Centre - FVector(0, 0, Mesh->Bounds.BoxExtent.Z * 0.5f);
	}
	const FVector ShotDir = HitDirection.GetSafeNormal();

	Mesh->SetVisibility(false, false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Lantern lights go out.
	TArray<ULightComponent*> Lights;
	GetComponents<ULightComponent>(Lights);
	for (ULightComponent* L : Lights)
	{
		L->SetVisibility(false);
	}

	for (UStaticMesh* ChunkMesh : Chunks)
	{
		if (!ChunkMesh)
		{
			continue;
		}
		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(this);
		Piece->SetStaticMesh(ChunkMesh);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetWorldTransform(Xf);
		Piece->SetCollisionObjectType(ECC_PhysicsBody);
		Piece->SetCollisionResponseToAllChannels(ECR_Ignore);
		Piece->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Piece->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Piece->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
		Piece->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		Piece->SetCanEverAffectNavigation(false);
		Piece->SetCastShadow(true);
		Piece->RegisterComponent();
		Piece->SetSimulatePhysics(true);

		// Outward from the hit point + along the shot + a little pop up, with a random tumble.
		const FVector PieceCentre = Piece->Bounds.Origin;
		FVector Out = (PieceCentre - HitLocation);
		Out.Z = FMath::Max(Out.Z, 0.f);
		Out = Out.GetSafeNormal();
		if (Out.IsNearlyZero())
		{
			Out = FMath::VRand();
		}
		const FVector Vel = Out * BurstSpeed * FMath::FRandRange(0.6f, 1.2f) + ShotDir * ShotPush * FMath::FRandRange(0.5f, 1.f)
			+ FVector(0, 0, UpPop * FMath::FRandRange(0.5f, 1.2f));
		Piece->SetPhysicsLinearVelocity(Vel);
		Piece->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * Spin * FMath::FRandRange(0.4f, 1.f));
		Pieces.Add(Piece);
	}

	if (BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, HitLocation);
	}
	if (BreakFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakFX, HitLocation, ShotDir.IsNearlyZero() ? FRotator::ZeroRotator : ShotDir.Rotation());
	}

	BrokenTime = 0.f;
	SetActorTickEnabled(true);
	OnBroken(HitLocation, ShotDir);
}

void ABreakableProp::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bBroken)
	{
		return;
	}
	BrokenTime += DeltaSeconds;
	if (BrokenTime < PieceLifetime)
	{
		return;
	}
	const float A = FMath::Clamp((BrokenTime - PieceLifetime) / ShrinkTime, 0.f, 1.f);
	if (A >= 1.f)
	{
		Destroy();
		return;
	}
	if (ShrinkCentres.Num() != Pieces.Num())
	{
		// Freeze the pieces and remember where each one's middle is: chunk pivots sit at the old prop pivot,
		// so they must shrink around their own middle, not slide back toward the pivot.
		ShrinkCentres.Reset();
		ShrinkOffsets.Reset();
		for (UStaticMeshComponent* P : Pieces)
		{
			FVector C = FVector::ZeroVector, O = FVector::ZeroVector;
			if (P)
			{
				P->SetSimulatePhysics(false);
				P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C = P->Bounds.Origin;
				O = C - P->GetComponentLocation();
			}
			ShrinkCentres.Add(C);
			ShrinkOffsets.Add(O);
		}
	}
	const float S = FMath::Max(1.f - A, 0.01f);
	for (int32 i = 0; i < Pieces.Num(); ++i)
	{
		if (UStaticMeshComponent* P = Pieces[i])
		{
			P->SetWorldScale3D(FVector(S));
			P->SetWorldLocation(ShrinkCentres[i] - ShrinkOffsets[i] * S);
		}
	}
}
