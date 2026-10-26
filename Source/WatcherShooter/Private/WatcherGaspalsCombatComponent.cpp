#include "WatcherGaspalsCombatComponent.h"

#include "WatcherGaspalsHealthComponent.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"

namespace WatcherGaspalsCombat
{
    const TCHAR* EquipmentClassPath = TEXT("/GASPALS/Systems/EquipmentSystem/AC_EquipmentSystem.AC_EquipmentSystem_C");
    const TCHAR* TraversalInterfacePath = TEXT("/GASPALS/Systems/TraversalSystem/BPI_TraversalSystem.BPI_TraversalSystem_C");

    bool FiniteVector(const FVector& Value)
    {
        return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
    }

    bool HasClassPath(const UObject* Object, const TCHAR* Path)
    {
        for (const UClass* Class = IsValid(Object) ? Object->GetClass() : nullptr;
            Class != nullptr; Class = Class->GetSuperClass())
        {
            if (Class->GetPathName().Equals(Path, ESearchCase::CaseSensitive)) return true;
        }
        return false;
    }

    const FStructProperty* VectorOutput(const UFunction* Function, const FName Name)
    {
        const FStructProperty* Property = FindFProperty<FStructProperty>(Function, Name);
        return Property != nullptr && Property->ArrayDim == 1
            && Property->Struct == TBaseStructure<FVector>::Get()
            && Property->HasAnyPropertyFlags(CPF_OutParm) ? Property : nullptr;
    }
}

UWatcherGaspalsCombatComponent::UWatcherGaspalsCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f;
    SetIsReplicatedByDefault(false);
}

void UWatcherGaspalsCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    MagazineCapacity = FMath::Max(1, MagazineCapacity);
    MagazineAmmo = MagazineCapacity;
    ReserveAmmo = FMath::Max(0, InitialReserveAmmo);
    ReloadSeconds = FMath::IsFinite(ReloadSeconds) ? FMath::Max(0.01f, ReloadSeconds) : 1.4f;
    RefireSeconds = FMath::IsFinite(RefireSeconds) ? FMath::Max(0.01f, RefireSeconds) : 0.2f;
    TraversalInterfaceClass = LoadObject<UClass>(nullptr, WatcherGaspalsCombat::TraversalInterfacePath);
    BindHealth();
    TryBindInput();
    BroadcastAmmo();
}

void UWatcherGaspalsCombatComponent::BindHealth()
{
    if (BoundHealth.IsValid()) return;
    UWatcherGaspalsHealthComponent* Health = IsValid(GetOwner())
        ? GetOwner()->FindComponentByClass<UWatcherGaspalsHealthComponent>() : nullptr;
    if (!IsValid(Health)) return;
    BoundHealth = Health;
    Health->OnDied.AddUniqueDynamic(this, &UWatcherGaspalsCombatComponent::HandleOwnerDied);
    if (!Health->IsAlive()) HandleOwnerDied(Health, nullptr, nullptr);
}

void UWatcherGaspalsCombatComponent::TryBindInput()
{
    APawn* Pawn = Cast<APawn>(GetOwner());
    UEnhancedInputComponent* Input = IsValid(Pawn) && Pawn->IsLocallyControlled()
        ? Cast<UEnhancedInputComponent>(Pawn->InputComponent) : nullptr;
    if (BoundInput.Get() == Input && OwnedInputHandles.Num() == 5) return;
    if (BoundInput.Get() != Input) { StopFire(); CancelReload(); RemoveOwnedBindings(); }
    if (!IsValid(Input) || !IsValid(FireAction) || !IsValid(ReloadAction) || !IsValid(RestartAction)
        || FireAction == ReloadAction || FireAction == RestartAction || ReloadAction == RestartAction
        || OwnedInputHandles.Num() != 0) return;
    // Deliberately do not remove any GASPALS or other pre-existing bindings.
    OwnedInputHandles.Add(Input->BindAction(FireAction.Get(), ETriggerEvent::Started,
        this, &UWatcherGaspalsCombatComponent::HandleFireStarted).GetHandle());
    OwnedInputHandles.Add(Input->BindAction(FireAction.Get(), ETriggerEvent::Completed,
        this, &UWatcherGaspalsCombatComponent::StopFire).GetHandle());
    OwnedInputHandles.Add(Input->BindAction(FireAction.Get(), ETriggerEvent::Canceled,
        this, &UWatcherGaspalsCombatComponent::StopFire).GetHandle());
    OwnedInputHandles.Add(Input->BindAction(ReloadAction.Get(), ETriggerEvent::Started,
        this, &UWatcherGaspalsCombatComponent::HandleReloadStarted).GetHandle());
    OwnedInputHandles.Add(Input->BindAction(RestartAction.Get(), ETriggerEvent::Started,
        this, &UWatcherGaspalsCombatComponent::HandleRestartStarted).GetHandle());
    BoundInput = Input;
}

