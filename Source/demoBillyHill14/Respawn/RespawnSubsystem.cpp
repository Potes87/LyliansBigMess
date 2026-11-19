// Fill out your copyright notice in the Description page of Project Settings.

#include "RespawnSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/ActorComponent.h"
#include "../Enemies/EnemyLevelComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

const FString URespawnSubsystem::SaveSlot = TEXT("PlayerCheckpoint");

namespace
{
	// The Blueprint health component (BPC_Health), found by its variables so C++ does not need the Blueprint class.
	UActorComponent* FindHealthComponent(const AActor* Actor)
	{
		if (!Actor) return nullptr;
		for (UActorComponent* Comp : Actor->GetComponents())
		{
			if (Comp && FindFProperty<FNumericProperty>(Comp->GetClass(), TEXT("CurrentHealth"))
				&& FindFProperty<FNumericProperty>(Comp->GetClass(), TEXT("MaxHealth")))
			{
				return Comp;
			}
		}
		return nullptr;
	}

	bool ReadNumber(const UObject* Obj, const TCHAR* Name, double& Out)
	{
		const FNumericProperty* Prop = Obj ? FindFProperty<FNumericProperty>(Obj->GetClass(), Name) : nullptr;
		if (!Prop) return false;
		const void* Value = Prop->ContainerPtrToValuePtr<void>(Obj);
		Out = Prop->IsFloatingPoint() ? Prop->GetFloatingPointPropertyValue(Value) : (double)Prop->GetSignedIntPropertyValue(Value);
		return true;
	}

	void WriteNumber(UObject* Obj, const TCHAR* Name, double In)
	{
		FNumericProperty* Prop = Obj ? FindFProperty<FNumericProperty>(Obj->GetClass(), Name) : nullptr;
		if (!Prop) return;
		void* Value = Prop->ContainerPtrToValuePtr<void>(Obj);
		if (Prop->IsFloatingPoint()) Prop->SetFloatingPointPropertyValue(Value, In);
		else Prop->SetIntPropertyValue(Value, (int64)FMath::RoundToDouble(In));
	}

	// Call a Blueprint function that takes one number, by name. Returns false if the function is not there.
	bool CallWithNumber(UObject* Obj, const TCHAR* FunctionName, double Number)
	{
		UFunction* Fn = Obj ? Obj->FindFunction(FunctionName) : nullptr;
		if (!Fn) return false;
		const int32 Size = FMath::Max<int32>(Fn->ParmsSize, 1);
		uint8* Params = (uint8*)FMemory_Alloca(Size);
		FMemory::Memzero(Params, Size);
		bool bSet = false;
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->InitializeValue_InContainer(Params);
			if (bSet || It->HasAnyPropertyFlags(CPF_ReturnParm | CPF_OutParm)) continue;
			if (const FNumericProperty* Num = CastField<FNumericProperty>(*It))
			{
				void* Value = Num->ContainerPtrToValuePtr<void>(Params);
				if (Num->IsFloatingPoint()) Num->SetFloatingPointPropertyValue(Value, Number);
				else Num->SetIntPropertyValue(Value, (int64)FMath::RoundToDouble(Number));
				bSet = true;
			}
		}
		Obj->ProcessEvent(Fn, Params);
		for (TFieldIterator<FProperty> It(Fn); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			It->DestroyValue_InContainer(Params);
		}
		return true;
	}

	// Type these in the ~ console while playing.
	FAutoConsoleCommandWithWorld CmdRespawn(
		TEXT("Daybreak.Respawn"),
		TEXT("Wake at the last shrine, healed, with every enemy returned (what dying does)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (URespawnSubsystem* S = World ? World->GetSubsystem<URespawnSubsystem>() : nullptr) S->RespawnPlayer();
		}));

	FAutoConsoleCommandWithWorld CmdResetEnemies(
		TEXT("Daybreak.ResetEnemies"),
		TEXT("Return every enemy without moving the player (what touching a shrine does)."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			URespawnSubsystem* S = World ? World->GetSubsystem<URespawnSubsystem>() : nullptr;
			const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
			if (S) S->ResetEnemies(Pawn ? Pawn->GetActorLocation() : FVector::ZeroVector);
		}));

	FAutoConsoleCommandWithWorld CmdEnemyCounts(
		TEXT("Daybreak.EnemyCounts"),
		TEXT("Print how many placed enemies are known and how many are dead."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const URespawnSubsystem* S = World ? World->GetSubsystem<URespawnSubsystem>() : nullptr)
			{
				int32 Known = 0, Dead = 0;
				S->GetEnemyCounts(Known, Dead);
				UE_LOG(LogTemp, Display, TEXT("Respawn: %d placed enemies known, %d dead, shrine '%s'"), Known, Dead, *S->GetCheckpointId().ToString());
			}
		}));
}

