$ErrorActionPreference = 'Stop'

$assetsDir = $PSScriptRoot
$repoRoot  = Resolve-Path (Join-Path $assetsDir '..\..')
$tool      = Join-Path $repoRoot 'tools\bmp_to_header.py'

Get-ChildItem -Path $assetsDir -Filter '*.bmp' | ForEach-Object {
    $bmp    = $_.FullName
    $stem   = [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
    $header = Join-Path $assetsDir "${stem}_data.h"
    $sym    = $stem.Substring(0,1).ToUpper() + $stem.Substring(1)

    $needs = -not (Test-Path $header) -or `
             ((Get-Item $bmp).LastWriteTime -gt (Get-Item $header).LastWriteTime) -or `
             ((Get-Item $tool).LastWriteTime -gt (Get-Item $header).LastWriteTime)

    if ($needs) {
        Write-Host "[assets] $($_.Name) -> $(Split-Path $header -Leaf)"
        & python $tool $bmp $header $sym --size 160
        if ($LASTEXITCODE -ne 0) { throw "bmp_to_header.py failed for $bmp" }
    }
}
