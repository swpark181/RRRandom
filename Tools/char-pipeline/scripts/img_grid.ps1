# Tiles any pictures into one labeled sheet, in the order given (2026-11-23, for the fur mask checks).
#   img_grid.ps1 -Files a.png,b.png,... -Cols 4 -Out sheet.jpg [-Cell 350] [-Labels A,B,...] [-Crop x,y,w,h]
# -Cell: each tile's width in pixels (its height follows the picture's or the crop's shape); -Crop: the part of every
# picture to show (source pixels); -Labels: one per picture (default: the file's name without extension).
param(
    [string[]]$Files,
    [int]$Cols = 4,
    [string]$Out,
    [int]$Cell = 350,
    [string[]]$Labels,
    [int[]]$Crop
)
Add-Type -AssemblyName System.Drawing
$first = [System.Drawing.Image]::FromFile($Files[0])
if ($Crop) { $sw = $Crop[2]; $sh = $Crop[3] } else { $sw = $first.Width; $sh = $first.Height }
$first.Dispose()
$ch = [int]($Cell * $sh / $sw)
$rows = [int][math]::Ceiling($Files.Count / $Cols)
$sheet = New-Object System.Drawing.Bitmap ($Cell * $Cols), ($ch * $rows)
$g = [System.Drawing.Graphics]::FromImage($sheet)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.Clear([System.Drawing.Color]::FromArgb(40, 40, 44))
$font = New-Object System.Drawing.Font "Arial", 11, ([System.Drawing.FontStyle]::Bold)
for ($i = 0; $i -lt $Files.Count; $i++) {
    if (-not (Test-Path $Files[$i])) { continue }
    $img = [System.Drawing.Image]::FromFile($Files[$i])
    $x = ($i % $Cols) * $Cell; $y = [int][math]::Floor($i / $Cols) * $ch
    if ($Crop) { $src = New-Object System.Drawing.Rectangle $Crop[0], $Crop[1], $Crop[2], $Crop[3] }
    else { $src = New-Object System.Drawing.Rectangle 0, 0, $img.Width, $img.Height }
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle $x, $y, $Cell, $ch), $src, [System.Drawing.GraphicsUnit]::Pixel)
    $img.Dispose()
    if ($Labels -and $i -lt $Labels.Count) { $label = $Labels[$i] } else { $label = [System.IO.Path]::GetFileNameWithoutExtension($Files[$i]) }
    $g.DrawString($label, $font, [System.Drawing.Brushes]::Black, ($x + 7), ($y + 7))
    $g.DrawString($label, $font, [System.Drawing.Brushes]::White, ($x + 6), ($y + 6))
}
$g.Dispose()
if ($Out -match "\.jpe?g$") { $sheet.Save($Out, [System.Drawing.Imaging.ImageFormat]::Jpeg) } else { $sheet.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png) }
$sheet.Dispose()
Write-Output "GRID $Out ($($Files.Count) pictures, $Cols across)"
