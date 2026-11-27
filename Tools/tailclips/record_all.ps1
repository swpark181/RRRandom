# Tail chain clips (2026-11-24): records every playable animal friend twice on the Studio map, the tail chain on and
# off (rr.Tail.Chain), with the same moves: walk 5-9 s, sprint back 11-15, a jump on the spot 16.5 and a running jump
# 19.2, five shots standing 22.5-24.1 and four walking 26.5-28. All of it twice: from the side (the tail's up and
# down shows), then 27 s later from above and behind (its side-to-side swing shows). On a clear lane away from the bots
# (they stand at X -1500 and 1500, Y 0 and +-300; the star cars at Y +-1250). cut_tail.py pair <Name> cuts and pairs them.
#   record_all.ps1 -Names Fox,Rabbit,Squirrel
param([string[]]$Names = @("Fox", "Rabbit", "Squirrel", "Tiger", "RedFox", "Dragon"), [string[]]$Sides = @("on", "off"))
$base = "2.5:hudoff;3:RRGod;3.2:goto=-1500,-2800,0;3.4:RRStudioCam 105 300 10;5:walk=1,0,4;11:RRSprint 1;11:walk=-1,0,4;15.2:RRSprint 0;16.5:jump;16.7:unjump;18.5:walk=1,0,2.6;19.2:jump;19.4:unjump;22.5:RRShoot;22.9:RRShoot;23.3:RRShoot;23.7:RRShoot;24.1:RRShoot;26:walk=-1,0,3;26.5:RRShoot;27:RRShoot;27.5:RRShoot;28:RRShoot;30.2:goto=-1500,-2800,0;30.5:RRStudioCam 180 230 330;32:walk=1,0,4;38:RRSprint 1;38:walk=-1,0,4;42.2:RRSprint 0;43.5:jump;43.7:unjump;45.5:walk=1,0,2.6;46.2:jump;46.4:unjump;49.5:RRShoot;49.9:RRShoot;50.3:RRShoot;50.7:RRShoot;51.1:RRShoot;53:walk=-1,0,3;53.5:RRShoot;54:RRShoot;54.5:RRShoot;55:RRShoot;58:quit"
foreach ($n in $Names) {
    foreach ($side in $Sides) {
        $seq = if ($side -eq "off") { "2.6:rr.Tail.Chain 0;" + $base } else { $base }
        & "$PSScriptRoot\rec_tail.ps1" -Species $n -Seq $seq -Tag "${n}_$side" -Seconds 67
    }
}
