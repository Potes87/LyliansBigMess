// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SkillTreeComponent.h"
#include "PassiveTreeComponent.generated.h"

/**
 * The main passive tree. Points come from the player's level, nodes can be refunded, and node effects
 * (keystones) hook into shooting, hits and reloads through the Notify functions below.
 * The shared node rules live in USkillTreeComponent.
 */
UCLASS(ClassGroup = (Stats), meta = (BlueprintSpawnableComponent))
class DEMOBILLYHILL14_API UPassiveTreeComponent : public USkillTreeComponent
{
	GENERATED_BODY()

public:
	// Points gained for each level above level 1.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Passive Tree", meta = (ClampMin = "0"))
	int32 PointsPerLevel = 1;

	virtual int32 GetTotalPoints() const override;

	// Tells the tree the player's current level. Call this at BeginPlay and on every level up.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree")
	void SetPlayerLevel(int32 NewLevel);

	UFUNCTION(BlueprintPure, Category = "Passive Tree")
	int32 GetPlayerLevel() const { return PlayerLevel; }

	// Writes the level, the XP passed in and the allocated nodes to the save slot. Returns false if that failed.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Saving")
	bool SaveProgress(float CurrentXP);

	// Restores the level and allocated nodes from the save slot, and outputs the saved level and XP for the
	// leveling component. Saved nodes that no longer exist, connect or fit the available points are dropped.
	// Returns false if there's no save, leaving the tree unchanged.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Saving")
	bool LoadProgress(int32& Level, float& CurrentXP);

	// The gun calls this before spawning a bullet. Returns the damage the bullet should carry.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Events")
	float NotifyShotFired(float Damage);

	// The bullet calls this when it hits an enemy. Returns the damage to deal.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Events")
	float NotifyEnemyHit(AActor* Enemy, float Damage);

	// The gun calls this when a reload finishes.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Events")
	void NotifyReloaded();

	// Whether the gun may fire right now, as far as the allocated nodes are concerned.
	UFUNCTION(BlueprintPure, Category = "Passive Tree|Events")
	bool CanFireShot() const;

	// How many rounds one shot takes from the magazine.
	UFUNCTION(BlueprintPure, Category = "Passive Tree|Events")
	int32 GetAmmoCost() const;

	// Whether the gun may reload, as far as the allocated nodes are concerned.
	UFUNCTION(BlueprintPure, Category = "Passive Tree|Events")
	bool CanReload() const;

	// The gun calls this every frame. Returns the whole rounds to add to the magazine, usually 0.
	UFUNCTION(BlueprintCallable, Category = "Passive Tree|Events")
	int32 TakeRegeneratedAmmo(float DeltaSeconds, int32 MagazineSize);

private:
	int32 PlayerLevel = 1;

	// Regenerated ammo that hasn't added up to a whole round yet.
	float PendingAmmo = 0.f;
};
