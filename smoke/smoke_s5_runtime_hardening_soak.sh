#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SOAK_LOOPS="${SOAK_LOOPS:-3}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s5_runtime_hardening_soak_${TS}.txt"
mkdir -p captures
: > "$OUT"

echo "base_url=${BASE_URL}" | tee -a "$OUT"
echo "soak_loops=${SOAK_LOOPS}" | tee -a "$OUT"

pass_count=0
fail_count=0

run_case() {
  local loop="$1"
  local case_id="$2"
  local script="$3"

  echo "### loop_${loop}_${case_id}" | tee -a "$OUT"
  if BASE_URL="$BASE_URL" bash "$script" >> "$OUT" 2>&1; then
    echo "loop_${loop}_${case_id}=pass" | tee -a "$OUT"
    pass_count=$((pass_count+1))
  else
    echo "loop_${loop}_${case_id}=fail" | tee -a "$OUT"
    fail_count=$((fail_count+1))
  fi
}

for loop in $(seq 1 "$SOAK_LOOPS"); do
  run_case "$loop" "startup" "./smoke/smoke_ebin_s10_011_startup_integration.sh"
  run_case "$loop" "suspend_restore" "./smoke/smoke_ebin_s10_012_suspend_restore_integration.sh"
  run_case "$loop" "conformance" "./smoke/smoke_ebin_s10_013_conformance_integration.sh"
done

echo "summary_pass=${pass_count}" | tee -a "$OUT"
echo "summary_fail=${fail_count}" | tee -a "$OUT"

if [[ "$fail_count" -eq 0 ]]; then
  echo "soak_decision=pass" | tee -a "$OUT"
  echo "Smoke PASS"
  echo "Evidence: $OUT"
  exit 0
fi

echo "soak_decision=fail" | tee -a "$OUT"
echo "Smoke FAIL"
echo "Evidence: $OUT"
exit 1
