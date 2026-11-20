# prints the RR_SEQ for a 3-angle walk capture: side, front, back
seq="6:RRGod;6.5:showhud;7:goto=0,8000,0;7.5:RRSlomo 0.2;8:walk=1,0,60"
T=9
for a in -90 0 180; do
  seq="$seq;$T:RRStudioCam $a 380 20"
  for i in $(seq 0 21); do seq="$seq;$(awk "BEGIN{print $T+2+$i*0.4}"):shot"; done
  T=$(awk "BEGIN{print $T+11.5}")
done
echo "$seq;$T:quit"
