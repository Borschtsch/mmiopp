param(
  [Parameter(Mandatory = $true)]
  [string]$Preset,

  [Parameter(Mandatory = $true)]
  [string]$BuildDir
)

$ErrorActionPreference = 'Stop'

& (Join-Path $PSScriptRoot 'run-ci-workflow.ps1') -Preset $Preset -BuildDir $BuildDir
& (Join-Path $PSScriptRoot 'ci-coverage.ps1') -BuildDir $BuildDir