void UWatcherGaspalsCombatComponent::RemoveOwnedBindings()
{
    if (UEnhancedInputComponent* Input = BoundInput.Get())
        for (const uint32 Handle : OwnedInputHandles) Input->RemoveBindingByHandle(Handle);
    OwnedInputHandles.Reset();
    BoundInput.Reset();
}

bool UWatcherGaspalsCombatComponent::ReadTraversalActive(bool& bOutActive) const
{
    bOutActive = false;
    if (!IsValid(GetOwner()) || !IsValid(TraversalInterfaceClass)) return false;
    UActorComponent* Provider = nullptr;
    TArray<UActorComponent*> Components;
    GetOwner()->GetComponents(Components);
    for (UActorComponent* Candidate : Components)
    {
        if (!IsValid(Candidate) || !Candidate->GetClass()->ImplementsInterface(TraversalInterfaceClass)) continue;
        if (Provider != nullptr) return false;
        Provider = Candidate;
    }
    UFunction* Function = IsValid(Provider) ? Provider->FindFunction(TEXT("IsDoingTraversalAction")) : nullptr;
    const FBoolProperty* Output = Function != nullptr
        ? FindFProperty<FBoolProperty>(Function, TEXT("bDoingTraversalAction")) : nullptr;
    if (!Function || Function->NumParms != 1 || !Output || Output->ArrayDim != 1
        || !Output->HasAnyPropertyFlags(CPF_OutParm)) return false;
    FStructOnScope Parameters(Function);
    Provider->ProcessEvent(Function, Parameters.GetStructMemory());
    bOutActive = Output->GetPropertyValue_InContainer(Parameters.GetStructMemory());
    return true;
}

bool UWatcherGaspalsCombatComponent::CanUseEquipment(AActor*& OutEquipment, FString& OutReason) const
{
    OutEquipment = nullptr;
    const APawn* Pawn = Cast<APawn>(GetOwner());
    const UWatcherGaspalsHealthComponent* Health = BoundHealth.Get();
    if (!IsValid(Pawn) || !Pawn->IsLocallyControlled()) { OutReason = TEXT("OwnerNotLocalPawn"); return false; }
    if (bDeathApplied || Pawn->ActorHasTag(TEXT("Dead")) || !IsValid(Health) || !Health->IsAlive())
    { OutReason = TEXT("HealthMissingOrDead"); return false; }
    bool bTraversing = false;
    if (!ReadTraversalActive(bTraversing)) { OutReason = TEXT("TraversalContractMissingOrAmbiguous"); return false; }
    if (bTraversing) { OutReason = TEXT("TraversalActive"); return false; }
    UActorComponent* Equipment = nullptr;
    TArray<UActorComponent*> Components;
    Pawn->GetComponents(Components);
    for (UActorComponent* Candidate : Components)
    {
        if (!WatcherGaspalsCombat::HasClassPath(Candidate, WatcherGaspalsCombat::EquipmentClassPath)) continue;
        if (Equipment != nullptr) { OutReason = TEXT("DuplicateEquipmentComponents"); return false; }
        Equipment = Candidate;
    }
    const FObjectPropertyBase* Property = IsValid(Equipment)
        ? FindFProperty<FObjectPropertyBase>(Equipment->GetClass(), TEXT("SpawnedActor")) : nullptr;
    OutEquipment = Property != nullptr ? Cast<AActor>(Property->GetObjectPropertyValue_InContainer(Equipment)) : nullptr;
    if (!IsValid(OutEquipment) || RequiredPistolClassPath.IsEmpty()
        || OutEquipment->GetClass()->GetPathName() != RequiredPistolClassPath)
    { OutEquipment = nullptr; OutReason = TEXT("RequiredPistolNotEquipped"); return false; }
    OutReason.Reset();
    return true;
}

