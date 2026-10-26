// Project-owned gameplay query library. No Blueprint asset edits.
// FindLowVault is spatial-only for reused GASP two-hand/one-hand vaults
// + MotionWarping EntryLift/Across/Exit targets. This file never
// moves, damages, teleports, or disables collision.

#include "WatcherGameplayLibrary.h"
#include "WatcherSightTarget.h"

#if WITH_EDITOR
#include "AnimGraphNode_Base.h"
#endif

bool UWatcherGameplayLibrary::ExposeAnimNodePinForEditor(UObject* Node, FName PinName)
{
#if WITH_EDITOR
    if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node))
    {
        for (int32 Index = 0; Index < AnimNode->ShowPinForProperties.Num(); ++Index)
        {
            if (AnimNode->ShowPinForProperties[Index].PropertyName == PinName)
            {
                AnimNode->SetPinVisibility(true, Index);
                return true;
            }
        }
    }
#endif
    return false;
}

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Sight.h"
#include "Engine/World.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"

FName UWatcherGameplayLibrary::GetAnimationStateName(UAnimInstance* Animation, FName MachineName)
{
    if (!IsValid(Animation))
    {
        return NAME_None;
    }
    const int32 Index = Animation->GetStateMachineIndex(MachineName);
    return Index != INDEX_NONE ? Animation->GetCurrentStateName(Index) : NAME_None;
}

namespace WatcherGameplay21
{
	constexpr float MinVaultHeightCm = 50.0f;
	constexpr float MaxVaultHeightCm = 115.0f;
	constexpr float MinThicknessCm = 20.0f;
	constexpr float MaxThicknessCm = 100.0f;
	constexpr float MinEntryDistCm = 30.0f;
	constexpr float MaxEntryDistCm = 150.0f;
	constexpr float MinFacingDot = 0.65f;
	// Run-vault wrist sampling placed contact 15cm before the obstacle edge.
	constexpr float NearOffsetCm = 25.0f;
	constexpr float FarOffsetCm = 45.0f;
	constexpr float ExitOffsetCm = 195.0f;
	constexpr float TopClearanceCm = 1.0f;
	constexpr float ExitClearanceCm = 5.0f;
	// Probe below the minimum allowed 50cm top, including floor-gap tolerance.
	constexpr float FrontProbeHeightCm = 40.0f;
	constexpr float FrontProbeLengthCm = 220.0f;
	constexpr float FrontSphereRadiusCm = 10.0f;
	constexpr float TopSearchHeightCm = 150.0f;
	constexpr float StepCm = 10.0f;

	bool HasDeadTag(const AActor* Actor)
	{
		if (Actor == nullptr)
		{
			return true;
		}
		return Actor->ActorHasTag(FName(TEXT("Dead")))
			|| Actor->ActorHasTag(FName(TEXT("WatcherDead")))
			|| Actor->ActorHasTag(FName(TEXT("Watcher.Dead")));
	}

	bool IsWorldUsable(const UWorld* World)
	{
		return World != nullptr && !World->bIsTearingDown;
	}

	FVector FlattenXY(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.0f);
	}

	AWatcherSightTarget* EnsureSightTarget(ACharacter* Character)
	{
		if (!IsValid(Character) || HasDeadTag(Character)) return nullptr;
		TArray<AActor*> Attached;
		Character->GetAttachedActors(Attached);
		for (AActor* Actor : Attached)
		{
			if (AWatcherSightTarget* Proxy = Cast<AWatcherSightTarget>(Actor))
			{
				if (IsValid(Proxy) && Proxy->GetOwner() == Character) return Proxy;
			}
		}
		FActorSpawnParameters Params;
		Params.Owner = Character;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		return Character->GetWorld()->SpawnActor<AWatcherSightTarget>(
			Character->GetActorLocation(), Character->GetActorRotation(), Params);
	}
}

