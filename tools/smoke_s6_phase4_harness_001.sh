#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s6_phase4_harness_001_smoke_postfix_${TS}.txt"
BUNDLE="captures/s6_phase4_harness_bundle_001_${TS}.json"
MATRIX="TRACKING/evidence/s6_fault_injection_matrix_001.json"
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

assert_health_survives() {
  local scenario="$1"
  call_json "${scenario}_health_probe" GET "/api/v2/engine/health" "" "^200$"
  local ok
  ok="$(extract_json 'str(d["ok"] is True)')"
  if [[ "$ok" != "True" ]]; then
    echo "${scenario}_health_probe=failed ok=$ok" | tee -a "$OUT"
    exit 1
  fi
  echo "${scenario}_health_probe=pass" | tee -a "$OUT"
}

VALIDATE_ABI_MISMATCH='{"module_id":"st.cpu.m68k","module_type":"cpu","machine_targets":["atari_st"],"abi_version":"2.0.0","api_contract_version":"1.0.0","exports":["init"],"dependencies":[],"build_fingerprint":"build_s6_001","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"}'
PROBE_TIMEOUT='{"limit":2,"timeout_ms":1,"mark_dead_after_failures":1}'
PROBE_INVALID='{"timeout_ms":0,"mark_dead_after_failures":1}'

wait_for_health
if [[ ! -f "$MATRIX" ]]; then
  echo "missing_matrix=$MATRIX" | tee -a "$OUT"
  exit 1
fi

call_json "engine_session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "fault_f001_bad_request_validate" POST "/api/v2/ebins/validate" '{"module_type":"cpu"}' "^400$"
F001_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$F001_CODE" != "BAD_REQUEST" ]]; then
  echo "fault_f001_bad_request_validate=failed code=$F001_CODE" | tee -a "$OUT"
  exit 1
fi
echo "fault_f001_bad_request_validate=pass code=$F001_CODE" | tee -a "$OUT"
assert_health_survives "fault_f001_bad_request_validate"

call_json "fault_f002_abi_mismatch_validate" POST "/api/v2/ebins/validate" "$VALIDATE_ABI_MISMATCH" "^409$"
F002_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$F002_CODE" != "EBIN_ABI_MISMATCH" ]]; then
  echo "fault_f002_abi_mismatch_validate=failed code=$F002_CODE" | tee -a "$OUT"
  exit 1
fi
echo "fault_f002_abi_mismatch_validate=pass code=$F002_CODE" | tee -a "$OUT"
assert_health_survives "fault_f002_abi_mismatch_validate"

call_json "fault_f003_probe_timeout_policy" POST "/api/v2/catalogs/floppies/probe-links" "$PROBE_TIMEOUT" "^200$"
F003_WORKER="$(extract_json 'd["data"]["worker_id"]')"
if [[ -z "$F003_WORKER" ]]; then
  echo "fault_f003_probe_timeout_policy=failed worker_id=empty" | tee -a "$OUT"
  exit 1
fi
echo "fault_f003_probe_timeout_policy=pass worker_id=$F003_WORKER" | tee -a "$OUT"
assert_health_survives "fault_f003_probe_timeout_policy"

call_json "fault_f004_invalid_timeout_config" POST "/api/v2/catalogs/floppies/probe-links" "$PROBE_INVALID" "^400$"
F004_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$F004_CODE" != "BAD_REQUEST" ]]; then
  echo "fault_f004_invalid_timeout_config=failed code=$F004_CODE" | tee -a "$OUT"
  exit 1
fi
echo "fault_f004_invalid_timeout_config=pass code=$F004_CODE" | tee -a "$OUT"
assert_health_survives "fault_f004_invalid_timeout_config"

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S6-001",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "fault_matrix": "${MATRIX}",
  "scenarios": [
    {"fault_id":"F001_BAD_REQUEST_VALIDATE","status":"pass","error_code":"BAD_REQUEST"},
    {"fault_id":"F002_ABI_MISMATCH_VALIDATE","status":"pass","error_code":"EBIN_ABI_MISMATCH"},
    {"fault_id":"F003_PROBE_TIMEOUT_POLICY","status":"pass","worker_id":"${F003_WORKER}"},
    {"fault_id":"F004_INVALID_TIMEOUT_CONFIG","status":"pass","error_code":"BAD_REQUEST"}
  ]
}
EOF

echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
