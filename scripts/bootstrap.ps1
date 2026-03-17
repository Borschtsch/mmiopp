param(
  [switch]$PortableFallbackOnly
)

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$toolRoot = Join-Path $repoRoot '.local'
$toolchainVersion = '15.2.0posix-13.0.0-ucrt-r4'
$toolchainArchive = 'winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4.zip'
$toolchainUrl = "https://github.com/brechtsanders/winlibs_mingw/releases/download/$toolchainVersion/$toolchainArchive"
$toolchainArchivePath = Join-Path $toolRoot $toolchainArchive
$toolInstallDir = Join-Path $toolRoot 'winlibs'
$gxx = Join-Path $toolInstallDir 'mingw64\bin\g++.exe'
$packages = @(
  @{ Id = 'Kitware.CMake'; Name = 'CMake' },
  @{ Id = 'Arm.ArmGnuToolchain'; Name = 'Arm GNU Toolchain' },
  @{ Id = 'SoftwareFreedomConservancy.QEMU'; Name = 'QEMU' }
)

function Install-WingetPackage {
  param(
    [Parameter(Mandatory = $true)]
    [string]$Id,
    [Parameter(Mandatory = $true)]
    [string]$Name
  )

  Write-Host "Installing $Name via winget ($Id)"
  & winget install --exact --id $Id --accept-package-agreements --accept-source-agreements
  if ($LASTEXITCODE -ne 0) {
    throw "winget install failed for $Id"
  }
}

function Install-PortableFallback {
  if (Test-Path $gxx) {
    Write-Host "Portable WinLibs toolchain already installed at $toolInstallDir"
    return
  }

  $previousProgressPreference = $ProgressPreference
  try {
    $ProgressPreference = 'SilentlyContinue'

    New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null

    if (Test-Path $toolchainArchivePath) {
      Remove-Item $toolchainArchivePath -Force
    }

    if (Test-Path $toolInstallDir) {
      Remove-Item $toolInstallDir -Recurse -Force
    }

    Write-Host "Downloading WinLibs toolchain from $toolchainUrl"
    Invoke-WebRequest -UseBasicParsing -Uri $toolchainUrl -OutFile $toolchainArchivePath

    Write-Host "Extracting toolchain into $toolInstallDir"
    Expand-Archive -Path $toolchainArchivePath -DestinationPath $toolInstallDir -Force
    Remove-Item $toolchainArchivePath -Force
  }
  finally {
    $ProgressPreference = $previousProgressPreference
  }

  if (-not (Test-Path $gxx)) {
    throw "Portable fallback install completed without $gxx"
  }
}

if (-not $PortableFallbackOnly) {
  $winget = Get-Command winget -ErrorAction SilentlyContinue
  if (-not $winget) {
    throw "winget was not found on PATH. Either install winget first or run 'powershell -ExecutionPolicy Bypass -File .\\scripts\\bootstrap.ps1 -PortableFallbackOnly'."
  }

  foreach ($package in $packages) {
    Install-WingetPackage -Id $package.Id -Name $package.Name
  }
}

Install-PortableFallback

Write-Host 'Bootstrap completed.'
Write-Host 'Host build path: cmake --workflow --preset host'
Write-Host 'Host test path: cmake --workflow --preset host-test'
Write-Host 'QEMU build path: cmake --workflow --preset qemu-m3-build'
Write-Host 'QEMU run path: cmake --workflow --preset qemu-m3-run'
Write-Host 'QEMU test path: cmake --workflow --preset qemu-m3-test'
Write-Host 'QEMU Cortex-R5 build path: cmake --workflow --preset qemu-r5-build'
Write-Host 'QEMU Cortex-R5 run path: cmake --workflow --preset qemu-r5-run'
Write-Host 'QEMU Cortex-R5 test path: cmake --workflow --preset qemu-r5-test'
