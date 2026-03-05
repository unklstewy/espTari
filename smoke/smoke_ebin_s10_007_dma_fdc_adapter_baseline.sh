#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_007_dma_fdc_${TS}.txt"
mkdir -p captures
: > "$OUT"

echo "run=smoke_dma_arbitration_102" | tee -a "$OUT"
bash ./smoke/smoke_dma_arbitration_102.sh | tee -a "$OUT"

echo "run=smoke_fdc_fsm_terminal_103" | tee -a "$OUT"
bash ./smoke/smoke_fdc_fsm_terminal_103.sh | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
