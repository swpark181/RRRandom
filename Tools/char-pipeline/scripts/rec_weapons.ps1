# Round 97: records the new weapons in use on the Studio map and cuts each into a GIF <OutDir>\<clip>.gif:
#   sniper (one shot at a target 9 m ahead), shotgun (two blasts at 6 m), sword (three swings at a target 2.3 m ahead),
#   deflect (the sword's guard held while the target fires six shots at it from 9 m).
# Runs RRRandom in a 1920x1080 physical window driven by ue_game_seq.py, records it in real time with ffmpeg (gdigrab),
# and cuts the clips on the GAMESEQ log lines' times (each clip starts with a goto back to the start spot). As rec_grip.ps1.
# Each clip films from beside the line between me and the target (ue_game_seq.py pathcam), with the HUD (damage, words).
#   rec_weapons.ps1 [-Species Fox] [-OutDir C:\work\char-pipeline\output\weapons97\gif]
param(
    [string]$Species = "Fox",
    [string]$OutDir = "C:\work\char-pipeline\output\weapons97\gif",
    [int]$CropW = 1500,
    [int]$CropH = 900,
    [int]$Size = 520,
    # Other clips instead: "name|gun|0.2:cmd;1.2:cmd|gif start|gif length#name|..."
    [string]$ClipSpec = ""
)
$Ff = "C:\work\char-pipeline\tools\ffmpeg\ffmpeg.exe"
$Clips = Join-Path $OutDir "..\clips"
New-Item -ItemType Directory -Force $OutDir, $Clips | Out-Null
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class WeapWin {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X; public int Y; }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@
[WeapWin]::SetProcessDPIAware() | Out-Null

# Each clip: its name, the gun (RRGun number), its steps after the goto (seconds: command), where its GIF starts after
# the goto and how long it lasts
$clipList = @(
    @("sniper", 7, @("0.2:RRIncoming 0 900", "0.75:pathcam=650,170,260", "1.2:RRShoot"), 0.9, 2.6),
    @("shotgun", 8, @("0.2:RRIncoming 0 600", "0.75:pathcam=560,160,180", "1.2:RRShoot", "3.4:RRShoot"), 0.9, 3.8),
    @("sword", 9, @("0.2:RRIncoming 0 230", "0.75:pathcam=400,140,70", "1.2:RRShoot", "1.75:RRShoot", "2.3:RRShoot", "3.6:RRShoot", "4.15:RRShoot", "4.7:RRShoot"), 0.9, 4.6),
    @("deflect", 9, @("0.2:RRIncoming 0 900", "0.75:frontcam=480,170,90", "1.1:RRGuard 3", "1.6:RRIncoming 6 900"), 0.9, 4.0)
)
if ($ClipSpec) {
    $clipList = @()
    foreach ($c in $ClipSpec.Split("#")) {
        $p = $c.Split("|")
        $clipList += ,@($p[0], [int]$p[1], $p[2].Split(";"), [double]$p[3], [double]$p[4])
    }
}
$seq = "3:RRGod;3.3:r.MotionBlurQuality 0"
$t = 5.0
foreach ($c in $clipList) {
    $seq += ";$($t):goto=0,0,0;$($t + 0.05):RRGun $($c[1])"
    foreach ($s in $c[2]) {
        $at, $cmd = $s.Split(":", 2)
        $seq += ";$($t + [double]$at):$cmd"
    }
    $t += 8.0
}
$seq += ";$($t):quit"
$env:RR_SEQ = $seq
$log = "weapons97_$Species.log"
$argList = "`"C:\work\RRRandom\RRRandom.uproject`" -game -windowed -ResX=1280 -ResY=720 -ForceRes -WinX=0 -WinY=0 -nosound -RRSolo -RRThirdPerson -RRMap=Studio -RRSpecies=$Species -RRIdleEnemies -RRIdleBots -RRNoOnlinePanel -log=$log -ExecCmds=`"py C:\work\char-pipeline\scripts\ue_game_seq.py`""
$p = Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $argList -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 160; $i++) {
    Start-Sleep -Milliseconds 250
    $p.Refresh()
    $h = $p.MainWindowHandle
    if ($h -ne [IntPtr]::Zero) {
        $r = New-Object WeapWin+RECT
        [WeapWin]::GetClientRect($h, [ref]$r) | Out-Null
        if (($r.R - $r.L) -ge 1900) { break }
    }
}
[WeapWin]::SetForegroundWindow($h) | Out-Null
# (kept on top of every other window while it records: another window over it would be filmed instead)
[WeapWin]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0003) | Out-Null
Start-Sleep -Milliseconds 300
$pt = New-Object WeapWin+POINT
[WeapWin]::ClientToScreen($h, [ref]$pt) | Out-Null
$mp4 = Join-Path $Clips "weapons97.mp4"
$began = (Get-Date).ToUniversalTime()
& $Ff -hide_banner -loglevel error -y -f gdigrab -framerate 30 -draw_mouse 0 -offset_x $pt.X -offset_y $pt.Y -video_size 1920x1080 -i desktop -t ($t + 12) -c:v h264_nvenc -preset p5 -cq 18 -pix_fmt yuv420p $mp4
$p.WaitForExit(30000) | Out-Null
if (!$p.HasExited) { Stop-Process -Id $p.Id -Force }

$gotos = @(Select-String -Path "C:\work\RRRandom\Saved\Logs\$log" -Pattern "GAMESEQ [0-9.]+ goto=" | ForEach-Object {
    if ($_.Line -match '^\[(\d+)\.(\d+)\.(\d+)-(\d+)\.(\d+)\.(\d+):(\d+)\]') {
        (New-Object DateTime ([int]$Matches[1]), ([int]$Matches[2]), ([int]$Matches[3]), ([int]$Matches[4]), ([int]$Matches[5]), ([int]$Matches[6]), ([int]$Matches[7]), ([DateTimeKind]::Utc))
    }
})
"began $($began.ToString('HH:mm:ss.fff')), $($gotos.Count) gotos"
$x = [int]((1920 - $CropW) / 2); $y = [int]((1080 - $CropH) / 2)
for ($k = 0; $k -lt $clipList.Count; $k++) {
    if ($k -ge $gotos.Count) { continue }
    $c = $clipList[$k]
    $start = ($gotos[$k] - $began).TotalSeconds + $c[3]
    $gif = Join-Path $OutDir "$($c[0]).gif"
    & $Ff -hide_banner -loglevel error -y -ss ([math]::Max(0, $start)) -t $c[4] -i $mp4 -vf "crop=$($CropW):$($CropH):$($x):$($y),fps=15,scale=$($Size):-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=128:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4" $gif
    $gif
}

