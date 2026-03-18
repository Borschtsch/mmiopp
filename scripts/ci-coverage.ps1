param(
  [Parameter(Mandatory = $true)]
  [string]$BuildDir,

  [string]$OutputDir = '',

  [string]$GcovExe = '',

  [string]$CoverageSource = 'the instrumented test build'
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
  $BuildDir
} else {
  Join-Path $repoRoot $BuildDir
}

$resolvedOutputDir = if ([string]::IsNullOrWhiteSpace($OutputDir)) {
  Join-Path $resolvedBuildDir 'coverage'
} elseif ([System.IO.Path]::IsPathRooted($OutputDir)) {
  $OutputDir
} else {
  Join-Path $repoRoot $OutputDir
}

New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null
$gcovOutputDir = Join-Path $resolvedOutputDir 'gcov'
if (Test-Path $gcovOutputDir) {
  Remove-Item -Path $gcovOutputDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $gcovOutputDir | Out-Null

function Get-CMakeCacheValue {
  param(
    [string]$CachePath,
    [string]$VariableName
  )

  if (-not (Test-Path $CachePath)) {
    return $null
  }

  $pattern = '^{0}:[^=]+=(.*)$' -f [regex]::Escape($VariableName)
  foreach ($line in [System.IO.File]::ReadLines($CachePath)) {
    $match = [regex]::Match($line, $pattern)
    if ($match.Success) {
      return $match.Groups[1].Value.Trim()
    }
  }

  return $null
}

function Resolve-GcovExecutable {
  param(
    [string]$RequestedPath,
    [string]$BuildDir,
    [string]$RepoRoot
  )

  if (-not [string]::IsNullOrWhiteSpace($RequestedPath)) {
    $candidate = if ([System.IO.Path]::IsPathRooted($RequestedPath)) {
      $RequestedPath
    } else {
      Join-Path $RepoRoot $RequestedPath
    }

    if (-not (Test-Path $candidate)) {
      throw "The requested gcov executable was not found: $candidate"
    }

    return (Resolve-Path $candidate).Path
  }

  $cachePath = Join-Path $BuildDir 'CMakeCache.txt'
  $compilerPath = Get-CMakeCacheValue -CachePath $cachePath -VariableName 'CMAKE_CXX_COMPILER'
  if (-not [string]::IsNullOrWhiteSpace($compilerPath)) {
    $compilerDir = Split-Path -Path $compilerPath -Parent
    $compilerLeaf = Split-Path -Path $compilerPath -Leaf
    if ($compilerLeaf -match 'g\+\+(\.exe)?$') {
      $gcovLeaf = $compilerLeaf -replace 'g\+\+(\.exe)?$', 'gcov$1'
      $siblingPath = Join-Path $compilerDir $gcovLeaf
      if (Test-Path $siblingPath) {
        return (Resolve-Path $siblingPath).Path
      }
    }
  }

  if (-not [string]::IsNullOrWhiteSpace($compilerPath) -and ($compilerPath -like '*arm-none-eabi*')) {
    $armCommand = Get-Command arm-none-eabi-gcov -ErrorAction SilentlyContinue
    if ($armCommand) {
      return $armCommand.Source
    }

    $armCandidates = @(
      Get-ChildItem -Path 'C:/Program Files (x86)/Arm GNU Toolchain arm-none-eabi' -Recurse -Filter arm-none-eabi-gcov.exe -ErrorAction SilentlyContinue |
        Select-Object -ExpandProperty FullName
    )
    if ($armCandidates.Count -gt 0) {
      return $armCandidates[0]
    }
  }

  $gcov = Get-Command gcov -ErrorAction SilentlyContinue
  if ($gcov) {
    return $gcov.Source
  }

  $repoLocalGcov = Join-Path $RepoRoot '.local\winlibs\mingw64\bin\gcov.exe'
  if (Test-Path $repoLocalGcov) {
    return (Resolve-Path $repoLocalGcov).Path
  }

  throw 'gcov was not found. Provide -GcovExe or ensure the matching toolchain gcov executable is installed.'
}

$gcovExe = Resolve-GcovExecutable -RequestedPath $GcovExe -BuildDir $resolvedBuildDir -RepoRoot $repoRoot

$gcdaFiles = @(
  Get-ChildItem -Path $resolvedBuildDir -Recurse -Filter *.gcda -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty FullName
)
if ($gcdaFiles.Count -eq 0) {
  throw 'No .gcda files were found. Run the coverage-instrumented tests before collecting coverage.'
}

$gcovRunIndex = 0
foreach ($gcdaFile in $gcdaFiles) {
  ++$gcovRunIndex
  $gcovRunOutputDir = Join-Path $gcovOutputDir ('run-{0:D4}' -f $gcovRunIndex)
  New-Item -ItemType Directory -Force -Path $gcovRunOutputDir | Out-Null

  Push-Location $gcovRunOutputDir
  try {
    & $gcovExe -b -c $gcdaFile 2>&1 | Out-Null
  } finally {
    Pop-Location
  }
}

$gcovFiles = @(
  Get-ChildItem -Path $gcovOutputDir -Recurse -Filter *.gcov -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty FullName
)
if ($gcovFiles.Count -eq 0) {
  throw 'gcov did not emit any .gcov files.'
}

function Get-NormalizedPath {
  param([string]$Path)

  return [System.IO.Path]::GetFullPath($Path).Replace('/', '\').TrimEnd('\')
}

function Get-OrCreateCoverageEntry {
  param(
    [hashtable]$CoverageByFile,
    [string]$SourcePath
  )

  if (-not $CoverageByFile.ContainsKey($SourcePath)) {
    $CoverageByFile[$SourcePath] = @{
      lines = @{}
      branches = @{}
    }
  }

  return $CoverageByFile[$SourcePath]
}

function Merge-LineCoveragePoint {
  param(
    [hashtable]$Points,
    [int]$LineNumber,
    [long]$Hits
  )

  $key = [string]$LineNumber
  if (-not $Points.ContainsKey($key)) {
    $Points[$key] = [pscustomobject]@{
      lineNumber = $LineNumber
      hits = [long]0
    }
  }

  if ($Hits -gt $Points[$key].hits) {
    $Points[$key].hits = $Hits
  }
}

function Merge-BranchCoveragePoint {
  param(
    [hashtable]$Points,
    [int]$LineNumber,
    [int]$BranchIndex,
    [long]$Hits
  )

  $key = '{0}.{1}' -f $LineNumber, $BranchIndex
  if (-not $Points.ContainsKey($key)) {
    $Points[$key] = [pscustomobject]@{
      lineNumber = $LineNumber
      branchIndex = $BranchIndex
      hits = [long]0
    }
  }

  if ($Hits -gt $Points[$key].hits) {
    $Points[$key].hits = $Hits
  }
}

function Get-BranchHits {
  param([string]$Status)

  if ($Status -notlike 'taken *') {
    return [long]0
  }

  $taken = $Status.Substring(6).Trim()
  if ($taken -match '^([0-9]+)$') {
    return [long]$Matches[1]
  }

  if ($taken -match '^([0-9]+(?:\.[0-9]+)?)%') {
    if ([double]$Matches[1] -gt 0) {
      return [long]1
    }
  }

  return [long]0
}

function Write-LcovReport {
  param(
    [string]$Path,
    [object[]]$Files
  )

  $lines = New-Object System.Collections.Generic.List[string]
  foreach ($file in $Files) {
    $linePoints = @($file.lines | Sort-Object lineNumber)
    $branchPoints = @($file.branches | Sort-Object lineNumber, branchIndex)

    $lines.Add("SF:$($file.relativePath)")
    foreach ($point in $linePoints) {
      $lines.Add("DA:$($point.lineNumber),$($point.hits)")
    }
    $lines.Add("LF:$($linePoints.Count)")
    $lines.Add("LH:$(@($linePoints | Where-Object { $_.hits -gt 0 }).Count)")

    foreach ($point in $branchPoints) {
      $lines.Add("BRDA:$($point.lineNumber),0,$($point.branchIndex),$($point.hits)")
    }
    $lines.Add("BRF:$($branchPoints.Count)")
    $lines.Add("BRH:$(@($branchPoints | Where-Object { $_.hits -gt 0 }).Count)")
    $lines.Add('end_of_record')
  }

  $content = if ($lines.Count -gt 0) {
    ($lines -join "`n") + "`n"
  } else {
    ''
  }
  [System.IO.File]::WriteAllText($Path, $content, [System.Text.UTF8Encoding]::new($false))
}

$coverageByFile = @{}

foreach ($gcovFile in $gcovFiles) {
  $sourcePath = $null
  $currentLineNumber = 0
  $branchOrdinalByLine = @{}

  foreach ($line in [System.IO.File]::ReadLines($gcovFile)) {
    if ($line -match '^\s*-:\s*0:Source:(.+)$') {
      $sourcePath = Get-NormalizedPath $Matches[1].Trim()
      continue
    }

    if ([string]::IsNullOrWhiteSpace($sourcePath)) {
      continue
    }

    if ($line -match '^\s*(?<count>-|#+|=+|\d+):\s*(?<line>\d+):(.*)$') {
      $lineNumber = [int]$Matches['line']
      if ($lineNumber -le 0) {
        continue
      }

      $currentLineNumber = $lineNumber
      if (-not $branchOrdinalByLine.ContainsKey($lineNumber)) {
        $branchOrdinalByLine[$lineNumber] = 0
      }

      $countToken = $Matches['count']
      if ($countToken -eq '-') {
        continue
      }

      $hits = if ($countToken -match '^\d+$') { [long]$countToken } else { [long]0 }
      $entry = Get-OrCreateCoverageEntry -CoverageByFile $coverageByFile -SourcePath $sourcePath
      Merge-LineCoveragePoint -Points $entry.lines -LineNumber $lineNumber -Hits $hits
      continue
    }

    if ($line -match '^\s*branch\s+\d+\s+(?<status>never executed|taken\s+.+)$') {
      if ($currentLineNumber -le 0) {
        continue
      }

      $entry = Get-OrCreateCoverageEntry -CoverageByFile $coverageByFile -SourcePath $sourcePath
      $branchIndex = [int]$branchOrdinalByLine[$currentLineNumber]
      $branchOrdinalByLine[$currentLineNumber] = $branchIndex + 1
      $branchHits = Get-BranchHits -Status $Matches['status']
      Merge-BranchCoveragePoint -Points $entry.branches -LineNumber $currentLineNumber -BranchIndex $branchIndex -Hits $branchHits
    }
  }
}

$normalizedRepoRoot = Get-NormalizedPath $repoRoot
$repoCoverageEntries = @(
  foreach ($sourcePath in ($coverageByFile.Keys | Sort-Object)) {
    if (-not $sourcePath.StartsWith($normalizedRepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
      continue
    }

    $entry = $coverageByFile[$sourcePath]
    $relativePath = $sourcePath.Substring($normalizedRepoRoot.Length + 1).Replace('\', '/')
    $linePoints = @($entry.lines.Values | Sort-Object lineNumber)
    $branchPoints = @($entry.branches.Values | Sort-Object lineNumber, branchIndex)
    $lineCovered = @($linePoints | Where-Object { $_.hits -gt 0 }).Count
    $lineTotal = $linePoints.Count
    $branchCovered = @($branchPoints | Where-Object { $_.hits -gt 0 }).Count
    $branchTotal = $branchPoints.Count

    [pscustomobject]@{
      path = $sourcePath
      relativePath = $relativePath
      lines = $linePoints
      branches = $branchPoints
      lineCovered = $lineCovered
      lineTotal = $lineTotal
      linePercent = if ($lineTotal -gt 0) { [math]::Round(($lineCovered * 100.0) / $lineTotal, 2) } else { 0.0 }
      branchCovered = $branchCovered
      branchTotal = $branchTotal
      branchPercent = if ($branchTotal -gt 0) { [math]::Round(($branchCovered * 100.0) / $branchTotal, 2) } else { 0.0 }
    }
  }
)

$repoEntries = @(
  $repoCoverageEntries | ForEach-Object {
    [pscustomobject]@{
      path = $_.path
      relativePath = $_.relativePath
      lineCovered = $_.lineCovered
      lineTotal = $_.lineTotal
      linePercent = $_.linePercent
      branchCovered = $_.branchCovered
      branchTotal = $_.branchTotal
      branchPercent = $_.branchPercent
    }
  }
)

function New-CoverageAggregate {
  param(
    [string]$Name,
    [object[]]$Files,
    [string]$FilterDescription
  )

  $lineCovered = [int](($Files | Measure-Object -Property lineCovered -Sum).Sum)
  $lineTotal = [int](($Files | Measure-Object -Property lineTotal -Sum).Sum)
  $branchCovered = [int](($Files | Measure-Object -Property branchCovered -Sum).Sum)
  $branchTotal = [int](($Files | Measure-Object -Property branchTotal -Sum).Sum)

  [pscustomobject]@{
    name = $Name
    filter = $FilterDescription
    fileCount = @($Files).Count
    lines = [pscustomobject]@{
      covered = $lineCovered
      total = $lineTotal
      percent = if ($lineTotal -gt 0) { [math]::Round(($lineCovered * 100.0) / $lineTotal, 2) } else { 0.0 }
    }
    branches = [pscustomobject]@{
      covered = $branchCovered
      total = $branchTotal
      percent = if ($branchTotal -gt 0) { [math]::Round(($branchCovered * 100.0) / $branchTotal, 2) } else { 0.0 }
    }
  }
}

$coreCoverageEntries = @($repoCoverageEntries | Where-Object { $_.relativePath -like 'include/*' })
$exampleCoverageEntries = @($repoCoverageEntries | Where-Object { $_.relativePath -like 'examples/*' })
$productCoverageEntries = @($repoCoverageEntries | Where-Object { $_.relativePath -like 'include/*' -or $_.relativePath -like 'examples/*' })
$coreEntries = @($coreCoverageEntries | Select-Object path, relativePath, lineCovered, lineTotal, linePercent, branchCovered, branchTotal, branchPercent)
$exampleEntries = @($exampleCoverageEntries | Select-Object path, relativePath, lineCovered, lineTotal, linePercent, branchCovered, branchTotal, branchPercent)
$productEntries = @($productCoverageEntries | Select-Object path, relativePath, lineCovered, lineTotal, linePercent, branchCovered, branchTotal, branchPercent)

$summary = [ordered]@{
  generatedAtUtc = (Get-Date).ToUniversalTime().ToString('o')
  buildDir = $resolvedBuildDir
  gcovOutputDir = $gcovOutputDir
  files = $repoEntries
  scopes = [ordered]@{
    core = (New-CoverageAggregate -Name 'core' -Files $coreEntries -FilterDescription 'include/*')
    examples = (New-CoverageAggregate -Name 'examples' -Files $exampleEntries -FilterDescription 'examples/*')
    product = (New-CoverageAggregate -Name 'product' -Files $productEntries -FilterDescription 'include/*, examples/*')
  }
}

$coverageJsonPath = Join-Path $resolvedOutputDir 'coverage-summary.json'
$coverageMarkdownPath = Join-Path $resolvedOutputDir 'coverage-summary.md'
$coreLcovPath = Join-Path $resolvedOutputDir 'coverage-core.info'
$productLcovPath = Join-Path $resolvedOutputDir 'coverage-product.info'

[System.IO.File]::WriteAllText($coverageJsonPath, ($summary | ConvertTo-Json -Depth 6), [System.Text.UTF8Encoding]::new($false))
Write-LcovReport -Path $coreLcovPath -Files $coreCoverageEntries
Write-LcovReport -Path $productLcovPath -Files $productCoverageEntries

$markdown = @"
# Coverage Summary

Generated at: $($summary.generatedAtUtc)

Coverage comes from $CoverageSource and reflects code reached through that instrumented path.

| Scope | Files | Line Coverage | Branch Coverage |
| --- | ---: | ---: | ---: |
| Core (`include/`) | $($summary.scopes.core.fileCount) | $($summary.scopes.core.lines.percent)% ($($summary.scopes.core.lines.covered)/$($summary.scopes.core.lines.total)) | $($summary.scopes.core.branches.percent)% ($($summary.scopes.core.branches.covered)/$($summary.scopes.core.branches.total)) |
| Examples (`examples/`) | $($summary.scopes.examples.fileCount) | $($summary.scopes.examples.lines.percent)% ($($summary.scopes.examples.lines.covered)/$($summary.scopes.examples.lines.total)) | $($summary.scopes.examples.branches.percent)% ($($summary.scopes.examples.branches.covered)/$($summary.scopes.examples.branches.total)) |
| Product (`include/` + `examples/`) | $($summary.scopes.product.fileCount) | $($summary.scopes.product.lines.percent)% ($($summary.scopes.product.lines.covered)/$($summary.scopes.product.lines.total)) | $($summary.scopes.product.branches.percent)% ($($summary.scopes.product.branches.covered)/$($summary.scopes.product.branches.total)) |
"@
[System.IO.File]::WriteAllText($coverageMarkdownPath, $markdown, [System.Text.UTF8Encoding]::new($false))

if ($env:GITHUB_ENV) {
  "COVERAGE_JSON_PATH=$coverageJsonPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
  "COVERAGE_MARKDOWN_PATH=$coverageMarkdownPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
  "COVERAGE_CORE_LCOV_PATH=$coreLcovPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
  "COVERAGE_PRODUCT_LCOV_PATH=$productLcovPath" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
}

if ($env:GITHUB_STEP_SUMMARY) {
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "### Coverage Summary`n"
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "| Scope | Line Coverage | Branch Coverage |`n| --- | ---: | ---: |"
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "| Core (`include/`) | $($summary.scopes.core.lines.percent)% | $($summary.scopes.core.branches.percent)% |"
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "| Product (`include/` + `examples/`) | $($summary.scopes.product.lines.percent)% | $($summary.scopes.product.branches.percent)% |"
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value ""
}

Write-Host "Coverage summary written to $coverageJsonPath"
Write-Host "Core LCOV report written to $coreLcovPath"
Write-Host "Product LCOV report written to $productLcovPath"
