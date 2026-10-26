# Project independent Stealth Gameplay

A small, source-only Unreal Engine plugin extracted from project-owned local integration work. It provides sight/spatial queries, an Actor health component, and an optional combat adapter prototype. The local integration target is **Unreal Engine 5.8.3**; this standalone plugin extraction is **work in progress**. A standalone plugin build and gameplay validation are pending.

This repository is not a finished game. It contains no game maps, animations, models, project conversations, private logs, Unreal Engine source, or GASPALS content. The included code and documentation are MIT licensed; separately obtained engine/content licenses still apply.

## Included code

| API | Purpose | Required integration |
|---|---|---|
| `UWatcherGameplayLibrary` | Built-in AI Sight configuration/queries, conservative vault-space checks, montage selection and diagnostics | A host world, actors and collision. Montage selectors reference external assets. |
| `AWatcherSightTarget` | Character-owned sight stimulus with head/torso visibility checks | An `ACharacter` owner and native AI Perception setup |
| `UWatcherGaspalsHealthComponent` | Local `AnyDamage` health/death events | Any Actor; no GASPALS asset is required for this component |
| `UWatcherGaspalsCombatComponent` | Local ammo/reload/input and projectile adapter draft | Explicit input assets, health, compatible GASPALS runtime contracts and a separately supplied projectile class |

Names retain the `WatcherShooter` module identity used by the integration code. Install into a clean host project; an existing game module also named `WatcherShooter` conflicts with this plugin.

## Quick start

1. Obtain Unreal Engine and its compatible C++ build tools separately.
2. Run the source checks from the repository root:

   ```powershell
   pwsh -File .\scripts\Validate-Source.ps1
   ```

3. Build the plugin with your local engine installation:

   ```powershell
   pwsh -File .\scripts\Build-Plugin.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
   ```

4. Copy this repository into a clean C++ host project's `Plugins/WatcherStealthGameplay` directory, enable **Watcher Stealth Gameplay**, and build the host's Editor target. See [build instructions](docs/build.md) and [integration](docs/integration.md).

The build script uses the engine-selected compiler. No specific 14.x compiler toolchain is claimed as verified for this public plugin. It writes local output under ignored `.build/` and does not publish or upload anything.

## External assets and expected blocked operations

GASPALS 1.2 and Epic's Game Animation Sample are **not included**. Acquire any optional integration content separately under its own terms. Sight queries and health can be integrated independently; the current combat adapter expects the GASPALS equipment, traversal and aim-ray contracts.

- Vault montage selectors return `nullptr` when their project montage paths and fallbacks are absent. The spatial query does not play a montage or move the pawn.
- Missing/ambiguous traversal, equipment, health or aim contracts block firing/reload where checked. Inspect `LastBlockReason`; do not replace these checks with unconditional success.
- `FireAction`, `ReloadAction` and `RestartAction` must all be assigned and distinct before the component installs its five owned input bindings. Add the mapping context in the host; the adapter does not add one.
- No projectile Blueprint is shipped. Assign a compatible projectile class with movement, collision and damage behavior. Invalid spawn/movement data can reject a shot and destroy the newly spawned projectile. Arbitrary projectile `BeginPlay` side effects are not rolled back.
- A missing muzzle socket uses the first mesh plus `MuzzleLocalOffset`; the offset needs asset-specific calibration.
- Runtime string references do not automatically guarantee asset cooking. See the host's packaging rules.

There is no complete AI behavior tree/state machine, weapon asset, HUD, firing/reload animation, movement system or multiplayer implementation in this repository. Health reset alone does not restore movement/input/collision disabled by the optional combat adapter's death path.

## Status and contribution

Read [validation](docs/validation.md), [roadmap](docs/roadmap.md), [contribution rules](CONTRIBUTING.md) and [third-party notices](THIRD_PARTY_NOTICES.md). No external adoption, user counts, download statistics, production deployments or program eligibility/approval are claimed. Any support application must use verified facts rather than assumed popularity.

## 한국어 요약

자체 제작 C++ 일부를 추출한 UE5 소스 플러그인입니다. 로컬 통합 대상은 UE 5.8.3이며 공개 플러그인 단독 빌드·실행 검증은 아직 남아 있습니다. 완성 게임·에셋·원본 GASPALS·엔진 코드는 포함하지 않습니다. 체력과 시야 질의는 별도 조립할 수 있고, 전투 어댑터는 사용자가 따로 확보한 입력·장비·Traversal·발사체 계약이 없으면 기능을 차단합니다. MIT는 이 저장소의 자체 코드에 적용되며 엔진/외부 콘텐츠의 사용권을 제공하지 않습니다. 신규 OSS의 외부 채택 실적이나 지원 프로그램 승인을 확보했다고 주장하지 않습니다.
