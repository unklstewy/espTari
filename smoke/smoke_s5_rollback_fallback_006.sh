#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s5_rollback_fallback_006_smoke_postfix_${TS}.txt"
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

CPU_LOAD='{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}]}'
VIDEO_LOAD='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[]}'
VIDEO_BIND_FAIL='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[],"simulate_fail_stage":"bind"}'
VIDEO_INIT_FAIL_FALLBACK='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[],"simulate_fail_stage":"init","force_fallback":true}'

wait_for_health

call_json "seed_lkg_load_cpu" POST "/api/v2/ebins/load" "$CPU_LOAD" "^200$"
call_json "seed_lkg_unload_cpu" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"

call_json "rollback_prepare_load_cpu" POST "/api/v2/ebins/load" "$CPU_LOAD" "^200$"

call_json "rollback_bind_fail_video" POST "/api/v2/ebins/load" "$VIDEO_BIND_FAIL" "^409$"
RB_CODE="$(extract_json 'd["error"]["code"]')"
RB_STATE="$(extract_json 'd["error"]["details"]["runtime_state"]')"
RB_ACTION="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovery_action"]')"
RB_RECOVERED="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovered_module_id"]')"
if [[ "$RB_CODE" != "EBIN_INVALID" || "$RB_STATE" != "running" || "$RB_ACTION" != "rollback_previous_active_module" || "$RB_RECOVERED" != "st.cpu.m68k" ]]; then
  echo "rollback_bind_fail_video=failed code=$RB_CODE state=$RB_STATE action=$RB_ACTION recovered=$RB_RECOVERED" | tee -a "$OUT"
  exit 1
fi
echo "rollback_bind_fail_video=pass code=$RB_CODE state=$RB_STATE action=$RB_ACTION recovered=$RB_RECOVERED" | tee -a "$OUT"

call_json "rollback_cleanup_unload_cpu" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"

call_json "fallback_init_fail_video" POST "/api/v2/ebins/load" "$VIDEO_INIT_FAIL_FALLBACK" "^409$"
FB_CODE="$(extract_json 'd["error"]["code"]')"
FB_STATE="$(extract_json 'd["error"]["details"]["runtime_state"]')"
FB_ACTION="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovery_action"]')"
FB_RECOVERED="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovered_module_id"]')"
FB_FAULT_REASON="$(extract_json 'd["error"]["details"]["fault_telemetry"]["fault_reason"]')"
if [[ "$FB_CODE" != "EBIN_INVALID" || "$FB_STATE" != "running" || "$FB_ACTION" != "fallback_module_set_activated" || "$FB_RECOVERED" != "st.cpu.m68k" || "$FB_FAULT_REASON" != "load_init_failed_simulated" ]]; then
  echo "fallback_init_fail_video=failed code=$FB_CODE state=$FB_STATE action=$FB_ACTION recovered=$FB_RECOVERED reason=$FB_FAULT_REASON" | tee -a "$OUT"
  exit 1
fi
echo "fallback_init_fail_video=pass code=$FB_CODE state=$FB_STATE action=$FB_ACTION recovered=$FB_RECOVERED reason=$FB_FAULT_REASON" | tee -a "$OUT"

call_json "fallback_cleanup_unload_cpu" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"

call_json "control_plane_load_cpu" POST "/api/v2/ebins/load" "$CPU_LOAD" "^200$"
CP_STATE="$(extract_json 'd["data"]["runtime_state"]')"
if [[ "$CP_STATE" != "running" ]]; then
  echo "control_plane_load_cpu=failed state=$CP_STATE" | tee -a "$OUT"
  exit 1
fi
echo "control_plane_load_cpu=pass state=$CP_STATE" | tee -a "$OUT"

call_json "control_plane_unload_cpu" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"
CP_UNLOAD_STATE="$(extract_json 'd["data"]["runtime_state"]')"
if [[ "$CP_UNLOAD_STATE" != "idle" ]]; then
  echo "control_plane_unload_cpu=failed state=$CP_UNLOAD_STATE" | tee -a "$OUT"
  exit 1
fi
echo "control_plane_unload_cpu=pass state=$CP_UNLOAD_STATE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
