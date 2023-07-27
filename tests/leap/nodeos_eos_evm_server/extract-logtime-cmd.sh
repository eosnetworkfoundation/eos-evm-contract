#!/bin/bash
echo INIT,DECODE,RECOVER,EXECUTE,SAVE,CLEANUP,TOTAL
tail -f $1 | grep --line-buffered -Po "tx: \K\{(.*)\}" | jq -r '[.timelogs[0][1], .timelogs[1][1]-.timelogs[0][1], .timelogs[2][1]-.timelogs[1][1], .timelogs[3][1]-.timelogs[2][1], .timelogs[4][1]-.timelogs[3][1], .elapsed-.timelogs[4][1], .elapsed] | join(",")'
