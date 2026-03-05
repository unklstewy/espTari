#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_008_psg_${TS}.txt"
mkdir -p captures
: > "$OUT"

echo "run=smoke_psg_audio_104" | tee -a "$OUT"
bash ./smoke/smoke_psg_audio_104.sh | tee -a "$OUT"

echo "run=smoke_psg_gpio_105" | tee -a "$OUT"
bash ./smoke/smoke_psg_gpio_105.sh | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
