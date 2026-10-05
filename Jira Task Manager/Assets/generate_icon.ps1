$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function RoundedPath([float]$x, [float]$y, [float]$width, [float]$height, [float]$radius) {
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $diameter = $radius * 2
    $path.AddArc($x, $y, $diameter, $diameter, 180, 90)
    $path.AddArc($x + $width - $diameter, $y, $diameter, $diameter, 270, 90)
    $path.AddArc($x + $width - $diameter, $y + $height - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($x, $y + $height - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return $path
}

$assetDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
$bitmap = [System.Drawing.Bitmap]::new(256, 256, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
$graphics.Clear([System.Drawing.Color]::Transparent)

$tile = RoundedPath 8 8 240 240 48
$blueTop = [System.Drawing.Color]::FromArgb(255, 43, 133, 244)
$blueBottom = [System.Drawing.Color]::FromArgb(255, 0, 82, 204)
$gradient = [System.Drawing.Drawing2D.LinearGradientBrush]::new(
    [System.Drawing.Rectangle]::new(8, 8, 240, 240), $blueTop, $blueBottom, 90.0)
$graphics.FillPath($gradient, $tile)

$white = [System.Drawing.Color]::White
# One check, without an inner card or extra bars, is the approved application
# identity. Use the same silhouette at every size, including the title bar.
$mark = [System.Drawing.Pen]::new($white, 28)
$mark.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
$mark.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
$mark.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
$checkPoints = [System.Drawing.PointF[]]@(
    [System.Drawing.PointF]::new(64,128),
    [System.Drawing.PointF]::new(108,172),
    [System.Drawing.PointF]::new(196,84)
)
$graphics.DrawLines($mark, $checkPoints)

$pngPath = Join-Path $assetDirectory 'Task Manager.png'
$bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)

$sizes = @(16, 24, 32, 48, 256)
$frames = [System.Collections.Generic.List[byte[]]]::new()
foreach ($size in $sizes) {
    $scaled = [System.Drawing.Bitmap]::new($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $painter = [System.Drawing.Graphics]::FromImage($scaled)
    $painter.Clear([System.Drawing.Color]::Transparent)
    $painter.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $painter.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $painter.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $painter.DrawImage($bitmap, 0, 0, $size, $size)
    if ($size -eq 16) { $scaled.Save((Join-Path $assetDirectory 'Task Manager 16.png'), [System.Drawing.Imaging.ImageFormat]::Png) }
    $stream = [System.IO.MemoryStream]::new()
    $scaled.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $frames.Add($stream.ToArray())
    $stream.Dispose()
    if ($painter) { $painter.Dispose() }
    $scaled.Dispose()
}

$icoPath = Join-Path $assetDirectory 'Task Manager.ico'
$file = [System.IO.File]::Create($icoPath)
$writer = [System.IO.BinaryWriter]::new($file)
$writer.Write([uint16]0)
$writer.Write([uint16]1)
$writer.Write([uint16]$frames.Count)
$offset = 6 + 16 * $frames.Count
for ($index = 0; $index -lt $frames.Count; $index++) {
    $size = $sizes[$index]
    $writer.Write([byte]($size % 256))
    $writer.Write([byte]($size % 256))
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]32)
    $writer.Write([uint32]$frames[$index].Length)
    $writer.Write([uint32]$offset)
    $offset += $frames[$index].Length
}
foreach ($frame in $frames) { $writer.Write($frame) }
$writer.Dispose()
$mark.Dispose()
$gradient.Dispose()
$tile.Dispose()
$graphics.Dispose()
$bitmap.Dispose()

Get-Item -LiteralPath $icoPath,$pngPath | Select-Object FullName,Length
