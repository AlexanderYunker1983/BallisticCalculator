# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$DistributionDirectory,
      [string]$QtVersion = '5.11.1', [string]$QtDirectory, [string]$CompilerVersion = $env:VCToolsVersion,
      [string]$GitExecutable = 'git', [string]$ExpectedSourceManifest)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'source-files.ps1')
. (Join-Path $PSScriptRoot 'release-functions.ps1')
$repository = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$distribution = (Resolve-Path -LiteralPath $DistributionDirectory).Path.TrimEnd([char[]]'\/')
Assert-QtArtifactPath $distribution -Tree
if ((Split-Path $distribution -Leaf) -ne 'Release' -or !(Test-Path -LiteralPath (Join-Path $distribution 'BallisticCalculator.exe'))) {
    throw 'Expected a Release directory containing BallisticCalculator.exe'
}
if ($QtDirectory) {
    $QtVersion = & (Join-Path $QtDirectory 'bin\qmake.exe') -query QT_VERSION
    if ($LASTEXITCODE -ne 0) { throw 'Cannot determine Qt version' }
    $QtVersion = $QtVersion.Trim()
}
if ($QtVersion -notmatch '^\d+\.\d+\.\d+$') { throw 'Invalid Qt version' }
$sources = @(Get-QtSourceManifest $repository)
if ($ExpectedSourceManifest) {
    $expected = Get-Content -LiteralPath $ExpectedSourceManifest -Raw | ConvertFrom-Json
    $a = @($expected | ForEach-Object { $_.path + ':' + $_.sha256 })
    $b = @($sources | ForEach-Object { $_.path + ':' + $_.sha256 })
    if (@(Compare-Object $a $b).Count) { throw 'Sources changed during the build; rebuild before packaging.' }
}
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sourceArchive = Join-Path $distribution 'BallisticCalculator-native-source.zip'
$stream = [IO.File]::Open($sourceArchive, [IO.FileMode]::Create)
$archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($entry in $sources) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, (Join-Path $repository $entry.path), $entry.path) | Out-Null
    }
} finally { $archive.Dispose() }
$revision = $null; $dirty = $null
if ($GitExecutable -and (Get-Command $GitExecutable -ErrorAction SilentlyContinue) -and (Test-Path -LiteralPath (Join-Path $repository '.git'))) {
    $revision = & $GitExecutable -C $repository rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read Git revision' }
    $status = & $GitExecutable -C $repository status --porcelain
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read Git status' }
    $dirty = [bool]$status
}
$header = Get-Content -LiteralPath (Join-Path $repository 'src\app\version.h') -Raw
if ($header -notmatch '#define BALLISTIC_VERSION "([^"]+)"') { throw 'Application version is missing' }
$version = $Matches[1]
$archiveName = "BallisticCalculator-Qt$QtVersion-win64.zip"
$files = @(Get-ChildItem -LiteralPath $distribution -Recurse -File | Where-Object { $_.Name -ne 'build-manifest.json' } | Sort-Object FullName | ForEach-Object {
    [ordered]@{ path = $_.FullName.Substring($distribution.Length + 1).Replace('\', '/'); sha256 = (Get-QtFileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash; size = $_.Length }
})
$manifest = [ordered]@{ schemaVersion = 1; applicationVersion = $version; qtVersion = $QtVersion; compilerVersion = $CompilerVersion;
    createdUtc = [DateTime]::UtcNow.ToString('o'); sourceRevision = $revision; sourceDirty = $dirty; archiveName = $archiveName; sources = $sources; files = $files }
ConvertTo-Json -InputObject $manifest -Depth 6 | Set-Content -LiteralPath (Join-Path $distribution 'build-manifest.json') -Encoding UTF8
$binaryArchive = Join-Path (Split-Path $distribution -Parent) $archiveName
Compress-Archive -LiteralPath $distribution -DestinationPath $binaryArchive -Force
Write-Output "Source: $sourceArchive"
Write-Output "Package: $binaryArchive"
