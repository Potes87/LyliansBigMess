// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "DamageType_Headshot.generated.h"

/**
 * Marks a hit as a headshot that didn't crit. Use UCombatHelpers::BreakHitDamageType to read the flags back.
 */
UCLASS()
class DEMOBILLYHILL14_API UDamageType_Headshot : public UDamageType
{
	GENERATED_BODY()
};
