// Fill out your copyright notice in the Description page of Project Settings.

#include "DeathShatterComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

int32 UDeathShatterComponent::LivePieces = 0;

UDeathShatterComponent::UDeathShatterComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while pieces are flying

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/FX/M_ShatterPiece.M_ShatterPiece"));
	if (Cube.Succeeded()) { PieceMeshes.Add(Cube.Object); PieceMeshes.Add(Cube.Object); } // cubes twice as common
	if (Cone.Succeeded()) { PieceMeshes.Add(Cone.Object); }
	if (Mat.Succeeded()) { PieceMaterial = Mat.Object; }

	// Palette (wardrobe cloth darks), sRGB hex -> linear.
	PieceColors = {
		FLinearColor(FColor::FromHex(TEXT("272332"))), // Lily hat
		FLinearColor(FColor::FromHex(TEXT("383549"))), // Gunsmith coat
		FLinearColor(FColor::FromHex(TEXT("40304C"))), // Lily dress
		FLinearColor(FColor::FromHex(TEXT("2F2C3D"))), // Gunsmith hood
		FLinearColor(FColor::FromHex(TEXT("201F26"))), // Ferryman robe
	};
	GlowColor = FLinearColor(FColor::FromHex(TEXT("DDC8FF"))); // lilac glow
}

void UDeathShatterComponent::ShatterActor(AActor* Target)
{
	if (UDeathShatterComponent* C = Target ? Target->FindComponentByClass<UDeathShatterComponent>() : nullptr)
	{
		C->Shatter();
	}
}

void UDeathShatterComponent::Shatter()
{
	if (bShattered)
	{
		return;
	}
	bShattered = true;

	UWorld* World = GetWorld();
	if (BurstDelay > 0.f && World)
	{
		FTimerHandle H;
		World->GetTimerManager().SetTimer(H, this, &UDeathShatterComponent::DoBurst, BurstDelay, false);
	}
	else
	{
		DoBurst();
	}
}

UMaterialInstanceDynamic* UDeathShatterComponent::GetMaterialFor(bool bGlow, int32 ColorIndex)
{
	if (!PieceMaterial)
	{
		return nullptr;
	}
	if (bGlow)
	{
		if (!GlowMaterial)
		{
			GlowMaterial = UMaterialInstanceDynamic::Create(PieceMaterial, this);
			GlowMaterial->SetVectorParameterValue(TEXT("Color"), GlowColor);
			GlowMaterial->SetScalarParameterValue(TEXT("Glow"), GlowStrength);
		}
		return GlowMaterial;
	}
	if (BodyMaterials.Num() != PieceColors.Num())
	{
		BodyMaterials.Reset();
		for (const FLinearColor& C : PieceColors)
		{
			UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(PieceMaterial, this);
			M->SetVectorParameterValue(TEXT("Color"), C);
			M->SetScalarParameterValue(TEXT("Glow"), 0.f);
			BodyMaterials.Add(M);
		}
	}
	return BodyMaterials.IsValidIndex(ColorIndex) ? BodyMaterials[ColorIndex].Get() : nullptr;
}

