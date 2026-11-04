// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "DamageType_Crit.generated.h"

/**
 * Marks a hit as a crit, so whatever receives the damage can react, such as showing a crit damage number.
 */
UCLASS()
class DEMOBILLYHILL14_API UDamageType_Crit : public UDamageType
{
	GENERATED_BODY()
};
