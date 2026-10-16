# Launches RRRandom solo, presses E a few times and saves window captures right after each press.
# Usage: powershell -File capture_dice_roll.ps1 -OutDir <folder> [-Rounds 3]
# DPI aware so the whole window is captured; no -log, because its console window would take over the window handle.
param([string]$OutDir, [int]$Rounds = 3)
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class Win {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
}
"@
[Win]::SetProcessDPIAware() | Out-Null
New-Item -ItemType Directory -Force $OutDir | Out-Null
$game = Start-Process "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList '"C:\work\RRRandom\RRRandom.uproject" -game -windowed -ResX=1280 -ResY=720 -RRSolo' -PassThru
$h = [IntPtr]::Zero
for ($i = 0; $i -lt 120 -and $h -eq [IntPtr]::Zero; $i++) { Start-Sleep 1; $p = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -like "RRRandom*" } | Select-Object -First 1; if ($p) { $h = $p.MainWindowHandle } }
# Load, then wait for a few dice to pile up (one per 5 s)
Start-Sleep 18

function Shot($name) {
  $r = New-Object Win+RECT; [Win]::GetWindowRect($h, [ref]$r) | Out-Null
  $bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.Left, $r.Top, 0, 0, $bmp.Size)
  $bmp.Save((Join-Path $OutDir "$name.png")); $g.Dispose(); $bmp.Dispose()
}

for ($round = 1; $round -le $Rounds; $round++) {
  [Win]::SetForegroundWindow($h) | Out-Null
  Start-Sleep -Milliseconds 300
  # E down/up
  [Win]::keybd_event(0x45, 0x12, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 60; [Win]::keybd_event(0x45, 0x12, 2, [UIntPtr]::Zero)
  $t0 = Get-Date
  foreach ($ms in 100, 250, 400, 550, 700, 900, 1300, 1800) {
    $wait = $ms - ((Get-Date) - $t0).TotalMilliseconds
    if ($wait -gt 0) { Start-Sleep -Milliseconds $wait }
    Shot ("r{0}_{1:D4}ms" -f $round, $ms)
  }
  Start-Sleep 2
}
Stop-Process -Id $game.Id
"done"