bool UWatcherGaspalsCombatComponent::ReadAimPoint(FVector& OutPoint) const
{
    AActor* Owner = GetOwner();
    UFunction* Function = IsValid(Owner) ? Owner->FindFunction(TEXT("Get_AimRay")) : nullptr;
    if (!Function || Function->NumParms != 7) return false;
    const FStructProperty* OriginProperty = WatcherGaspalsCombat::VectorOutput(Function, TEXT("Origin"));
    const FStructProperty* DirectionProperty = WatcherGaspalsCombat::VectorOutput(Function, TEXT("Direction"));
    const FStructProperty* PointProperty = WatcherGaspalsCombat::VectorOutput(Function, TEXT("Point"));
    const FNumericProperty* DistanceProperty = FindFProperty<FNumericProperty>(Function, TEXT("TraceDistance"));
    if (!OriginProperty || !DirectionProperty || !PointProperty || !DistanceProperty
        || !DistanceProperty->IsFloatingPoint() || !DistanceProperty->HasAnyPropertyFlags(CPF_OutParm)) return false;
    FStructOnScope Parameters(Function);
    Owner->ProcessEvent(Function, Parameters.GetStructMemory());
    const FVector Origin = *OriginProperty->ContainerPtrToValuePtr<FVector>(Parameters.GetStructMemory());
    const FVector Direction = *DirectionProperty->ContainerPtrToValuePtr<FVector>(Parameters.GetStructMemory());
    const FVector Point = *PointProperty->ContainerPtrToValuePtr<FVector>(Parameters.GetStructMemory());
    const double Distance = DistanceProperty->GetFloatingPointPropertyValue(
        DistanceProperty->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()));
    if (!WatcherGaspalsCombat::FiniteVector(Origin) || !WatcherGaspalsCombat::FiniteVector(Direction)
        || Direction.IsNearlyZero() || !FMath::IsFinite(Distance) || Distance <= 1.0) return false;
    OutPoint = WatcherGaspalsCombat::FiniteVector(Point) && FVector::DistSquared(Point, Origin) > 1.0
        ? Point : Origin + Direction.GetSafeNormal() * Distance;
    return WatcherGaspalsCombat::FiniteVector(OutPoint);
}

bool UWatcherGaspalsCombatComponent::ReadMuzzle(AActor* Equipment, FVector& OutLocation) const
{
    if (!IsValid(Equipment)) return false;
    TArray<UMeshComponent*> Meshes;
    Equipment->GetComponents<UMeshComponent>(Meshes);
    UMeshComponent* Fallback = nullptr;
    UMeshComponent* SocketMesh = nullptr;
    for (UMeshComponent* Mesh : Meshes)
    {
        if (!IsValid(Mesh)) continue;
        if (Fallback == nullptr) Fallback = Mesh;
        if (!MuzzleSocket.IsNone() && Mesh->DoesSocketExist(MuzzleSocket))
        {
            if (SocketMesh != nullptr) return false;
            SocketMesh = Mesh;
        }
    }
    if (SocketMesh) OutLocation = SocketMesh->GetSocketLocation(MuzzleSocket);
    else if (Fallback && WatcherGaspalsCombat::FiniteVector(MuzzleLocalOffset))
        OutLocation = Fallback->GetComponentTransform().TransformPosition(MuzzleLocalOffset);
    else return false;
    return WatcherGaspalsCombat::FiniteVector(OutLocation);
}

