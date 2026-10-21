# Tiles the middle of each capture <Dir>\<Prefix>_<Row>_<Col>.png (capture_game_seq.ps1 names them so) into one labeled
# sheet <Dir>\<Out>: one row per Rows entry (e.g. the species), columns 0 .. Cols-1 (e.g. the vehicles). Round 71.
#   shot_sheet.ps1 -Dir C:\work\char-pipeline\output\screens\ride -Prefix ride -Rows Fox,Rabbit -Cols 3 -Out ride_sheet.png
param(
    [string]$Dir = "C:\work\char-pipeline\output\screens\ride",
    [string]$Prefix,
    [string[]]$Rows,
    [int]$Cols,
    [string]$Out,
    [int]$X = 390, [int]$Y = 110, [int]$W = 500, [int]$H = 480,
    [double]$Scale = 0.5
)
Add-Type -AssemblyName System.Drawing
$cw = [int]($W * $Scale); $ch = [int]($H * $Scale)
$sheet = New-Object System.Drawing.Bitmap ($cw * $Cols), ($ch * $Rows.Count)
$g = [System.Drawing.Graphics]::FromImage($sheet)
$font = New-Object System.Drawing.Font "Arial", 11
for ($r = 0; $r -lt $Rows.Count; $r++) {
    for ($c = 0; $c -lt $Cols; $c++) {
        $path = "$Dir\${Prefix}_$($Rows[$r])_$c.png"
        if (Test-Path $path) {
            $img = [System.Drawing.Image]::FromFile($path)
            $g.DrawImage($img, (New-Object System.Drawing.Rectangle ($c * $cw), ($r * $ch), $cw, $ch), (New-Object System.Drawing.Rectangle $X, $Y, $W, $H), [System.Drawing.GraphicsUnit]::Pixel)
            $img.Dispose()
        }
        $g.DrawString("$($Rows[$r]) $c", $font, [System.Drawing.Brushes]::Yellow, ($c * $cw + 3), ($r * $ch + 3))
    }
}
$sheet.Save("$Dir\$Out")
"$Dir\$Out"