bool UWatcherGameplayLibrary::FindLowVault(
	ACharacter* Character,
	FVector& OutEntry,
	FVector& OutTopNear,
	FVector& OutTopFar,
	FVector& OutExit,
	FRotator& OutFacing,
	FString& OutRejectReason)
{
	OutEntry = FVector::ZeroVector;
	OutTopNear = FVector::ZeroVector;
	OutTopFar = FVector::ZeroVector;
	OutExit = FVector::ZeroVector;
	OutFacing = FRotator::ZeroRotator;
	OutRejectReason = TEXT("");

	using namespace WatcherGameplay21;

	if (!IsValid(Character))
	{
		OutRejectReason = TEXT("InvalidCharacter");
		return false;
	}
	if (HasDeadTag(Character))
	{
		OutRejectReason = TEXT("CharacterDead");
		return false;
	}
	UWorld* World = Character->GetWorld();
	if (!IsWorldUsable(World))
	{
		OutRejectReason = TEXT("InvalidWorld");
		return false;
	}
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
	if (!IsValid(Capsule) || !IsValid(Movement))
	{
		OutRejectReason = TEXT("MissingCapsuleOrMovement");
		return false;
	}
	if (Movement->MovementMode != MOVE_Walking || !Movement->IsMovingOnGround() || Movement->IsCrouching() || Character->bIsCrouched)
	{
		OutRejectReason = TEXT("MustBeGroundedStanding");
		return false;
	}
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	if (HalfHeight <= 10.0f || Radius <= 5.0f)
	{
		OutRejectReason = TEXT("InvalidCapsuleSize");
		return false;
	}

	const FVector ActorLoc = Character->GetActorLocation();
	const FVector Feet = FVector(ActorLoc.X, ActorLoc.Y, ActorLoc.Z - HalfHeight);
	FVector ForwardXY = FlattenXY(Character->GetActorForwardVector());
	if (ForwardXY.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		OutRejectReason = TEXT("InvalidForward");
		return false;
	}
	ForwardXY.Normalize();

	// Ignore owner only. Never ignore the wall, floor, or any other actor.
	FCollisionQueryParams Query(TEXT("WatcherFindLowVault"), false, Character);
	Query.AddIgnoredActor(Character);

	// 1) Front obstacle: sphere sweep first, line fallback. Visibility channel.
	const FVector ProbeStart = Feet + FVector(0.0f, 0.0f, FrontProbeHeightCm);
	const FVector ProbeEnd = ProbeStart + ForwardXY * FrontProbeLengthCm;
	FHitResult FrontHit;
	const FCollisionShape FrontSphere = FCollisionShape::MakeSphere(FrontSphereRadiusCm);
	bool bFront = World->SweepSingleByChannel(FrontHit, ProbeStart, ProbeEnd, FQuat::Identity, ECC_Visibility, FrontSphere, Query);
	if (!bFront)
	{
		bFront = World->LineTraceSingleByChannel(FrontHit, ProbeStart, ProbeEnd, ECC_Visibility, Query);
	}
	if (!bFront || !FrontHit.bBlockingHit)
	{
		OutRejectReason = TEXT("NoObstacleAhead");
		return false;
	}
	AActor* Obstacle = FrontHit.GetActor();
	UPrimitiveComponent* ObstacleComp = FrontHit.GetComponent();
	if (!IsValid(Obstacle) || !IsValid(ObstacleComp))
	{
		OutRejectReason = TEXT("InvalidObstacle");
		return false;
	}
	if (Obstacle == Character || Obstacle->IsA(ACharacter::StaticClass()) || Obstacle->IsA(APawn::StaticClass()))
	{
		OutRejectReason = TEXT("DynamicObstacle");
		return false;
	}
	// Actual static obstacle only.
	if (ObstacleComp->Mobility != EComponentMobility::Static)
	{
		OutRejectReason = TEXT("NonStaticObstacle");
		return false;
	}

	// 2) Wall-orthogonal direction and facing gate.
	FVector NormalXY = FlattenXY(FrontHit.ImpactNormal);
	if (NormalXY.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		OutRejectReason = TEXT("InvalidImpactNormal");
		return false;
	}
	NormalXY.Normalize();
	const FVector VaultDir = -NormalXY;
	const float FacingDot = FVector::DotProduct(ForwardXY, VaultDir);
	if (FacingDot <= MinFacingDot)
	{
		OutRejectReason = TEXT("NotFacingWall");
		return false;
	}

	// 3) Entry distance gate, measured on XY from feet to impact.
	const float EntryDist = FVector::Dist2D(Feet, FrontHit.ImpactPoint);
	if (EntryDist < MinEntryDistCm || EntryDist > MaxEntryDistCm)
	{
		OutRejectReason = TEXT("EntryDistanceOutOfRange");
		return false;
	}

	// 4) Top surface via vertical down trace. Must belong to the same obstacle.
	const FVector FrontFaceXY(FrontHit.ImpactPoint.X, FrontHit.ImpactPoint.Y, 0.0f);
	const FVector TopStart = FVector(FrontFaceXY.X, FrontFaceXY.Y, Feet.Z + TopSearchHeightCm) + VaultDir * 5.0f;
	const FVector TopEnd = FVector(TopStart.X, TopStart.Y, Feet.Z - 50.0f);
	FHitResult TopHit;
	if (!World->LineTraceSingleByChannel(TopHit, TopStart, TopEnd, ECC_Visibility, Query) || !TopHit.bBlockingHit)
	{
		OutRejectReason = TEXT("NoTopSurface");
		return false;
	}
	if (TopHit.GetActor() != Obstacle)
	{
		OutRejectReason = TEXT("TopMismatch");
		return false;
	}
	const float TopZ = TopHit.ImpactPoint.Z;
	// Walking capsules retain a small floor gap; measure obstacle height from
	// the actual walkable floor, not that gap, including the 50cm boundary.
	const float HeightBaseZ = Movement->CurrentFloor.IsWalkableFloor()
		? Movement->CurrentFloor.HitResult.ImpactPoint.Z : Feet.Z;
	const float VaultHeight = TopZ - HeightBaseZ;
	// Sub-centimeter Chaos contact/float precision at the inclusive boundaries.
	constexpr float HeightToleranceCm = 0.5f;
	if (VaultHeight < MinVaultHeightCm - HeightToleranceCm || VaultHeight > MaxVaultHeightCm + HeightToleranceCm)
	{
		OutRejectReason = FString::Printf(TEXT("HeightOutOfRange: %.3f cm"), VaultHeight);
		return false;
	}

	// 5) Back face / thickness via stepping down traces along vault direction.
	// Actor bounds are only a sanity window; the stepping trace decides.
	FVector Origin(ForceInit);
	FVector BoxExtent(ForceInit);
	Obstacle->GetActorBounds(true, Origin, BoxExtent);
	FVector BackFaceXY = FrontFaceXY;
	bool bFoundBack = false;
	float MeasuredThickness = 0.0f;
	for (float D = StepCm; D <= (MaxThicknessCm + StepCm * 2.0f); D += StepCm)
	{
		const FVector SampleXY = FrontFaceXY + VaultDir * D;
		const FVector StepStart(SampleXY.X, SampleXY.Y, TopZ + 30.0f);
		const FVector StepEnd(SampleXY.X, SampleXY.Y, TopZ - 50.0f);
		FHitResult StepHit;
		const bool bStep = World->LineTraceSingleByChannel(StepHit, StepStart, StepEnd, ECC_Visibility, Query);
		const bool bStillTop = bStep && StepHit.bBlockingHit
			&& StepHit.GetActor() == Obstacle
			&& FMath::Abs(StepHit.ImpactPoint.Z - TopZ) <= 15.0f;
		if (bStillTop)
		{
			BackFaceXY = SampleXY;
			MeasuredThickness = D;
			continue;
		}
		// First drop after at least one top sample marks the back edge.
		if (MeasuredThickness > 0.0f)
		{
			bFoundBack = true;
			break;
		}
		OutRejectReason = TEXT("ThicknessTooThin");
		return false;
	}
	if (MeasuredThickness < MinThicknessCm || MeasuredThickness > MaxThicknessCm)
	{
		OutRejectReason = TEXT("ThicknessOutOfRange");
		return false;
	}
	if (!bFoundBack)
	{
		// Stepping never observed a drop; thickness is unbounded.
		OutRejectReason = TEXT("BackFaceNotFound");
		return false;
	}
	// Bounds sanity: back face should still sit inside the obstacle AABB (+tolerance).
	{
		const FVector BackWithZ(BackFaceXY.X, BackFaceXY.Y, TopZ);
		const FBox BoundsBox = FBox::BuildAABB(Origin, BoxExtent);
		const FBox Expanded = BoundsBox.ExpandBy(20.0f);
		if (!Expanded.IsInsideOrOn(BackWithZ))
		{
			OutRejectReason = TEXT("BackOutsideBounds");
			return false;
		}
	}

	// 6) Exit ground via vertical down trace. Must be real static ground.
	const FVector ExitXY = BackFaceXY + VaultDir * ExitOffsetCm;
	const FVector ExitStart(ExitXY.X, ExitXY.Y, TopZ + 50.0f);
	const FVector ExitEnd(ExitXY.X, ExitXY.Y, Feet.Z - 150.0f);
	FHitResult GroundHit;
	if (!World->LineTraceSingleByChannel(GroundHit, ExitStart, ExitEnd, ECC_Visibility, Query) || !GroundHit.bBlockingHit)
	{
		OutRejectReason = TEXT("NoExitGround");
		return false;
	}
	const UPrimitiveComponent* GroundComp = GroundHit.GetComponent();
	if (!IsValid(GroundHit.GetActor()) || !IsValid(GroundComp) || GroundComp->Mobility != EComponentMobility::Static)
	{
		OutRejectReason = TEXT("ExitGroundNotStatic");
		return false;
	}
	const float GroundZ = GroundHit.ImpactPoint.Z;
	if (FMath::Abs(GroundZ - Feet.Z) > 50.0f)
	{
		OutRejectReason = TEXT("ExitGroundHeightInvalid");
		return false;
	}

	// 7) Outputs. Entry is the character feet current position.
	OutEntry = FVector(Feet.X, Feet.Y, HeightBaseZ);
	// At point-blank range lift before advancing the full standing capsule;
	// the normal approach can keep the improved hand-contact alignment.
	const float EffectiveNearOffsetCm = EntryDist < 70.0f ? 45.0f : NearOffsetCm;
	OutTopNear = FVector(FrontFaceXY.X, FrontFaceXY.Y, 0.0f) - VaultDir * EffectiveNearOffsetCm;
	OutTopNear.Z = TopZ + TopClearanceCm;
	OutTopFar = FVector(BackFaceXY.X, BackFaceXY.Y, 0.0f) + VaultDir * FarOffsetCm;
	OutTopFar.Z = TopZ + TopClearanceCm;
	OutExit = FVector(ExitXY.X, ExitXY.Y, GroundZ + ExitClearanceCm);
	OutFacing = VaultDir.ToOrientationRotator();
	OutFacing.Pitch = 0.0f;
	OutFacing.Roll = 0.0f;

	// 8) Full standing capsule swept path + landing clearance.
	// No wall penetration, teleport, or collision disabling anywhere.
	const FCollisionShape Standing = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const FVector CenterNear = OutTopNear + FVector(0.0f, 0.0f, HalfHeight);
	const FVector CenterFar = OutTopFar + FVector(0.0f, 0.0f, HalfHeight);
	const FVector CenterExit = OutExit + FVector(0.0f, 0.0f, HalfHeight);

	FHitResult PathHit;
	const FVector CenterStart = Feet + FVector(0.0f, 0.0f, HalfHeight + 0.5f);
	if (World->SweepSingleByChannel(PathHit, CenterStart, CenterNear, FQuat::Identity, ECC_Pawn, Standing, Query))
	{
		OutRejectReason = TEXT("AscentBlocked");
		return false;
	}
	if (World->SweepSingleByChannel(PathHit, CenterNear, CenterFar, FQuat::Identity, ECC_Pawn, Standing, Query))
	{
		OutRejectReason = TEXT("TopPathBlocked");
		return false;
	}
	FHitResult DescendHit;
	if (World->SweepSingleByChannel(DescendHit, CenterFar, CenterExit, FQuat::Identity, ECC_Pawn, Standing, Query))
	{
		OutRejectReason = TEXT("DescentBlocked");
		return false;
	}
	if (World->OverlapBlockingTestByChannel(CenterExit, FQuat::Identity, ECC_Pawn, Standing, Query))
	{
		OutRejectReason = TEXT("ExitBlocked");
		return false;
	}

	OutRejectReason = TEXT("");
	return true;
}

