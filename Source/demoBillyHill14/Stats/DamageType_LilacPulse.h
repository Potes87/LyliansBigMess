// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "DamageType_LilacPulse.generated.h"

/** Marks damage from Lilac Pulse, so enemies and UI can tell it apart from bullets. It never counts as a crit or headshot. */
UCLASS()
class DEMOBILLYHILL14_API UDamageType_LilacPulse : public UDamageType
{
	GENERATED_BODY()
};
