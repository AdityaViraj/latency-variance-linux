#!/usr/bin/env bash
set -euo pipefail

OUT="results/run_$(date +%Y%m%d_%H%M%S).txt"
echo "Building..."
make -s clean && make -s

echo "Running 10 trials..."
for i in {1..10}; do
  echo "---- trial $i ----" >> "$OUT"
  ./latency >> "$OUT"
  echo "" >> "$OUT"
done

echo "Saved: $OUT"
