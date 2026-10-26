# Integration

Use a clean C++ host project with the plugin enabled. This source extraction contains no `.uproject` or `.uasset` files. Keep the host's movement/animation/camera ownership explicit.

## Health without external content

1. Add **Watcher Gaspals Health Component** to an Actor Blueprint. Set `MaxHealth` (default 100).
2. At play start the component binds the owner's `OnTakeAnyDamage` and initializes health.
3. Use Unreal's `Apply Damage` node with that owner as `Damaged Actor` and positive `Base Damage`; read `CurrentHealth` or bind `OnHealthChanged`/`OnDied`.
4. `ResetHealth` starts a new health life. The host owns death animation, tags, collision, movement, respawn and UI.

Damage with invalid/non-positive values is ignored. These local components provide no network authority, replication, RPC or prediction policy.

## Sight and spatial queries

`ConfigureGuardSenses` configures the engine's AI Sight on an AIController. `GuardCanSee` queries current perception and visibility; the host owns patrol, memory, investigation and state transitions. `AWatcherSightTarget` needs an `ACharacter` owner and registers a sight stimulus, then follows capsule height. Set collision responses on the host's world geometry so `Visibility` actually blocks sight.

`FindLowVault` only returns candidate transforms/rejection reasons. It does not play animation, disable collision, warp, teleport or launch a character. `SelectVaultMontage` and `ChooseContextVault` reference project-specific montage paths under `/Game/Watcher/Animations/Traversal/`. Those assets are absent here, so callers must handle `nullptr`. Provide licensed host montages and explicit traversal/motion-warping orchestration before using their results. Do not run two traversal drivers on the same input.

## Optional GASPALS 1.2 combat adapter

Acquire the editable GASPALS content separately. Preserve its license and install it following its own instructions. No claim is made that this repository grants access to it. The following interface/class names are references, not bundled vendor implementation.

1. Use a project-owned child of a compatible local Pawn, keeping its native movement, equipment, aim and traversal behavior.
2. Add `UWatcherGaspalsHealthComponent` and `UWatcherGaspalsCombatComponent` to the Pawn. Start in single-player/local control.
3. Create distinct host Input Actions for fire/reload/restart; assign all three component properties and register their host Input Mapping Context once. Restart reloads the current map. Avoid duplicate GASPALS or host fire bindings.
4. Supply the compatible equipment component (`AC_EquipmentSystem` with `SpawnedActor`), traversal interface/provider (`BPI_TraversalSystem`, `IsDoingTraversalAction`) and Pawn `Get_AimRay` outputs expected by the adapter. Reflection signatures must match; absent/ambiguous contracts block the operation.
5. Set `RequiredPistolClassPath` to the actual equipped actor class, `ProjectileClass` to the host's licensed projectile Blueprint, and verify its ProjectileMovement speed, collision, owner/instigator and damage behavior. No projectile or damage Blueprint is bundled.
6. Calibrate `MuzzleSocket`/`MuzzleLocalOffset` to the real mesh. Test camera-to-target and muzzle obstruction separately. The first-mesh fallback is a starting point, not a verified universal weapon fit.
7. Bind `OnAmmoChanged` to host UI. Inspect `LastBlockReason`, and test equipment changes, reload cancellation, death, input release and component removal. Host code must restore full movement/input/collision or reload a level after death; `ResetHealth` alone is insufficient.

Animations, montage slots, visual recoil, hit reactions, ragdoll, melee/takedown behavior and final AI reactions are host responsibilities. Runtime-loaded string paths require explicit cooking. Do not interpret the presence of a source component as proof of an entire working combat loop.