UAnimMontage* UWatcherGameplayLibrary::SelectVaultMontage(FVector Entry, FVector TopNear)
{
	if (Entry.ContainsNaN() || TopNear.ContainsNaN()) return nullptr;
	const float HeightCm = TopNear.Z - Entry.Z - WatcherGameplay21::TopClearanceCm;
	// The traversal BP evaluates this exactly once before playback and stores
	// ActiveVaultMontage; all monitoring/cleanup reads that stored montage.
	if (HeightCm < 85.0f)
	{
		if (UAnimMontage* Low = LoadObject<UAnimMontage>(nullptr,
			TEXT("/Game/Watcher/Animations/Traversal/AM_WatcherStepVault.AM_WatcherStepVault")))
		{
			return Low;
		}
		return LoadObject<UAnimMontage>(nullptr,
			TEXT("/Game/Watcher/Animations/Traversal/AM_WatcherKongVault.AM_WatcherKongVault"));
	}
	if (FMath::RandBool())
	{
		if (UAnimMontage* Side = LoadObject<UAnimMontage>(nullptr,
			TEXT("/Game/Watcher/Animations/Traversal/AM_WatcherSideVault.AM_WatcherSideVault")))
		{
			return Side;
		}
	}
	return LoadObject<UAnimMontage>(nullptr,
		TEXT("/Game/Watcher/Animations/Traversal/AM_WatcherOneHandVault.AM_WatcherOneHandVault"));
}

