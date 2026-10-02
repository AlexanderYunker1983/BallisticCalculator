# SPDX-License-Identifier: GPL-3.0-or-later
function Get-QtFileHash {
    param([Parameter(Mandatory=$true)][string]$LiteralPath, [ValidateSet('SHA256')][string]$Algorithm = 'SHA256')
    $stream = [IO.File]::OpenRead($LiteralPath)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return [PSCustomObject]@{ Hash = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-', '') } }
    finally { $sha.Dispose(); $stream.Dispose() }
}
function Get-QtSourceFiles([string]$Repository) {
    $files = @('BallisticCalculator.pro', '.gitignore', 'build-release.cmd', 'rebuild-release.cmd',
               'LICENSE', 'NOTICE', 'README.md', 'LICENSES\MIT-legacy.txt') | ForEach-Object {
        Get-Item -LiteralPath (Join-Path $Repository $_)
    }
    foreach ($directory in @('src', 'tests', 'docs', 'scripts')) {
        $files += Get-ChildItem -LiteralPath (Join-Path $Repository $directory) -Recurse -File | Where-Object {
            $_.Extension -in @('.cpp', '.h', '.inc', '.pro', '.pri', '.md', '.conf', '.qrc', '.svg', '.cmd', '.ps1', '.py') -and
            $_.Name -notmatch '^(moc_|qrc_).+\.cpp$|^ui_.+\.h$'
        }
    }
    return $files | Sort-Object FullName -Unique
}
function Get-QtSourceManifest([string]$Repository) {
    @(Get-QtSourceFiles $Repository | ForEach-Object {
        [ordered]@{ path = $_.FullName.Substring($Repository.Length + 1).Replace('\', '/'); sha256 = (Get-QtFileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
    })
}
