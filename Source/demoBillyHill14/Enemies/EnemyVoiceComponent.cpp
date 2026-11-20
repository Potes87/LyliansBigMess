// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyVoiceComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

double UEnemyVoiceComponent::LastGlobalHurtTime = -1000.0;

UEnemyVoiceComponent::UEnemyVoiceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyVoiceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		Owner->OnTakeAnyDamage.AddUniqueDynamic(this, &UEnemyVoiceComponent::HandleOwnerDamaged);   // unique: BeginPlay can run twice when a cell streams back in
	}
}

bool UEnemyVoiceComponent::IsOwnerDead()
{
	const AActor* Owner = GetOwner();
	if (!Owner) return false;

	// The Blueprint health component (BPC_Health) is found by its CurrentHealth variable.
	const UActorComponent* Health = HealthComp.Get();
	if (!Health)
	{
		for (const UActorComponent* Comp : Owner->GetComponents())
		{
			if (Comp && Comp != this && FindFProperty<FNumericProperty>(Comp->GetClass(), TEXT("CurrentHealth")))
			{
				Health = Comp;
				HealthComp = Comp;
				break;
			}
		}
	}
	if (!Health) return false;

	const FNumericProperty* Prop = FindFProperty<FNumericProperty>(Health->GetClass(), TEXT("CurrentHealth"));
	if (!Prop) return false;
	const void* Value = Prop->ContainerPtrToValuePtr<void>(Health);
	const double Current = Prop->IsFloatingPoint() ? Prop->GetFloatingPointPropertyValue(Value) : (double)Prop->GetSignedIntPropertyValue(Value);
	return Current <= 0.0;
}

void UEnemyVoiceComponent::HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
                                              AController* InstigatedBy, AActor* DamageCauser)
{
	if (Damage <= 0.f || bDeathPlayed) return;
	const UWorld* World = GetWorld();
	if (!World) return;

	// The enemy Blueprint has already taken the health off by the time this runs, so "dead" is known here.
	if (IsOwnerDead())
	{
		PlayDeath();
		return;
	}

	if (Damage < HurtMinDamage || HurtSounds.Num() == 0) return;

	const double Now = World->GetTimeSeconds();
	if (Now < LastGlobalHurtTime) LastGlobalHurtTime = -1000.0;   // a new play session started: the clock went back
	if (Now < NextHurtTime || Now - LastGlobalHurtTime < HurtGlobalGap) return;
	if (HurtChance < 1.f && FMath::FRand() > HurtChance) return;

	NextHurtTime = Now + HurtMinGap;
	LastGlobalHurtTime = Now;
	PlayFrom(HurtSounds, LastHurt, HurtVolume);
}

void UEnemyVoiceComponent::PlayHurt()
{
	PlayFrom(HurtSounds, LastHurt, HurtVolume);
}

void UEnemyVoiceComponent::PlayDeath()
{
	if (bDeathPlayed) return;
	bDeathPlayed = true;
	if (DeathSounds.Num() > 0)
	{
		PlayFrom(DeathSounds, LastDeath, DeathVolume);
	}
	else
	{
		PlayFrom(HurtSounds, LastHurt, HurtVolume);   // no death sounds yet: the killing hit still gets a noise
	}
}

void UEnemyVoiceComponent::PlayFrom(const TArray<TObjectPtr<USoundBase>>& Sounds, int32& LastIndex, float Volume)
{
	const AActor* Owner = GetOwner();
	const int32 Num = Sounds.Num();
	if (!Owner || Num == 0) return;

	// Random pick that is never the one played last time.
	int32 Index = FMath::RandRange(0, Num - 1);
	if (Num > 1 && Index == LastIndex)
	{
		Index = (Index + FMath::RandRange(1, Num - 1)) % Num;
	}
	LastIndex = Index;
	USoundBase* Sound = Sounds[Index];
	if (!Sound) return;

	const FVector Where = Owner->GetActorLocation();

	// Our own distance fade: full volume in fighting range, silent far away.
	float Fade = 1.f;
	if (const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		const float Dist = FVector::Dist(Cam->GetCameraLocation(), Where);
		if (SilentDistance > FullVolumeDistance)
		{
			Fade = 1.f - FMath::Clamp((Dist - FullVolumeDistance) / (SilentDistance - FullVolumeDistance), 0.f, 1.f);
		}
		else
		{
			Fade = Dist <= FullVolumeDistance ? 1.f : 0.f;
		}
	}
	if (Fade <= 0.01f) return;

	const float Vol = Volume * Fade * FMath::FRandRange(FMath::Min(VolumeRange.X, VolumeRange.Y), FMath::Max(VolumeRange.X, VolumeRange.Y));
	const float Pitch = FMath::FRandRange(FMath::Min(PitchRange.X, PitchRange.Y), FMath::Max(PitchRange.X, PitchRange.Y));
	if (Vol <= 0.f) return;

	UGameplayStatics::PlaySoundAtLocation(this, Sound, Where, FRotator::ZeroRotator, Vol, FMath::Max(Pitch, 0.01f), 0.f, Attenuation);
}
