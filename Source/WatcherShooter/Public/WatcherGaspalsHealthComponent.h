#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WatcherGaspalsHealthComponent.generated.h"

class AActor;
class AController;
class UDamageType;
class UWatcherGaspalsHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FWatcherGaspalsHealthChanged,
    UWatcherGaspalsHealthComponent*, HealthComponent,
    float, PreviousHealth,
    float, NewHealth,
    float, MaximumHealth);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FWatcherGaspalsDied,
    UWatcherGaspalsHealthComponent*, HealthComponent,
    AController*, InstigatedBy,
    AActor*, DamageCauser);

/**
 * Local prototype extracted from the existing damage/health responsibility.
 * No replication, RPCs, prediction, or multiplayer authority handling is provided.
 * Works with an Actor owner; no Character or movement backend is required.
 * Consumers own animation, movement, collision, ragdoll, and Actor "Dead" tags.
 * Draft only: not installed in a game module or compiled by its author.
 */
UCLASS(ClassGroup=(Watcher), meta=(BlueprintSpawnableComponent))
class WATCHERSHOOTER_API UWatcherGaspalsHealthComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UWatcherGaspalsHealthComponent();

    /** Invalid or non-positive configured values are replaced with 100 on reset. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Watcher|Health",
        meta=(ClampMin="1.0", UIMin="1.0"))
    float MaxHealth = 100.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Watcher|Health")
    float CurrentHealth = 100.0f;

    /** Component state only; does not add or remove an Actor tag. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Watcher|Health")
    bool bDead = false;

    /** Also broadcasts when ResetHealth initializes a new life. */
    UPROPERTY(BlueprintAssignable, Category="Watcher|Health")
    FWatcherGaspalsHealthChanged OnHealthChanged;

    /** Once per transition from living to dead; ResetHealth permits a new life. */
    UPROPERTY(BlueprintAssignable, Category="Watcher|Health")
    FWatcherGaspalsDied OnDied;

    UFUNCTION(BlueprintPure, Category="Watcher|Health")
    bool IsAlive() const;

    /** Restores configured maximum health without changing the owner's other state. */
    UFUNCTION(BlueprintCallable, Category="Watcher|Health")
    void ResetHealth();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleTakeAnyDamage(
        AActor* DamagedActor,
        float Damage,
        const UDamageType* DamageType,
        AController* InstigatedBy,
        AActor* DamageCauser);
};
