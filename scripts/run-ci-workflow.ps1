param(
  [Parameter(Mandatory = $true)]
  [string]$Preset,

  [Parameter(Mandatory = $true)]
  [string]$BuildDir
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
  $BuildDir
} else {
  Join-Path $repoRoot $BuildDir
}

New-Item -ItemType Directory -Force -Path $resolvedBuildDir | Out-Null
$workflowLogPath = Join-Path $resolvedBuildDir 'ci-workflow.log'

$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
  $cmake = Get-Command (Join-Path $repoRoot '.local\winlibs\mingw64\bin\cmake.exe') -ErrorAction SilentlyContinue
}

if (-not $cmake) {
  throw 'cmake was not found on PATH and the repo-local fallback was not present.'
}

$start = Get-Date
& $cmake.Source --workflow --preset $Preset 2>&1 | Tee-Object -FilePath $workflowLogPath
$exitCode = $LASTEXITCODE
$duration = [math]::Round(((Get-Date) - $start).TotalSeconds, 2)

if ($env:GITHUB_ENV) {
  "WORKFLOW_DURATION_SECONDS=$duration" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
  "WORKFLOW_EXIT_CODE=$exitCode" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
  "WORKFLOW_LOG_PATH=$workflowLogPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
}

if ($exitCode -ne 0) {
  exit $exitCode
}
