// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyLevelZone.generated.h"

class UBoxComponent;

/**
 * A box you drop over part of a map. Every enemy that starts inside it becomes ZoneLevel
 * (plus a random 0..LevelVariance on top). Enemies outside every zone use their own DefaultLevel (1).
 * Zones may overlap: the one with the highest Priority wins, then the highest level.
 * Invisible in game, no collision. Scale the box with the normal scale tool or set BoxExtent.
 */
UCLASS(Blueprintable)
class DEMOBILLYHILL14_API AEnemyLevelZone : public AActor
{
	GENERATED_BODY()

public:
	AEnemyLevelZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Enemy Level")
	TObjectPtr<UBoxComponent> Box;

	// Level of the enemies in this area.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Level", meta = (ClampMin = "1"))
	int32 ZoneLevel = 1;

	// Each enemy gets 0 up to this many extra levels at random, so a pack isn't all identical. 0 = all exactly ZoneLevel.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Level", meta = (ClampMin = "0"))
	int32 LevelVariance = 0;

	// When zones overlap, the higher Priority wins (e.g. a small boss arena inside a big area).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy Level")
	int32 Priority = 0;

	// True if a world position is inside this zone's box.
	UFUNCTION(BlueprintPure, Category = "Enemy Level")
	bool ContainsPoint(const FVector& Point) const;
};
