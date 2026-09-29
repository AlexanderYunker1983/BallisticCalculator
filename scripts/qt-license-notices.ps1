# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$QtDir,
      [Parameter(Mandatory=$true)][string]$Destination)
$ErrorActionPreference = 'Stop'
$qtSource = Join-Path (Split-Path $QtDir -Parent) 'Src'
if (!(Test-Path -LiteralPath $qtSource)) { throw "Qt source licenses not found: $qtSource" }
New-Item -ItemType Directory -Path $Destination -Force | Out-Null
foreach ($module in @('qtbase', 'qtsvg', 'qtimageformats')) {
    $source = Join-Path $qtSource $module
    if (!(Test-Path -LiteralPath $source)) { continue }
    $moduleDestination = Join-Path $Destination $module
    New-Item -ItemType Directory -Path $moduleDestination -Force | Out-Null
    Get-ChildItem -LiteralPath $source -File -Filter 'LICENSE*' | Copy-Item -Destination $moduleDestination -Force
    $thirdParty = Join-Path $source 'src\3rdparty'
    if (Test-Path -LiteralPath $thirdParty) {
        # Preserve source-relative paths to avoid overwriting identically named licenses.
        Get-ChildItem -LiteralPath $thirdParty -File -Recurse | Where-Object {
            $_.Name -match '^(LICENSE|LICENCE|COPYING|COPYRIGHT|NOTICE|AUTHORS|qt_attribution)' -or
            $_.Name -eq 'FTL.TXT'
        } | ForEach-Object {
            $relative = $_.FullName.Substring($source.Length + 1)
            $target = Join-Path $moduleDestination $relative
            New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
            Copy-Item -LiteralPath $_.FullName -Destination $target -Force
        }
    }
}
$notice = @'
The native BallisticCalculator application and core are GPL-3.0-or-later.
See ../LICENSE and ../NOTICE. The original MIT notice is preserved in MIT-legacy.txt.
This build dynamically links Qt 5.11.1 (Qt Core, GUI, Widgets and deployed plugins).
Qt license texts and bundled third-party notices accompany this directory.
Qt Charts is not used by this application.
Qt source for this version: https://download.qt.io/archive/qt/5.11/5.11.1/single/
The matching application source and build scripts are in ../BallisticCalculator-native-source.zip.
MSVC runtime DLLs are copied from the installed Microsoft Visual C++ 2017 redistributable.
'@
Set-Content -LiteralPath (Join-Path $Destination 'THIRD-PARTY.txt') -Value $notice -Encoding UTF8
