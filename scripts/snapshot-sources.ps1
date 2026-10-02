# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'source-files.ps1')
$repository = [IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
ConvertTo-Json -InputObject @(Get-QtSourceManifest $repository) -Depth 4 | Set-Content -LiteralPath $Output -Encoding UTF8
