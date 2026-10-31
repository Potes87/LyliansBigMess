// Fill out your copyright notice in the Description page of Project Settings.


#include "ItemInstance.h"

float UItemInstance::GetStatValue(FGameplayTag StatTag)
{
	return StatsMap.FindRef(StatTag); 
}

void UItemInstance::SetStatValue(FGameplayTag StatTag, float StatValue)
{
	StatsMap.Add(StatTag, StatValue);
}