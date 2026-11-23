# Close views of an animal friend's face in the game, to check the shell fur round its eyes and mouth (2026-11-23, the
# fur mask).
#   fur_mask_shots.ps1 -Species Fox -Tag fox_after [-Show] [-Expr] [-NoGame]
# The game's animal friend (Studio map, facing the sun, pose frozen), seven views -> <OutDir>\<Tag>_g<n>.png and the
# sheet <Tag>_game.jpg:
#   0 front, 1 close front, 2 three-quarter, 3 side, 4 three-quarter from a little below, 5 the eyes, 6 from above,
#   and with -Show 7 = the front with the masked share painted cyan ("FurMaskShow").
# With -Expr, a second run on the cinematic's map: its animal friend of that species (ue_game_seq.py cineface=) lifted
#   into the open sky facing the sun, wearing the game face and then each expression the way the cinematic swaps them
#   (body + fur material), two views each (front, three-quarter from a little below) -> <Tag>_e<nn>.png and the sheet
#   <Tag>_expr.jpg. -NoGame skips the first run.
param(
    [string]$Species = "Fox",
    [string]$Tag = "fm",
    [switch]$Show,
    [switch]$Expr,
    [switch]$NoGame,
    [string[]]$Expressions = @("-", "Determined", "Grin", "GrinBlink", "Joy", "Scared", "Worried", "WorriedBlink"),
    [string]$OutDir = "C:\work\char-pipeline\output\screens\fur_mask"
)
$index = @("Fox", "Rabbit", "Squirrel", "Tiger", "Dragon", "RedFox").IndexOf($Species)

function Run-Shots([string]$RunTag, [string]$Extra, [string]$Seq, [double]$Seconds, [string]$Prefix) {
    Remove-Item "$OutDir\${RunTag}_*.png", "$OutDir\${Prefix}*.png" -ErrorAction SilentlyContinue
    & "$PSScriptRoot\capture_game_seq.ps1" -Species $Species -Tag $RunTag -OutDir $OutDir -Timeout ([int]$Seconds + 60) -Extra $Extra -Seq $Seq | Out-Null
    $raw = @(Get-ChildItem $OutDir -Filter "${RunTag}_*.png" | Sort-Object { [int]($_.BaseName -replace ".*_", "") })
    $files = @()
    for ($i = 0; $i -lt $raw.Count; $i++) { $to = "$OutDir\$Prefix{0:D2}.png" -f $i; Move-Item $raw[$i].FullName $to -Force; $files += $to }
    Select-String -Path "C:\work\RRRandom\Saved\Logs\cap_$RunTag.log" -Pattern "Traceback|Ensure condition|Assertion failed|Fatal error" | Select-Object -First 5 | ForEach-Object { "$RunTag log: " + $_.Line.Substring([Math]::Min(30, $_.Line.Length)) } | Out-Host
    return $files
}

if (!$NoGame) {
    $views = @("130,0,18", "70,0,16", "120,40,18", "120,90,18", "75,25,14,-12", "60,-12,24,4", "110,-25,18,35")
    $t = 13.0
    $seq = "7:RRGod;8:goto=0,0,180;12:RRSlomo 0.01"
    foreach ($v in $views) { $seq += ";${t}:facecam=$v"; $t += 0.9; $seq += ";${t}:shot"; $t += 0.6 }
    if ($Show) { $seq += ";${t}:facecam=$($views[0])"; $t += 0.3; $seq += ";${t}:furparam=FurMaskShow,1"; $t += 0.9; $seq += ";${t}:shot"; $t += 0.6 }
    $seq += ";${t}:quit"
    $files = Run-Shots "${Tag}_rawg" "-RRMap=Studio -RRIdleBots -RRThirdPerson -RRNoOnlinePanel" $seq $t "${Tag}_g"
    if ($files.Count) { & "$PSScriptRoot\img_grid.ps1" -Files $files -Cols 4 -Out "$OutDir\${Tag}_game.jpg" -Cell 480 -Crop 240,60,800,600 }
}
if ($Expr) {
    $t = 10.0
    $seq = "${t}:cineface=$index,-"; $t += 0.4
    $seq += ";${t}:tplace=0,0,1500,0"; $t += 0.6
    $labels = @()
    foreach ($e in $Expressions) {
        $seq += ";${t}:cineface=$index,$e"; $t += 0.4
        foreach ($v in @("105,0,20", "100,30,18,-14")) { $seq += ";${t}:facecam=$v"; $t += 0.9; $seq += ";${t}:shot"; $t += 0.6 }
        $name = $e; if ($e -eq "-") { $name = "game face" }
        $labels += "$name"; $labels += "$name 3/4"
    }
    $seq += ";${t}:quit"
    $files = Run-Shots "${Tag}_rawe" "/Game/Cinematics/FoxVsTiger/Map_FoxVsTiger -RRThirdPerson -RRNoOnlinePanel" $seq $t "${Tag}_e"
    if ($files.Count) { & "$PSScriptRoot\img_grid.ps1" -Files $files -Cols 4 -Out "$OutDir\${Tag}_expr.jpg" -Cell 480 -Crop 240,60,800,600 -Labels $labels }
}
