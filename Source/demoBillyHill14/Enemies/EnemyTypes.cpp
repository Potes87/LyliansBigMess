// Fill out your copyright notice in the Description page of Project Settings.

#include "EnemyTypes.h"

FLinearColor UEnemyTierLibrary::GetTierColor(EEnemyTier Tier)
{
	switch (Tier)
	{
	case EEnemyTier::Rare:
		return FLinearColor::FromSRGBColor(FColor(0xEB, 0xD5, 0x92));   // Gold
	case EEnemyTier::Elite:
	case EEnemyTier::Unique:
		return FLinearColor::FromSRGBColor(FColor(0x96, 0x6C, 0x5D));   // Peach mixed into Shadow
	case EEnemyTier::Normal:
	default:
		return FLinearColor::FromSRGBColor(FColor(0xE9, 0xE4, 0xF0));   // Pale
	}
}
