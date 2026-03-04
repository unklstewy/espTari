#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_003_rollback_fallback_lock_${TS}.txt"
mkdir -p captures
: > "$OUT"

AUTH_BEARER="${AUTH_BEARER:-}"
AUTH_HEADER="${AUTH_HEADER:-}"
AUTH_ARGS=()
if [[ -n "$AUTH_BEARER" ]]; then
  AUTH_ARGS+=( -H "Authorization: Bearer ${AUTH_BEARER}" )
fi
if [[ -n "$AUTH_HEADER" ]]; then
  AUTH_ARGS+=( -H "$AUTH_HEADER" )
fi
if [[ ${#AUTH_ARGS[@]} -eq 0 ]]; then
  echo "auth_header=none" | tee -a "$OUT"
else
  echo "auth_header=configured" | tee -a "$OUT"
fi

wait_for_health() {
  local attempts="${1:-60}"
  local interval="${2:-1}"
  local i
  for ((i=1; i<=attempts; i++)); do
    if curl --max-time 2 -sS "${AUTH_ARGS[@]}" "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
      echo "health_check=ok attempts=${i}" | tee -a "$OUT"
      return 0
    fi
    sleep "$interval"
  done
  echo "health_check=timeout attempts=${attempts}" | tee -a "$OUT"
  exit 1
}

post_json() {
  local name="$1" path="$2" data="$3" expected="$4"
  local body_file code body
  body_file="$(mktemp)"
  code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X POST "$BASE_URL$path" "${AUTH_ARGS[@]}" -H "Content-Type: application/json" -d "$data")
  body="$(cat "$body_file")"
  rm -f "$body_file"

  {
    echo "### $name"
    echo "POST $path"
    echo "HTTP $code"
    echo "$body"
    echo
  } >> "$OUT"

  if [[ ! "$code" =~ $expected ]]; then
    echo "step_failed=$name http=$code" | tee -a "$OUT"
    exit 1
  fi

  printf '%s' "$body"
}

extract_expr() {
  local expr="$1"
  /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print(${expr})"
}

CPU='{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}]}'
VIDEO_BIND_FAIL='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[],"simulate_fail_stage":"bind"}'
VIDEO_INIT_FAIL_FALLBACK='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[],"simulate_fail_stage":"init","force_fallback":true}'

wait_for_health

echo "determinism_runs=3" | tee -a "$OUT"
for i in $(seq 1 3); do
  post_json "seed_lkg_load_cpu_${i}" "/api/v2/ebins/load" "$CPU" "^200$" >/dev/null
  post_json "seed_lkg_unload_cpu_${i}" "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$" >/dev/null

  post_json "rollback_prepare_load_cpu_${i}" "/api/v2/ebins/load" "$CPU" "^200$" >/dev/null
  body="$(post_json "rollback_bind_fail_video_${i}" "/api/v2/ebins/load" "$VIDEO_BIND_FAIL" "^409$")"
  code="$(printf '%s' "$body" | extract_expr 'd["error"]["code"]')"
  state="$(printf '%s' "$body" | extract_expr 'd["error"]["details"]["runtime_state"]')"
  action="$(printf '%s' "$body" | extract_expr 'd["error"]["details"]["fault_telemetry"]["recovery_action"]')"
  recovered="$(printf '%s' "$body" | extract_expr 'd["error"]["details"]["fault_telemetry"]["recovered_module_id"]')"
  [[ "$code" == "EBIN_INVALID" && "$state" == "running" && "$action" == "rollback_previous_active_module" && "$recovered" == "st.cpu.m68k" ]] || { echo "rollback_check=failed run=$i" | tee -a "$OUT"; exit 1; }
  echo "rollback_check=pass run=$i code=$code action=$action" | tee -a "$OUT"

  post_json "rollback_cleanup_unload_cpu_${i}" "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$" >/dev/null

  body="$(post_json "fallback_init_fail_video_${i}" "/api/v2/ebins/load" "$VIDEO_INIT_FAIL_FALLBACK" "^409$")"
  code="$(printf '%s' "$body" | extract_expr 'd["error"]["code"]')"
  state="$(printf '%s' "$body" | extract_expr 'd["error"]["details"]["runtime_state"]')"
  action="$(printf '%s' "$body" | extract_expr 'd["error"]["details"]["fault_telemetry"]["recovery_action"]')"
  recovered="$(printf '%s' "$body" | extract_expr 'd["error"]["details"]["fault_telemetry"]["recovered_module_id"]')"
  reason="$(printf '%s' "$body" | extract_expr 'd["error"]["details"]["fault_telemetry"]["fault_reason"]')"
  [[ "$code" == "EBIN_INVALID" && "$state" == "running" && "$action" == "fallback_module_set_activated" && "$recovered" == "st.cpu.m68k" && "$reason" == "load_init_failed_simulated" ]] || { echo "fallback_check=failed run=$i" | tee -a "$OUT"; exit 1; }
  echo "fallback_check=pass run=$i code=$code action=$action" | tee -a "$OUT"

  post_json "fallback_cleanup_unload_cpu_${i}" "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$" >/dev/null
done

echo "determinism_check=pass" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
