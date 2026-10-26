$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path $PSScriptRoot -Parent
$required = @(
    'WatcherStealthGameplay.uplugin', 'LICENSE', 'README.md', 'CONTRIBUTING.md', 'THIRD_PARTY_NOTICES.md', '.gitignore',
    'Source/WatcherShooter/WatcherShooter.Build.cs', 'Source/WatcherShooter/Private/WatcherShooter.cpp',
    'docs/build.md', 'docs/integration.md', 'docs/validation.md', 'docs/roadmap.md',
    'scripts/Build-Plugin.ps1', 'scripts/Validate-Source.ps1'
)
foreach ($stem in @('WatcherGameplayLibrary', 'WatcherSightTarget', 'WatcherGaspalsHealthComponent', 'WatcherGaspalsCombatComponent')) {
    $required += "Source/WatcherShooter/Public/$stem.h"
    $required += "Source/WatcherShooter/Private/$stem.cpp"
}
foreach ($relative in $required) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $relative) -PathType Leaf)) { throw "Missing source file: $relative" }
}
$descriptor = Get-Content -LiteralPath (Join-Path $repoRoot 'WatcherStealthGameplay.uplugin') -Raw | ConvertFrom-Json
if (@($descriptor.Modules).Count -ne 1 -or $descriptor.Modules[0].Name -ne 'WatcherShooter' -or
    $descriptor.Modules[0].Type -ne 'Runtime' -or $descriptor.CanContainContent) { throw 'Unexpected module/content descriptor' }
foreach ($dependency in @('EnhancedInput', 'MotionWarping', 'Niagara')) {
    if (-not @($descriptor.Plugins | Where-Object { $_.Name -eq $dependency -and $_.Enabled }).Count) { throw "Missing plugin dependency: $dependency" }
}
$entry = Get-Content -LiteralPath (Join-Path $repoRoot 'Source/WatcherShooter/Private/WatcherShooter.cpp') -Raw
if ($entry -notmatch 'IMPLEMENT_MODULE\s*\(\s*FDefaultModuleImpl\s*,\s*WatcherShooter\s*\)' -or
    $entry -match 'IMPLEMENT_PRIMARY_GAME_MODULE') { throw 'Runtime module entry point mismatch' }
$files = @(Get-ChildItem -LiteralPath $repoRoot -Recurse -File -Force | Where-Object {
    $_.FullName.Substring($repoRoot.Length + 1) -notmatch '^(\.git|\.build|Binaries|Intermediate|Saved|DerivedDataCache)[\\/]'
})
foreach ($file in $files) {
    $relative = $file.FullName.Substring($repoRoot.Length + 1).Replace('\', '/')
    if ($required -notcontains $relative) { throw "Unreviewed public export file: $relative" }
    $text = Get-Content -LiteralPath $file.FullName -Raw
    if ($text -match '(?i)[A-Z]:[\\/]Users[\\/][^\\/\s]+' -or
        $text -match '(?i)gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----') {
        throw "Possible private path/credential in: $relative"
    }
}
[pscustomobject]@{ status = 'PASS_STATIC_STRUCTURE_AND_EXPORT_SCAN'; files = $files.Count; compiled = $false; gameplay_tested = $false } | ConvertTo-Json -Compress
