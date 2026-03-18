param(
  [switch]$PortableFallbackOnly,
  [switch]$InstallPortableFallback,
  [switch]$InstallCMake,
  [switch]$InstallArmToolchain,
  [switch]$InstallQemu,
  [switch]$InstallNinja
)

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$toolRoot = Join-Path $repoRoot '.local'
$toolchainVersion = '15.2.0posix-13.0.0-ucrt-r4'
$toolchainArchive = 'winlibs-x86_64-posix-seh-gcc-15.2.0-mingw-w64ucrt-13.0.0-r4.zip'
$toolchainUrl = "https://github.com/brechtsanders/winlibs_mingw/releases/download/$toolchainVersion/$toolchainArchive"
$toolchainArchivePath = Join-Path $toolRoot $toolchainArchive
$toolInstallDir = Join-Path $toolRoot 'winlibs'
$gxx = Join-Path $toolInstallDir 'mingw64\bin\g++.exe'
$repoLocalCmake = Join-Path $toolInstallDir 'mingw64\bin\cmake.exe'
$ninjaInstallDir = Join-Path $toolRoot 'ninja'
$repoLocalNinja = Join-Path $ninjaInstallDir 'ninja.exe'

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

function Test-ReadableFile {
  param([string]$Path)

  if (-not (Test-Path $Path -PathType Leaf)) {
    return $false
  }

  try {
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    $stream.Dispose()
    return $true
  } catch {
    return $false
  }
}

function Resolve-NinjaExecutable {
  if (Test-ReadableFile $repoLocalNinja) {
    return $repoLocalNinja
  }

  $candidates = New-Object System.Collections.Generic.List[string]
  $command = Get-Command ninja -ErrorAction SilentlyContinue
  if ($command -and $command.Source) {
    $candidates.Add($command.Source)
  }

  $globs = @(
    'C:/Program Files/Microsoft Visual Studio/*/*/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe',
    'C:/Program Files/Ninja/ninja.exe',
    'C:/Program Files (x86)/Ninja/ninja.exe'
  )

  foreach ($glob in $globs) {
    foreach ($match in @(Get-ChildItem -Path $glob -ErrorAction SilentlyContinue)) {
      $candidates.Add($match.FullName)
    }
  }

  foreach ($candidate in ($candidates | Select-Object -Unique)) {
    if (Test-ReadableFile $candidate) {
      return $candidate
    }
  }

  return $null
}

function Ensure-RepoLocalNinja {
  $ninjaPath = Resolve-NinjaExecutable
  if (-not $ninjaPath) {
    throw 'Ninja executable was not found after installation.'
  }

  if ((Test-ReadableFile $repoLocalNinja) -and ((Resolve-Path $repoLocalNinja).Path -eq (Resolve-Path $ninjaPath).Path)) {
    return
  }

  New-Item -ItemType Directory -Force -Path $ninjaInstallDir | Out-Null
  Copy-Item -Path $ninjaPath -Destination $repoLocalNinja -Force
}

function Test-CMakeAvailable {
  if (Get-Command cmake -ErrorAction SilentlyContinue) {
    return $true
  }

  return (Test-Path $repoLocalCmake)
}

function Test-ArmToolchainAvailable {
  if (Get-Command arm-none-eabi-g++ -ErrorAction SilentlyContinue) {
    return $true
  }

  $candidate = @(Get-ChildItem -Path 'C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi/*/bin/arm-none-eabi-g++.exe' -ErrorAction SilentlyContinue)
  return $candidate.Count -gt 0
}

function Test-QemuAvailable {
  if (Get-Command qemu-system-arm -ErrorAction SilentlyContinue) {
    return $true
  }

  return (Test-Path 'C:/Program Files/qemu/qemu-system-arm.exe')
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

if ($PortableFallbackOnly) {
  $InstallPortableFallback = $true
  $InstallCMake = $false
  $InstallArmToolchain = $false
  $InstallQemu = $false
  $InstallNinja = $false
} elseif (-not ($InstallPortableFallback -or $InstallCMake -or $InstallArmToolchain -or $InstallQemu -or $InstallNinja)) {
  $InstallPortableFallback = $true
  $InstallCMake = $true
  $InstallArmToolchain = $true
  $InstallQemu = $true
  $InstallNinja = $true
}

$packages = @()
if ($InstallCMake) {
  if (Test-CMakeAvailable) {
    Write-Host 'CMake is already available.'
  } else {
    $packages += @{ Id = 'Kitware.CMake'; Name = 'CMake' }
  }
}

if ($InstallArmToolchain) {
  if (Test-ArmToolchainAvailable) {
    Write-Host 'Arm GNU Toolchain is already available.'
  } else {
    $packages += @{ Id = 'Arm.ArmGnuToolchain'; Name = 'Arm GNU Toolchain' }
  }
}

if ($InstallQemu) {
  if (Test-QemuAvailable) {
    Write-Host 'QEMU is already available.'
  } else {
    $packages += @{ Id = 'SoftwareFreedomConservancy.QEMU'; Name = 'QEMU' }
  }
}

if ($InstallNinja) {
  if (Resolve-NinjaExecutable) {
    Write-Host 'Ninja is already available.'
  } else {
    $packages += @{ Id = 'Ninja-build.Ninja'; Name = 'Ninja' }
  }
}

if ($packages.Count -gt 0) {
  $winget = Get-Command winget -ErrorAction SilentlyContinue
  if (-not $winget) {
    throw "winget was not found on PATH. Either install winget first or run 'powershell -ExecutionPolicy Bypass -File .\\scripts\\bootstrap.ps1 -PortableFallbackOnly'."
  }

  foreach ($package in $packages) {
    Install-WingetPackage -Id $package.Id -Name $package.Name
  }
}

if ($InstallNinja) {
  Ensure-RepoLocalNinja
}

if ($InstallPortableFallback) {
  Install-PortableFallback
}

Write-Host 'Bootstrap completed.'
Write-Host 'Host build path: cmake --workflow --preset host'
Write-Host 'Host test path: cmake --workflow --preset host-test'
Write-Host 'QEMU build path: cmake --workflow --preset qemu-m3-build'
Write-Host 'QEMU run path: cmake --workflow --preset qemu-m3-run'
Write-Host 'QEMU test path: cmake --workflow --preset qemu-m3-test'
Write-Host 'QEMU Cortex-R5 build path: cmake --workflow --preset qemu-r5-build'
Write-Host 'QEMU Cortex-R5 run path: cmake --workflow --preset qemu-r5-run'
Write-Host 'QEMU Cortex-R5 test path: cmake --workflow --preset qemu-r5-test'