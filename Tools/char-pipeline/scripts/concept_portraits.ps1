# Cut each animal friend's HUD / title-card portrait out of its front concept art (round 73: the user wants the concept
# pictures in the UI instead of 3D renders). A square around the head and shoulders, scaled to 256 x 256 PNG, written to
# C:\work\char-pipeline\output\ui\T_<Name>Portrait.png (then ue_import_ui_texture.py puts them in /Game/UI). The UI draws
# them in a circle, so the corners don't matter. Crops are x, y, size in the source image's pixels.
param([string]$OutDir = "C:\work\char-pipeline\output\ui", [string]$Only = "")
Add-Type -AssemblyName System.Drawing
$In = "C:\work\char-pipeline\input"
$portraits = @(
    @("Fox",      "$In\fox_film\concept\fox_neck_front.png",                237, 60, 550),
    @("Rabbit",   "$In\rabbit_mjc\concept\rabbit_front.png",                257, 150, 520),
    @("Squirrel", "$In\squirrel_film\concept\squirrel_front.png",           252, 70, 530),
    @("Tiger",    "$In\tiger_film\concept\tiger_front.png",                 252, 50, 540),
    @("RedFox",   "$In\redfox_mjc\concept\mjc_front.png",                   212, 90, 600),
    # Round 77: the film-style dragon (front with its nose smoke edited out)
    @("Dragon",   "$In\dragon_film\concept\dragon_front.png",               245, 70, 540)
)
foreach ($p in $portraits) {
    $name, $src, $x, $y, $size = $p
    if ($Only -and $Only -ne $name) { continue }
    $img = [System.Drawing.Image]::FromFile($src)
    $bmp = New-Object System.Drawing.Bitmap 256, 256
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = "HighQualityBicubic"; $g.PixelOffsetMode = "HighQuality"
    $g.DrawImage($img, (New-Object System.Drawing.Rectangle 0, 0, 256, 256), (New-Object System.Drawing.Rectangle $x, $y, $size, $size), [System.Drawing.GraphicsUnit]::Pixel)
    $out = "$OutDir\T_${name}Portrait.png"
    $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose(); $img.Dispose()
    $out
}
