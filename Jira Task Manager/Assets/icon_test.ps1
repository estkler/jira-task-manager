$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$assetDirectory = $PSScriptRoot

function AssertCheckSilhouette([System.Drawing.Bitmap]$bitmap, [string]$label) {
    # Check the two strokes and the clear space above the short stroke. These
    # samples catch a missing/reversed stroke or accidental extra bars/frame.
    foreach ($sample in @(
        @{ X=.30; Y=.55; White=$true },
        @{ X=.55; Y=.55; White=$true },
        @{ X=.30; Y=.23; White=$false }
    )) {
        $x = [int][Math]::Floor($sample.X * $bitmap.Width)
        $y = [int][Math]::Floor($sample.Y * $bitmap.Height)
        $pixel = $bitmap.GetPixel($x,$y)
        $white = $pixel.A -gt 200 -and $pixel.R -gt 195 -and $pixel.G -gt 195 -and $pixel.B -gt 195
        if ($sample.White -ne $white) { throw "$label does not have the approved check silhouette at ($x,$y)." }
    }
    if ($bitmap.GetPixel(0,0).A -ne 0) { throw "$label lost its transparent rounded corner." }
}

$master = [System.Drawing.Bitmap]::new((Join-Path $assetDirectory 'Task Manager.png'))
try { AssertCheckSilhouette $master 'Master PNG' } finally { $master.Dispose() }
$small = [System.Drawing.Bitmap]::new((Join-Path $assetDirectory 'Task Manager 16.png'))
try { AssertCheckSilhouette $small '16 px PNG' } finally { $small.Dispose() }

$bytes = [IO.File]::ReadAllBytes((Join-Path $assetDirectory 'Task Manager.ico'))
$count = [BitConverter]::ToUInt16($bytes,4)
$expectedSizes = @(16,24,32,48,256)
if ($count -ne $expectedSizes.Count) { throw 'ICO does not contain every required Windows icon size.' }
for ($index=0; $index -lt $count; $index++) {
    $entry=6+16*$index
    $size=if ($bytes[$entry] -eq 0) { 256 } else { [int]$bytes[$entry] }
    if ($size -ne $expectedSizes[$index]) { throw 'ICO frame sizes are incorrect.' }
    $length=[BitConverter]::ToUInt32($bytes,$entry+8)
    $offset=[BitConverter]::ToUInt32($bytes,$entry+12)
    $stream=[IO.MemoryStream]::new($bytes,[int]$offset,[int]$length)
    $frame=[System.Drawing.Bitmap]::new($stream)
    try {
        if ($frame.Width -ne $size -or $frame.Height -ne $size) { throw 'ICO directory and image dimensions disagree.' }
        AssertCheckSilhouette $frame "$size px ICO"
    } finally { $frame.Dispose(); $stream.Dispose() }
}
Write-Output 'Icon tests passed: master PNG, small PNG and all five ICO sizes.'
