// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "RespawnSubsystem.generated.h"

class APawn;

/** Where the player wakes up. Written to disk every time a shrine is touched. */
UCLASS()
class DEMOBILLYHILL14_API UCheckpointSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FString MapName;

	UPROPERTY()
	FName ShrineId;

	UPROPERTY()
	FTransform Transform;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRespawnPlayerSignal, APawn*, Player, FTransform, At);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCheckpointSignal, FName, ShrineId, APawn*, Player);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEnemiesResetSignal);

/**
 * Checkpoints, dying, and bringing the enemies back. One per world, no setup: it exists in every map.
 *
 * THE RULES
 *   - Touching a shrine (ACheckpointShrine) makes it the place you wake up, heals you, and brings back every enemy.
 *   - Dying (call RespawnPlayer) puts you at the last shrine, heals you, and brings back every enemy.
 *   - The map is never reloaded, so the player pawn is the same one: level, XP, passives, runes, gear, ammo and
 *     currency are untouched. Nothing has to be saved or restored for a respawn.
 *   - With no shrine touched yet, you wake where you started the map.
 *
 * HOW ENEMIES COME BACK
 *   Every enemy with a UEnemyLevelComponent (it is on BP_Enemy_Base, so all of them) reports in when it starts.
 *   Enemies that were PLACED in the level get an entry here: their class, where they stand, and their LevelOverride.
 *   Enemies spawned during play (summons) are ignored: they never come back.
 *     - An enemy that dies is marked dead. If you walk away far enough for its part of the map to unload and come
 *       back, World Partition would normally hand you a fresh copy. This subsystem removes that copy, so dead
 *       stays dead until the next shrine or death.
 *     - A reset clears every mark, removes every enemy that is currently loaded (alive, hurt, or chasing you) and
 *       puts fresh ones back at their posts. Far-away enemies simply load fresh when you get there.
 *     - A fresh enemy is a new actor of the same class at the same spot. Settings changed on ONE placed enemy in
 *       the level are not carried over, except LevelOverride. Put per-enemy differences in a Blueprint child.
 *
 * WAKING UP FAR AWAY
 *   The shrine may be in a part of the map that is not loaded. The player is moved there, held in place behind a
 *   black screen until there is ground under the spot (or MaxHoldSeconds passes), then let go.
 */
UCLASS()
class DEMOBILLYHILL14_API URespawnSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	static URespawnSubsystem* Get(const UObject* WorldContextObject);

	// ---- Call these ----

	/** The player died: back to the last shrine, healed, every enemy returned. Player = null uses player 0. */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void RespawnPlayer(APawn* Player = nullptr);

	/** Same as RespawnPlayer, callable from anywhere (a widget button, for example) with no subsystem node. */
	UFUNCTION(BlueprintCallable, Category = "Respawn", meta = (WorldContext = "WorldContextObject"))
	static void RespawnPlayerNow(const UObject* WorldContextObject);

	/** A shrine was touched. Sets the wake-up point, saves it, heals the player and returns every enemy. */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void ActivateCheckpoint(FName ShrineId, FTransform WakeTransform, APawn* Player);

	/** Bring every enemy back without touching the player. Near = where the player is (or is about to be). */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void ResetEnemies(FVector Near);

	/** Fill the player's health, even from zero. Works on the Blueprint health component (BPC_Health). */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	static void RestoreFullHealth(AActor* Actor);

	/** Forget the saved shrine (new game). */
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void ClearSavedCheckpoint();

	UFUNCTION(BlueprintPure, Category = "Respawn")
	bool HasCheckpoint() const { return bHasCheckpoint; }

	UFUNCTION(BlueprintPure, Category = "Respawn")
	FName GetCheckpointId() const { return CheckpointId; }

	/** Where the player would wake up right now. */
	UFUNCTION(BlueprintPure, Category = "Respawn")
	FTransform GetWakeTransform() const;

	/** How many placed enemies are known, and how many of them are currently dead. For debugging. */
	UFUNCTION(BlueprintPure, Category = "Respawn")
	void GetEnemyCounts(int32& Known, int32& Dead) const;

	// ---- Listen to these ----

	/** After the player has been put back and let go. Good place for a sound or a "you woke" effect. */
	UPROPERTY(BlueprintAssignable, Category = "Respawn")
	FRespawnPlayerSignal OnPlayerRespawned;

	/** A shrine was touched. */
	UPROPERTY(BlueprintAssignable, Category = "Respawn")
	FCheckpointSignal OnCheckpointActivated;

	/** Every enemy was just returned (shrine or death). */
	UPROPERTY(BlueprintAssignable, Category = "Respawn")
	FEnemiesResetSignal OnEnemiesReset;

	// ---- Tuning ----

	/** Fresh enemies are put back when the player is within this distance of their post (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn|Tuning")
	float SpawnDistance = 20000.f;

	/** Enemies this subsystem put back are removed again beyond this distance (cm), and return when you come back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn|Tuning")
	float CullDistance = 30000.f;

	/** Seconds the screen takes to come back from black after waking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn|Tuning")
	float FadeInSeconds = 0.6f;

	/** Longest the player is held waiting for the ground to load before being let go anyway. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn|Tuning")
	float MaxHoldSeconds = 6.f;

	/** When a map starts, wake at the saved shrine instead of the Player Start. Off while playing in the editor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Respawn|Tuning")
	bool bStartAtSavedCheckpoint = true;

	// ---- Used by UEnemyLevelComponent ----
	void EnemyStarted(AActor* Enemy, int32 LevelOverride);
	void EnemyEnded(AActor* Enemy, EEndPlayReason::Type Reason);

private:
	struct FEnemyEntry
	{
		TSubclassOf<AActor> Class;
		FTransform Home;
		int32 LevelOverride = 0;
		bool bDead = false;
		// The placed original was removed while its part of the map stayed loaded, so this subsystem must put a new one back.
		bool bOriginalGone = false;
		// The live actor is one this subsystem spawned (not the placed original).
		bool bActorIsOurs = false;
		TWeakObjectPtr<AActor> Actor;
	};

	TMap<FName, FEnemyEntry> Entries;
	TMap<TWeakObjectPtr<AActor>, FName> ActorToKey;
	TSet<TWeakObjectPtr<AActor>> Silenced;     // being removed by us: do not count as a kill
	bool bSpawning = false;                     // an enemy starting right now is one we are spawning
	FName SpawningKey;

	bool bHasCheckpoint = false;
	FName CheckpointId;
	FTransform CheckpointTransform;
	bool bHasStart = false;
	FTransform StartTransform;

	FTimerHandle UpkeepTimer;
	FTimerHandle HoldTimer;
	FTimerHandle StartTimer;
	TWeakObjectPtr<APawn> HeldPawn;
	FTransform HoldTransform;
	float HoldElapsed = 0.f;
	bool bBroadcastOnRelease = false;

	static const FString SaveSlot;

	void Upkeep();
	void SpawnPass(const FVector& Near);
	AActor* SpawnFresh(FName Key, FEnemyEntry& Entry);
	void RemoveSilently(AActor* Actor);
	APawn* PlayerPawn(APawn* Preferred = nullptr) const;
	void RememberStart(APawn* Pawn);
	void MoveAndHold(APawn* Pawn, const FTransform& To, bool bBroadcast);
	void HoldTick();
	void Release();
	void TryStartAtCheckpoint();
	void SaveCheckpoint() const;
	FString MapName() const;
	static bool IsDeadByHealth(const AActor* Actor);
};
