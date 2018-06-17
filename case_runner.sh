#!/bin/bash

cases=(
"--TXOP=0 --cwMin=15 --cwMax=1023"
"--TXOP=0 --cwMin=7 --cwMax=15"
"--TXOP=0 --cwMin=3 --cwMax=7"
"--TXOP=0 --cwMin=0 --cwMax=3"
"--TXOP=1504 --cwMin=15 --cwMax=1023"
"--TXOP=1504 --cwMin=7 --cwMax=15"
"--TXOP=1504 --cwMin=3 --cwMax=7"
"--TXOP=1504 --cwMin=0 --cwMax=3"
"--TXOP=3008 --cwMin=15 --cwMax=1023"
"--TXOP=3008 --cwMin=7 --cwMax=15"
"--TXOP=3008 --cwMin=3 --cwMax=7"
"--TXOP=3008 --cwMin=0 --cwMax=3"
"--TXOP=6016 --cwMin=15 --cwMax=1023"
"--TXOP=6016 --cwMin=7 --cwMax=15"
"--TXOP=6016 --cwMin=3 --cwMax=7"
"--TXOP=6016 --cwMin=0 --cwMax=3"
)

for i in "${cases[@]}"
do
	./waf --run "scratch/qosattack802.11e $i" >> wyniki.txt
	echo >> wyniki.txt
	echo >> wyniki.txt
done