#!/bin/bash
# usage: fur_shots.sh <tag>   (round 99: a close front view of every furred animal friend, to see where its shell fur lies)
TAG=$1
for S in ${FUR_SPECIES:-Fox Rabbit Squirrel Tiger RedFox}; do
  timeout 90 bash /c/work/RRRandom/Saved/Rounds99/seq.sh ${TAG}_$S "7:RRGod;8:frontcam=150,25,58;9.5:Shot;10.2:frontcam=95,10,78;11.5:Shot;12.5:quit" -RRMap=Studio -RRSpecies=$S -RRIdleBots < /dev/null > /c/work/RRRandom/Saved/Rounds99/${TAG}_$S.out 2>&1
done
echo done
