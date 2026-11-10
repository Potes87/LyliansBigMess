// Fill out your copyright notice in the Description page of Project Settings.

#include "PassiveTreeComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PassiveEffect.h"
#include "PlayerProgressSaveGame.h"

int32 UPassiveTreeComponent::GetTotalPoints() const
{
	return FMath::Max(0, PlayerLevel - 1) * PointsPerLevel;
}

void UPassiveTreeComponent::SetPlayerLevel(int32 NewLevel)
{
	if (NewLevel == PlayerLevel) return;

	PlayerLevel = NewLevel;
	OnPassiveTreeChanged.Broadcast();
}

bool UPassiveTreeComponent::SaveProgress(float CurrentXP)
{
	UPlayerProgressSaveGame* SaveGame = Cast<UPlayerProgressSaveGame>(UGameplayStatics::CreateSaveGameObject(UPlayerProgressSaveGame::StaticClass()));
	if (!SaveGame) return false;

	SaveGame->PlayerLevel = PlayerLevel;
	SaveGame->CurrentXP = CurrentXP;
	SaveGame->AllocatedNodes = AllocatedNodes;

	const bool bSaved = UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlotName, 0);
	if (!bSaved)
	{
		UE_LOG(LogTemp, Warning, TEXT("PassiveTreeComponent couldn't write save slot %s."), *SaveSlotName);
	}
	return bSaved;
}

bool UPassiveTreeComponent::LoadProgress(int32& Level, float& CurrentXP)
{
	Level = PlayerLevel;
	CurrentXP = 0.f;

	const UPlayerProgressSaveGame* SaveGame = Cast<UPlayerProgressSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));
	if (!SaveGame) return false;

	Level = FMath::Max(1, SaveGame->PlayerLevel);
	CurrentXP = SaveGame->CurrentXP;
	PlayerLevel = Level;

	RefundAllWithoutBroadcast();
	RestoreNodes(SaveGame->AllocatedNodes);
	OnPassiveTreeChanged.Broadcast();
	return true;
}

float UPassiveTreeComponent::NotifyShotFired(float Damage)
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		Damage = ActiveEffect.Value->OnShotFired(Damage);
	}
	return Damage;
}

float UPassiveTreeComponent::NotifyEnemyHit(AActor* Enemy, float Damage)
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		Damage = ActiveEffect.Value->OnEnemyHit(Enemy, Damage);
	}
	return Damage;
}

void UPassiveTreeComponent::NotifyReloaded()
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		ActiveEffect.Value->OnReloaded();
	}
}

bool UPassiveTreeComponent::CanFireShot() const
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		if (!ActiveEffect.Value->CanFireShot()) return false;
	}
	return true;
}

int32 UPassiveTreeComponent::GetAmmoCost() const
{
	int32 Cost = 1;
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		Cost = ActiveEffect.Value->GetAmmoCost(Cost);
	}
	return FMath::Max(0, Cost);
}

bool UPassiveTreeComponent::CanReload() const
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		if (!ActiveEffect.Value->CanReload()) return false;
	}
	return true;
}

int32 UPassiveTreeComponent::TakeRegeneratedAmmo(float DeltaSeconds, int32 MagazineSize)
{
	for (const TPair<FName, TObjectPtr<UPassiveEffect>>& ActiveEffect : ActiveEffects)
	{
		PendingAmmo += ActiveEffect.Value->GetAmmoRegen(DeltaSeconds, MagazineSize);
	}

	const int32 WholeRounds = FMath::FloorToInt(PendingAmmo);
	PendingAmmo -= WholeRounds;
	return WholeRounds;
}