bool UWatcherGameplayLibrary::CanRollFromExit(ACharacter* Character, FVector Exit, FVector Direction, FString& RejectReason)
{
	RejectReason = TEXT("InvalidRollContext");
	if (!IsValid(Character) || WatcherGameplay21::HasDeadTag(Character) ||
		!WatcherGameplay21::IsWorldUsable(Character->GetWorld()) || Exit.ContainsNaN() || Direction.ContainsNaN()) return false;
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	UCharacterMovementComponent* Move = Character->GetCharacterMovement();
	if (!IsValid(Capsule) || !IsValid(Move)) return false;
	Direction.Z = 0.0f;
	if (!Direction.Normalize()) return false;
	const float Half = Capsule->GetScaledCapsuleHalfHeight();
	const float Radius = Capsule->GetScaledCapsuleRadius();
	const FVector Center = Exit + FVector(0, 0, Half + 1.0f);
	const FVector End = Center + Direction * 500.0f;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(WatcherRollClearance), false, Character);
	Query.AddIgnoredActor(Character);
	FHitResult Hit;
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, Half);
	UWorld* World = Character->GetWorld();
	if (World->OverlapBlockingTestByChannel(Center, FQuat::Identity, ECC_Pawn, Shape, Query) ||
		World->SweepSingleByChannel(Hit, Center, End, FQuat::Identity, ECC_Pawn, Shape, Query))
	{
		RejectReason = TEXT("RollCorridorBlocked");
		return false;
	}
	float FloorZ = 0.0f;
	for (int32 Index = 0; Index <= 10; ++Index)
	{
		const FVector Feet = Exit + Direction * (50.0f * Index);
		FHitResult Floor;
		if (!World->LineTraceSingleByChannel(Floor, Feet + FVector(0,0,40), Feet - FVector(0,0,45), ECC_Pawn, Query) ||
			!Move->IsWalkable(Floor))
		{
			RejectReason = TEXT("RollFloorMissingOrSteep");
			return false;
		}
		if (Index == 0) FloorZ = Floor.ImpactPoint.Z;
		if (FMath::Abs(Floor.ImpactPoint.Z - FloorZ) > 8.0f)
		{
			RejectReason = TEXT("RollFloorNotFlat");
			return false;
		}
	}
	RejectReason.Reset();
	return true;
}

