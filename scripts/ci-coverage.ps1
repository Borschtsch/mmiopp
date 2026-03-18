param(
  [Parameter(Mandatory = $true)]
  [string]$BuildDir,

  [string]$OutputDir = ''
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

$gcov = Get-Command gcov -ErrorAction SilentlyContinue
if (-not $gcov) {
  $gcov = Get-Command (Join-Path $repoRoot '.local\winlibs\mingw64\bin\gcov.exe') -ErrorAction SilentlyContinue
}
if (-not $gcov) {
  throw 'gcov was not found on PATH and the repo-local fallback was not present.'
}

$gcdaFiles = @(
  Get-ChildItem -Path $resolvedBuildDir -Recurse -Filter *.gcda -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty FullName
)
if ($gcdaFiles.Count -eq 0) {
  throw 'No .gcda files were found. Run the coverage-instrumented tests before collecting coverage.'
}

Push-Location $gcovOutputDir
try {
  foreach ($gcdaFile in $gcdaFiles) {
    & $gcov.Source -b -c -l -p $gcdaFile 2>&1 | Out-Null
  }
} finally {
  Pop-Location
}

$gcovFiles = @(
  Get-ChildItem -Path $gcovOutputDir -Filter *.gcov -ErrorAction SilentlyContinue |
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

function Merge-CoveragePoint {
  param(
    [hashtable]$Points,
    [string]$Key,
    [bool]$Covered
  )

  if (-not $Points.ContainsKey($Key)) {
    $Points[$Key] = @{
      coverable = $true
      covered = $false
    }
  }

  if ($Covered) {
    $Points[$Key].covered = $true
  }
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

      $isCovered = $countToken -match '^\d+$' -and [int64]$countToken -gt 0
      $entry = Get-OrCreateCoverageEntry -CoverageByFile $coverageByFile -SourcePath $sourcePath
      Merge-CoveragePoint -Points $entry.lines -Key ([string]$lineNumber) -Covered $isCovered
      continue
    }

    if ($line -match '^\s*branch\s+\d+\s+(?<status>never executed|taken\s+.+)$') {
      if ($currentLineNumber -le 0) {
        continue
      }

      $entry = Get-OrCreateCoverageEntry -CoverageByFile $coverageByFile -SourcePath $sourcePath
      $branchOrdinal = $branchOrdinalByLine[$currentLineNumber]
      $branchOrdinalByLine[$currentLineNumber] = $branchOrdinal + 1
      $branchKey = '{0}.{1}' -f $currentLineNumber, $branchOrdinal

      $status = $Matches['status']
      $isCovered = $false
      if ($status -like 'taken *') {
        $taken = $status.Substring(6).Trim()
        if ($taken -match '^([0-9]+(?:\.[0-9]+)?)%') {
          $isCovered = [double]$Matches[1] -gt 0
        } elseif ($taken -match '^([0-9]+)') {
          $isCovered = [int64]$Matches[1] -gt 0
        }
      }

      Merge-CoveragePoint -Points $entry.branches -Key $branchKey -Covered $isCovered
    }
  }
}

$normalizedRepoRoot = Get-NormalizedPath $repoRoot
$repoEntries = @(
  foreach ($sourcePath in ($coverageByFile.Keys | Sort-Object)) {
    if (-not $sourcePath.StartsWith($normalizedRepoRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
      continue
    }

    $entry = $coverageByFile[$sourcePath]
    $relativePath = $sourcePath.Substring($normalizedRepoRoot.Length + 1).Replace('\', '/')
    $linePoints = @($entry.lines.Values)
    $branchPoints = @($entry.branches.Values)
    $lineCovered = @($linePoints | Where-Object { $_.covered }).Count
    $lineTotal = $linePoints.Count
    $branchCovered = @($branchPoints | Where-Object { $_.covered }).Count
    $branchTotal = $branchPoints.Count

    [pscustomobject]@{
      path = $sourcePath
      relativePath = $relativePath
      lineCovered = $lineCovered
      lineTotal = $lineTotal
      linePercent = if ($lineTotal -gt 0) { [math]::Round(($lineCovered * 100.0) / $lineTotal, 2) } else { 0.0 }
      branchCovered = $branchCovered
      branchTotal = $branchTotal
      branchPercent = if ($branchTotal -gt 0) { [math]::Round(($branchCovered * 100.0) / $branchTotal, 2) } else { 0.0 }
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

$coreEntries = @($repoEntries | Where-Object { $_.relativePath -like 'include/*' })
$exampleEntries = @($repoEntries | Where-Object { $_.relativePath -like 'examples/*' })
$productEntries = @($repoEntries | Where-Object { $_.relativePath -like 'include/*' -or $_.relativePath -like 'examples/*' })

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
[System.IO.File]::WriteAllText($coverageJsonPath, ($summary | ConvertTo-Json -Depth 6), [System.Text.UTF8Encoding]::new($false))

$markdown = @"
# Coverage Summary

Generated at: $($summary.generatedAtUtc)

Coverage comes from the instrumented host test build and reflects code reached through the host test executable.

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
}

if ($env:GITHUB_STEP_SUMMARY) {
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "### Coverage Summary`n"
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "| Scope | Line Coverage | Branch Coverage |`n| --- | ---: | ---: |"
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "| Core (`include/`) | $($summary.scopes.core.lines.percent)% | $($summary.scopes.core.branches.percent)% |"
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "| Product (`include/` + `examples/`) | $($summary.scopes.product.lines.percent)% | $($summary.scopes.product.branches.percent)% |"
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value ""
}

Write-Host "Coverage summary written to $coverageJsonPath"