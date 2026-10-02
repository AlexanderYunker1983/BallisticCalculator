# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$Directory, [switch]$WithArchive)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'release-functions.ps1')
$manifest = Test-QtRelease ([IO.Path]::GetFullPath($Directory)) -WithArchive:$WithArchive
Write-Output "Verified $($manifest.applicationVersion): $($manifest.files.Count) files, $($manifest.sources.Count) source files"
