# Runs RRRandom windowed (1280x720) with ue_game_seq.py driving it and copies the new HighResShots to <OutDir>\<Tag>_<n>.png.
# Round 71 (riding checks). Example, one animal friend on the three map vehicles, enemies standing still:
#   capture_game_seq.ps1 -Species Rabbit -Tag ride_Rabbit -Extra "-RRIdleEnemies" -Seq "6:RRGod;8:nearmesh=SM_Car;9:RRRide;
#     10:goto=-1530,0,0;12.5:RRStudioCam -90 380 20;14:shot;14.5:RRStudioCamOff;15:RRRide;17:nearmesh=SM_Motorbike;..."
# The star cars: -Extra "-RRMap=Studio" (no obstacles, only the star cars). Then shot_sheet.ps1 tiles the shots.
param(
    [string]$Species,
    [string]$Seq,
    [string]$Tag,
    [string]$Extra = "",
    [int]$Timeout = 120,
    [string]$OutDir = "C:\work\char-pipeline\output\screens\ride"
)
$Shots = "C:\work\RRRandom\Saved\Screenshots\WindowsEditor"
New-Item -ItemType Directory -Force $OutDir | Out-Null
New-Item -ItemType Directory -Force $Shots | Out-Null
$before = @(Get-ChildItem $Shots | ForEach-Object { $_.Name })
$env:RR_SEQ = $Seq
$script = Join-Path $PSScriptRoot "ue_game_seq.py"
$p = Start-Process -FilePath "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList "`"C:\work\RRRandom\RRRandom.uproject`" -game -windowed -ResX=1280 -ResY=720 -nosound -RRSolo $Extra -RRSpecies=$Species -log=cap_$Tag.log -ExecCmds=`"py $script`"" -PassThru
$p.WaitForExit($Timeout * 1000) | Out-Null
if (!$p.HasExited) { Stop-Process -Id $p.Id -Force }
$n = 0
Get-ChildItem $Shots | Where-Object { $before -notcontains $_.Name } | Sort-Object LastWriteTime | ForEach-Object {
    Copy-Item $_.FullName "$OutDir\${Tag}_$n.png"; "$OutDir\${Tag}_$n.png"; $n++
}
Select-String -Path "C:\work\RRRandom\Saved\Logs\cap_$Tag.log" -Pattern "Traceback|GAMESEQ near" | ForEach-Object { $_.Line }
