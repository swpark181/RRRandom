# Round 92: a LAN pair on this PC (a host and a joiner, 960x540 side by side), each driven by ue_game_seq.py with its own
# sequence, its own log (cap_<Tag>_host.log / cap_<Tag>_join.log), HighResShots copied to <OutDir>\<Tag>_<n>.png.
# Example (the battle royale's test map):
#   lan_pair_seq.ps1 -Tag br_lan -HostExtra "-RRBRTest" -HostSeq "60:shot;100:quit" -JoinSeq "55:shot;95:quit"
param(
    [string]$Tag,
    [string]$HostExtra = "",
    [string]$JoinExtra = "",
    [string]$HostSeq,
    [string]$JoinSeq,
    [int]$JoinDelay = 15,
    [int]$Timeout = 180,
    [string]$OutDir = "C:\work\char-pipeline\output\screens\br92"
)
$ue = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$proj = "C:\work\RRRandom\RRRandom.uproject"
$Shots = "C:\work\RRRandom\Saved\Screenshots\WindowsEditor"
New-Item -ItemType Directory -Force $OutDir | Out-Null
New-Item -ItemType Directory -Force $Shots | Out-Null
$before = @(Get-ChildItem $Shots | ForEach-Object { $_.Name })
$script = Join-Path $PSScriptRoot "ue_game_seq.py"
$env:RR_SEQ = $HostSeq
$env:RR_NETWORLD = ""
$h = Start-Process -FilePath $ue -ArgumentList "`"$proj`" -game -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=40 -nosound -RRLan -RRHost $HostExtra -log=cap_${Tag}_host.log -ExecCmds=`"py $script`"" -PassThru
Start-Sleep -Seconds $JoinDelay
$env:RR_SEQ = $JoinSeq
$env:RR_NETWORLD = "1"
$j = Start-Process -FilePath $ue -ArgumentList "`"$proj`" -game -windowed -ResX=960 -ResY=540 -WinX=960 -WinY=40 -nosound -RRLan -RRJoin $JoinExtra -log=cap_${Tag}_join.log -ExecCmds=`"py $script`"" -PassThru
$h.WaitForExit($Timeout * 1000) | Out-Null
$j.WaitForExit(20000) | Out-Null
foreach ($p in @($h, $j)) { if (!$p.HasExited) { Stop-Process -Id $p.Id -Force } }
$n = 0
Get-ChildItem $Shots | Where-Object { $before -notcontains $_.Name } | Sort-Object LastWriteTime | ForEach-Object {
    Copy-Item $_.FullName "$OutDir\${Tag}_$n.png"; "$OutDir\${Tag}_$n.png"; $n++
}
