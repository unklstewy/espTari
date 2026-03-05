#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
AUTH_BEARER="${AUTH_BEARER:-}"
AUTH_HEADER="${AUTH_HEADER:-}"
AUTH_CLIENT_ID="${AUTH_CLIENT_ID:-esptari-smoke}"
AUTH_CLIENT_SECRET="${AUTH_CLIENT_SECRET:-esptari-smoke-secret}"
AUTH_SCOPE="${AUTH_SCOPE:-engine:control inspect:read ebin:manage}"
PYTHON_BIN="${PYTHON_BIN:-python3}"
RERUN_PHASE_A=0
MANUAL_CONFIRM=0
STRICT_CONTRACT_DRIFT=0

for arg in "$@"; do
  case "$arg" in
    --rerun-phase-a)
      RERUN_PHASE_A=1
      ;;
    --manual-confirm)
      MANUAL_CONFIRM=1
      ;;
    --strict-contract-drift)
      STRICT_CONTRACT_DRIFT=1
      ;;
    *)
      echo "unknown_arg=$arg"
      echo "usage: $0 [--rerun-phase-a] [--manual-confirm] [--strict-contract-drift]"
      exit 2
      ;;
  esac
done

TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_009_gate_check_${TS}.txt"
mkdir -p captures
: > "$OUT"

log() {
  echo "$*" | tee -a "$OUT"
}

mint_auth_if_needed() {
  if [[ -n "$AUTH_HEADER" ]]; then
    return 0
  fi

  if [[ -n "$AUTH_BEARER" ]]; then
    AUTH_HEADER="Authorization: Bearer ${AUTH_BEARER}"
    return 0
  fi

  local mint_resp minted
  mint_resp="$(curl --max-time 15 -sS -X POST "${BASE_URL}/api/v2/auth/token" \
    -H "Content-Type: application/json" \
    -d "{\"grant_type\":\"client_credentials\",\"client_id\":\"${AUTH_CLIENT_ID}\",\"client_secret\":\"${AUTH_CLIENT_SECRET}\",\"scope\":\"${AUTH_SCOPE}\"}" || true)"

  minted="$(printf '%s' "$mint_resp" | "$PYTHON_BIN" -c 'import json,sys
raw=sys.stdin.read().strip()
token=""
try:
  d=json.loads(raw) if raw else {}
  token=d.get("data",{}).get("access_token","") if d.get("ok") else ""
except Exception:
  token=""
print(token)')"

  if [[ -n "$minted" ]]; then
    AUTH_BEARER="$minted"
    AUTH_HEADER="Authorization: Bearer ${AUTH_BEARER}"
  fi
}

find_latest_two() {
  local pattern="$1"
  ls -1 $pattern 2>/dev/null | tail -n 2 || true
}

file_has_smoke_pass() {
  local f="$1"
  grep -q '^Smoke PASS$' "$f"
}

normalized_signature() {
  local f="$1"
  grep -E '(^run=|=pass($| )|^determinism_check=pass$|^summary_pass=|^summary_fail=)' "$f" \
    | sed -E 's/[[:space:]].*$//' \
    | sed '/^$/d' \
    | sort -u
}

