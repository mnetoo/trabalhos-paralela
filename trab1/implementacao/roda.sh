#!/bin/bash

if [ $# -lt 3 ]; then
    echo "USAGE: $0 <nElements> <maxThreads> <chunkSize>"
    echo "Exemplo: $0 8000000 8 0"
    exit 1
fi

NELEMENTS=$1
MAXTHREADS=$2
CHUNKSIZE=$3

for i in $(seq 1 $MAXTHREADS)
do
    echo "Executando 10 vezes com $NELEMENTS elementos e $i threads:"
    for j in {1..10}
    do
        ./prefixSumPth $NELEMENTS $i $CHUNKSIZE | grep -oP '(?<=total_time_in_seconds: )[^ ]*'
    done
done