void UDeathShatterComponent::DoBurst()
{
	AActor* Owner = GetOwner();
	if (!Owner || PieceMeshes.Num() == 0)
	{
		return;
	}

	// Where the pieces come from: the body's bones (so the burst is body-shaped), or the actor bounds as a fallback.
	USkeletalMeshComponent* Body = Owner->FindComponentByClass<USkeletalMeshComponent>();
	TArray<FName> Bones;
	if (Body)
	{
		for (int32 i = 0; i < Body->GetNumBones(); ++i)
		{
			const FName Bone = Body->GetBoneName(i);
			const FString S = Bone.ToString();
			if (!S.StartsWith(TEXT("ik_")) && !S.Equals(TEXT("root"), ESearchCase::IgnoreCase))
			{
				Bones.Add(Bone);
			}
		}
	}

	FVector Center;
	FVector Extent;
	Owner->GetActorBounds(true, Center, Extent);
	float Radius = Extent.Size();
	if (Body)
	{
		Center = Body->Bounds.Origin;
		Radius = Body->Bounds.SphereRadius;
	}

	const float SizeFactor = FMath::Clamp(Radius / ReferenceRadius, 0.6f, 3.f);
	int32 Count = FMath::Min(FMath::RoundToInt(PieceCount * SizeFactor), MaxPieces);
	Count = FMath::Min(Count, FMath::Max(0, MaxLivePieces - LivePieces));
	if (Count <= 0)
	{
		return;
	}

	const bool bBodySimulating = Body && Body->IsSimulatingPhysics();

	for (int32 i = 0; i < Count; ++i)
	{
		FVector Loc;
		FVector BodyVel = FVector::ZeroVector;
		if (Bones.Num() > 0)
		{
			const FName Bone = Bones[FMath::RandRange(0, Bones.Num() - 1)];
			Loc = Body->GetBoneLocation(Bone) + FMath::VRand() * FMath::FRandRange(0.f, 12.f);
			if (bBodySimulating)
			{
				BodyVel = Body->GetPhysicsLinearVelocity(Bone);
			}
		}
		else
		{
			Loc = Center + FVector(FMath::FRandRange(-Extent.X, Extent.X), FMath::FRandRange(-Extent.Y, Extent.Y), FMath::FRandRange(-Extent.Z, Extent.Z)) * 0.7f;
		}

		UStaticMeshComponent* Piece = NewObject<UStaticMeshComponent>(Owner);
		if (!Piece)
		{
			continue;
		}
		Piece->SetStaticMesh(PieceMeshes[FMath::RandRange(0, PieceMeshes.Num() - 1)]);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetCastShadow(false);
		Piece->SetCollisionObjectType(ECC_PhysicsBody);
		Piece->SetCollisionResponseToAllChannels(ECR_Ignore);
		Piece->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		Piece->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
		Piece->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		Piece->SetGenerateOverlapEvents(false);
		Piece->SetCanEverAffectNavigation(false);

		const float Size = FMath::FRandRange(PieceSizeMin, FMath::Max(PieceSizeMin, PieceSizeMax));
		const FVector Scale = FVector(Size / 100.f) * FVector(FMath::FRandRange(0.7f, 1.3f), FMath::FRandRange(0.7f, 1.3f), FMath::FRandRange(0.7f, 1.3f));
		Piece->SetWorldLocationAndRotation(Loc, FMath::VRand().Rotation());
		Piece->SetWorldScale3D(Scale);

		const bool bGlow = FMath::FRand() < GlowFraction;
		if (UMaterialInstanceDynamic* M = GetMaterialFor(bGlow, PieceColors.Num() > 0 ? FMath::RandRange(0, PieceColors.Num() - 1) : 0))
		{
			Piece->SetMaterial(0, M);
		}

		Piece->RegisterComponent();
		Piece->SetLinearDamping(0.15f);
		Piece->SetAngularDamping(0.4f);
		Piece->SetSimulatePhysics(true);

		// Outward from the body's middle, plus some randomness so it reads as an explosion, not a ring.
		FVector Dir = (Loc - Center);
		Dir.Z *= 0.4f;
		Dir = (Dir.GetSafeNormal() + FMath::VRand() * 0.6f).GetSafeNormal();
		const FVector Vel = Dir * BurstSpeed * FMath::FRandRange(0.6f, 1.3f)
			+ FVector::UpVector * UpSpeed * FMath::FRandRange(0.5f, 1.2f)
			+ BodyVel * InheritBodyVelocity;

		if (Piece->IsSimulatingPhysics())
		{
			Piece->SetPhysicsLinearVelocity(Vel);
			Piece->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * FMath::FRandRange(SpinSpeed * 0.3f, SpinSpeed));
		}

		FPiece P;
		P.Mesh = Piece;
		P.Lifetime = FMath::FRandRange(LifetimeMin, FMath::Max(LifetimeMin, LifetimeMax));
		P.StartScale = Scale;
		Pieces.Add(P);
		++LivePieces;
	}

	if (Pieces.Num() > 0)
	{
		SetComponentTickEnabled(true);
	}
}

void UDeathShatterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (int32 i = Pieces.Num() - 1; i >= 0; --i)
	{
		FPiece& P = Pieces[i];
		UStaticMeshComponent* M = P.Mesh.Get();
		if (!M)
		{
			Pieces.RemoveAtSwap(i);
			LivePieces = FMath::Max(0, LivePieces - 1);
			continue;
		}
		P.Age += DeltaTime;
		if (P.Age <= P.Lifetime)
		{
			continue;
		}
		if (!P.bFrozen)
		{
			P.bFrozen = true;
			M->SetSimulatePhysics(false); // freeze where it lies, then shrink
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		const float T = FMath::Clamp((P.Age - P.Lifetime) / ShrinkTime, 0.f, 1.f);
		M->SetWorldScale3D(P.StartScale * FMath::Max(1.f - T, 0.01f));
		if (T >= 1.f)
		{
			M->DestroyComponent();
			Pieces.RemoveAtSwap(i);
			LivePieces = FMath::Max(0, LivePieces - 1);
		}
	}

	if (Pieces.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}

void UDeathShatterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// The enemy is going away (its lifespan ran out, or the level ended): its pieces go with it.
	LivePieces = FMath::Max(0, LivePieces - Pieces.Num());
	for (FPiece& P : Pieces)
	{
		if (UStaticMeshComponent* M = P.Mesh.Get())
		{
			M->DestroyComponent();
		}
	}
	Pieces.Reset();
	Super::EndPlay(EndPlayReason);
}
