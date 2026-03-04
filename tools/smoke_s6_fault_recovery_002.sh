#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
CYCLES="${CYCLES:-3}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s6_fault_recovery_002_smoke_postfix_${TS}.txt"
BUNDLE="captures/s6_fault_recovery_bundle_002_${TS}.json"
MATRIX="TRACKING/evidence/s6_stress_fault_recovery_matrix_002.json"
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

assert_survivability() {
  local tag="$1"
  call_json "${tag}_health_probe" GET "/api/v2/engine/health" "" "^200$"
  local health_ok
  health_ok="$(extract_json 'str(d["ok"] is True)')"
  if [[ "$health_ok" != "True" ]]; then
    echo "${tag}_health_probe=failed ok=$health_ok" | tee -a "$OUT"
    exit 1
  fi

  call_json "${tag}_session_start_probe" POST "/api/v2/engine/session/start" "" "^(200|409)$"
  echo "${tag}_survivability=pass" | tee -a "$OUT"
}

CPU_LOAD='{"module_id":"st.cpu.m68k","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[{"module_id":"st.video.shifter","abi_range":"1.0.x","required":true}]}'
VIDEO_BIND_FAIL='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[],"simulate_fail_stage":"bind"}'
VIDEO_INIT_FAIL_FALLBACK='{"module_id":"st.video.shifter","abi_version":"1.0.0","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef","signature":{"algorithm":"ed25519","key_id":"dev_key_01","value":"sig_stub_01"},"dependencies":[],"simulate_fail_stage":"init","force_fallback":true}'
UNLOAD_DRAIN_FAIL='{"module_id":"st.cpu.m68k","simulate_fail_stage":"drain"}'

wait_for_health
if [[ ! -f "$MATRIX" ]]; then
  echo "missing_matrix=$MATRIX" | tee -a "$OUT"
  exit 1
fi

rollback_pass=0
fallback_pass=0
drain_pass=0
survive_pass=0

for cycle in $(seq 1 "$CYCLES"); do
  echo "cycle_start=${cycle}" | tee -a "$OUT"

  call_json "cycle_${cycle}_seed_load_cpu" POST "/api/v2/ebins/load" "$CPU_LOAD" "^200$"

  call_json "cycle_${cycle}_fault_bind_fail" POST "/api/v2/ebins/load" "$VIDEO_BIND_FAIL" "^409$"
  rb_code="$(extract_json 'd["error"]["code"]')"
  rb_action="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovery_action"]')"
  rb_state="$(extract_json 'd["error"]["details"]["runtime_state"]')"
  if [[ "$rb_code" != "EBIN_INVALID" || "$rb_action" != "rollback_previous_active_module" || "$rb_state" != "running" ]]; then
    echo "cycle_${cycle}_fault_bind_fail=failed code=$rb_code action=$rb_action state=$rb_state" | tee -a "$OUT"
    exit 1
  fi
  rollback_pass=$((rollback_pass + 1))
  assert_survivability "cycle_${cycle}_post_bind_fail"
  survive_pass=$((survive_pass + 1))

  call_json "cycle_${cycle}_fault_init_fallback" POST "/api/v2/ebins/load" "$VIDEO_INIT_FAIL_FALLBACK" "^409$"
  fb_code="$(extract_json 'd["error"]["code"]')"
  fb_action="$(extract_json 'd["error"]["details"]["fault_telemetry"]["recovery_action"]')"
  fb_state="$(extract_json 'd["error"]["details"]["runtime_state"]')"
  if [[ "$fb_code" != "EBIN_INVALID" || "$fb_action" != "fallback_module_set_activated" || "$fb_state" != "running" ]]; then
    echo "cycle_${cycle}_fault_init_fallback=failed code=$fb_code action=$fb_action state=$fb_state" | tee -a "$OUT"
    exit 1
  fi
  fallback_pass=$((fallback_pass + 1))
  assert_survivability "cycle_${cycle}_post_init_fallback"
  survive_pass=$((survive_pass + 1))

  call_json "cycle_${cycle}_fault_unload_drain" POST "/api/v2/ebins/unload" "$UNLOAD_DRAIN_FAIL" "^409$"
  dr_code="$(extract_json 'd["error"]["code"]')"
  dr_stage="$(extract_json 'd["error"]["details"]["stage"]')"
  dr_state="$(extract_json 'd["error"]["details"]["runtime_state"]')"
  if [[ "$dr_code" != "EBIN_INVALID" || "$dr_stage" != "drain" || "$dr_state" != "recoverable_error" ]]; then
    echo "cycle_${cycle}_fault_unload_drain=failed code=$dr_code stage=$dr_stage state=$dr_state" | tee -a "$OUT"
    exit 1
  fi
  drain_pass=$((drain_pass + 1))

  call_json "cycle_${cycle}_recover_unload" POST "/api/v2/ebins/unload" '{"module_id":"st.cpu.m68k"}' "^200$"
  rec_state="$(extract_json 'd["data"]["runtime_state"]')"
  if [[ "$rec_state" != "idle" ]]; then
    echo "cycle_${cycle}_recover_unload=failed state=$rec_state" | tee -a "$OUT"
    exit 1
  fi
  assert_survivability "cycle_${cycle}_post_recover_unload"
  survive_pass=$((survive_pass + 1))

  echo "cycle_complete=${cycle}" | tee -a "$OUT"
done

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S6-002",
  "generated_at": "${TS}",
  "cycles": ${CYCLES},
  "harness_capture": "${OUT}",
  "fault_matrix": "${MATRIX}",
  "summary": {
    "rollback_pass": ${rollback_pass},
    "fallback_pass": ${fallback_pass},
    "drain_pass": ${drain_pass},
    "survivability_pass": ${survive_pass}
  }
}
EOF

echo "stress_fault_summary=pass cycles=${CYCLES} rollback=${rollback_pass} fallback=${fallback_pass} drain=${drain_pass} survivability=${survive_pass}" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
