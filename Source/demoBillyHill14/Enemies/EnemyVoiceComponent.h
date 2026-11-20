// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyVoiceComponent.generated.h"

class USoundBase;
class USoundAttenuation;
class UDamageType;
class AController;

/**
 * The noises an enemy makes when it is hit and when it dies. Add it to an enemy Blueprint (or the enemy parent) and
 * fill the sound lists. It listens for damage by itself: there is nothing to wire.
 *
 * Each hit picks a random sound from the list and never plays the same one twice in a row.
 * Hit sounds are feedback for the player's shot, so they stay at full volume out to FullVolumeDistance
 * and only then fade, reaching silence at SilentDistance.
 *
 * A child Blueprint can replace the lists to give one enemy its own voice.
 */
UCLASS(ClassGroup = (Enemies), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UEnemyVoiceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEnemyVoiceComponent();

	// ---- Hurt ----

	// Sounds for being hit. One is picked at random each time (never the same one twice in a row).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Hurt")
	TArray<TObjectPtr<USoundBase>> HurtSounds;

	// Chance that a hit makes a sound. 1 = every hit.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Hurt", meta = (ClampMin = "0", ClampMax = "1"))
	float HurtChance = 1.f;

	// Shortest time between two hurt sounds from this enemy, in seconds. Stops fast guns stacking into noise.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Hurt", meta = (ClampMin = "0"))
	float HurtMinGap = 0.12f;

	// Shortest time between hurt sounds from ANY enemy. Stops one blast that hits ten enemies playing ten sounds at once.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Hurt", meta = (ClampMin = "0"))
	float HurtGlobalGap = 0.03f;

	// Hits weaker than this make no sound (for example small burn ticks). 0 = every hit counts.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Hurt", meta = (ClampMin = "0"))
	float HurtMinDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Hurt", meta = (ClampMin = "0"))
	float HurtVolume = 1.f;

	// ---- Death ----

	// Sounds for dying. Empty = no death sound.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Death")
	TArray<TObjectPtr<USoundBase>> DeathSounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Death", meta = (ClampMin = "0"))
	float DeathVolume = 1.f;

	// ---- Shared ----

	// Each play gets a random volume in this range (times the volume above). Keeps repeats from sounding identical.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	FVector2D VolumeRange = FVector2D(0.92f, 1.05f);

	// Each play gets a random pitch in this range. 1 and 1 = always the same pitch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice")
	FVector2D PitchRange = FVector2D(1.f, 1.f);

	// Full volume when the player is closer than this (in cm; 2500 = 25 m).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Distance", meta = (ClampMin = "0"))
	float FullVolumeDistance = 2500.f;

	// Silent when the player is further than this (in cm). The volume fades between the two distances.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Distance", meta = (ClampMin = "0"))
	float SilentDistance = 7000.f;

	// Optional. Leave empty and the sound plays "in your ears" with only the fade above. Set one if you want
	// left/right panning; make its own falloff very wide, or it will make the sounds quiet again.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voice|Distance")
	TObjectPtr<USoundAttenuation> Attenuation;

	// Plays a hurt sound now, ignoring chance and gaps. For special cases in Blueprint.
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void PlayHurt();

	// Plays a death sound now.
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void PlayDeath();

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOwnerDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser);

	bool IsOwnerDead();
	void PlayFrom(const TArray<TObjectPtr<USoundBase>>& Sounds, int32& LastIndex, float Volume);

	TWeakObjectPtr<const UActorComponent> HealthComp;
	int32 LastHurt = INDEX_NONE;
	int32 LastDeath = INDEX_NONE;
	float NextHurtTime = 0.f;
	bool bDeathPlayed = false;

	static double LastGlobalHurtTime;
};
