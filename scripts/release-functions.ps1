# SPDX-License-Identifier: GPL-3.0-or-later
. (Join-Path $PSScriptRoot 'source-files.ps1')
$script:QtReleaseRepository = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Assert-QtArtifactPath([string]$Path, [switch]$Tree) {
    $absolute = [IO.Path]::GetFullPath($Path)
    $allowed = $false
    foreach ($directory in @('dist', 'artifacts')) {
        $root = Join-Path $script:QtReleaseRepository $directory
        if ($absolute.Equals($root, [StringComparison]::OrdinalIgnoreCase) -or
            $absolute.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { $allowed = $true }
    }
    if (!$allowed) { throw "Artifact path is outside dist/artifacts: $absolute" }
    $ancestor = $absolute
    while ($ancestor -and $ancestor -ne $script:QtReleaseRepository) {
        if (Test-Path -LiteralPath $ancestor) {
            if ((Get-Item -LiteralPath $ancestor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Reparse point is not permitted: $ancestor"
            }
        }
        $ancestor = Split-Path $ancestor -Parent
    }
    if ($Tree -and (Test-Path -LiteralPath $absolute -PathType Container)) {
        $pending = [Collections.Generic.Stack[string]]::new(); $pending.Push($absolute)
        while ($pending.Count) {
            foreach ($item in Get-ChildItem -LiteralPath $pending.Pop() -Force) {
                if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point is not permitted: $($item.FullName)" }
                if ($item.PSIsContainer) { $pending.Push($item.FullName) }
            }
        }
    }
}
function Get-QtZipHashes([string]$Path) {
    $result = @{}
    $zip = [IO.Compression.ZipFile]::OpenRead($Path)
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
        foreach ($entry in $zip.Entries) {
            $name = $entry.FullName.Replace('\', '/')
            if ($name.EndsWith('/')) { continue }
            if ($result.ContainsKey($name)) { throw "Duplicate ZIP entry: $name" }
            $stream = $entry.Open()
            try { $result[$name] = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') }
            finally { $stream.Dispose() }
        }
    } finally { $sha.Dispose(); $zip.Dispose() }
    return $result
}
function Assert-QtHashes($Expected, $Actual) {
    if ($Expected.Count -ne $Actual.Count) { throw 'Manifest file count differs from the archive/directory' }
    foreach ($key in $Expected.Keys) {
        if (!$Actual.ContainsKey($key) -or $Actual[$key] -ne $Expected[$key]) { throw "Hash mismatch or missing entry: $key" }
    }
}
function Get-QtManifestHashes($Entries, [string]$Prefix = '') {
    $hashes = @{}
    foreach ($entry in $Entries) {
        if (!$entry.path -or $entry.path -match '(^/|\\|:|(^|/)\.\.?(/|$))' -or $entry.sha256 -notmatch '^[0-9A-Fa-f]{64}$') {
            throw 'Invalid manifest entry'
        }
        $key = $Prefix + $entry.path
        if ($hashes.ContainsKey($key)) { throw "Duplicate manifest entry: $key" }
        $hashes[$key] = $entry.sha256
    }
    return $hashes
}
function Test-QtRelease([string]$Directory, [switch]$WithArchive) {
    $Directory = [IO.Path]::GetFullPath($Directory).TrimEnd([char[]]'\/')
    Assert-QtArtifactPath $Directory -Tree
    $manifestPath = Join-Path $Directory 'build-manifest.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    if ($manifest.schemaVersion -ne 1 -or $manifest.archiveName -notmatch '^BallisticCalculator-Qt\d+\.\d+\.\d+-win64\.zip$') { throw 'Invalid release manifest' }
    $expected = Get-QtManifestHashes $manifest.files
    foreach ($required in @('BallisticCalculator.exe', 'qt.conf', 'Qt5Core.dll', 'Qt5Gui.dll', 'Qt5Widgets.dll',
                            'vcruntime140.dll', 'platforms/qwindows.dll', 'LICENSE', 'licenses/THIRD-PARTY.txt', 'BallisticCalculator-native-source.zip')) {
        if (!$expected.ContainsKey($required)) { throw "Required release file is absent: $required" }
    }
    $actual = @{}
    foreach ($file in Get-ChildItem -LiteralPath $Directory -Recurse -File) {
        $relative = $file.FullName.Substring($Directory.Length + 1).Replace('\', '/')
        if ($relative -ne 'build-manifest.json') { $actual[$relative] = (Get-QtFileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
    }
    Assert-QtHashes $expected $actual
    $sources = Get-QtManifestHashes $manifest.sources
    if (!$sources.ContainsKey('BallisticCalculator.pro') -or !$sources.ContainsKey('scripts/reference-solver.py')) { throw 'Incomplete source manifest' }
    Assert-QtHashes $sources (Get-QtZipHashes (Join-Path $Directory 'BallisticCalculator-native-source.zip'))
    if ($WithArchive) {
        $archive = Join-Path (Split-Path $Directory -Parent) $manifest.archiveName
        Assert-QtArtifactPath $archive
        $packaged = Get-QtManifestHashes $manifest.files 'Release/'
        $packaged['Release/build-manifest.json'] = (Get-QtFileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
        Assert-QtHashes $packaged (Get-QtZipHashes $archive)
    }
    return $manifest
}
function Move-QtArtifact([string]$Source, [string]$Destination) {
    Assert-QtArtifactPath $Source -Tree
    Assert-QtArtifactPath $Destination
    Move-Item -LiteralPath $Source -Destination $Destination -ErrorAction Stop
}
function Publish-QtRelease([string]$StagingDirectory, [string]$DestinationRoot, [scriptblock]$Checkpoint = {}) {
    $stage = [IO.Path]::GetFullPath($StagingDirectory).TrimEnd([char[]]'\/')
    $destination = [IO.Path]::GetFullPath($DestinationRoot).TrimEnd([char[]]'\/')
    Assert-QtArtifactPath $stage -Tree; Assert-QtArtifactPath $destination
    if ($stage.Equals($destination, [StringComparison]::OrdinalIgnoreCase)) { throw 'Staging and destination must differ' }
    $candidate = Join-Path $stage 'Release'; $target = Join-Path $destination 'Release'
    if ($stage.StartsWith($target + '\', [StringComparison]::OrdinalIgnoreCase) -or $stage -eq $target) { throw 'Staging must be outside the installed release' }
    $manifest = Test-QtRelease $candidate -WithArchive
    $archive = Join-Path $stage $manifest.archiveName; $targetArchive = Join-Path $destination $manifest.archiveName
    Assert-QtArtifactPath $target -Tree; Assert-QtArtifactPath $targetArchive
    $backup = Join-Path $destination ('previous-' + [Guid]::NewGuid().ToString('N'))
    Assert-QtArtifactPath $backup
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $journal = Join-Path $backup 'transaction.json'
    $state = [ordered]@{ staging = $stage; destination = $destination; archiveName = $manifest.archiveName; phase = 'Prepared' }
    $save = { param($phase) $state.phase = $phase; ConvertTo-Json -InputObject $state | Set-Content -LiteralPath $journal -Encoding UTF8 }
    & $save 'Prepared'
    $oldRelease = $false; $oldArchive = $false; $newRelease = $false; $newArchive = $false
    try {
        if (Test-Path -LiteralPath $target) { Move-QtArtifact $target (Join-Path $backup 'Release'); $oldRelease = $true }
        if (Test-Path -LiteralPath $targetArchive) { Move-QtArtifact $targetArchive (Join-Path $backup $manifest.archiveName); $oldArchive = $true }
        & $save 'BackedUp'; & $Checkpoint 'BackedUp'
        Move-QtArtifact $candidate $target; $newRelease = $true
        & $save 'ReleaseInstalled'; & $Checkpoint 'ReleaseInstalled'
        Move-QtArtifact $archive $targetArchive; $newArchive = $true
        & $Checkpoint 'ArchiveInstalled'; & $save 'Completed'
    } catch {
        $failure = $_
        if ($newArchive) { Move-QtArtifact $targetArchive $archive }
        if ($newRelease) { Move-QtArtifact $target $candidate }
        if ($oldArchive) { Move-QtArtifact (Join-Path $backup $manifest.archiveName) $targetArchive }
        if ($oldRelease) { Move-QtArtifact (Join-Path $backup 'Release') $target }
        & $save 'RolledBack'
        throw $failure
    }
    return [PSCustomObject]@{ Release = $target; Archive = $targetArchive; Backup = $backup }
}