check_phase_a_task() {
  local task_id="$1"
  local pattern="$2"
  local ok=1

  mapfile -t files < <(find_latest_two "$pattern")
  if (( ${#files[@]} < 2 )); then
    log "${task_id}.evidence=fail reason=need_two_captures pattern=${pattern}"
    return 1
  fi

  local f1="${files[0]}"
  local f2="${files[1]}"
  if [[ ! -r "$f1" || ! -r "$f2" ]]; then
    log "${task_id}.evidence=fail reason=unreadable_capture first=${f1} second=${f2}"
    return 1
  fi

  if ! file_has_smoke_pass "$f1" || ! file_has_smoke_pass "$f2"; then
    log "${task_id}.evidence=fail reason=missing_smoke_pass first=${f1} second=${f2}"
    return 1
  fi

  local sig1 sig2
  sig1="$(normalized_signature "$f1")"
  sig2="$(normalized_signature "$f2")"

  if [[ "$sig1" != "$sig2" ]]; then
    log "${task_id}.determinism=fail reason=signature_mismatch first=${f1} second=${f2}"
    ok=0
  else
    log "${task_id}.determinism=pass first=${f1} second=${f2}"
  fi

  if (( ok == 1 )); then
    log "${task_id}.evidence=pass"
    return 0
  fi

  return 1
}

check_recovery_safety() {
  local latest
  latest="$(ls -1 captures/ebin_s10_003_rollback_fallback_lock_*.txt 2>/dev/null | tail -n 1 || true)"
  if [[ -z "$latest" ]]; then
    log "recovery_safety=fail reason=missing_s10_003_capture"
    return 1
  fi

  local rollback_count fallback_count
  rollback_count="$(grep -c '^rollback_check=pass' "$latest" || true)"
  fallback_count="$(grep -c '^fallback_check=pass' "$latest" || true)"

  if ! grep -q '^determinism_check=pass$' "$latest"; then
    log "recovery_safety=fail reason=determinism_check_missing capture=${latest}"
    return 1
  fi

  if (( rollback_count < 1 || fallback_count < 1 )); then
    log "recovery_safety=fail reason=rollback_or_fallback_missing capture=${latest} rollback_count=${rollback_count} fallback_count=${fallback_count}"
    return 1
  fi

  log "recovery_safety=pass capture=${latest} rollback_count=${rollback_count} fallback_count=${fallback_count}"
  return 0
}

check_contract_drift() {
  local drift=0
  local files=(
    "docs/emu_engine_v2/EBIN_RUNTIME_ABI_V1.md"
    "docs/EMU_ENGINE_V2_API_SPEC.md"
    "docs/EMU_ENGINE_V2_IMPLEMENTATION_PLAN.md"
    "docs/emu_engine_v2/ebin_arch_pack/EBIN-INTF-001_INTERFACE_CONTRACT_MATRIX.md"
  )

  for f in "${files[@]}"; do
    if git diff --quiet -- "$f"; then
      :
    else
      log "contract_drift_detected=file:${f}"
      drift=1
    fi
  done

  if (( drift == 0 )); then
    log "contract_drift=pass"
    return 0
  fi

  if (( STRICT_CONTRACT_DRIFT == 1 )); then
    log "contract_drift=fail"
    return 1
  fi

  log "contract_drift=warn mode=non_strict"
  return 0
}

check_traceability_rows() {
  local plan_file="TRACKING/EBIN_S10_PHASE_A_EVIDENCE_PLAN_2026-03-04.md"
  if [[ ! -r "$plan_file" ]]; then
    log "traceability=fail reason=missing_plan file=${plan_file}"
    return 1
  fi

  local ids=(EBIN-S10-004 EBIN-S10-005 EBIN-S10-006 EBIN-S10-007 EBIN-S10-008)
  local missing=0
  local id
  for id in "${ids[@]}"; do
    if grep -q "${id}" "$plan_file"; then
      log "traceability_row=pass id=${id}"
    else
      log "traceability_row=fail id=${id}"
      missing=1
    fi
  done

  if (( missing == 0 )); then
    log "traceability=pass"
    return 0
  fi

  log "traceability=fail"
  return 1
}

rerun_phase_a_if_requested() {
  if (( RERUN_PHASE_A == 0 )); then
    return 0
  fi

  mint_auth_if_needed
  if [[ -z "$AUTH_HEADER" ]]; then
    log "rerun=fail reason=missing_auth_header"
    return 1
  fi

  log "rerun=begin scope=A-04..A-08"
  AUTH_HEADER="$AUTH_HEADER" BASE_URL="$BASE_URL" bash ./smoke/smoke_ebin_s10_004_glue_mmu_shifter_adapter_baseline.sh | tee -a "$OUT"
  AUTH_HEADER="$AUTH_HEADER" BASE_URL="$BASE_URL" bash ./smoke/smoke_ebin_s10_005_mfp_adapter_baseline.sh | tee -a "$OUT"
  AUTH_HEADER="$AUTH_HEADER" BASE_URL="$BASE_URL" bash ./smoke/smoke_ebin_s10_006_acia_ikbd_adapter_baseline.sh | tee -a "$OUT"
  AUTH_HEADER="$AUTH_HEADER" BASE_URL="$BASE_URL" bash ./smoke/smoke_ebin_s10_007_dma_fdc_adapter_baseline.sh | tee -a "$OUT"
  AUTH_HEADER="$AUTH_HEADER" BASE_URL="$BASE_URL" bash ./smoke/smoke_ebin_s10_008_psg_adapter_baseline.sh | tee -a "$OUT"
  log "rerun=end"
}

main() {
  local failures=0

  log "gate_check=EBIN-S10-008-to-009"
  log "base_url=${BASE_URL}"
  log "report=${OUT}"

  rerun_phase_a_if_requested || failures=$((failures+1))

  check_phase_a_task "EBIN-S10-004" "captures/ebin_s10_004_glue_mmu_shifter_*.txt" || failures=$((failures+1))
  check_phase_a_task "EBIN-S10-005" "captures/ebin_s10_005_mfp_*.txt" || failures=$((failures+1))
  check_phase_a_task "EBIN-S10-006" "captures/ebin_s10_006_acia_ikbd_*.txt" || failures=$((failures+1))
  check_phase_a_task "EBIN-S10-007" "captures/ebin_s10_007_dma_fdc_*.txt" || failures=$((failures+1))
  check_phase_a_task "EBIN-S10-008" "captures/ebin_s10_008_psg_*.txt" || failures=$((failures+1))

  check_recovery_safety || failures=$((failures+1))
  check_contract_drift || failures=$((failures+1))
  check_traceability_rows || failures=$((failures+1))

  if (( MANUAL_CONFIRM == 0 )); then
    log "manual_confirmation=required flag=--manual-confirm"
    failures=$((failures+1))
  else
    log "manual_confirmation=pass"
  fi

  if (( failures == 0 )); then
    log "decision=go"
    log "scope=EBIN-S10-009 start eligibility only"
    log "summary_failures=0"
    exit 0
  fi

  log "decision=non_go"
  log "scope=EBIN-S10-009 start eligibility only"
  log "summary_failures=${failures}"
  exit 1
}

main
