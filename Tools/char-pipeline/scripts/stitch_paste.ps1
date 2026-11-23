# Two screen captures of one picture (its upper and lower half, each <W> x <H> pixels from the capture's top-left corner)
# stacked into one square picture, and optionally laid onto a base picture (2026-11-23, expression drafts looked at in
# the browser and captured instead of downloaded).
#   stitch_paste.ps1 -Top a.jpg -Bottom b.jpg -W 1516 -H 758 -Out whole.png
#   stitch_paste.ps1 -Top a.jpg -Bottom b.jpg -W 1516 -H 758 -Out front.png -Base base_front.png -X 256 -Y 380 -Size 1536
#     (the stacked picture drawn <Size> x <Size> at (<X>, <Y>) of the base; without -Size it fills the whole base)
param([string]$Top, [string]$Bottom, [int]$W, [int]$H, [string]$Out, [string]$Base = "", [int]$X = 0, [int]$Y = 0, [int]$Size = 0)
Add-Type -AssemblyName System.Drawing
$a = [System.Drawing.Image]::FromFile($Top); $b = [System.Drawing.Image]::FromFile($Bottom)
$whole = New-Object System.Drawing.Bitmap $W, (2 * $H)
$g = [System.Drawing.Graphics]::FromImage($whole)
$g.DrawImage($a, (New-Object System.Drawing.Rectangle 0, 0, $W, $H), (New-Object System.Drawing.Rectangle 0, 0, $W, $H), [System.Drawing.GraphicsUnit]::Pixel)
$g.DrawImage($b, (New-Object System.Drawing.Rectangle 0, $H, $W, $H), (New-Object System.Drawing.Rectangle 0, 0, $W, $H), [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose(); $a.Dispose(); $b.Dispose()
if ($Base) {
    $src = [System.Drawing.Image]::FromFile($Base)
    $res = New-Object System.Drawing.Bitmap $src.Width, $src.Height
    $g = [System.Drawing.Graphics]::FromImage($res)
    $g.InterpolationMode = 'HighQualityBicubic'
    $g.DrawImage($src, 0, 0, $src.Width, $src.Height)
    if ($Size -le 0) { $Size = $src.Width }
    $g.DrawImage($whole, (New-Object System.Drawing.Rectangle $X, $Y, $Size, $Size), (New-Object System.Drawing.Rectangle 0, 0, $W, (2 * $H)), [System.Drawing.GraphicsUnit]::Pixel)
    $g.Dispose(); $src.Dispose(); $whole.Dispose(); $whole = $res
}
$whole.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
"STITCH $Out $($whole.Width)x$($whole.Height)"
$whole.Dispose()
