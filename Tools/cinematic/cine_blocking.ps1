# (2026-11-20, sixth pass: 17 s, eight camera cuts: the chase is two)
# Fox vs tiger blocking (2026-11-19): rebuilds the level + sequence (ue_cine_build.py), the render preset, renders with
# Movie Render Queue, then makes a review mp4 (cut number and time in the corner) and a sheet of one frame per cut.
#   cine_blocking.ps1 -Take take2            # everything
#   cine_blocking.ps1 -Take take2 -NoBuild   # render only
param([string]$Take = "take", [switch]$NoBuild, [string]$Res = "1280x720", [string]$Sequence = "LS_FoxVsTiger")
$UE = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64"
$Proj = "C:\work\RRRandom\RRRandom.uproject"
$Here = $PSScriptRoot
$Out = "C:\work\char-pipeline\output\cinematic_blocking\render"
$FF = "C:\work\char-pipeline\tools\ffmpeg\ffmpeg.exe"

function Invoke-EditorPython([string]$Script, [string]$Tag, [string]$DoneMark) {
    $log = "C:\work\RRRandom\Saved\Logs\$Tag.log"; if (Test-Path $log) { Remove-Item $log }
    $p = Start-Process -FilePath "$UE\UnrealEditor-Cmd.exe" -ArgumentList "`"$Proj`" -run=pythonscript -script=`"$Here\$Script`" -unattended -nosplash -log=$Tag.log" -PassThru
    # (the pythonscript commandlet stays alive after the script: wait for its last line, then stop it)
    $t = 0; while ($t -lt 400) { Start-Sleep 5; $t += 5; if ((Test-Path $log) -and (Select-String -Path $log -Pattern "$DoneMark|Traceback" -Quiet)) { Start-Sleep 8; break }; if ($p.HasExited) { break } }
    if (!$p.HasExited) { Stop-Process -Id $p.Id -Force }
    Select-String -Path $log -Pattern "CINE\w+ |Traceback|LogPython: Error" | Where-Object { $_.Line -notmatch "LogInit: Display" } | ForEach-Object { $_.Line.Substring([Math]::Min(30, $_.Line.Length)) }
}

if (!$NoBuild) { Invoke-EditorPython "ue_cine_build.py" "cine_build" "CINEBUILD DONE" }
$env:RR_CINE_TAKE = $Take; $env:RR_CINE_RES = $Res
Invoke-EditorPython "ue_cine_render_config.py" "cine_cfg" "CINECFG DONE"

$log = "C:\work\RRRandom\Saved\Logs\cine_render.log"; if (Test-Path $log) { Remove-Item $log }
$w, $h = $Res.Split("x")
$args = "`"$Proj`" /Game/Cinematics/FoxVsTiger/Map_FoxVsTiger -game -LevelSequence=`"/Game/Cinematics/FoxVsTiger/$Sequence`" -MoviePipelineConfig=`"/Game/Cinematics/FoxVsTiger/MRQ_Blocking720`" -windowed -ResX=$w -ResY=$h -nosound -unattended -dpcvars=r.PSOPrecaching=0 -log=cine_render.log"
$p = Start-Process -FilePath "$UE\UnrealEditor.exe" -ArgumentList $args -PassThru
$p.WaitForExit(900000) | Out-Null
if (!$p.HasExited) { Stop-Process -Id $p.Id -Force; "render killed (timeout)" }
Select-String -Path $log -Pattern "RRCine|wrecks: \d+ in force|Movie Pipeline completed" | ForEach-Object { $_.Line.Substring([Math]::Min(30, $_.Line.Length)) }

$dir = "$Out\$Take"
$n = (Get-ChildItem $dir -Filter "fvt.*").Count
"frames: $n in $dir"
if ($n -eq 0) { return }
$ext = (Get-ChildItem $dir -Filter "fvt.*" | Select-Object -First 1).Extension
$font = "C\:/Windows/Fonts/arialbd.ttf"
$cuts = @(@(0, 2.4, "1A CHASE"), @(2.4, 5, "1B BARRICADE"), @(5, 6.5, "2 DEAD END"), @(6.5, 8.5, "3 TIGER"), @(8.5, 10.05, "4A GOLD DIE"), @(10.05, 10.5, "4B TIGER STARTS"), @(10.5, 12.5, "5 CLONES"), @(12.5, 15, "6 SURROUNDED"), @(15, 17, "7 GRIN"))
$draw = ($cuts | ForEach-Object { "drawtext=fontfile='$font':text='CUT $($_[2])':x=20:y=18:fontsize=26:fontcolor=white:box=1:boxcolor=black@0.45:boxborderw=8:enable='between(t,$($_[0]),$($_[1]))'" }) -join ","
$draw += ",drawtext=fontfile='$font':text='%{eif\:t\:d}.%{eif\:mod(t*10\,10)\:d}s  f%{frame_num}':x=w-tw-20:y=18:fontsize=24:fontcolor=white:box=1:boxcolor=black@0.45:boxborderw=8"
& $FF -y -loglevel error -framerate 24 -i "$dir\fvt.%04d$ext" -vf $draw -c:v libx264 -pix_fmt yuv420p -crf 18 "$Out\$Take.mp4"
"mp4: $Out\$Take.mp4"
# One frame from the middle of each cut
$mids = $cuts | ForEach-Object { [int](($_[0] + $_[1]) / 2 * 24) }
$inputs = $mids | ForEach-Object { "-i"; ("$dir\fvt.{0:D4}$ext" -f $_) }
& $FF -y -loglevel error @inputs -filter_complex "[0]scale=640:-2[a];[1]scale=640:-2[b];[2]scale=640:-2[c];[3]scale=640:-2[d];[4]scale=640:-2[e];[5]scale=640:-2[f];[6]scale=640:-2[g];[7]scale=640:-2[h];[8]scale=640:-2[i];[a][b][c]hstack=3[r1];[d][e][f]hstack=3[r2];[g][h][i]hstack=3[r3];[r1][r2][r3]vstack=3" -q:v 3 "$Out\${Take}_sheet.jpg"
"sheet: $Out\${Take}_sheet.jpg"
