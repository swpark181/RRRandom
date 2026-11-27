# Tail chain clips (2026-11-24): runs RRRandom on the Studio map (third person, bots standing still) in a 1920x1080
# window driven by r88\video_seq.py (RR_SEQ, game seconds) and records the window's inside in real time with ffmpeg
# (gdigrab) to clips\<Tag>.mp4. clips\<Tag>.txt gets the moment the recording began and the GAMESEQ lines (with their
# UTC times), so cut_tail.py can cut the clip on the game's events. From r88\rec_cap.ps1.
param(
    [string]$Species = "Fox",
    [string]$Seq,
    [string]$Tag,
    [string]$Extra = "-RRIdleEnemies -RRIdleBots",
    [int]$Fps = 60,
    [int]$Seconds = 40,
    [string]$Map = "Studio"
)
$V = "C:\work\char-pipeline\output\video\tail"
$SeqPy = "C:\work\char-pipeline\output\video\r88\video_seq.py"
$Ff = "C:\work\char-pipeline\tools\ffmpeg\ffmpeg.exe"
New-Item -ItemType Directory -Force "$V\clips" | Out-Null
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X; public int Y; }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L; public int T; public int R; public int B; }
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
}
"@
[Win]::SetProcessDPIAware() | Out-Null
$env:RR_SEQ = $Seq
$log = "tail_$Tag.log"
$argList = "`"C:\work\RRRandom\RRRandom.uproject`" -game -windowed -ResX=1280 -ResY=720 -ForceRes -WinX=0 -WinY=0 -nosound -RRSolo -RRThirdPerson -RRMap=$Map -RRSpecies=$Species -RRNoOnlinePanel $Extra -log=$log -ExecCmds=`"py $SeqPy`""
$p = Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $argList -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 120; $i++) {
    Start-Sleep -Milliseconds 250
    $p.Refresh()
    $h = $p.MainWindowHandle
    if ($h -ne [IntPtr]::Zero) {
        $r = New-Object Win+RECT
        [Win]::GetClientRect($h, [ref]$r) | Out-Null
        if (($r.R - $r.L) -ge 1900) { break }
    }
}
[Win]::SetForegroundWindow($h) | Out-Null
# (2026-11-24: a browser window in use stayed in front of the game and three takes recorded it instead. The game
# window is kept above everything while it records: no move, no resize, no activation.)
[Win]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x13) | Out-Null
$pt = New-Object Win+POINT
[Win]::ClientToScreen($h, [ref]$pt) | Out-Null
$out = "$V\clips\$Tag.mp4"
$began = (Get-Date).ToUniversalTime()
& $Ff -hide_banner -loglevel error -y -f gdigrab -framerate $Fps -draw_mouse 0 -offset_x $pt.X -offset_y $pt.Y -video_size 1920x1080 -i desktop -t $Seconds -c:v h264_nvenc -preset p5 -cq 24 -pix_fmt yuv420p $out
$p.WaitForExit(30000) | Out-Null
if (!$p.HasExited) { Stop-Process -Id $p.Id -Force }
$lines = @("began " + $began.ToString("HH.mm.ss.fff"))
$lines += Select-String -Path "C:\work\RRRandom\Saved\Logs\$log" -Pattern "GAMESEQ|Traceback|4551|has a tail of" | ForEach-Object { $_.Line }
$lines | Set-Content -Encoding utf8 "$V\clips\$Tag.txt"
"$Tag : client at $($pt.X),$($pt.Y) size $($r.R - $r.L)x$($r.B - $r.T); $($lines.Count) lines; " + (($lines | Select-String "Traceback|4551").Count) + " problems"