// ---------------------------------------------------------------------------------------------------- lifetime

bool URespawnSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

URespawnSubsystem* URespawnSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<URespawnSubsystem>() : nullptr;
}

void URespawnSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// The last shrine touched in this map, from disk.
	if (const UCheckpointSaveGame* Save = Cast<UCheckpointSaveGame>(UGameplayStatics::LoadGameFromSlot(SaveSlot, 0)))
	{
		if (Save->MapName == MapName())
		{
			bHasCheckpoint = true;
			CheckpointId = Save->ShrineId;
			CheckpointTransform = Save->Transform;
		}
	}

	FTimerManager& Timers = InWorld.GetTimerManager();
	Timers.SetTimer(UpkeepTimer, this, &URespawnSubsystem::Upkeep, 0.5f, true);
	Timers.SetTimer(StartTimer, this, &URespawnSubsystem::TryStartAtCheckpoint, 0.1f, true);
}

void URespawnSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Entries.Empty();
	ActorToKey.Empty();
	Silenced.Empty();
	Super::Deinitialize();
}

FString URespawnSubsystem::MapName() const
{
	return UGameplayStatics::GetCurrentLevelName(GetWorld(), true);
}

APawn* URespawnSubsystem::PlayerPawn(APawn* Preferred) const
{
	if (Preferred) return Preferred;
	return UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

void URespawnSubsystem::RememberStart(APawn* Pawn)
{
	if (bHasStart || !Pawn) return;
	bHasStart = true;
	StartTransform = FTransform(FRotator(0.f, Pawn->GetActorRotation().Yaw, 0.f), Pawn->GetActorLocation());
}

void URespawnSubsystem::TryStartAtCheckpoint()
{
	APawn* Pawn = PlayerPawn();
	if (!Pawn) return;                       // not spawned yet, try again in a moment

	UWorld* World = GetWorld();
	World->GetTimerManager().ClearTimer(StartTimer);
	RememberStart(Pawn);

	// In the editor you start where you pressed Play. In the real game you wake at your shrine.
	if (bHasCheckpoint && bStartAtSavedCheckpoint && World->WorldType == EWorldType::Game)
	{
		MoveAndHold(Pawn, CheckpointTransform, false);
	}
}

// ---------------------------------------------------------------------------------------------------- enemies

void URespawnSubsystem::EnemyStarted(AActor* Enemy, int32 LevelOverride)
{
	if (!Enemy || bSpawning) return;          // bSpawning: SpawnFresh fills the entry in itself
	if (!Enemy->IsNetStartupActor()) return;  // spawned during play (a summon): never comes back

	const FName Key = Enemy->GetFName();
	FEnemyEntry* Entry = Entries.Find(Key);
	if (!Entry)
	{
		Entry = &Entries.Add(Key);
		Entry->Class = Enemy->GetClass();
		Entry->Home = Enemy->GetActorTransform();
		Entry->LevelOverride = LevelOverride;
	}

	if (Entry->bDead || Entry->Actor.IsValid())
	{
		// Its part of the map was loaded again and handed us a fresh copy of an enemy that is dead, or that this
		// subsystem has already replaced. Take the copy away before it is seen. (Next tick: it is mid-BeginPlay now.)
		Entry->bOriginalGone = true;
		Enemy->SetActorHiddenInGame(true);
		Enemy->SetActorEnableCollision(false);
		Enemy->SetActorTickEnabled(false);
		TWeakObjectPtr<AActor> Weak(Enemy);
		GetWorld()->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this, Weak]()
		{
			if (AActor* A = Weak.Get()) RemoveSilently(A);
		}));
		return;
	}

	Entry->Actor = Enemy;
	Entry->bActorIsOurs = false;
	Entry->bOriginalGone = false;
	ActorToKey.Add(Enemy, Key);
}

void URespawnSubsystem::EnemyEnded(AActor* Enemy, EEndPlayReason::Type Reason)
{
	if (!Enemy) return;
	const TWeakObjectPtr<AActor> Weak(Enemy);
	const bool bSilent = Silenced.Remove(Weak) > 0;

	FName Key;
	if (!ActorToKey.RemoveAndCopyValue(Weak, Key)) return;
	FEnemyEntry* Entry = Entries.Find(Key);
	if (!Entry) return;

	const bool bWasOurs = Entry->bActorIsOurs;
	if (Entry->Actor.Get() == Enemy || !Entry->Actor.IsValid())
	{
		Entry->Actor = nullptr;
		Entry->bActorIsOurs = false;
	}
	if (bSilent) return;                      // we removed it ourselves: not a kill

	if (Reason == EEndPlayReason::Destroyed)
	{
		Entry->bDead = true;                  // killed (enemies are only destroyed by dying)
		if (!bWasOurs) Entry->bOriginalGone = true;
	}
	else if (IsDeadByHealth(Enemy))
	{
		Entry->bDead = true;                  // its part of the map unloaded while the body was still lying there
	}
}

