# Crops (and optionally resizes) a picture: crop_img.ps1 -In a.png -Out b.jpg -X 0 -Y 0 -W 512 -H 512 [-Size 512]
param([string]$In, [string]$Out, [int]$X = 0, [int]$Y = 0, [int]$W = 0, [int]$H = 0, [int]$Size = 0, [int]$Quality = 88)
Add-Type -AssemblyName System.Drawing
$im = [System.Drawing.Image]::FromFile((Resolve-Path $In))
if ($W -eq 0) { $W = $im.Width - $X }; if ($H -eq 0) { $H = $im.Height - $Y }
$ow = $W; $oh = $H
if ($Size -gt 0) { $s = $Size / [math]::Max($W, $H); $ow = [int]($W * $s); $oh = [int]($H * $s) }
$bmp = New-Object System.Drawing.Bitmap($ow, $oh)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.InterpolationMode = 'HighQualityBicubic'
$g.DrawImage($im, (New-Object System.Drawing.Rectangle(0, 0, $ow, $oh)), (New-Object System.Drawing.Rectangle($X, $Y, $W, $H)), [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose(); $im.Dispose()
if ($Out -match '\.jpe?g$') {
  $enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
  $p = New-Object System.Drawing.Imaging.EncoderParameters(1)
  $p.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, [long]$Quality)
  $bmp.Save($Out, $enc, $p)
} else { $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png) }
$bmp.Dispose()
Write-Output "CROP $Out ${ow}x${oh}"
