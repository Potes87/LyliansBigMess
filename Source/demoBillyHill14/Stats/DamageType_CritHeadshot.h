// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DamageType_Crit.h"
#include "DamageType_CritHeadshot.generated.h"

/**
 * Marks a hit as a headshot that also crit. It's a child of UDamageType_Crit, so existing crit casts still succeed.
 */
UCLASS()
class DEMOBILLYHILL14_API UDamageType_CritHeadshot : public UDamageType_Crit
{
	GENERATED_BODY()
};
