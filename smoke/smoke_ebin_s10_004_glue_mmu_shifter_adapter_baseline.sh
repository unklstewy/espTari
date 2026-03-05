#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_004_glue_mmu_shifter_${TS}.txt"
mkdir -p captures
: > "$OUT"

echo "run=smoke_chipset_windows_096" | tee -a "$OUT"
bash ./smoke/smoke_chipset_windows_096.sh | tee -a "$OUT"

echo "run=smoke_chipset_timing_097" | tee -a "$OUT"
bash ./smoke/smoke_chipset_timing_097.sh | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
