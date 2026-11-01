// Fill out your copyright notice in the Description page of Project Settings.


#include "EquipmentInstance.h"
#include "EquipmentDefinition.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "demoBillyHill14//Fragments/InventoryFragment_Equippable.h"
#include "demoBillyHill14//Items/ItemInstance.h"

void UEquipmentInstance::Initialize(UItemInstance* ItemInstance, ACharacter* Character)
{
	if (!ItemInstance) return;
	
	SourceItemInstance = ItemInstance;
	
	const UInventoryFragment_Equippable* EquippableFragment = Cast<UInventoryFragment_Equippable>(
		ItemInstance->FindFragmentByClass(UInventoryFragment_Equippable::StaticClass())
	);
	
	if (!EquippableFragment) return;
	
	EquipmentDefinition = EquippableFragment->EquipmentDefinition;
	HandleEquipItem(Character);
}

void UEquipmentInstance::HandleEquipItem(ACharacter* Character)
{
	UEquipmentDefinition* DefinitionCDO = EquipmentDefinition.GetDefaultObject();
	
	if (!DefinitionCDO) return;
	
	SpawnedEquipmentActor = GetWorld()->SpawnActor(DefinitionCDO->EquipmentActorClass);
	
	if (!SpawnedEquipmentActor) return;
	
	SpawnedEquipmentActor->AttachToComponent(
		Character->GetMesh(),
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		DefinitionCDO->AttachSocketName
	);
	
	float MovementSpeedIncrease = SourceItemInstance->GetStatValue(FGameplayTag::RequestGameplayTag(FName("Item.Stat.MovementSpeed")));
	
	Character->GetCharacterMovement()->MaxWalkSpeed += MovementSpeedIncrease;
}

void UEquipmentInstance::HandleUnequipItem(ACharacter* Character)
{
	if (SpawnedEquipmentActor)
	{
		SpawnedEquipmentActor->Destroy();
		SpawnedEquipmentActor = nullptr;
	}
	
	float MovementSpeedIncrease = SourceItemInstance->GetStatValue(FGameplayTag::RequestGameplayTag(FName("Item.Stat.MovementSpeed")));
	
	Character->GetCharacterMovement()->MaxWalkSpeed -= MovementSpeedIncrease;
}
