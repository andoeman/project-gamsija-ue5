param([Parameter(Mandatory = $true)][string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$repoRoot = Split-Path $PSScriptRoot -Parent
$descriptor = Join-Path $repoRoot 'WatcherStealthGameplay.uplugin'
$uat = Join-Path ([IO.Path]::GetFullPath($EngineRoot)) 'Engine/Build/BatchFiles/RunUAT.bat'
foreach ($required in @($descriptor, $uat)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) { throw "Required file missing: $required" }
}
& (Join-Path $PSScriptRoot 'Validate-Source.ps1')
$runId = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$buildRoot = Join-Path $repoRoot '.build'
$package = Join-Path $buildRoot ("Package-$runId")
$log = Join-Path $buildRoot ("build-plugin-$runId.log")
$report = Join-Path $buildRoot ("build-plugin-$runId.json")
if (Test-Path -LiteralPath $package) { throw "Output already exists: $package" }
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null
$result = [ordered]@{
    status = 'RUNNING'; exit_code = $null; package = $package; log = $log
    compiler_version_verified = $false; game_packaging_tested = $false
    gameplay_tested = $false; physical_input_tested = $false
    error = $null; completed_at = $null
}
try {
    $uatArgs = @('BuildPlugin', "-Plugin=$descriptor", "-Package=$package",
        '-TargetPlatforms=Win64', '-NoP4', '-Unattended', '-UTF8Output')
    & $uat @uatArgs *> $log
    $result.exit_code = $LASTEXITCODE
    if ($result.exit_code -ne 0) { throw "BuildPlugin failed: $($result.exit_code). See $log" }
    if (-not (Test-Path -LiteralPath (Join-Path $package 'WatcherStealthGameplay.uplugin') -PathType Leaf)) {
        throw 'BuildPlugin returned success but the packaged descriptor is missing'
    }
    $result.status = 'BUILT_PLUGIN_NOT_GAMEPLAY_TESTED'
}
catch { $result.status = 'FAILED'; $result.error = $_.Exception.Message; throw }
finally {
    $result.completed_at = [DateTimeOffset]::Now.ToString('o')
    $result | ConvertTo-Json | Set-Content -LiteralPath $report -Encoding utf8
}
Write-Output ($result | ConvertTo-Json -Compress)
