#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_006_acia_ikbd_${TS}.txt"
mkdir -p captures
: > "$OUT"

echo "run=smoke_acia_framing_100" | tee -a "$OUT"
bash ./tools/smoke_acia_framing_100.sh | tee -a "$OUT"

echo "run=smoke_ikbd_timing_101" | tee -a "$OUT"
bash ./tools/smoke_ikbd_timing_101.sh | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
