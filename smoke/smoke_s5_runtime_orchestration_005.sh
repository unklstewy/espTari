#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s5_runtime_orchestration_005_smoke_postfix_${TS}.txt"
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

VALID_LOAD='{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}]}'

wait_for_health

call_json "load_success" POST "/api/v2/ebins/load" "$VALID_LOAD" "^200$"
LOAD_OK="$(extract_json 'str(d["ok"] is True)')"
LOAD_STATUS_OK="$(extract_json 'str(d["data"]["status"]=="loaded")')"
LOAD_TRANS_OK="$(extract_json 'str(d["data"]["transitions"]==["resolve","validate","bind","init"])')"
LOAD_STATE_OK="$(extract_json 'str(d["data"]["runtime_state"]=="running")')"
if [[ "$LOAD_OK" != "True" || "$LOAD_STATUS_OK" != "True" || "$LOAD_TRANS_OK" != "True" || "$LOAD_STATE_OK" != "True" ]]; then
  echo "load_success_contract=failed ok=$LOAD_OK status=$LOAD_STATUS_OK trans=$LOAD_TRANS_OK state=$LOAD_STATE_OK" | tee -a "$OUT"
  exit 1
fi
echo "load_success_contract=pass ok=$LOAD_OK status=$LOAD_STATUS_OK trans=$LOAD_TRANS_OK state=$LOAD_STATE_OK" | tee -a "$OUT"

call_json "unload_success" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"
UNLOAD_OK="$(extract_json 'str(d["ok"] is True)')"
UNLOAD_STATUS_OK="$(extract_json 'str(d["data"]["status"]=="unloaded")')"
UNLOAD_TRANS_OK="$(extract_json 'str(d["data"]["transitions"]==["pause","drain","deinit","release"])')"
UNLOAD_STATE_OK="$(extract_json 'str(d["data"]["runtime_state"]=="idle")')"
if [[ "$UNLOAD_OK" != "True" || "$UNLOAD_STATUS_OK" != "True" || "$UNLOAD_TRANS_OK" != "True" || "$UNLOAD_STATE_OK" != "True" ]]; then
  echo "unload_success_contract=failed ok=$UNLOAD_OK status=$UNLOAD_STATUS_OK trans=$UNLOAD_TRANS_OK state=$UNLOAD_STATE_OK" | tee -a "$OUT"
  exit 1
fi
echo "unload_success_contract=pass ok=$UNLOAD_OK status=$UNLOAD_STATUS_OK trans=$UNLOAD_TRANS_OK state=$UNLOAD_STATE_OK" | tee -a "$OUT"

call_json "load_bind_failure" POST "/api/v2/ebins/load" '{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}],"simulate_fail_stage":"bind"}' "^409$"
BIND_CODE="$(extract_json 'd["error"]["code"]')"
BIND_STAGE="$(extract_json 'd["error"]["details"]["stage"]')"
BIND_REASON="$(extract_json 'd["error"]["details"]["reason"]')"
BIND_STATE="$(extract_json 'd["error"]["details"]["runtime_state"]')"
if [[ "$BIND_CODE" != "EBIN_INVALID" || "$BIND_STAGE" != "bind" || "$BIND_REASON" != "load_bind_failed_simulated" || "$BIND_STATE" != "recoverable_error" ]]; then
  echo "load_bind_failure=failed code=$BIND_CODE stage=$BIND_STAGE reason=$BIND_REASON state=$BIND_STATE" | tee -a "$OUT"
  exit 1
fi
echo "load_bind_failure=pass code=$BIND_CODE stage=$BIND_STAGE reason=$BIND_REASON state=$BIND_STATE" | tee -a "$OUT"

call_json "load_after_bind_failure_success" POST "/api/v2/ebins/load" "$VALID_LOAD" "^200$"
RECOVER_LOAD_STATE_OK="$(extract_json 'str(d["data"]["runtime_state"]=="running")')"
if [[ "$RECOVER_LOAD_STATE_OK" != "True" ]]; then
  echo "load_after_bind_failure_success=failed state=$RECOVER_LOAD_STATE_OK" | tee -a "$OUT"
  exit 1
fi
echo "load_after_bind_failure_success=pass state=$RECOVER_LOAD_STATE_OK" | tee -a "$OUT"

call_json "unload_drain_failure" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k","simulate_fail_stage":"drain"}' "^409$"
DRAIN_CODE="$(extract_json 'd["error"]["code"]')"
DRAIN_STAGE="$(extract_json 'd["error"]["details"]["stage"]')"
DRAIN_REASON="$(extract_json 'd["error"]["details"]["reason"]')"
DRAIN_STATE="$(extract_json 'd["error"]["details"]["runtime_state"]')"
if [[ "$DRAIN_CODE" != "EBIN_INVALID" || "$DRAIN_STAGE" != "drain" || "$DRAIN_REASON" != "unload_drain_failed_simulated" || "$DRAIN_STATE" != "recoverable_error" ]]; then
  echo "unload_drain_failure=failed code=$DRAIN_CODE stage=$DRAIN_STAGE reason=$DRAIN_REASON state=$DRAIN_STATE" | tee -a "$OUT"
  exit 1
fi
echo "unload_drain_failure=pass code=$DRAIN_CODE stage=$DRAIN_STAGE reason=$DRAIN_REASON state=$DRAIN_STATE" | tee -a "$OUT"

call_json "unload_after_drain_failure_success" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"
RECOVER_UNLOAD_STATE_OK="$(extract_json 'str(d["data"]["runtime_state"]=="idle")')"
if [[ "$RECOVER_UNLOAD_STATE_OK" != "True" ]]; then
  echo "unload_after_drain_failure_success=failed state=$RECOVER_UNLOAD_STATE_OK" | tee -a "$OUT"
  exit 1
fi
echo "unload_after_drain_failure_success=pass state=$RECOVER_UNLOAD_STATE_OK" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
