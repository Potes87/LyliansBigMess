// Fill out your copyright notice in the Description page of Project Settings.

#include "ArmorComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Blueprint/UserWidget.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

UArmorComponent::UArmorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while broken pieces are flying
}

void UArmorComponent::BeginPlay()
{
	Super::BeginPlay();

	Armor = MaxArmor;
	if (AActor* Owner = GetOwner())
	{
		TArray<UStaticMeshComponent*> Found;
		Owner->GetComponents<UStaticMeshComponent>(Found);
		for (UStaticMeshComponent* C : Found)
		{
			if (C && C->ComponentHasTag(ArmorPieceTag))
			{
				Pieces.Add(C);
			}
		}
	}
	TotalPieces = Pieces.Num();
	RefreshBar();
}

float UArmorComponent::AbsorbDamage(AActor* Target, float Damage)
{
	UArmorComponent* ArmorComp = Target ? Target->FindComponentByClass<UArmorComponent>() : nullptr;
	return ArmorComp ? ArmorComp->Absorb(Damage) : Damage;
}

float UArmorComponent::Absorb(float Damage)
{
	if (Armor <= 0.f || Damage <= 0.f)
	{
		return Damage;
	}

	const float Used = FMath::Min(Armor, Damage);
	Armor -= Used;

	// Pieces broken so far should match the armor lost: 1/7 gone = 1 piece off, and so on.
	if (TotalPieces > 0 && MaxArmor > 0.f)
	{
		const int32 ShouldBeBroken = Armor <= 0.f
			? TotalPieces
			: FMath::Clamp(FMath::FloorToInt((1.f - Armor / MaxArmor) * TotalPieces + KINDA_SMALL_NUMBER), 0, TotalPieces);
		while (BrokenPieces < ShouldBeBroken && Pieces.Num() > 0)
		{
			BreakOnePiece();
		}
	}

	OnArmorChanged.Broadcast(Armor, MaxArmor);
	if (Armor <= 0.f && !bBrokenFired)
	{
		bBrokenFired = true;
		OnArmorBroken.Broadcast();
	}
	RefreshBar();

	return Damage - Used;
}

void UArmorComponent::BreakOnePiece()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Pick the remaining piece that faces the player the most.
	const FVector Center = Owner->GetActorLocation();
	FVector ToPlayer = Owner->GetActorForwardVector();
	if (const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		ToPlayer = (Player->GetActorLocation() - Center).GetSafeNormal2D();
	}

	int32 Best = INDEX_NONE;
	float BestDot = -2.f;
	for (int32 i = 0; i < Pieces.Num(); ++i)
	{
		if (!Pieces[i])
		{
			continue;
		}
		const FVector Dir = (Pieces[i]->Bounds.Origin - Center).GetSafeNormal2D();
		const float Dot = FVector::DotProduct(Dir, ToPlayer);
		if (Dot > BestDot)
		{
			BestDot = Dot;
			Best = i;
		}
	}

	++BrokenPieces;
	if (Best == INDEX_NONE)
	{
		Pieces.Reset();
		return;
	}

	UStaticMeshComponent* Piece = Pieces[Best];
	Pieces.RemoveAt(Best);

	const FVector PieceCenter = Piece->Bounds.Origin;
	const FVector Out = (PieceCenter - Center).GetSafeNormal2D();

	Piece->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

	if (BreakFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, BreakFX, PieceCenter);
	}

	FDebris D;
	D.Mesh = Piece;
	D.Velocity = Out * PopOutSpeed * FMath::FRandRange(0.8f, 1.2f) + FVector::UpVector * PopUpSpeed * FMath::FRandRange(0.8f, 1.2f);
	D.SpinAxis = FVector::CrossProduct(FVector::UpVector, Out).GetSafeNormal();
	if (D.SpinAxis.IsNearlyZero())
	{
		D.SpinAxis = FVector::ForwardVector;
	}
	D.SpinDegPerSec = FMath::FRandRange(240.f, 420.f);
	D.StartScale = Piece->GetComponentScale();

	// Real physics: the plate only bumps into the world (ground, walls). It ignores the player, enemies,
	// bullets and the other plates, so it never blocks shots or trips anyone.
	Piece->SetMobility(EComponentMobility::Movable);
	Piece->SetCollisionObjectType(ECC_PhysicsBody);
	Piece->SetCollisionResponseToAllChannels(ECR_Ignore);
	Piece->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Piece->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Piece->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	Piece->SetSimulatePhysics(true);

	if (Piece->IsSimulatingPhysics())
	{
		Piece->SetPhysicsLinearVelocity(D.Velocity);
		Piece->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * FMath::FRandRange(200.f, 420.f));
		D.bLanded = true; // physics moves it; the tick below only handles the fade
	}
	else
	{
		// No collision shape on this mesh: fall back to the hand-made tumble.
		Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	Debris.Add(D);
	SetComponentTickEnabled(true);
}

void UArmorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UWorld* World = GetWorld();
	const float ShrinkTime = 0.4f;

	for (int32 i = Debris.Num() - 1; i >= 0; --i)
	{
		FDebris& D = Debris[i];
		UStaticMeshComponent* M = D.Mesh.Get();
		if (!M)
		{
			Debris.RemoveAtSwap(i);
			continue;
		}
		D.Age += DeltaTime;

		if (!D.bLanded)
		{
			D.Velocity.Z -= 980.f * DeltaTime;
			const FVector From = M->GetComponentLocation();
			const FVector To = From + D.Velocity * DeltaTime;

			FHitResult Hit;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(ArmorDebris), false, GetOwner());
			if (World && World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params))
			{
				M->SetWorldLocation(Hit.Location + Hit.ImpactNormal * 4.f);
				D.bLanded = true;
			}
			else
			{
				M->SetWorldLocation(To);
				M->AddWorldRotation(FQuat(D.SpinAxis, FMath::DegreesToRadians(D.SpinDegPerSec * DeltaTime)));
			}
		}

		if (D.Age > DebrisLifetime)
		{
			if (M->IsSimulatingPhysics())
			{
				M->SetSimulatePhysics(false); // freeze where it lies, then shrink
				M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			const float T = FMath::Clamp((D.Age - DebrisLifetime) / ShrinkTime, 0.f, 1.f);
			M->SetWorldScale3D(D.StartScale * FMath::Max(1.f - T, 0.01f));
			if (T >= 1.f)
			{
				M->DestroyComponent();
				Debris.RemoveAtSwap(i);
			}
		}
	}

	if (Debris.Num() == 0)
	{
		SetComponentTickEnabled(false);
	}
}

void UArmorComponent::RefreshBar()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	TArray<UWidgetComponent*> Widgets;
	Owner->GetComponents<UWidgetComponent>(Widgets);
	for (UWidgetComponent* WC : Widgets)
	{
		UUserWidget* UW = WC ? WC->GetUserWidgetObject() : nullptr;
		UProgressBar* Bar = UW ? Cast<UProgressBar>(UW->GetWidgetFromName(BarWidgetName)) : nullptr;
		if (!Bar)
		{
			continue;
		}
		Bar->SetPercent(GetArmorPercent());
		const ESlateVisibility Vis = Armor > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		Bar->SetVisibility(Vis);

		// The armor bar sits in its own framed row ("ArmorRow"): show/hide the whole row, frame included,
		// and give its frame the same tier colour as the health frame ("TierFrame").
		if (UBorder* Row = Cast<UBorder>(UW->GetWidgetFromName(TEXT("ArmorRow"))))
		{
			Row->SetVisibility(Vis);
			if (const UBorder* HealthFrame = Cast<UBorder>(UW->GetWidgetFromName(TEXT("TierFrame"))))
			{
				Row->SetBrushColor(HealthFrame->GetBrushColor());
			}
		}
		if (Armor < MaxArmor)
		{
			WC->SetVisibility(true); // the health bar only appears after the first hit; armor hits count too
		}
	}
}
