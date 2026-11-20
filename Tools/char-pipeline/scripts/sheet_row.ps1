# Joins pictures into one sheet: sheet_row.ps1 -Out <png/jpg> -Cols <n> -Cell <px> -Files a.png,b.png,... [-Labels "a","b"]
param([string]$Out, [int]$Cols = 4, [int]$Cell = 400, [string[]]$Files, [string[]]$Labels = @(), [int]$Quality = 88)
Add-Type -AssemblyName System.Drawing
$Files = @($Files | ForEach-Object { $_ -split "," }); $Labels = @($Labels | ForEach-Object { $_ -split "," })
$n = $Files.Count
$rows = [math]::Ceiling($n / $Cols)
$lab = if ($Labels.Count -gt 0) { 30 } else { 0 }
$bmp = New-Object System.Drawing.Bitmap(($Cols * $Cell), ($rows * ($Cell + $lab)))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = 'HighQualityBicubic'
$g.Clear([System.Drawing.Color]::White)
$font = New-Object System.Drawing.Font('Malgun Gothic', 13, [System.Drawing.FontStyle]::Bold)
for ($i = 0; $i -lt $n; $i++) {
  $x = ($i % $Cols) * $Cell; $y = [math]::Floor($i / $Cols) * ($Cell + $lab)
  $im = [System.Drawing.Image]::FromFile((Resolve-Path $Files[$i]))
  $s = [math]::Min($Cell / $im.Width, $Cell / $im.Height)
  $w = [int]($im.Width * $s); $h = [int]($im.Height * $s)
  $g.DrawImage($im, $x + [int](($Cell - $w) / 2), $y + $lab + [int](($Cell - $h) / 2), $w, $h)
  $im.Dispose()
  if ($lab -gt 0 -and $i -lt $Labels.Count) { $g.DrawString($Labels[$i], $font, [System.Drawing.Brushes]::Black, $x + 6, $y + 3) }
}
$g.Dispose()
if ($Out -match '\.jpe?g$') {
  $enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
  $p = New-Object System.Drawing.Imaging.EncoderParameters(1)
  $p.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, [long]$Quality)
  $bmp.Save($Out, $enc, $p)
} else { $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png) }
$bmp.Dispose()
Write-Output "SHEET $Out"