bool URespawnSubsystem::IsDeadByHealth(const AActor* Actor)
{
	double Health = 1.0;
	const UActorComponent* Comp = FindHealthComponent(Actor);
	return Comp && ReadNumber(Comp, TEXT("CurrentHealth"), Health) && Health <= 0.0;
}

void URespawnSubsystem::RemoveSilently(AActor* Actor)
{
	if (!Actor || Actor->IsActorBeingDestroyed()) return;
	Silenced.Add(Actor);
	Actor->Destroy();
}

AActor* URespawnSubsystem::SpawnFresh(FName Key, FEnemyEntry& Entry)
{
	UWorld* World = GetWorld();
	if (!World || !Entry.Class) return nullptr;

	TGuardValue<bool> Guard(bSpawning, true);
	AActor* Actor = World->SpawnActorDeferred<AActor>(Entry.Class, Entry.Home, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Actor) return nullptr;

	if (UEnemyLevelComponent* Level = Actor->FindComponentByClass<UEnemyLevelComponent>())
	{
		Level->LevelOverride = Entry.LevelOverride;
	}
	Actor->FinishSpawning(Entry.Home);

	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		if (!Pawn->GetController()) Pawn->SpawnDefaultController();   // enemies only get their AI by themselves when placed
	}

	Entry.Actor = Actor;
	Entry.bActorIsOurs = true;
	ActorToKey.Add(Actor, Key);
	return Actor;
}

void URespawnSubsystem::SpawnPass(const FVector& Near)
{
	const double MaxSq = FMath::Square((double)SpawnDistance);
	for (TPair<FName, FEnemyEntry>& Pair : Entries)
	{
		FEnemyEntry& Entry = Pair.Value;
		if (Entry.bDead || !Entry.bOriginalGone || Entry.Actor.IsValid()) continue;
		if (FVector::DistSquared(Entry.Home.GetLocation(), Near) > MaxSq) continue;
		SpawnFresh(Pair.Key, Entry);
	}
}

void URespawnSubsystem::ResetEnemies(FVector Near)
{
	for (TPair<FName, FEnemyEntry>& Pair : Entries)
	{
		FEnemyEntry& Entry = Pair.Value;
		Entry.bDead = false;
		if (AActor* Actor = Entry.Actor.Get())
		{
			if (!Entry.bActorIsOurs) Entry.bOriginalGone = true;
			RemoveSilently(Actor);            // alive, hurt, chasing, or a body: all go
		}
		Entry.Actor = nullptr;
		Entry.bActorIsOurs = false;
	}
	SpawnPass(Near);
	OnEnemiesReset.Broadcast();
}

void URespawnSubsystem::Upkeep()
{
	APawn* Pawn = PlayerPawn();
	if (!Pawn) return;
	RememberStart(Pawn);

	const FVector Near = HeldPawn.IsValid() ? HoldTransform.GetLocation() : Pawn->GetActorLocation();

	// Enemies we put back do not belong to any part of the map, so nothing unloads them. Do it by distance.
	const double CullSq = FMath::Square((double)FMath::Max(CullDistance, SpawnDistance + 2000.f));
	for (TPair<FName, FEnemyEntry>& Pair : Entries)
	{
		FEnemyEntry& Entry = Pair.Value;
		AActor* Actor = Entry.bActorIsOurs ? Entry.Actor.Get() : nullptr;
		if (!Actor) continue;
		if (FVector::DistSquared(Actor->GetActorLocation(), Near) > CullSq && !IsDeadByHealth(Actor))
		{
			RemoveSilently(Actor);
			Entry.Actor = nullptr;
			Entry.bActorIsOurs = false;
		}
	}
	SpawnPass(Near);
}

void URespawnSubsystem::GetEnemyCounts(int32& Known, int32& Dead) const
{
	Known = Entries.Num();
	Dead = 0;
	for (const TPair<FName, FEnemyEntry>& Pair : Entries)
	{
		if (Pair.Value.bDead) ++Dead;
	}
}

// ---------------------------------------------------------------------------------------------------- player

FTransform URespawnSubsystem::GetWakeTransform() const
{
	if (bHasCheckpoint) return CheckpointTransform;
	if (bHasStart) return StartTransform;
	return FTransform::Identity;
}

void URespawnSubsystem::RestoreFullHealth(AActor* Actor)
{
	UActorComponent* Health = FindHealthComponent(Actor);
	if (!Health) return;

	double Max = 0.0, Current = 0.0;
	ReadNumber(Health, TEXT("MaxHealth"), Max);
	ReadNumber(Health, TEXT("CurrentHealth"), Current);

	// BPC_Health's Heal does nothing at zero health (dead things stay dead), so lift it off zero first.
	// Heal then fills it to the top and tells the HUD.
	if (Current <= 0.0) WriteNumber(Health, TEXT("CurrentHealth"), FMath::Min(1.0, FMath::Max(Max, 1.0)));
	if (!CallWithNumber(Health, TEXT("Heal"), 1.0e9))
	{
		WriteNumber(Health, TEXT("CurrentHealth"), Max);
	}
}