UAnimMontage* UWatcherGameplayLibrary::ChooseContextVault(ACharacter* Character, FVector Entry,
	FVector TopNear, FVector Exit, float ApproachSpeed)
{
	if (!IsValid(Character) || WatcherGameplay21::HasDeadTag(Character) || Entry.ContainsNaN() || TopNear.ContainsNaN() ||
		!FMath::IsFinite(ApproachSpeed)) return nullptr;
	auto Load = [](const TCHAR* Name) -> UAnimMontage*
	{
		const FString Path = FString(TEXT("/Game/Watcher/Animations/Traversal/")) + Name + TEXT(".") + Name;
		return LoadObject<UAnimMontage>(nullptr, *Path);
	};
	const float Height = TopNear.Z - Entry.Z - WatcherGameplay21::TopClearanceCm;
	const bool bLow = Height < 85.0f;
	const bool bRunning = ApproachSpeed >= 330.0f;
	if (bLow && ApproachSpeed < 120.0f) return Load(TEXT("AM_WatcherKongVault"));
	// Keep the original 50% branch. On low obstacles this is the restored Kong.
	if (FMath::RandBool())
	{
		if (bLow && bRunning)
		{
			FString Reason;
			if (CanRollFromExit(Character, Exit, TopNear-Entry, Reason))
				if (UAnimMontage* Roll = Load(TEXT("AM_WatcherRunningKongRoll"))) return Roll;
		}
		return Load(bLow ? TEXT("AM_WatcherKongVault") : TEXT("AM_WatcherOneHandVault"));
	}
	if (bRunning)
	{
		FString Reason;
		if (!bLow && CanRollFromExit(Character, Exit, TopNear-Entry, Reason))
			if (UAnimMontage* Roll = Load(TEXT("AM_WatcherRunningKongRoll"))) return Roll;
		return Load(TEXT("AM_WatcherHurdleRunR"));
	}
	if (bLow) return Load(TEXT("AM_WatcherHurdleRunR"));
	if (ApproachSpeed < 120.0f) return Load(TEXT("AM_WatcherHurdleStandR"));
	return Load(FMath::RandBool() ? TEXT("AM_WatcherHurdleWalkL") : TEXT("AM_WatcherHurdleWalkR"));
}

