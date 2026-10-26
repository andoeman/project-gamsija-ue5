#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "WatcherGaspalsCombatComponent.generated.h"

class AActor;
class AController;
class UEnhancedInputComponent;
class UInputAction;
class UWatcherGaspalsHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FWatcherGaspalsAmmoChanged, int32, MagazineAmmo, int32, ReserveAmmo, bool, bReloading);

/**
 * Local prototype adapter; not installed or compiled by its author.
 * Adds bindings only for project-owned actions. Does not add mapping contexts,
 * replace GASPALS bindings, or change normal movement/camera/animation settings.
 * The explicit death compatibility path disables Character movement/capsule and
 * controller input for the existing enemy's dead-target checks. A project-owned
 * restart action reloads the map; health reset alone does not restore movement.
 * No replication, RPCs, prediction, ragdoll, hit reactions, or fire animation.
 * Requires the matching WatcherGaspalsHealthComponent draft in the same module.
 */
UCLASS(ClassGroup=(Watcher), meta=(BlueprintSpawnableComponent))
class WATCHERSHOOTER_API UWatcherGaspalsCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWatcherGaspalsCombatComponent();

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Input")
    TObjectPtr<UInputAction> FireAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Input")
    TObjectPtr<UInputAction> ReloadAction;

    /** Root owns the F5 mapping; this component only binds its Started event. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Input")
    TObjectPtr<UInputAction> RestartAction;

    /** Assign the migrated BP_ShooterProjectile_Bullet class; no new damage code. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Projectile")
    TSubclassOf<AActor> ProjectileClass;

    /** Exact spawned equipment class path. A missing/different item blocks firing. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Equipment")
    FString RequiredPistolClassPath = TEXT("/GASPALS/Systems/EquipmentSystem/Actors/BP_Equipment_Pistol.BP_Equipment_Pistol_C");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Equipment")
    FName MuzzleSocket = TEXT("Muzzle");

    /** Mesh-local cm, used only without a muzzle socket; tune to the real asset. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Equipment")
    FVector MuzzleLocalOffset = FVector(40.0, 0.0, 0.0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Ammo", meta=(ClampMin="1"))
    int32 MagazineCapacity = 12;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Ammo", meta=(ClampMin="0"))
    int32 InitialReserveAmmo = 48;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Watcher|Combat|Ammo")
    int32 MagazineAmmo = 12;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Watcher|Combat|Ammo")
    int32 ReserveAmmo = 48;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Ammo", meta=(ClampMin="0.01"))
    float ReloadSeconds = 1.4f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Ammo", meta=(ClampMin="0.01"))
    float RefireSeconds = 0.2f;

    /** False is semi-automatic: one shot per Started, release before next shot. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Combat|Ammo")
    bool bAutomaticFire = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Watcher|Combat|Ammo")
    bool bReloading = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Watcher|Combat|Debug")
    FString LastBlockReason;

    UPROPERTY(BlueprintAssignable, Category="Watcher|Combat")
    FWatcherGaspalsAmmoChanged OnAmmoChanged;

    UFUNCTION(BlueprintCallable, Category="Watcher|Combat")
    bool TryFire();

    UFUNCTION(BlueprintCallable, Category="Watcher|Combat")
    void StopFire();

    UFUNCTION(BlueprintCallable, Category="Watcher|Combat")
    bool TryReload();

    UFUNCTION(BlueprintCallable, Category="Watcher|Combat")
    void CancelReload();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    void TryBindInput();
    void RemoveOwnedBindings();
    void BindHealth();
    void HandleFireStarted();
    void HandleReloadStarted();
    void HandleRestartStarted();
    void HandleAutomaticFire();
    void FinishReload();
    bool CanUseEquipment(AActor*& OutEquipment, FString& OutReason) const;
    bool ReadTraversalActive(bool& bOutActive) const;
    bool ReadAimPoint(FVector& OutPoint) const;
    bool ReadMuzzle(AActor* Equipment, FVector& OutLocation) const;
    bool SpawnLegacyBullet(const FVector& Muzzle, const FVector& Direction);
    void BroadcastAmmo();

    UFUNCTION()
    void HandleOwnerDied(UWatcherGaspalsHealthComponent* HealthComponent,
        AController* InstigatedBy, AActor* DamageCauser);

    UPROPERTY(Transient)
    TObjectPtr<UClass> TraversalInterfaceClass;

    TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
    TWeakObjectPtr<UWatcherGaspalsHealthComponent> BoundHealth;
    TWeakObjectPtr<AActor> ReloadEquipment;
    TArray<uint32> OwnedInputHandles;
    FTimerHandle FireTimer;
    FTimerHandle ReloadTimer;
    double LastShotTime = -1.0e30;
    bool bTriggerHeld = false;
    bool bDeathApplied = false;
};
