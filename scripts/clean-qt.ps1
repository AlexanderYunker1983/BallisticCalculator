# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][ValidateSet('Build', 'Release')][string]$Target)
$ErrorActionPreference = 'Stop'
$repository = [System.IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$relative = if ($Target -eq 'Build') { 'artifacts\qt-release\native' } else { 'dist\Release' }
$destination = [System.IO.Path]::GetFullPath((Join-Path $repository $relative))
if (!$destination.StartsWith($repository + '\', [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to clean a directory outside the repository: $destination"
}

# Check ancestors and descendants so cleanup cannot traverse directory links.
$ancestor = $destination
while ($ancestor -ne $repository) {
    if (Test-Path -LiteralPath $ancestor) {
        $item = Get-Item -LiteralPath $ancestor -Force
        if ($item.Attributes -band [System.IO.FileAttributes]::ReparsePoint) {
            throw "Refusing to clean through a reparse point: $ancestor"
        }
    }
    $ancestor = Split-Path $ancestor -Parent
}
if (Test-Path -LiteralPath $destination) {
    $links = Get-ChildItem -LiteralPath $destination -Recurse -Force | Where-Object {
        $_.Attributes -band [System.IO.FileAttributes]::ReparsePoint
    }
    if ($links) { throw "Refusing to clean a directory containing reparse points: $destination" }
    Write-Output "Cleaning generated directory: $destination"
    Remove-Item -LiteralPath $destination -Recurse -Force
}
