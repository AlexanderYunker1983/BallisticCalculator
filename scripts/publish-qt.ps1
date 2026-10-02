# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$StagingDirectory)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'release-functions.ps1')
Publish-QtRelease $StagingDirectory (Join-Path (Split-Path $PSScriptRoot -Parent) 'dist')
