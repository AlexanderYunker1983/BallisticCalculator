# SPDX-License-Identifier: GPL-3.0-or-later
$ErrorActionPreference = 'Stop'
$repository = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
. (Join-Path $repository 'scripts\release-functions.ps1')
$testRoot = Join-Path $repository ('artifacts\release-tests\' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$fixture = Join-Path $testRoot 'fixture'
$release = Join-Path $fixture 'Release'
foreach ($file in @('BallisticCalculator.exe', 'qt.conf', 'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll', 'vcruntime140.dll',
                    'platforms/qwindows.dll', 'LICENSE', 'licenses/THIRD-PARTY.txt')) {
    $path = Join-Path $release $file
    New-Item -ItemType Directory -Path (Split-Path $path -Parent) -Force | Out-Null
    Set-Content -LiteralPath $path -Value "Fixture $file" -Encoding ASCII
}
& (Join-Path $repository 'scripts\package-qt.ps1') -DistributionDirectory $release -GitExecutable '' | Out-Null
$matchingSnapshot = Join-Path $testRoot 'matching-source.json'
ConvertTo-Json -InputObject @(Get-QtSourceManifest $repository) -Depth 4 | Set-Content -LiteralPath $matchingSnapshot -Encoding UTF8
& (Join-Path $repository 'scripts\package-qt.ps1') -DistributionDirectory $release -GitExecutable '' -ExpectedSourceManifest $matchingSnapshot | Out-Null
$manifest = Test-QtRelease $release -WithArchive
$archiveName = $manifest.archiveName
$passed = 1
foreach ($scenario in @('success', 'corrupt-file', 'corrupt-zip', 'extra-file', 'corrupt-source', 'BackedUp', 'ReleaseInstalled', 'ArchiveInstalled')) {
    $root = Join-Path $testRoot $scenario; $stage = Join-Path $root 'stage'; $installed = Join-Path $root 'installed'
    New-Item -ItemType Directory -Path $stage, (Join-Path $installed 'Release') -Force | Out-Null
    Copy-Item -LiteralPath $release -Destination (Join-Path $stage 'Release') -Recurse
    Copy-Item -LiteralPath (Join-Path $fixture $archiveName) -Destination (Join-Path $stage $archiveName)
    $sentinel = Join-Path $installed 'Release\previous.txt'
    Set-Content -LiteralPath $sentinel -Value 'previous release' -Encoding ASCII
    Set-Content -LiteralPath (Join-Path $installed $archiveName) -Value 'previous archive' -Encoding ASCII
    $oldHash = (Get-QtFileHash -LiteralPath $sentinel).Hash
    $oldZipHash = (Get-QtFileHash -LiteralPath (Join-Path $installed $archiveName)).Hash
    if ($scenario -eq 'corrupt-file') { Add-Content -LiteralPath (Join-Path $stage 'Release\BallisticCalculator.exe') -Value 'corrupt' }
    if ($scenario -eq 'corrupt-zip') { Set-Content -LiteralPath (Join-Path $stage $archiveName) -Value 'corrupt' }
    if ($scenario -eq 'extra-file') { Set-Content -LiteralPath (Join-Path $stage 'Release\unexpected.txt') -Value 'extra' }
    if ($scenario -eq 'corrupt-source') {
        $sourceZip = Join-Path $stage 'Release\BallisticCalculator-native-source.zip'
        $zip = [IO.Compression.ZipFile]::Open($sourceZip, [IO.Compression.ZipArchiveMode]::Update)
        try { $zip.CreateEntry('unexpected.cpp') | Out-Null } finally { $zip.Dispose() }
        $manifestFile = Join-Path $stage 'Release\build-manifest.json'
        $changed = Get-Content -LiteralPath $manifestFile -Raw | ConvertFrom-Json
        ($changed.files | Where-Object { $_.path -eq 'BallisticCalculator-native-source.zip' }).sha256 = (Get-QtFileHash -LiteralPath $sourceZip).Hash
        ConvertTo-Json -InputObject $changed -Depth 6 | Set-Content -LiteralPath $manifestFile -Encoding UTF8
    }
    $failurePoint = $scenario
    $checkpoint = { param($phase) if ($phase -eq $failurePoint) { throw "Injected failure at $phase" } }.GetNewClosure()
    $failed = $false
    try { $published = Publish-QtRelease $stage $installed $checkpoint }
    catch { $failed = $true }
    if ($scenario -eq 'success') {
        if ($failed) { throw 'Successful publish failed' }
        Test-QtRelease (Join-Path $installed 'Release') -WithArchive | Out-Null
        if ((Get-QtFileHash -LiteralPath (Join-Path $published.Backup 'Release\previous.txt')).Hash -ne $oldHash) { throw 'Backup was lost' }
    } else {
        if (!$failed) { throw "Expected failure was not detected: $scenario" }
        if ((Get-QtFileHash -LiteralPath $sentinel).Hash -ne $oldHash -or
            (Get-QtFileHash -LiteralPath (Join-Path $installed $archiveName)).Hash -ne $oldZipHash) { throw "Previous release changed: $scenario" }
        if ($scenario -in @('BackedUp', 'ReleaseInstalled', 'ArchiveInstalled')) {
            Test-QtRelease (Join-Path $stage 'Release') -WithArchive | Out-Null
            $journal = Get-ChildItem -LiteralPath $installed -Filter transaction.json -Recurse | Select-Object -First 1
            if ((Get-Content -LiteralPath $journal.FullName -Raw | ConvertFrom-Json).phase -ne 'RolledBack') { throw 'Rollback journal is missing' }
        }
    }
    ++$passed; Write-Output "PASS $scenario"
}
$snapshot = Join-Path $testRoot 'wrong-source.json'
'[{"path":"wrong.cpp","sha256":"bad"}]' | Set-Content -LiteralPath $snapshot -Encoding UTF8
$failed = $false
try { & (Join-Path $repository 'scripts\package-qt.ps1') -DistributionDirectory $release -GitExecutable '' -ExpectedSourceManifest $snapshot | Out-Null }
catch { $failed = $true }
if (!$failed) { throw 'Source changes were not detected' }
++ $passed
$failed = $false
try { Assert-QtArtifactPath (Join-Path $repository 'src') }
catch { $failed = $true }
if (!$failed) { throw 'Non-artifact path was accepted' }
++ $passed
$link = Join-Path $testRoot 'linked-release'
New-Item -ItemType Junction -Path $link -Value $release | Out-Null
try {
    $failed = $false
    try { Assert-QtArtifactPath $link -Tree } catch { $failed = $true }
    if (!$failed) { throw 'Reparse point was accepted' }
    ++ $passed
} finally { [IO.Directory]::Delete($link) }
Write-Output "Release tests: $passed passed; fixtures: $testRoot"