void URespawnSubsystem::RespawnPlayerNow(const UObject* WorldContextObject)
{
	if (URespawnSubsystem* S = Get(WorldContextObject)) S->RespawnPlayer();
}

void URespawnSubsystem::RespawnPlayer(APawn* Player)
{
	APawn* Pawn = PlayerPawn(Player);
	if (!Pawn) return;
	RememberStart(Pawn);

	// The death screen leaves the game in menu mode: hand control back.
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}
	UGameplayStatics::SetGamePaused(this, false);

	const FTransform To = GetWakeTransform();
	ResetEnemies(To.GetLocation());
	RestoreFullHealth(Pawn);
	MoveAndHold(Pawn, To, true);
}

void URespawnSubsystem::ActivateCheckpoint(FName ShrineId, FTransform WakeTransform, APawn* Player)
{
	bHasCheckpoint = true;
	CheckpointId = ShrineId;
	CheckpointTransform = FTransform(FRotator(0.f, WakeTransform.Rotator().Yaw, 0.f), WakeTransform.GetLocation());
	SaveCheckpoint();

	APawn* Pawn = PlayerPawn(Player);
	RestoreFullHealth(Pawn);
	ResetEnemies(Pawn ? Pawn->GetActorLocation() : CheckpointTransform.GetLocation());
	OnCheckpointActivated.Broadcast(ShrineId, Pawn);
}

void URespawnSubsystem::SaveCheckpoint() const
{
	UCheckpointSaveGame* Save = Cast<UCheckpointSaveGame>(UGameplayStatics::CreateSaveGameObject(UCheckpointSaveGame::StaticClass()));
	if (!Save) return;
	Save->MapName = MapName();
	Save->ShrineId = CheckpointId;
	Save->Transform = CheckpointTransform;
	UGameplayStatics::SaveGameToSlot(Save, SaveSlot, 0);
}

void URespawnSubsystem::ClearSavedCheckpoint()
{
	bHasCheckpoint = false;
	CheckpointId = NAME_None;
	UGameplayStatics::DeleteGameInSlot(SaveSlot, 0);
}

void URespawnSubsystem::MoveAndHold(APawn* Pawn, const FTransform& To, bool bBroadcast)
{
	UWorld* World = GetWorld();
	if (!World || !Pawn) return;

	const FRotator Facing(0.f, To.Rotator().Yaw, 0.f);
	Pawn->SetActorLocationAndRotation(To.GetLocation(), Facing, false, nullptr, ETeleportType::TeleportPhysics);

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->StopMovementImmediately();
			Move->DisableMovement();          // hold in the air until the ground has loaded
		}
	}
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		PC->SetControlRotation(Facing);
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 1.f, 0.01f, FLinearColor::Black, false, true);
		}
	}

	HeldPawn = Pawn;
	HoldTransform = FTransform(Facing, To.GetLocation());
	HoldElapsed = 0.f;
	bBroadcastOnRelease = bBroadcast;
	World->GetTimerManager().SetTimer(HoldTimer, this, &URespawnSubsystem::HoldTick, 0.05f, true);
}

void URespawnSubsystem::HoldTick()
{
	UWorld* World = GetWorld();
	APawn* Pawn = HeldPawn.Get();
	if (!World || !Pawn)
	{
		Release();
		return;
	}
	HoldElapsed += 0.05f;

	// Keep the player pinned to the spot while the world loads in around it.
	Pawn->SetActorLocation(HoldTransform.GetLocation(), false, nullptr, ETeleportType::TeleportPhysics);

	const FVector Start = HoldTransform.GetLocation() + FVector(0.f, 0.f, 50.f);
	const FVector End = HoldTransform.GetLocation() - FVector(0.f, 0.f, 600.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(RespawnGround), false, Pawn);
	FHitResult Hit;
	const bool bGround = World->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params);

	if ((bGround && HoldElapsed >= 0.15f) || HoldElapsed >= MaxHoldSeconds)
	{
		Release();
	}
}

void URespawnSubsystem::Release()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimer);
	}
	APawn* Pawn = HeldPawn.Get();
	HeldPawn = nullptr;
	if (!Pawn) return;

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_Falling);   // settles onto the ground and becomes walking by itself
		}
	}
	if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeInSeconds, FLinearColor::Black, false, false);
		}
	}
	if (bBroadcastOnRelease)
	{
		OnPlayerRespawned.Broadcast(Pawn, HoldTransform);
	}
}
