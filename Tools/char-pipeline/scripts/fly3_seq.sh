# Prints the RR_SEQ for a flight check (capture_game_seq.ps1 -Seq, -RRMap=Studio): flight cheat, slow motion 0.25,
# 4 sets of 22 shots 0.4 s apart (0.1 s of game time): rising on a held jump from the side, gliding from the side,
# rising from the front, gliding from behind. Round 85 (the A-pose dragon's wings).
seq="6:RRGod;6.5:showhud;7:goto=0,8000,0;7.5:RRFly;8:RRSlomo 0.25;9:walk=1,0,90"
T=9.5
for set in "jump -90" "unjump -90" "jump 0" "unjump 180"; do
  set -- $set
  seq="$seq;$T:$1;$T:RRStudioCam $2 480 20"
  for i in $(seq 0 21); do seq="$seq;$(awk "BEGIN{print $T+1.5+$i*0.4}"):shot"; done
  T=$(awk "BEGIN{print $T+11}")
done
echo "$seq;$T:quit"
