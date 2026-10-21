# Cut each animal friend's HUD / title-card portrait out of its front concept art (round 73: the user wants the concept
# pictures in the UI instead of 3D renders). A square around the head and shoulders, scaled to 256 x 256 PNG, written to
# C:\work\char-pipeline\output\ui\T_<Name>Portrait.png (then ue_import_ui_texture.py puts them in /Game/UI). The UI draws
# them in a circle, so the corners don't matter. Crops are x, y, size in the source image's pixels.
param([string]$OutDir = "C:\work\char-pipeline\output\ui", [string]$Only = "")
Add-Type -AssemblyName System.Drawing
$In = "C:\work\char-pipeline\input"
$portraits = @(
    @("Fox",      "$In\fox_v3\concept\fox_v3_front_clean.png",              237, 25, 550),
    @("Rabbit",   "$In\rabbit_v3\concept\rabbit_v3_front_clean.png",        252, 90, 520),
    @("Squirrel", "$In\squirrel_v3\concept\squirrel_v3_front_clean.png",    230, 40, 500),
    @("Tiger",    "$In\tiger_fluffy\concept\tiger_fluffy_v2_front.png",     237, 25, 550),
    @("RedFox",   "$In\fox_girl\concept\fox_girl_v2_front.png",             227,  5, 590),
    # The dragon has no shirt in the game (round 44): its concept is a 404 px capture
    @("Dragon",   "$In\dragon_v2\concept\dragon_front_noshirt_capture.png",  82, 18, 240)
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
