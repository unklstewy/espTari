#!/usr/bin/env bash
set -euo pipefail

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_phase_a_${TS}.txt"
mkdir -p captures
: > "$OUT"

run_step() {
  local id="$1"
  local script="$2"
  echo "=== ${id} ${script}" | tee -a "$OUT"
  bash "$script" | tee -a "$OUT"
}

run_step "EBIN-S10-001" "./smoke/smoke_ebin_s10_001_admission_lock.sh"
run_step "EBIN-S10-002" "./smoke/smoke_ebin_s10_002_resolver_determinism_lock.sh"
run_step "EBIN-S10-003" "./smoke/smoke_ebin_s10_003_rollback_fallback_lock.sh"
run_step "EBIN-S10-004" "./smoke/smoke_ebin_s10_004_glue_mmu_shifter_adapter_baseline.sh"
run_step "EBIN-S10-005" "./smoke/smoke_ebin_s10_005_mfp_adapter_baseline.sh"
run_step "EBIN-S10-006" "./smoke/smoke_ebin_s10_006_acia_ikbd_adapter_baseline.sh"
run_step "EBIN-S10-007" "./smoke/smoke_ebin_s10_007_dma_fdc_adapter_baseline.sh"
run_step "EBIN-S10-008" "./smoke/smoke_ebin_s10_008_psg_adapter_baseline.sh"

echo "Smoke PASS"
echo "Evidence: $OUT"
