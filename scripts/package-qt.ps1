# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$DistributionDirectory)
$ErrorActionPreference = 'Stop'
$repository = Split-Path $PSScriptRoot -Parent
$distribution = (Resolve-Path -LiteralPath $DistributionDirectory).Path
if (!(Test-Path -LiteralPath (Join-Path $distribution 'BallisticCalculator.exe'))) {
    throw "Native application not found in $distribution"
}

# Include the actual sources used for this build, including uncommitted files.
# A source allowlist excludes build outputs and Qt Creator settings.
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sourceArchive = Join-Path $distribution 'BallisticCalculator-native-source.zip'
$stream = [System.IO.File]::Open($sourceArchive, [System.IO.FileMode]::Create)
$archive = [System.IO.Compression.ZipArchive]::new($stream, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    $files = @('BallisticCalculator.pro', '.gitignore', 'build-release.cmd', 'rebuild-release.cmd',
               'LICENSE', 'NOTICE', 'README.md', 'LICENSES\MIT-legacy.txt') | ForEach-Object {
        Get-Item -LiteralPath (Join-Path $repository $_)
    }
    foreach ($directory in @('src', 'tests', 'docs')) {
        $files += Get-ChildItem -LiteralPath (Join-Path $repository $directory) -Recurse -File | Where-Object {
            $_.Extension -in @('.cpp', '.h', '.inc', '.pro', '.pri', '.md', '.conf', '.qrc', '.svg') -and
            $_.Name -notmatch '^(moc_|qrc_).+\.cpp$|^ui_.+\.h$'
        }
    }
    $files += Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in @('.cmd', '.ps1') }
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($repository.Length + 1).Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $relative) | Out-Null
    }
} finally { $archive.Dispose() }

$binaryArchive = Join-Path (Split-Path $distribution -Parent) 'BallisticCalculator-Qt5.11.1-win64.zip'
Compress-Archive -LiteralPath $distribution -DestinationPath $binaryArchive -Force
Write-Output "Source: $sourceArchive"
Write-Output "Package: $binaryArchive"
