param([string[]]$Files, [string]$Out, [int]$Cols = 3, [int]$X = 240, [int]$Y = 60, [int]$W = 800, [int]$H = 560, [double]$Scale = 0.5)
Add-Type -AssemblyName System.Drawing
$cw = [int]($W * $Scale); $ch = [int]($H * $Scale)
$rows = [math]::Ceiling($Files.Count / $Cols)
$sheet = New-Object System.Drawing.Bitmap -ArgumentList ([int]($cw * $Cols)), ([int]($ch * $rows))
$g = [System.Drawing.Graphics]::FromImage($sheet)
for ($i = 0; $i -lt $Files.Count; $i++) {
  $img = [System.Drawing.Image]::FromFile($Files[$i])
  $dst = New-Object System.Drawing.Rectangle -ArgumentList ([int](($i % $Cols) * $cw)), ([int]([math]::Floor($i / $Cols) * $ch)), $cw, $ch
  $src = New-Object System.Drawing.Rectangle -ArgumentList $X, $Y, $W, $H
  $g.DrawImage($img, $dst, $src, [System.Drawing.GraphicsUnit]::Pixel)
  $g.DrawString([System.IO.Path]::GetFileNameWithoutExtension($Files[$i]), (New-Object System.Drawing.Font("Arial", 11)), [System.Drawing.Brushes]::Black, ($i % $Cols) * $cw + 4, [math]::Floor($i / $Cols) * $ch + 4)
  $img.Dispose()
}
$sheet.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
