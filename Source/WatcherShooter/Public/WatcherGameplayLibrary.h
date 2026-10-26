#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WatcherGameplayLibrary.generated.h"

class ACharacter;
class AAIController;
class AActor;
class APlayerController;
class UAnimMontage;
class UAnimInstance;

/**
 * Gameplay query library with runtime-safe guards.
 * - No movement, damage, collision-mode, teleport, or Blueprint-original changes.
 * - FindLowVault computes space for reused GASP two-hand/one-hand vaults
 *   with MotionWarping 3 targets (EntryLift/Across/Exit).
 *   Actual montage/warp playback stays in the existing traversal runtime.
 * - ConfigureGuardSenses / GuardCanSee only configure and query Sight sense.
 *   They never write LastSeen; existing BP keeps LastSeen handling.
 * - All setup/query paths are null/dead-tag/invalid-world safe.
 * - Module export is WATCHERSHOOTER_API for the WatcherShooter module.
 */
UCLASS()
class WATCHERSHOOTER_API UWatcherGameplayLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
    /** Development diagnostics use the actual baked state machine, not pose heuristics. */
    UFUNCTION(BlueprintPure, Category = "Watcher|Testing")
    static FName GetAnimationStateName(UAnimInstance* Animation, FName MachineName);

    /** Editor scripting bridge for UE's built-in optional AnimGraph pins. */
    UFUNCTION(BlueprintCallable, Category = "Watcher|Editor")
    static bool ExposeAnimNodePinForEditor(UObject* Node, FName PinName);

	/**
	 * Pure spatial check for a low vault. No movement or damage is applied.
	 * OutEntry = current XY with measured walkable-floor Z.
	 * OutTopNear = front face -25cm (45cm point-blank), TopZ+1cm.
	 * OutTopFar = back face +45cm along vault dir, TopZ+1cm.
	 * OutExit = back face +195cm along vault dir, exit-ground+5cm.
	 * OutFacing = vault direction (-front impact normal XY) yaw only.
	 * Returns true only when every gate passes; otherwise OutRejectReason explains why.
	 */
	UFUNCTION(BlueprintCallable, Category = "Watcher|Traversal", meta = (DisplayName = "Find Low Vault"))
	static bool FindLowVault(
		ACharacter* Character,
		FVector& OutEntry,
		FVector& OutTopNear,
		FVector& OutTopFar,
		FVector& OutExit,
		FRotator& OutFacing,
		FString& OutRejectReason);

	/** Select a stable height profile from query outputs. Entry.Z is floor Z. */
	UFUNCTION(BlueprintPure, Category = "Watcher|Traversal")
	static UAnimMontage* SelectVaultMontage(FVector Entry, FVector TopNear);

	/** One-time context selection. Capture ApproachSpeed before stopping movement. */
	UFUNCTION(BlueprintPure, Category = "Watcher|Traversal")
	static UAnimMontage* ChooseContextVault(ACharacter* Character, FVector Entry, FVector TopNear,
		FVector Exit, float ApproachSpeed);

	/** Conservative standing-capsule corridor and flat floor test for a landing roll. */
	UFUNCTION(BlueprintCallable, Category = "Watcher|Traversal")
	static bool CanRollFromExit(ACharacter* Character, FVector Exit, FVector Direction, FString& RejectReason);

	/**
	 * Create/register UAIPerceptionComponent and configure UAISenseConfig_Sight.
	 * Detects all affiliations. MaxAge=4. AutoSuccessRangeFromLastSeenLocation=-1
	 * disables automatic re-detection from memory.
	 */
	UFUNCTION(BlueprintCallable, Category = "Watcher|AI", meta = (DisplayName = "Configure Guard Senses"))
	static void ConfigureGuardSenses(
		AAIController* Controller,
		float SightRadius = 1800.0f,
		float LoseSightRadius = 2200.0f,
		float HalfFovDeg = 60.0f);

	/**
	 * True only when Sight currently perceives Target, hard distance passes,
	 * and at least one eye->head/torso visibility sample is clear.
	 * Ignores guard pawn only. Never updates LastSeen.
	 */
	UFUNCTION(BlueprintCallable, Category = "Watcher|AI", meta = (DisplayName = "Guard Can See"))
	static bool GuardCanSee(
		AAIController* Controller,
		AActor* Target,
		float HardLimit = 2200.0f);

	/** Engine input routing QA only; does not synthesize Windows input. */
	UFUNCTION(BlueprintCallable, Category = "Watcher|Testing", meta = (DevelopmentOnly))
	static bool SetJumpKeyForTest(APlayerController* Controller, bool bPressed);
};
