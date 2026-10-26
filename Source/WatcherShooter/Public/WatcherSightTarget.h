#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Perception/AISightTargetInterface.h"
#include "WatcherSightTarget.generated.h"

class ACharacter;

/** Head-height stimulus only. Detection range, FOV and memory remain owned by AI Sight. */
UCLASS(NotBlueprintable, Transient)
class WATCHERSHOOTER_API AWatcherSightTarget : public AActor, public IAISightTargetInterface
{
    GENERATED_BODY()

public:
    AWatcherSightTarget();
    virtual void Tick(float DeltaSeconds) override;

    /** AI Sight invokes this only after its native range/FOV gates have passed. */
    virtual UAISense_Sight::EVisibilityResult CanBeSeenFrom(
        const FCanBeSeenFromContext& Context,
        FVector& OutSeenLocation,
        int32& OutNumberOfLoSChecksPerformed,
        int32& OutNumberOfAsyncLosCheckRequested,
        float& OutSightStrength,
        int32* UserData = nullptr,
        const FOnPendingVisibilityQueryProcessedDelegate* Delegate = nullptr) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    bool RefreshHeadHeight();

    UPROPERTY(Transient)
    TWeakObjectPtr<ACharacter> OwningCharacter;
};
