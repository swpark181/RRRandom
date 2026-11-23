# Puts four screen captures of an edited 2 x 2 mouth sheet back onto the sheet (2026-11-23, blender_mouth_views.py).
#   sheet_paste.ps1 -Sheet <orig sheet png> -Caps front.jpg,low.jpg,left.jpg,right.jpg -Out <edited sheet png>
# Each capture is the middle band of one panel, taken with the result shown at 4x the browser's height (the page helper
# __band): in a 2048-wide sheet it covers CoverW x 512 pixels, centred in its panel across and from a quarter down.
# Order of -Caps: top-left, top-right, bottom-left, bottom-right. Everything outside the bands stays the original.
param(
    [string]$Sheet,
    [string[]]$Caps,
    [string]$Out,
    [double]$CoverW = 987.7359
)
Add-Type -AssemblyName System.Drawing
$base = [System.Drawing.Image]::FromFile($Sheet)
$res = New-Object System.Drawing.Bitmap $base.Width, $base.Height
$g = [System.Drawing.Graphics]::FromImage($res)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($base, 0, 0, $base.Width, $base.Height)
$s = $base.Width / 2048.0
for ($i = 0; $i -lt 4; $i++) {
    $cap = [System.Drawing.Image]::FromFile($Caps[$i])
    $col = $i % 2; $row = [int][math]::Floor($i / 2)
    $x = ($col * 1024 + (1024 - $CoverW) / 2) * $s; $y = ($row * 1024 + 256) * $s
    $g.DrawImage($cap, [single]$x, [single]$y, [single]($CoverW * $s), [single](512 * $s))
    $cap.Dispose()
}
$g.Dispose(); $base.Dispose()
$res.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png); $res.Dispose()
"PASTE $Out"
