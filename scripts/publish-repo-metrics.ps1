param(
  [Parameter(Mandatory = $true)]
  [string]$ArtifactsRoot,

  [string]$OutputDir = 'assets/metrics'
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$resolvedArtifactsRoot = if ([System.IO.Path]::IsPathRooted($ArtifactsRoot)) {
  $ArtifactsRoot
} else {
  Join-Path $repoRoot $ArtifactsRoot
}
$resolvedOutputDir = if ([System.IO.Path]::IsPathRooted($OutputDir)) {
  $OutputDir
} else {
  Join-Path $repoRoot $OutputDir
}

New-Item -ItemType Directory -Force -Path $resolvedOutputDir | Out-Null

function Escape-Xml {
  param([string]$Text)
  return [System.Security.SecurityElement]::Escape($Text)
}

function Write-BadgeSvg {
  param(
    [string]$Label,
    [string]$Message,
    [string]$Color,
    [string]$Path
  )

  $labelWidth = [math]::Max(72, [int][math]::Ceiling(($Label.Length * 6.7) + 18))
  $messageWidth = [math]::Max(52, [int][math]::Ceiling(($Message.Length * 6.7) + 18))
  $totalWidth = $labelWidth + $messageWidth
  $labelTextX = [math]::Round($labelWidth / 2.0, 1)
  $messageTextX = [math]::Round($labelWidth + ($messageWidth / 2.0), 1)
  $title = Escape-Xml "${Label}: $Message"
  $safeLabel = Escape-Xml $Label
  $safeMessage = Escape-Xml $Message

  $svg = @"
<svg xmlns="http://www.w3.org/2000/svg" width="$totalWidth" height="20" role="img" aria-label="$title">
  <title>$title</title>
  <linearGradient id="s" x2="0" y2="100%">
    <stop offset="0" stop-color="#fff" stop-opacity=".7"/>
    <stop offset=".1" stop-opacity=".1"/>
    <stop offset=".9" stop-opacity=".3"/>
    <stop offset="1" stop-opacity=".5"/>
  </linearGradient>
  <clipPath id="r">
    <rect width="$totalWidth" height="20" rx="3" fill="#fff"/>
  </clipPath>
  <g clip-path="url(#r)">
    <rect width="$labelWidth" height="20" fill="#555"/>
    <rect x="$labelWidth" width="$messageWidth" height="20" fill="$Color"/>
    <rect width="$totalWidth" height="20" fill="url(#s)"/>
  </g>
  <g fill="#fff" text-anchor="middle" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11">
    <text x="$labelTextX" y="15" fill="#010101" fill-opacity=".3">$safeLabel</text>
    <text x="$labelTextX" y="14">$safeLabel</text>
    <text x="$messageTextX" y="15" fill="#010101" fill-opacity=".3">$safeMessage</text>
    <text x="$messageTextX" y="14">$safeMessage</text>
  </g>
</svg>
"@

  [System.IO.File]::WriteAllText($Path, $svg, [System.Text.UTF8Encoding]::new($false))
}

function Get-PercentColor {
  param([double]$Percent)

  if ($Percent -ge 85) { return '#2ea043' }
  if ($Percent -ge 70) { return '#3fb950' }
  if ($Percent -ge 50) { return '#d29922' }
  if ($Percent -ge 30) { return '#fb8500' }
  return '#cf222e'
}

function Get-StatusColor {
  param([bool]$Success)
  if ($Success) { return '#2ea043' }
  return '#cf222e'
}

$metricFiles = @(
  Get-ChildItem -Path $resolvedArtifactsRoot -Recurse -Filter ci-metrics.json -ErrorAction SilentlyContinue |
    Select-Object -ExpandProperty FullName
)
$metricsByPreset = @{}
foreach ($metricFile in $metricFiles) {
  $metric = Get-Content -Path $metricFile -Raw | ConvertFrom-Json
  $metricsByPreset[$metric.preset] = $metric
}

$coverageCandidates = @(
  Get-ChildItem -Path $resolvedArtifactsRoot -Recurse -Filter coverage-summary.json -ErrorAction SilentlyContinue |
    Sort-Object @{ Expression = { if ($_.FullName -like '*host-coverage*') { 1 } else { 0 } }; Descending = $true }, @{ Expression = { $_.LastWriteTimeUtc }; Descending = $true }
)
$coverageFile = $coverageCandidates | Select-Object -First 1
$coverage = if ($coverageFile) { Get-Content -Path $coverageFile.FullName -Raw | ConvertFrom-Json } else { $null }

$hostMetrics = $metricsByPreset['host-test']
$qemuM3 = $metricsByPreset['qemu-m3-test']
$qemuR5 = $metricsByPreset['qemu-r5-test']
$coreCoverage = if ($coverage) { $coverage.scopes.core } else { $null }

$hostBadge = if ($hostMetrics) {
  [pscustomobject]@{
    label = 'host tests'
    message = "$($hostMetrics.passedTests)/$($hostMetrics.totalTests) passing"
    color = Get-StatusColor (($hostMetrics.failedTests -eq 0) -and ($hostMetrics.status -eq 'passed'))
  }
} else {
  [pscustomobject]@{ label = 'host tests'; message = 'pending'; color = '#6e7781' }
}

$compileFailBadge = if ($hostMetrics) {
  [pscustomobject]@{
    label = 'compile-fail'
    message = "$($hostMetrics.compileFailTests) cases"
    color = Get-StatusColor (($hostMetrics.failedTests -eq 0) -and ($hostMetrics.status -eq 'passed'))
  }
} else {
  [pscustomobject]@{ label = 'compile-fail'; message = 'pending'; color = '#6e7781' }
}

$reportedTargets = @(@($qemuM3, $qemuR5) | Where-Object { $null -ne $_ })
$passedTargetSuites = @($reportedTargets | Where-Object { $_.failedTests -eq 0 -and $_.status -eq 'passed' }).Count
$targetBadge = if ($reportedTargets.Count -gt 0) {
  [pscustomobject]@{
    label = 'target suites'
    message = "$passedTargetSuites/$($reportedTargets.Count) passing"
    color = Get-StatusColor ($passedTargetSuites -eq $reportedTargets.Count)
  }
} else {
  [pscustomobject]@{ label = 'target suites'; message = 'pending'; color = '#6e7781' }
}

$coreLineBadge = if ($coreCoverage) {
  [pscustomobject]@{
    label = 'core line coverage'
    message = "$($coreCoverage.lines.percent)%"
    color = Get-PercentColor ([double]$coreCoverage.lines.percent)
  }
} else {
  [pscustomobject]@{ label = 'core line coverage'; message = 'pending'; color = '#6e7781' }
}

$coreBranchBadge = if ($coreCoverage) {
  [pscustomobject]@{
    label = 'core branch coverage'
    message = "$($coreCoverage.branches.percent)%"
    color = Get-PercentColor ([double]$coreCoverage.branches.percent)
  }
} else {
  [pscustomobject]@{ label = 'core branch coverage'; message = 'pending'; color = '#6e7781' }
}

$badges = [ordered]@{
  'host-tests.svg' = $hostBadge
  'compile-fail.svg' = $compileFailBadge
  'target-suites.svg' = $targetBadge
  'core-line-coverage.svg' = $coreLineBadge
  'core-branch-coverage.svg' = $coreBranchBadge
}

foreach ($badgeName in $badges.Keys) {
  $badgePath = Join-Path $resolvedOutputDir $badgeName
  $badge = $badges[$badgeName]
  Write-BadgeSvg -Label $badge.label -Message $badge.message -Color $badge.color -Path $badgePath
}

$summary = [ordered]@{
  generatedAtUtc = (Get-Date).ToUniversalTime().ToString('o')
  tests = [ordered]@{
    host = $hostMetrics
    qemuM3 = $qemuM3
    qemuR5 = $qemuR5
  }
  coverage = if ($coverage) { $coverage.scopes } else { $null }
}

$summaryJsonPath = Join-Path $resolvedOutputDir 'summary.json'
$summaryMarkdownPath = Join-Path $resolvedOutputDir 'summary.md'
[System.IO.File]::WriteAllText($summaryJsonPath, ($summary | ConvertTo-Json -Depth 7), [System.Text.UTF8Encoding]::new($false))

$coverageSection = if ($coverage) {
@"
## Coverage

Coverage is generated from the host-coverage preset and currently reflects code reached through the host test executable.

| Scope | Files | Line Coverage | Branch Coverage |
| --- | ---: | ---: | ---: |
| Core (`include/`) | $($coverage.scopes.core.fileCount) | $($coverage.scopes.core.lines.percent)% ($($coverage.scopes.core.lines.covered)/$($coverage.scopes.core.lines.total)) | $($coverage.scopes.core.branches.percent)% ($($coverage.scopes.core.branches.covered)/$($coverage.scopes.core.branches.total)) |
| Product (`include/` + `examples/`) | $($coverage.scopes.product.fileCount) | $($coverage.scopes.product.lines.percent)% ($($coverage.scopes.product.lines.covered)/$($coverage.scopes.product.lines.total)) | $($coverage.scopes.product.branches.percent)% ($($coverage.scopes.product.branches.covered)/$($coverage.scopes.product.branches.total)) |
"@
} else {
@"
## Coverage

Coverage has not been published yet.
"@
}

$summaryMarkdown = @"
# Latest CI Signals

Generated at: $($summary.generatedAtUtc)

## Test Matrix

| Preset | Status | Passed | Failed | Total | Compile-Fail | Runtime | Workflow Duration (s) |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Host | $(if ($hostMetrics) { $hostMetrics.status } else { 'pending' }) | $(if ($hostMetrics) { $hostMetrics.passedTests } else { '-' }) | $(if ($hostMetrics) { $hostMetrics.failedTests } else { '-' }) | $(if ($hostMetrics) { $hostMetrics.totalTests } else { '-' }) | $(if ($hostMetrics) { $hostMetrics.compileFailTests } else { '-' }) | $(if ($hostMetrics) { $hostMetrics.runtimeTests } else { '-' }) | $(if ($hostMetrics) { $hostMetrics.workflowDurationSeconds } else { '-' }) |
| QEMU Cortex-M3 | $(if ($qemuM3) { $qemuM3.status } else { 'pending' }) | $(if ($qemuM3) { $qemuM3.passedTests } else { '-' }) | $(if ($qemuM3) { $qemuM3.failedTests } else { '-' }) | $(if ($qemuM3) { $qemuM3.totalTests } else { '-' }) | $(if ($qemuM3) { $qemuM3.compileFailTests } else { '-' }) | $(if ($qemuM3) { $qemuM3.runtimeTests } else { '-' }) | $(if ($qemuM3) { $qemuM3.workflowDurationSeconds } else { '-' }) |
| QEMU Cortex-R5 | $(if ($qemuR5) { $qemuR5.status } else { 'pending' }) | $(if ($qemuR5) { $qemuR5.passedTests } else { '-' }) | $(if ($qemuR5) { $qemuR5.failedTests } else { '-' }) | $(if ($qemuR5) { $qemuR5.totalTests } else { '-' }) | $(if ($qemuR5) { $qemuR5.compileFailTests } else { '-' }) | $(if ($qemuR5) { $qemuR5.runtimeTests } else { '-' }) | $(if ($qemuR5) { $qemuR5.workflowDurationSeconds } else { '-' }) |

$coverageSection
"@
[System.IO.File]::WriteAllText($summaryMarkdownPath, $summaryMarkdown, [System.Text.UTF8Encoding]::new($false))

if ($env:GITHUB_STEP_SUMMARY) {
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "### Published Repo Metrics`n"
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value "Badges and summary written to $resolvedOutputDir."
  Add-Content -Path $env:GITHUB_STEP_SUMMARY -Value ""
}

Write-Host "Repo metrics written to $resolvedOutputDir"