void UWatcherGameplayLibrary::ConfigureGuardSenses(
	AAIController* Controller,
	float SightRadius,
	float LoseSightRadius,
	float HalfFovDeg)
{
	using namespace WatcherGameplay21;

	if (!IsValid(Controller))
	{
		return;
	}
	UWorld* World = Controller->GetWorld();
	if (!IsWorldUsable(World))
	{
		return;
	}
	if (APawn* Controlled = Controller->GetPawn())
	{
		if (HasDeadTag(Controlled))
		{
			return;
		}
	}

	SightRadius = FMath::Clamp(SightRadius, 100.0f, 5000.0f);
	LoseSightRadius = FMath::Clamp(LoseSightRadius, SightRadius, 6000.0f);
	HalfFovDeg = FMath::Clamp(HalfFovDeg, 5.0f, 85.0f);

	UAIPerceptionComponent* Perception = Controller->FindComponentByClass<UAIPerceptionComponent>();
	bool bNewPerception = false;
	if (!IsValid(Perception))
	{
		Perception = NewObject<UAIPerceptionComponent>(Controller, TEXT("WatcherPerception"));
		if (!IsValid(Perception))
		{
			return;
		}
		Controller->SetPerceptionComponent(*Perception);
		bNewPerception = true;
	}

	UAISenseConfig_Sight* SightConfig = NewObject<UAISenseConfig_Sight>(Perception, TEXT("WatcherSight"));
	if (!IsValid(SightConfig))
	{
		return;
	}
	SightConfig->SightRadius = SightRadius;
	SightConfig->LoseSightRadius = LoseSightRadius;
	SightConfig->PeripheralVisionAngleDegrees = HalfFovDeg;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
	SightConfig->SetMaxAge(4.0f);
	// Never auto re-detect from remembered LastSeen alone.
	SightConfig->AutoSuccessRangeFromLastSeenLocation = -1.0f;

	Perception->ConfigureSense(*SightConfig);
	Perception->SetDominantSense(UAISense_Sight::StaticClass());
	if (bNewPerception)
	{
		// OnRegister needs a populated SensesConfig to create a valid listener.
		Perception->RegisterComponent();
	}
	Perception->RequestStimuliListenerUpdate();
	Perception->Activate();
}

