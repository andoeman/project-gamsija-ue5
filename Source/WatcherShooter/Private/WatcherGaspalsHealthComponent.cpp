#include "WatcherGaspalsHealthComponent.h"

#include "GameFramework/Actor.h"

UWatcherGaspalsHealthComponent::UWatcherGaspalsHealthComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

void UWatcherGaspalsHealthComponent::BeginPlay()
{
    Super::BeginPlay();

    if (AActor* Owner = GetOwner())
    {
        Owner->OnTakeAnyDamage.AddUniqueDynamic(
            this, &UWatcherGaspalsHealthComponent::HandleTakeAnyDamage);
    }

    ResetHealth();
}

void UWatcherGaspalsHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (AActor* Owner = GetOwner())
    {
        Owner->OnTakeAnyDamage.RemoveDynamic(
            this, &UWatcherGaspalsHealthComponent::HandleTakeAnyDamage);
    }

    Super::EndPlay(EndPlayReason);
}

bool UWatcherGaspalsHealthComponent::IsAlive() const
{
    return !bDead && FMath::IsFinite(CurrentHealth) && CurrentHealth > 0.0f;
}

void UWatcherGaspalsHealthComponent::ResetHealth()
{
    if (!FMath::IsFinite(MaxHealth) || MaxHealth <= 0.0f)
    {
        MaxHealth = 100.0f;
    }

    const float PreviousHealth = FMath::IsFinite(CurrentHealth) ? CurrentHealth : 0.0f;
    CurrentHealth = MaxHealth;
    bDead = false;

    OnHealthChanged.Broadcast(this, PreviousHealth, CurrentHealth, MaxHealth);
}

void UWatcherGaspalsHealthComponent::HandleTakeAnyDamage(
    AActor* DamagedActor,
    float Damage,
    const UDamageType* DamageType,
    AController* InstigatedBy,
    AActor* DamageCauser)
{
    // DamageType is intentionally left to subscribers or a later damage policy.
    (void)DamageType;

    if (DamagedActor != GetOwner() || !IsAlive()
        || !FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return;
    }

    const float PreviousHealth = CurrentHealth;
    CurrentHealth = FMath::Max(0.0f, PreviousHealth - Damage);
    if (CurrentHealth == PreviousHealth)
    {
        return;
    }

    const bool bDiedThisDamage = CurrentHealth <= 0.0f;
    // Commit before callbacks so another damage event cannot kill this life twice.
    bDead = bDiedThisDamage;
    const float ResultingHealth = CurrentHealth;
    const float MaximumHealth = MaxHealth;

    OnHealthChanged.Broadcast(this, PreviousHealth, ResultingHealth, MaximumHealth);
    if (bDiedThisDamage)
    {
        // This reports the committed transition even if a subscriber resets health.
        OnDied.Broadcast(this, InstigatedBy, DamageCauser);
    }
}
