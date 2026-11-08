#!/usr/bin/env bash
# Reproduce all results: ./run_all.sh  (writes results/raw.csv)
set -euo pipefail
cd "$(dirname "$0")"
g++ -O2 -march=native -std=c++17 -o bench bench.cpp
mkdir -p results
OUT=results/raw.csv
echo "family,n,m,algo,source,time_ms,relaxations,pq_ops,status,param" > $OUT
K=10; TMO=30; SEED=42
run() { ./bench "$1" "$2" $SEED $K $TMO "$3" | sed "s/\$/,$3/" >> $OUT; echo "done $*" >&2; }
for n in 10000 100000 1000000; do
  run sparse $n 4; run grid $n 0; run scalefree $n 3
done
for n in 1000 2000 4000; do run dense $n 50; done
# density sweep at fixed n
for p in 1 2 5 10 25 50 100; do run dense 2000 $p; done
lscpu | grep -E "Model name|^CPU\(s\)" > results/machine.txt
g++ --version | head -1 >> results/machine.txt
# A* heuristic-strength sweep: grid n=10^6, weights U[wmin,1000], heuristic = wmin * Manhattan
A=results/astar.csv
echo "family,n,m,algo,source,time_ms,relaxations,pq_ops,status,param" > $A
for wmin in 1 100 250 500 750 900; do P2P_ONLY=1 ./bench grid 1000000 $SEED 20 $TMO $wmin | sed "s/\$/,$wmin/" >> $A; done