bool UWatcherGameplayLibrary::GuardCanSee(
	AAIController* Controller,
	AActor* Target,
	float HardLimit)
{
	using namespace WatcherGameplay21;

	if (!IsValid(Controller) || !IsValid(Target))
	{
		return false;
	}
	if (HardLimit <= 0.0f)
	{
		return false;
	}
	UWorld* World = Controller->GetWorld();
	if (!IsWorldUsable(World))
	{
		return false;
	}
	APawn* GuardPawn = Controller->GetPawn();
	if (!IsValid(GuardPawn))
	{
		return false;
	}
	if (HasDeadTag(GuardPawn) || HasDeadTag(Target))
	{
		return false;
	}
	if (GuardPawn == Target)
	{
		return false;
	}

	UAIPerceptionComponent* Perception = Cast<UAIPerceptionComponent>(Controller->GetPerceptionComponent());
	if (!IsValid(Perception))
	{
		Perception = Controller->FindComponentByClass<UAIPerceptionComponent>();
	}
	if (!IsValid(Perception))
	{
		return false;
	}

	// Engine Sight owns range, field of view, memory and trace scheduling.
	// The attached target interface samples head and torso for partial cover.
	AWatcherSightTarget* HeadProxy = EnsureSightTarget(Cast<ACharacter>(Target));
	TArray<AActor*> CurrentlyPerceived;
	Perception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), CurrentlyPerceived);
	if (!CurrentlyPerceived.Contains(Target) && !CurrentlyPerceived.Contains(HeadProxy))
	{
		return false;
	}

	FVector EyeLoc(ForceInit);
	FRotator EyeRot = FRotator::ZeroRotator;
	GuardPawn->GetActorEyesViewPoint(EyeLoc, EyeRot);

	FVector Origin(ForceInit);
	FVector Extent(ForceInit);
	Target->GetActorBounds(true, Origin, Extent);
	FVector HeadSample = Origin;
	FVector TorsoSample = Origin;
	if (Extent.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		HeadSample.Z = Origin.Z + Extent.Z * 0.9f;
		TorsoSample = Origin;
	}
	else
	{
		// Fallback for actors without meaningful bounds.
		const FVector TargetLoc = Target->GetActorLocation();
		HeadSample = TargetLoc + FVector(0.0f, 0.0f, 140.0f);
		TorsoSample = TargetLoc + FVector(0.0f, 0.0f, 60.0f);
	}
	if (IsValid(HeadProxy))
	{
		HeadSample = HeadProxy->GetActorLocation();
		TorsoSample = Target->GetActorLocation();
	}

	const float DistToTorso = FVector::Dist(EyeLoc, TorsoSample);
	const float DistToHead = FVector::Dist(EyeLoc, HeadSample);
	if (FMath::Min(DistToTorso, DistToHead) > HardLimit)
	{
		return false;
	}

	// Ignore guard only. A hit on Target itself counts as clear.
	FCollisionQueryParams Query(TEXT("WatcherGuardCanSee"), false, GuardPawn);
	Query.AddIgnoredActor(GuardPawn);

	const FVector Samples[2] = { HeadSample, TorsoSample };
	for (const FVector& Sample : Samples)
	{
		FHitResult VisHit;
		const bool bHit = World->LineTraceSingleByChannel(VisHit, EyeLoc, Sample, ECC_Visibility, Query);
		if (!bHit || !VisHit.bBlockingHit)
		{
			return true;
		}
		if (VisHit.GetActor() == Target || (VisHit.GetActor() && VisHit.GetActor()->IsOwnedBy(Target)))
		{
			return true;
		}
	}

	// No clear eye->head/torso sample. Do not touch LastSeen here;
	// existing BP keeps LastSeen handling.
	return false;
}

bool UWatcherGameplayLibrary::SetJumpKeyForTest(APlayerController* Controller, bool bPressed)
{
#if UE_BUILD_SHIPPING
	return false;
#else
	if (!IsValid(Controller) || !Controller->PlayerInput)
	{
		return false;
	}
	return Controller->InputKey(FInputKeyParams(EKeys::SpaceBar, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0 : 0.0));
#endif
}
