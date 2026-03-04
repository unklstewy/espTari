#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_005_mfp_${TS}.txt"
mkdir -p captures
: > "$OUT"

echo "run=smoke_mfp_windows_098" | tee -a "$OUT"
bash ./tools/smoke_mfp_windows_098.sh | tee -a "$OUT"

echo "run=smoke_mfp_irq_099" | tee -a "$OUT"
bash ./tools/smoke_mfp_irq_099.sh | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