bool UWatcherGaspalsCombatComponent::SpawnLegacyBullet(const FVector& Muzzle, const FVector& Direction)
{
    UWorld* World = GetWorld();
    APawn* Pawn = Cast<APawn>(GetOwner());
    if (!IsValid(World) || !IsValid(Pawn) || !IsValid(ProjectileClass.Get())) return false;
    const FTransform SpawnTransform(Direction.Rotation(), Muzzle);
    AActor* Projectile = World->SpawnActorDeferred<AActor>(ProjectileClass.Get(), SpawnTransform,
        Pawn, Pawn, ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
    if (!IsValid(Projectile)) return false;
    // The preserved weapon SpawnActor exposes Noise Tag (name, default Projectile),
    // Owner and Instigator. BeginPlay ignores Owner for movement collisions.
    // No StartVelocity/Start Velocity exposed pin was found in that graph export.
    if (const FNameProperty* NoiseTag = FindFProperty<FNameProperty>(Projectile->GetClass(), TEXT("Noise Tag")))
        NoiseTag->SetPropertyValue_InContainer(Projectile, FName(TEXT("Projectile")));
    UGameplayStatics::FinishSpawningActor(Projectile, SpawnTransform);
    if (!IsValid(Projectile)) return false;
    UProjectileMovementComponent* Movement = Projectile->FindComponentByClass<UProjectileMovementComponent>();
    const float Speed = IsValid(Movement) ? Movement->InitialSpeed : 0.0f;
    if (!IsValid(Movement) || !IsValid(Movement->UpdatedComponent) || !FMath::IsFinite(Speed) || Speed <= 0.0f)
    {
        Projectile->Destroy();
        return false;
    }
    // Preserve the existing BP's damage/collision/physics/default speed. Direction
    // is world-space and applied after construction/BeginPlay have configured it.
    Movement->Activate(true);
    Movement->Velocity = Direction * Speed;
    Movement->UpdateComponentVelocity();
    return true;
}

bool UWatcherGaspalsCombatComponent::TryFire()
{
    if (!IsValid(GetWorld())) { LastBlockReason = TEXT("WorldMissing"); return false; }
    AActor* Equipment = nullptr;
    if (!CanUseEquipment(Equipment, LastBlockReason)) return false;
    if (bReloading) { LastBlockReason = TEXT("Reloading"); return false; }
    if (MagazineAmmo <= 0) { LastBlockReason = TEXT("EmptyMagazine"); return false; }
    const double Time = GetWorld()->GetTimeSeconds();
    if (Time - LastShotTime + 1.0e-6 < RefireSeconds) { LastBlockReason = TEXT("RefireCooldown"); return false; }
    FVector AimPoint, Muzzle;
    if (!ReadAimPoint(AimPoint)) { LastBlockReason = TEXT("AimRayContractInvalid"); return false; }
    if (!ReadMuzzle(Equipment, Muzzle)) { LastBlockReason = TEXT("MuzzleMissingOrAmbiguous"); return false; }
    APawn* Pawn = Cast<APawn>(GetOwner());
    FVector EyeLocation;
    FRotator EyeRotation;
    Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
    if (!WatcherGaspalsCombat::FiniteVector(EyeLocation))
    { LastBlockReason = TEXT("InvalidEyeLocation"); return false; }
    FCollisionQueryParams MuzzleQuery(SCENE_QUERY_STAT(WatcherGaspalsMuzzleGuard), false, Pawn);
    MuzzleQuery.AddIgnoredActor(Equipment);
    FHitResult MuzzleHit;
    if (GetWorld()->LineTraceSingleByChannel(MuzzleHit, EyeLocation, Muzzle, ECC_Visibility, MuzzleQuery))
    { LastBlockReason = TEXT("MuzzleObstructed"); return false; }
    const FVector Direction = (AimPoint - Muzzle).GetSafeNormal();
    if (!WatcherGaspalsCombat::FiniteVector(Direction) || Direction.IsNearlyZero())
    { LastBlockReason = TEXT("InvalidShotDirection"); return false; }
    if (!SpawnLegacyBullet(Muzzle, Direction)) { LastBlockReason = TEXT("ProjectileSpawnOrSpeedFailed"); return false; }
    --MagazineAmmo;
    LastShotTime = Time;
    LastBlockReason.Reset();
    BroadcastAmmo();
    return true;
}

void UWatcherGaspalsCombatComponent::HandleFireStarted()
{
    if (bTriggerHeld) return;
    bTriggerHeld = true;
    if (!TryFire()) { StopFire(); return; }
    if (bAutomaticFire && IsValid(GetWorld()))
        GetWorld()->GetTimerManager().SetTimer(FireTimer, this,
            &UWatcherGaspalsCombatComponent::HandleAutomaticFire, RefireSeconds, true);
}

void UWatcherGaspalsCombatComponent::HandleAutomaticFire()
{
    if (!bTriggerHeld) { StopFire(); return; }
    if (!TryFire() && LastBlockReason != TEXT("RefireCooldown")) StopFire();
}

void UWatcherGaspalsCombatComponent::StopFire()
{
    bTriggerHeld = false;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(FireTimer);
}

void UWatcherGaspalsCombatComponent::HandleReloadStarted()
{
    TryReload();
}

void UWatcherGaspalsCombatComponent::HandleRestartStarted()
{
    if (!IsValid(GetWorld())) return;
    const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
    if (!LevelName.IsEmpty()) UGameplayStatics::OpenLevel(this, FName(*LevelName), true);
}

bool UWatcherGaspalsCombatComponent::TryReload()
{
    if (!IsValid(GetWorld())) { LastBlockReason = TEXT("WorldMissing"); return false; }
    AActor* Equipment = nullptr;
    if (!CanUseEquipment(Equipment, LastBlockReason)) return false;
    if (bReloading || MagazineAmmo >= MagazineCapacity || ReserveAmmo <= 0) return false;
    StopFire();
    bReloading = true;
    ReloadEquipment = Equipment;
    GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this,
        &UWatcherGaspalsCombatComponent::FinishReload, ReloadSeconds, false);
    BroadcastAmmo();
    return true;
}

