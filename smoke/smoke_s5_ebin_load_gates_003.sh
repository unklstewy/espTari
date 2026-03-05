#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s5_ebin_load_gates_003_smoke_postfix_${TS}.txt"
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

VALID='{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}]}'

wait_for_health
call_json "ebin_load_success" POST "/api/v2/ebins/load" "$VALID" "^200$"
OK_OK="$(extract_json 'str(d["ok"] is True)')"
STATUS_OK="$(extract_json 'str(d["data"]["status"]=="loaded")')"
GATES_OK="$(extract_json 'str(d["data"]["gates"]==["integrity","signature","dependency_compatibility"])')"
if [[ "$OK_OK" != "True" || "$STATUS_OK" != "True" || "$GATES_OK" != "True" ]]; then
  echo "ebin_load_success_contract=failed ok=$OK_OK status=$STATUS_OK gates=$GATES_OK" | tee -a "$OUT"
  exit 1
fi
echo "ebin_load_success_contract=pass ok=$OK_OK status=$STATUS_OK gates=$GATES_OK" | tee -a "$OUT"

call_json "ebin_load_integrity_fail" POST "/api/v2/ebins/load" '{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"bad","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}]}' "^400$"
INT_CODE="$(extract_json 'd["error"]["code"]')"
INT_REASON="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$INT_CODE" != "EBIN_INVALID" || "$INT_REASON" != "integrity_gate_failed_invalid_hash" ]]; then
  echo "ebin_load_integrity_fail=failed code=$INT_CODE reason=$INT_REASON" | tee -a "$OUT"
  exit 1
fi
echo "ebin_load_integrity_fail=pass code=$INT_CODE reason=$INT_REASON" | tee -a "$OUT"

call_json "ebin_load_signature_fail" POST "/api/v2/ebins/load" '{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}]}' "^409$"
SIG_CODE="$(extract_json 'd["error"]["code"]')"
SIG_REASON="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$SIG_CODE" != "EBIN_SIGNATURE_INVALID" || "$SIG_REASON" != "signature_gate_failed_missing_signature_object" ]]; then
  echo "ebin_load_signature_fail=failed code=$SIG_CODE reason=$SIG_REASON" | tee -a "$OUT"
  exit 1
fi
echo "ebin_load_signature_fail=pass code=$SIG_CODE reason=$SIG_REASON" | tee -a "$OUT"

call_json "ebin_load_dependency_missing" POST "/api/v2/ebins/load" '{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.unknown","abi_range":"1.0.x","required":true}]}' "^409$"
DEP_CODE="$(extract_json 'd["error"]["code"]')"
DEP_REASON="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$DEP_CODE" != "EBIN_DEPENDENCY_MISSING" || "$DEP_REASON" != "dependency_gate_failed_required_dependency_missing" ]]; then
  echo "ebin_load_dependency_missing=failed code=$DEP_CODE reason=$DEP_REASON" | tee -a "$OUT"
  exit 1
fi
echo "ebin_load_dependency_missing=pass code=$DEP_CODE reason=$DEP_REASON" | tee -a "$OUT"

call_json "ebin_load_dependency_abi_fail" POST "/api/v2/ebins/load" '{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.shifter","abi_range":"2.0.x","required":true}]}' "^409$"
DABI_CODE="$(extract_json 'd["error"]["code"]')"
DABI_REASON="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$DABI_CODE" != "EBIN_ABI_MISMATCH" || "$DABI_REASON" != "dependency_gate_failed_dependency_abi_range_unsupported" ]]; then
  echo "ebin_load_dependency_abi_fail=failed code=$DABI_CODE reason=$DABI_REASON" | tee -a "$OUT"
  exit 1
fi
echo "ebin_load_dependency_abi_fail=pass code=$DABI_CODE reason=$DABI_REASON" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
