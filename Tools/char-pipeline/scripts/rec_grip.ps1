# Records one animal friend holding every gun, idle, walking and running (round 90, the grip fitting), and cuts each into
# a GIF: <OutDir>\<Species>_<GUN>_<idle|walk|run>.gif. Runs RRRandom on the Studio map (open floor) in a 1920x1080
# physical window driven by ue_game_seq.py, records the window in real time with ffmpeg (gdigrab), and cuts the clips on
# the GAMESEQ log lines' times (each state starts with a goto back to the start spot).
#   rec_grip.ps1 -Species Fox [-Guns PISTOL,SMG] [-OutDir C:\work\char-pipeline\output\grip90\gif]
param(
    [string]$Species = "Fox",
    [string[]]$Guns = @("PISTOL", "SMG", "MINIGUN", "RIFLE", "BLASTER", "CANNON"),
    [string]$OutDir = "C:\work\char-pipeline\output\grip90\gif",
    [string]$Cam = "guncam=body,190,-30,-40",
    [string]$Extra = "",
    [int]$Crop = 760,
    [int]$Size = 300
)
$Ff = "C:\work\char-pipeline\tools\ffmpeg\ffmpeg.exe"
$Clips = Join-Path $OutDir "..\clips"
New-Item -ItemType Directory -Force $OutDir, $Clips | Out-Null
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class GripWin {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X; public int Y; }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
}
"@
[GripWin]::SetProcessDPIAware() | Out-Null

# Per gun: idle at the start spot, then back to it and walk ahead, then back and run (Shift) ahead
$seq = "3:RRGod;3.2:showhud;3.3:r.MotionBlurQuality 0;3.5:$Cam"
$t = 5.0
foreach ($g in $Guns) {
    $seq += ";$($t):goto=0,0,0;$($t + 0.05):RRGun $g"
    $seq += ";$($t + 3.2):goto=0,0,0;$($t + 3.3):walk=1,0,3.0"
    $seq += ";$($t + 6.6):goto=0,0,0;$($t + 6.65):Input.+key LeftShift;$($t + 6.7):walk=1,0,3.0;$($t + 9.7):Input.-key LeftShift"
    $t += 10.2
}
$seq += ";$($t):quit"
$env:RR_SEQ = $seq
$log = "grip_$Species.log"
$argList = "`"C:\work\RRRandom\RRRandom.uproject`" -game -windowed -ResX=1280 -ResY=720 -ForceRes -WinX=0 -WinY=0 -nosound -RRSolo -RRThirdPerson -RRMap=Studio -RRSpecies=$Species -RRIdleEnemies -RRIdleBots -RRNoOnlinePanel $Extra -log=$log -ExecCmds=`"py C:\work\char-pipeline\scripts\ue_game_seq.py`""
$p = Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $argList -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 160; $i++) {
    Start-Sleep -Milliseconds 250
    $p.Refresh()
    $h = $p.MainWindowHandle
    if ($h -ne [IntPtr]::Zero) {
        $r = New-Object GripWin+RECT
        [GripWin]::GetClientRect($h, [ref]$r) | Out-Null
        if (($r.R - $r.L) -ge 1900) { break }
    }
}
[GripWin]::SetForegroundWindow($h) | Out-Null
$pt = New-Object GripWin+POINT
[GripWin]::ClientToScreen($h, [ref]$pt) | Out-Null
$mp4 = Join-Path $Clips "$Species.mp4"
$began = (Get-Date).ToUniversalTime()
& $Ff -hide_banner -loglevel error -y -f gdigrab -framerate 30 -draw_mouse 0 -offset_x $pt.X -offset_y $pt.Y -video_size 1920x1080 -i desktop -t ($t + 12) -c:v h264_nvenc -preset p5 -cq 18 -pix_fmt yuv420p $mp4
$p.WaitForExit(30000) | Out-Null
if (!$p.HasExited) { Stop-Process -Id $p.Id -Force }

# The log's UTC times of each goto, in order: per gun idle, walk, run
$gotos = @(Select-String -Path "C:\work\RRRandom\Saved\Logs\$log" -Pattern "GAMESEQ [0-9.]+ goto=" | ForEach-Object {
    if ($_.Line -match '^\[(\d+)\.(\d+)\.(\d+)-(\d+)\.(\d+)\.(\d+):(\d+)\]') {
        (New-Object DateTime ([int]$Matches[1]), ([int]$Matches[2]), ([int]$Matches[3]), ([int]$Matches[4]), ([int]$Matches[5]), ([int]$Matches[6]), ([int]$Matches[7]), ([DateTimeKind]::Utc))
    }
})
"began $($began.ToString('HH:mm:ss.fff')), $($gotos.Count) gotos"
$states = @(@("idle", 1.4, 1.6), @("walk", 1.5, 1.6), @("run", 1.5, 1.6))
$x = [int]((1920 - $Crop) / 2); $y = [int]((1080 - $Crop) / 2)
for ($gi = 0; $gi -lt $Guns.Count; $gi++) {
    for ($si = 0; $si -lt 3; $si++) {
        $k = $gi * 3 + $si
        if ($k -ge $gotos.Count) { continue }
        $start = ($gotos[$k] - $began).TotalSeconds + $states[$si][1]
        $gif = Join-Path $OutDir ("{0}_{1}_{2}.gif" -f $Species, $Guns[$gi], $states[$si][0])
        & $Ff -hide_banner -loglevel error -y -ss ([math]::Max(0, $start)) -t $states[$si][2] -i $mp4 -vf "crop=$($Crop):$($Crop):$($x):$($y),fps=10,scale=$($Size):-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=96:stats_mode=diff[p];[b][p]paletteuse=dither=bayer:bayer_scale=4" $gif
        $gif
    }
}