void UWatcherGaspalsCombatComponent::FinishReload()
{
    AActor* Equipment = nullptr;
    if (!bReloading || !CanUseEquipment(Equipment, LastBlockReason) || ReloadEquipment.Get() != Equipment)
    { CancelReload(); return; }
    const int32 Transfer = FMath::Min(FMath::Max(0, MagazineCapacity - MagazineAmmo), ReserveAmmo);
    MagazineAmmo += Transfer;
    ReserveAmmo -= Transfer;
    bReloading = false;
    ReloadEquipment.Reset();
    BroadcastAmmo();
}

void UWatcherGaspalsCombatComponent::CancelReload()
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(ReloadTimer);
    const bool bWasReloading = bReloading;
    bReloading = false;
    ReloadEquipment.Reset();
    if (bWasReloading) BroadcastAmmo();
}

void UWatcherGaspalsCombatComponent::BroadcastAmmo()
{
    OnAmmoChanged.Broadcast(MagazineAmmo, ReserveAmmo, bReloading);
}

void UWatcherGaspalsCombatComponent::HandleOwnerDied(UWatcherGaspalsHealthComponent* HealthComponent,
    AController* InstigatedBy, AActor* DamageCauser)
{
    (void)InstigatedBy;
    (void)DamageCauser;
    if (bDeathApplied || HealthComponent != BoundHealth.Get() || !IsValid(GetOwner())) return;
    bDeathApplied = true;
    StopFire();
    CancelReload();
    GetOwner()->Tags.AddUnique(FName(TEXT("Dead")));
    if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
    {
        if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
        {
            Movement->StopMovementImmediately();
            Movement->DisableMovement();
        }
        if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
            Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    if (APawn* Pawn = Cast<APawn>(GetOwner()))
        if (APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
        {
            Controller->SetIgnoreMoveInput(true);
            Controller->SetIgnoreLookInput(true);
        }
    LastBlockReason = TEXT("DeadRestartRequired");
    // GASPALS ragdoll/hit/death animation is deliberately not connected here.
}

void UWatcherGaspalsCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    BindHealth();
    TryBindInput();
    if (BoundHealth.IsValid() && !BoundHealth->IsAlive() && !bDeathApplied)
        HandleOwnerDied(BoundHealth.Get(), nullptr, nullptr);
    if (bTriggerHeld || bReloading)
    {
        AActor* Equipment = nullptr;
        FString Reason;
        const bool bAllowed = CanUseEquipment(Equipment, Reason);
        if (!bAllowed) StopFire();
        if (bReloading && (!bAllowed || ReloadEquipment.Get() != Equipment)) CancelReload();
    }
}

void UWatcherGaspalsCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopFire();
    CancelReload();
    RemoveOwnedBindings();
    if (UWatcherGaspalsHealthComponent* Health = BoundHealth.Get())
        Health->OnDied.RemoveDynamic(this, &UWatcherGaspalsCombatComponent::HandleOwnerDied);
    BoundHealth.Reset();
    Super::EndPlay(EndPlayReason);
}
