$ErrorActionPreference = 'Stop'

$assetsDir = $PSScriptRoot
$repoRoot  = Resolve-Path (Join-Path $assetsDir '..\..')
$tool      = Join-Path $repoRoot 'tools\image_to_header.py'

$imgExts = @('.bmp','.png','.jpg','.jpeg','.gif','.webp','.tif','.tiff')

Get-ChildItem -Path $assetsDir -File |
    Where-Object { $imgExts -contains $_.Extension.ToLower() } |
    ForEach-Object {
        $src    = $_.FullName
        $stem   = [System.IO.Path]::GetFileNameWithoutExtension($_.Name)
        $header = Join-Path $assetsDir "${stem}_data.h"
        $sym    = $stem.Substring(0,1).ToUpper() + $stem.Substring(1)

        $needs = -not (Test-Path $header) -or `
                 ((Get-Item $src).LastWriteTime  -gt (Get-Item $header).LastWriteTime) -or `
                 ((Get-Item $tool).LastWriteTime -gt (Get-Item $header).LastWriteTime)

        if ($needs) {
            Write-Host "[assets] $($_.Name) -> $(Split-Path $header -Leaf)"
            & python $tool $src $header $sym --size 160
            if ($LASTEXITCODE -ne 0) { throw "image_to_header.py failed for $src" }
        }
    }
