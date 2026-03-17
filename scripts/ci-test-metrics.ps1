param(
  [Parameter(Mandatory = $true)]
  [string]$Preset,

  [Parameter(Mandatory = $true)]
  [string]$TestPreset,

  [Parameter(Mandatory = $true)]
  [string]$BuildDir,

  [double]$WorkflowDurationSeconds = 0,

  [int]$WorkflowExitCode = 0,

  [string]$WorkflowLogPath = ''
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resolvedBuildDir = if ([System.IO.Path]::IsPathRooted($BuildDir)) {
  $BuildDir
} else {
  (Join-Path $repoRoot $BuildDir)
}

$resolvedWorkflowLogPath = if ([string]::IsNullOrWhiteSpace($WorkflowLogPath)) {
  Join-Path $resolvedBuildDir 'ci-workflow.log'
} elseif ([System.IO.Path]::IsPathRooted($WorkflowLogPath)) {
  $WorkflowLogPath
} else {
  Join-Path $repoRoot $WorkflowLogPath
}

$ctest = Get-Command ctest -ErrorAction SilentlyContinue
if (-not $ctest) {
  $ctest = Get-Command (Join-Path $repoRoot '.local\winlibs\mingw64\bin\ctest.exe') -ErrorAction SilentlyContinue
}

if (-not $ctest) {
  throw 'ctest was not found on PATH and the repo-local fallback was not present.'
}

$testsJson = & $ctest.Source --preset $TestPreset -N --show-only=json-v1 | Out-String
$testsData = $testsJson | ConvertFrom-Json
$tests = @($testsData.tests)

$totalTests = $tests.Count
$compileFailTests = @($tests | Where-Object { $_.name -like 'compile_fail_*' }).Count
$runtimeTests = $totalTests - $compileFailTests

$costData = Join-Path $resolvedBuildDir 'Testing\Temporary\CTestCostData.txt'
$lastTestLog = Join-Path $resolvedBuildDir 'Testing\Temporary\LastTest.log'
$passedTests = 0
$failedTests = 0
$ctestDurationSeconds = 0.0
$slowestTests = @()
$workflowStatus = if ($WorkflowExitCode -eq 0) { 'passed' } else { 'failed' }

if (Test-Path $resolvedWorkflowLogPath) {
  $workflowLogText = [System.IO.File]::ReadAllText($resolvedWorkflowLogPath)
  $testLinePattern = 'Test\s+#\d+:\s+(.+?)\s+\.+\s+(Passed|\*\*\*Failed)\s+([0-9.]+)\s+sec'
  $testLineMatches = [regex]::Matches($workflowLogText, $testLinePattern)

  if ($testLineMatches.Count -gt 0) {
    $testEntries = foreach ($match in $testLineMatches) {
      $duration = 0.0
      [double]::TryParse($match.Groups[3].Value,
                         [System.Globalization.NumberStyles]::Float,
                         [System.Globalization.CultureInfo]::InvariantCulture,
                         [ref]$duration) | Out-Null

      [pscustomobject]@{
        name = $match.Groups[1].Value.Trim()
        status = $match.Groups[2].Value
        durationSeconds = [math]::Round($duration, 4)
      }
    }

    $testEntries = @($testEntries)
    $passedTests = @($testEntries | Where-Object { $_.status -eq 'Passed' }).Count
    $failedTests = @($testEntries | Where-Object { $_.status -eq '***Failed' }).Count

    foreach ($entry in $testEntries) {
      $ctestDurationSeconds += $entry.durationSeconds
    }

    $slowestTests = @(
      $testEntries |
        Sort-Object durationSeconds -Descending |
        Select-Object -First 5 name, durationSeconds, status
    )
  }

  $summaryMatch = [regex]::Match($workflowLogText, '(\d+)% tests passed, (\d+) tests failed out of (\d+)')
  if ($summaryMatch.Success -and ($passedTests + $failedTests) -eq 0) {
    $failedTests = [int]$summaryMatch.Groups[2].Value
    $passedTests = [int]$summaryMatch.Groups[3].Value - $failedTests
  }

  if ($failedTests -gt 0) {
    $workflowStatus = 'failed'
  } elseif ($WorkflowExitCode -ne 0) {
    $workflowStatus = 'failed'
  } elseif ($totalTests -gt 0 -and ($passedTests + $failedTests) -eq 0) {
    $workflowStatus = 'incomplete'
  }
}

$passRatePercent = if ($totalTests -gt 0) {
  [math]::Round(($passedTests / $totalTests) * 100.0, 2)
} else {
  0.0
}

$metrics = [ordered]@{
  preset = $Preset
  testPreset = $TestPreset
  buildDir = $resolvedBuildDir
  totalTests = $totalTests
  compileFailTests = $compileFailTests
  runtimeTests = $runtimeTests
  passedTests = $passedTests
  failedTests = $failedTests
  passRatePercent = $passRatePercent
  ctestDurationSeconds = [math]::Round($ctestDurationSeconds, 2)
  workflowDurationSeconds = [math]::Round($WorkflowDurationSeconds, 2)
  workflowExitCode = $WorkflowExitCode
  status = $workflowStatus
  workflowLog = if (Test-Path $resolvedWorkflowLogPath) { $resolvedWorkflowLogPath } else { $null }
  lastTestLog = if (Test-Path $lastTestLog) { $lastTestLog } else { $null }
  costData = if (Test-Path $costData) { $costData } else { $null }
  slowestTests = $slowestTests
}

$metricsJsonPath = Join-Path $resolvedBuildDir 'ci-metrics.json'
$metrics | ConvertTo-Json -Depth 6 | Set-Content -Path $metricsJsonPath -Encoding utf8

if ($env:GITHUB_STEP_SUMMARY) {
  $summary = @"
### Test Metrics: $Preset

| Metric | Value |
| --- | ---: |
| Status | $workflowStatus |
| Total tests | $totalTests |
| Compile-fail tests | $compileFailTests |
| Runtime tests | $runtimeTests |
| Passed tests | $passedTests |
| Failed tests | $failedTests |
| Pass rate | $passRatePercent% |
| CTest duration (s) | $([math]::Round($ctestDurationSeconds, 2)) |
| Workflow duration (s) | $([math]::Round($WorkflowDurationSeconds, 2)) |

"@
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value $summary

  if ($slowestTests.Count -gt 0) {
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "Slowest tests from this run:`n"
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "| Test | Status | Duration (s) |`n| --- | --- | ---: |"
    foreach ($entry in $slowestTests) {
      Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "| $($entry.name) | $($entry.status) | $($entry.durationSeconds) |"
    }
    Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value ""
  }
}

Write-Host "Metrics written to $metricsJsonPath"
