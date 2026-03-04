#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/chipset_windows_096_smoke_postfix_${TS}.txt"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

wait_for_health() {
  local attempts="${1:-60}"
  local interval="${2:-1}"
  local i
  for ((i=1; i<=attempts; i++)); do
    if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
      echo "health_check=ok attempts=${i}" | tee -a "$OUT"
      return 0
    fi
    sleep "$interval"
  done
  echo "health_check=timeout attempts=${attempts}" | tee -a "$OUT"
  exit 1
}

call_json() {
  local name="$1" method="$2" path="$3" data="$4" expected="$5"
  local body_file code body
  body_file="$(mktemp)"
  if [[ -n "$data" ]]; then
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path" -H "Content-Type: application/json" -d "$data")
  else
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path")
  fi
  body="$(cat "$body_file")"
  rm -f "$body_file"

  {
    echo "### $name"
    echo "$method $path"
    echo "HTTP $code"
    echo "$body"
    echo
  } >> "$OUT"

  if [[ ! "$code" =~ $expected ]]; then
    echo "Step failed: $name HTTP=$code" | tee -a "$OUT"
    echo "$body" | tee -a "$OUT"
    exit 1
  fi

  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

wait_for_health
call_json "session_reset" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
call_json "session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "register_windows_all_groups" GET "/api/v2/inspect/chipset/windows/registers?session_id=ses_local&group=glue,mmu,shifter" "" "^200$"
WIN_COUNT="$(extract_json 'len(d["data"]["windows"])')"
HAS_SHIFTER="$(extract_json 'str(any(w["group"]=="shifter" for w in d["data"]["windows"]))')"
FIRST_REG_REQUIRED="$(extract_json 'str(all(k in d["data"]["windows"][0]["registers"][0] for k in ["name","offset","address","width_bits","access"]))')"
if [[ "$WIN_COUNT" != "3" || "$HAS_SHIFTER" != "True" || "$FIRST_REG_REQUIRED" != "True" ]]; then
  echo "register_window_contract=failed windows=$WIN_COUNT has_shifter=$HAS_SHIFTER reg_shape=$FIRST_REG_REQUIRED" | tee -a "$OUT"
  exit 1
fi
echo "register_window_contract=pass windows=$WIN_COUNT has_shifter=$HAS_SHIFTER reg_shape=$FIRST_REG_REQUIRED" | tee -a "$OUT"

call_json "memory_windows_subset" GET "/api/v2/inspect/chipset/windows/memory?session_id=ses_local&group=mmu,shifter" "" "^200$"
MEM_COUNT="$(extract_json 'len(d["data"]["windows"])')"
HAS_MMU_LINEAR="$(extract_json 'str(any(w["group"]=="mmu" and w["addressing_mode"]=="linear" for w in d["data"]["windows"]))')"
HAS_SHIFTER_BANKED="$(extract_json 'str(any(w["group"]=="shifter" and w["addressing_mode"]=="banked" for w in d["data"]["windows"]))')"
if [[ "$MEM_COUNT" != "2" || "$HAS_MMU_LINEAR" != "True" || "$HAS_SHIFTER_BANKED" != "True" ]]; then
  echo "memory_window_contract=failed windows=$MEM_COUNT mmu_linear=$HAS_MMU_LINEAR shifter_banked=$HAS_SHIFTER_BANKED" | tee -a "$OUT"
  exit 1
fi
echo "memory_window_contract=pass windows=$MEM_COUNT mmu_linear=$HAS_MMU_LINEAR shifter_banked=$HAS_SHIFTER_BANKED" | tee -a "$OUT"

call_json "invalid_group_bad_request" GET "/api/v2/inspect/chipset/windows/registers?session_id=ses_local&group=glue,invalid" "" "^400$"
BAD_REQUEST_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_REQUEST_CODE" != "BAD_REQUEST" ]]; then
  echo "chipset_window_bad_request_mapping=failed code=$BAD_REQUEST_CODE" | tee -a "$OUT"
  exit 1
fi
echo "chipset_window_bad_request_mapping=pass code=$BAD_REQUEST_CODE" | tee -a "$OUT"

call_json "unknown_session_engine_not_running" GET "/api/v2/inspect/chipset/windows/memory?session_id=ses_unknown&group=mmu" "" "^409$"
ENGINE_NOT_RUNNING_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ENGINE_NOT_RUNNING_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "chipset_window_engine_not_running_mapping=failed code=$ENGINE_NOT_RUNNING_CODE" | tee -a "$OUT"
  exit 1
fi
echo "chipset_window_engine_not_running_mapping=pass code=$ENGINE_NOT_RUNNING_CODE" | tee -a "$OUT"

call_json "force_unresolved_internal_error" GET "/api/v2/inspect/chipset/windows/memory?session_id=ses_local&group=mmu&force_unresolved_group=mmu" "" "^500$"
INTERNAL_ERROR_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$INTERNAL_ERROR_CODE" != "INTERNAL_ERROR" ]]; then
  echo "chipset_window_internal_error_mapping=failed code=$INTERNAL_ERROR_CODE" | tee -a "$OUT"
  exit 1
fi
echo "chipset_window_internal_error_mapping=pass code=$INTERNAL_ERROR_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"