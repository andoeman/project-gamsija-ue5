# Build

## Target and current evidence

The local integration target is Unreal Engine **5.8.3**, Windows, with a C++ toolchain supported by that engine. The descriptor uses the 5.8 engine baseline (`5.8.0`). Other engine versions/platforms are unverified. A successful private game build does not establish that this extracted plugin builds independently. Specific 14.x compiler versions remain pending verification for the public extraction.

## BuildPlugin

From this repository root, run:

```powershell
pwsh -File .\scripts\Validate-Source.ps1
pwsh -File .\scripts\Build-Plugin.ps1 -EngineRoot 'C:\Program Files\Epic Games\UE_5.8'
```

The script calls the engine's `RunUAT.bat BuildPlugin` for Win64. It chooses a fresh `.build/Package-<timestamp>` destination, records a local log/result, checks the native exit code and packaged descriptor, and fails on missing inputs/output. It does not select a compiler version, run a game, modify a desktop shortcut, upload files or initialize Git. Build output/logs are ignored and must not be committed.

The equivalent UAT command is:

```powershell
& '<EngineRoot>\Engine\Build\BatchFiles\RunUAT.bat' BuildPlugin `
  '-Plugin=<RepositoryRoot>\WatcherStealthGameplay.uplugin' `
  '-Package=<FreshOutputDirectory>' -TargetPlatforms=Win64 -NoP4 -Unattended -UTF8Output
```

## Host integration build

Place the source at `<HostProject>/Plugins/WatcherStealthGameplay/`. Enable the plugin, regenerate project files if needed, then build `<HostProject>Editor` Development Win64 through the host's normal UnrealBuildTool workflow. The plugin's single runtime module is **WatcherShooter**. A host cannot also provide a different module with that name. Resolve that module ownership before installing into an existing project.

`AIModule` is a public dependency because `WatcherSightTarget.h` publicly implements its sight interface. Core/CoreUObject/Engine and the original integration dependency list are retained. InputCore, NavigationSystem, MotionWarping, EnhancedInput and Niagara are private; AnimGraph/BlueprintGraph are added only for Editor builds. The `.uplugin` enables the engine's EnhancedInput, MotionWarping and Niagara plugins. Several retained integration modules are not directly referenced by every extracted class.

No GASPALS C++ module or binary is linked. Its optional combat integration uses runtime class paths/reflection; absent content prevents that feature from operating. Host-owned maps, input assets, montage paths and runtime-loaded classes need explicit cook/reference rules before game packaging.

## Recording a result

Report the commit, exact engine patch, compiler/SDK, command, exit code and tested target. Redact personal absolute paths before sharing a minimal diagnostic. Record plugin compilation, game packaging, NullRHI loading, rendered play and physical keyboard/mouse testing as separate results.
