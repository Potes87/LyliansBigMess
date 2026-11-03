// Fill out your copyright notice in the Description page of Project Settings.


#include "EquipmentInstance.h"
#include "EquipmentDefinition.h"
#include "EquipmentActorInterface.h"
#include "GameFramework/Character.h"
#include "demoBillyHill14//Fragments/InventoryFragment_Equippable.h"
#include "demoBillyHill14//Items/ItemInstance.h"
#include "Components/SkeletalMeshComponent.h"

void UEquipmentInstance::Initialize(UItemInstance* ItemInstance, ACharacter* Character)
{
	if (!ItemInstance) return;
	
	SourceItemInstance = ItemInstance;
	
	const UInventoryFragment_Equippable* EquippableFragment = Cast<UInventoryFragment_Equippable>(ItemInstance->FindFragmentByClass(UInventoryFragment_Equippable::StaticClass()));
	
	if (!EquippableFragment) return;
	
	EquipmentDefinition = EquippableFragment->EquipmentDefinition;
	HandleEquipItem(Character);
}

void UEquipmentInstance::HandleEquipItem(ACharacter* Character)
{
	if (!Character) return;

	UEquipmentDefinition* DefinitionCDO = EquipmentDefinition.GetDefaultObject();
	
	if (!DefinitionCDO) return;

	const TArray<UActorComponent*> MeshMatches = Character->GetComponentsByTag(
		USkeletalMeshComponent::StaticClass(),
		FName("EquipmentMesh"));

	if (MeshMatches.Num() != 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Expected exactly one skeletal mesh component tagged 'EquipmentMesh' on %s; found %d."),
			*GetNameSafe(Character),
			MeshMatches.Num());
		return;
	}

	USkeletalMeshComponent* EquipmentMesh = Cast<USkeletalMeshComponent>(MeshMatches[0]);
	if (!EquipmentMesh) return;
	
	SpawnedEquipmentActor = GetWorld()->SpawnActor(DefinitionCDO->EquipmentActorClass);
	
	if (!SpawnedEquipmentActor) return;
	
	SpawnedEquipmentActor->AttachToComponent(
		EquipmentMesh,
		FAttachmentTransformRules::SnapToTargetIncludingScale,
		DefinitionCDO->AttachSocketName
	);

	// Hand the spawned actor the item it came from (e.g. so a gun can read its rolled stats).
	if (SpawnedEquipmentActor->Implements<UEquipmentActorInterface>())
	{
		IEquipmentActorInterface::Execute_OnEquipped(SpawnedEquipmentActor, SourceItemInstance, Character);
	}
}

void UEquipmentInstance::HandleUnequipItem(ACharacter* Character)
{
	if (SpawnedEquipmentActor)
	{
		SpawnedEquipmentActor->Destroy();
		SpawnedEquipmentActor = nullptr;
	}
}
