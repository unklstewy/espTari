#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s5_runtime_unlock_harness_007_smoke_postfix_${TS}.txt"
BUNDLE="captures/s5_runtime_unlock_bundle_007_${TS}.json"
REF_MANIFEST="docs/emu_engine_v2/reference_ebin_packages/st_cpu_m68k_reference_manifest_v1.json"
REF_METADATA="docs/emu_engine_v2/reference_ebin_packages/st_cpu_m68k_reference_metadata_v1.json"
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

ensure_reference_package() {
  if [[ ! -f "$REF_MANIFEST" ]]; then
    echo "missing_reference_manifest=$REF_MANIFEST" | tee -a "$OUT"
    exit 1
  fi
  if [[ ! -f "$REF_METADATA" ]]; then
    echo "missing_reference_metadata=$REF_METADATA" | tee -a "$OUT"
    exit 1
  fi

  /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python - <<'PY' "$REF_MANIFEST" "$REF_METADATA" >> "$OUT"
import json, sys
manifest_path, metadata_path = sys.argv[1], sys.argv[2]
with open(manifest_path, 'r', encoding='utf-8') as f:
    m = json.load(f)
with open(metadata_path, 'r', encoding='utf-8') as f:
    md = json.load(f)
assert m['module_id'] == 'st.cpu.m68k'
assert m['abi_version'] == '1.0.0'
assert md['module_selector'] == 'st.cpu.m68k@1.0.0'
print(f"reference_manifest_check=pass package_id={m['package_id']} module={m['module_id']}")
print(f"reference_metadata_check=pass selector={md['module_selector']}")
PY
}

CPU_LOAD='{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}]}'
VIDEO_BASE='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[]}'
VIDEO_BIND_FAIL='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[],"simulate_fail_stage":"bind"}'
VIDEO_INIT_FAIL_FALLBACK='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[],"simulate_fail_stage":"init","force_fallback":true}'

wait_for_health
ensure_reference_package

call_json "scenario_load_unload_success_load" POST "/api/v2/ebins/load" "$CPU_LOAD" "^200$"
S1A="$(extract_json 'str(d["data"]["runtime_state"]=="running")')"
call_json "scenario_load_unload_success_unload" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"
S1B="$(extract_json 'str(d["data"]["runtime_state"]=="idle")')"
if [[ "$S1A" != "True" || "$S1B" != "True" ]]; then
  echo "scenario_load_unload_success=failed load=$S1A unload=$S1B" | tee -a "$OUT"
  exit 1
fi
echo "scenario_load_unload_success=pass" | tee -a "$OUT"

call_json "scenario_rollback_seed_load" POST "/api/v2/ebins/load" "$CPU_LOAD" "^200$"
call_json "scenario_rollback_probe" POST "/api/v2/ebins/load" "$VIDEO_BIND_FAIL" "^409$"
S2_ACTION="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovery_action"]')"
S2_REC="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovered_module_id"]')"
if [[ "$S2_ACTION" != "rollback_previous_active_module" || "$S2_REC" != "st.cpu.m68k" ]]; then
  echo "scenario_rollback_probe=failed action=$S2_ACTION recovered=$S2_REC" | tee -a "$OUT"
  exit 1
fi
echo "scenario_rollback_probe=pass action=$S2_ACTION recovered=$S2_REC" | tee -a "$OUT"
call_json "scenario_rollback_cleanup" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"

call_json "scenario_fallback_probe" POST "/api/v2/ebins/load" "$VIDEO_INIT_FAIL_FALLBACK" "^409$"
S3_ACTION="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovery_action"]')"
S3_REC="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovered_module_id"]')"
if [[ "$S3_ACTION" != "fallback_module_set_activated" || "$S3_REC" != "st.cpu.m68k" ]]; then
  echo "scenario_fallback_probe=failed action=$S3_ACTION recovered=$S3_REC" | tee -a "$OUT"
  exit 1
fi
echo "scenario_fallback_probe=pass action=$S3_ACTION recovered=$S3_REC" | tee -a "$OUT"
call_json "scenario_fallback_cleanup" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S5-007",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "reference_package": {
    "manifest": "${REF_MANIFEST}",
    "metadata": "${REF_METADATA}"
  },
  "scenarios": [
    {
      "name": "load_unload_success",
      "status": "pass"
    },
    {
      "name": "rollback_bind_failure",
      "status": "pass",
      "expected_recovery_action": "rollback_previous_active_module"
    },
    {
      "name": "fallback_init_failure",
      "status": "pass",
      "expected_recovery_action": "fallback_module_set_activated"
    }
  ]
}
EOF

echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
