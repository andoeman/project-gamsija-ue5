#include "WatcherSightTarget.h"

#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Perception/AIPerceptionSystem.h"
#include "Perception/AISense_Sight.h"

DEFINE_LOG_CATEGORY_STATIC(LogWatcherSightTarget, Log, All);

AWatcherSightTarget::AWatcherSightTarget()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
    PrimaryActorTick.TickInterval = 0.1f;
    bReplicates = false;

    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("SightRoot")));
}

void AWatcherSightTarget::BeginPlay()
{
    Super::BeginPlay();

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!IsValid(Character) || !IsValid(Character->GetCapsuleComponent()))
    {
        Destroy();
        return;
    }

    OwningCharacter = Character;
    if (!AttachToComponent(
        Character->GetCapsuleComponent(),
        FAttachmentTransformRules::SnapToTargetNotIncludingScale) || !RefreshHeadHeight())
    {
        Destroy();
        return;
    }

    if (!UAIPerceptionSystem::RegisterPerceptionStimuliSource(this, UAISense_Sight::StaticClass(), this))
    {
        UE_LOG(LogWatcherSightTarget, Warning, TEXT("Could not register head-height Sight stimulus for %s."), *GetNameSafe(Character));
        Destroy();
    }
}

bool AWatcherSightTarget::RefreshHeadHeight()
{
    ACharacter* Character = OwningCharacter.Get();
    if (!IsValid(Character) || Character->ActorHasTag(FName(TEXT("Dead"))))
    {
        return false;
    }

    const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    if (!IsValid(Capsule) || !IsValid(GetRootComponent()))
    {
        return false;
    }

    // Approximate eye stimulus; crouching follows the current capsule height.
    GetRootComponent()->SetRelativeLocation(
        FVector(0.0, 0.0, Capsule->GetScaledCapsuleHalfHeight() * 0.75));
    return true;
}

void AWatcherSightTarget::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!RefreshHeadHeight())
    {
        Destroy();
    }
}

UAISense_Sight::EVisibilityResult AWatcherSightTarget::CanBeSeenFrom(
    const FCanBeSeenFromContext& Context,
    FVector& OutSeenLocation,
    int32& OutNumberOfLoSChecksPerformed,
    int32& OutNumberOfAsyncLosCheckRequested,
    float& OutSightStrength,
    int32* UserData,
    const FOnPendingVisibilityQueryProcessedDelegate* Delegate)
{
    OutSeenLocation = FVector::ZeroVector;
    OutNumberOfLoSChecksPerformed = 0;
    OutNumberOfAsyncLosCheckRequested = 0;
    OutSightStrength = 0.0f;

    ACharacter* Character = OwningCharacter.Get();
    UWorld* World = GetWorld();
    if (!IsValid(Character) || !IsValid(World) || Character->ActorHasTag(FName(TEXT("Dead"))))
    {
        return UAISense_Sight::EVisibilityResult::NotVisible;
    }

    FCollisionQueryParams Query(TEXT("WatcherSightTargetVisibility"), false);
    Query.AddIgnoredActor(this);
    if (Context.IgnoreActor != nullptr)
    {
        Query.AddIgnoredActor(Context.IgnoreActor);
    }

    const FVector Samples[] = {GetActorLocation(), Character->GetActorLocation()};
    for (const FVector& Sample : Samples)
    {
        FHitResult Hit;
        ++OutNumberOfLoSChecksPerformed;
        const bool bBlocked = World->LineTraceSingleByChannel(
            Hit, Context.ObserverLocation, Sample, ECC_Visibility, Query);
        const AActor* HitActor = Hit.GetActor();

        // The pawn owns the stimulus proxy, so a pawn hit must be accepted here.
        if (!bBlocked || HitActor == Character ||
            (HitActor != nullptr && HitActor->IsOwnedBy(Character)))
        {
            OutSeenLocation = Sample;
            OutSightStrength = 1.0f;
            return UAISense_Sight::EVisibilityResult::Visible;
        }
    }

    return UAISense_Sight::EVisibilityResult::NotVisible;
}

void AWatcherSightTarget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UAIPerceptionSystem* Perception = UAIPerceptionSystem::GetCurrent(GetWorld()))
    {
        Perception->UnregisterSource(*this);
    }

    OwningCharacter.Reset();
    Super::EndPlay(EndPlayReason);
}
