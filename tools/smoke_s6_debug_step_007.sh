#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s6_debug_step_007_smoke_postfix_${TS}.txt"
BUNDLE="captures/s6_debug_step_bundle_007_${TS}.json"
MATRIX="TRACKING/evidence/s6_debug_step_matrix_007.json"
PYTHON_BIN="/home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python"
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
    echo "step_failed=$name http=$code" | tee -a "$OUT"
    echo "$body" | tee -a "$OUT"
    exit 1
  fi
  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | "$PYTHON_BIN" -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

wait_for_health
[[ -f "$MATRIX" ]] || { echo "missing_matrix=$MATRIX" | tee -a "$OUT"; exit 1; }

call_json "engine_session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "clock_mode_slow_motion" POST "/api/v2/debug/clock/mode" '{"session_id":"ses_local","mode":"slow_motion","ratio":0.5}' '^200$'
slow_to_mode="$(extract_json 'd["data"]["to_mode"]')"
slow_ratio="$(extract_json 'float(d["data"]["effective_ratio"])')"
if [[ "$slow_to_mode" != "slow_motion" || "$slow_ratio" != "0.5" ]]; then
  echo "clock_slow_motion=failed to_mode=$slow_to_mode ratio=$slow_ratio" | tee -a "$OUT"
  exit 1
fi

call_json "clock_mode_invalid_ratio_for_single_step" POST "/api/v2/debug/clock/mode" '{"session_id":"ses_local","mode":"single_step","ratio":0.5}' '^400$'
invalid_clock_code="$(extract_json 'd["error"]["code"]')"
if [[ "$invalid_clock_code" != "DEBUG_CLOCK_INVALID" ]]; then
  echo "clock_invalid=failed code=$invalid_clock_code" | tee -a "$OUT"
  exit 1
fi

call_json "clock_mode_single_step" POST "/api/v2/debug/clock/mode" '{"session_id":"ses_local","mode":"single_step"}' '^200$'
step_to_mode="$(extract_json 'd["data"]["to_mode"]')"
if [[ "$step_to_mode" != "single_step" ]]; then
  echo "clock_single_step=failed to_mode=$step_to_mode" | tee -a "$OUT"
  exit 1
fi

call_json "single_step_capture_opcode_bus_error" POST "/api/v2/debug/clock/step" '{"session_id":"ses_local","steps":3,"capture":["opcode","bus_error"]}' '^200$'
step_checks_ok="$($PYTHON_BIN -c 'import json,sys
d=json.load(sys.stdin)["data"]
step_ok=all(v=="pass" for v in d.get("step_checks",{}).values())
capture_ok=all(v=="pass" for v in d.get("capture_checks",{}).values())
payloads=d.get("capture_payloads",[])
opcode=[p for p in payloads if p.get("kind")=="opcode_capture_v1"]
bus=[p for p in payloads if p.get("kind")=="bus_error_capture_v1"]
opcode_fields=all(all(k in x for k in ["opcode_word","instruction_size_bytes","pc","tick_counter","cycle_counter"]) for x in opcode)
bus_fields=all(all(k in x for k in ["fault_address","access_type","fault_phase","vector","tick_counter","cycle_counter"]) for x in bus)
ticks_ok=int(d.get("ticks_committed",0))==int(d.get("steps_requested",0))
print(f"{step_ok},{capture_ok},{opcode_fields},{bus_fields},{ticks_ok},{len(opcode)},{len(bus)},{int(d.get("tick_counter_after",0))}")
' <<<"$LAST_BODY")"
IFS=',' read -r step_ok capture_ok opcode_fields_ok bus_fields_ok ticks_ok opcode_count bus_count tick_after <<<"$step_checks_ok"
if [[ "$step_ok" != "True" || "$capture_ok" != "True" || "$opcode_fields_ok" != "True" || "$bus_fields_ok" != "True" || "$ticks_ok" != "True" ]]; then
  echo "single_step_capture=failed step_ok=$step_ok capture_ok=$capture_ok opcode_fields_ok=$opcode_fields_ok bus_fields_ok=$bus_fields_ok ticks_ok=$ticks_ok" | tee -a "$OUT"
  exit 1
fi

call_json "single_step_register_delta_guard" POST "/api/v2/debug/clock/step" '{"session_id":"ses_local","steps":1,"capture":["register_delta"]}' '^409$'
reg_guard_id="$(extract_json 'd["error"]["details"]["guard_id"]')"
if [[ "$reg_guard_id" != "CAP-DIAG-PROFILE" ]]; then
  echo "register_delta_guard=failed guard_id=$reg_guard_id" | tee -a "$OUT"
  exit 1
fi

call_json "single_step_opcode_schema_forced_failure" POST "/api/v2/debug/clock/step" '{"session_id":"ses_local","steps":1,"capture":["opcode"],"force_opcode_schema_invalid":true}' '^500$'
opcode_check_id="$(extract_json 'd["error"]["details"]["check_id"]')"
if [[ "$opcode_check_id" != "CAP-DIAG-02" ]]; then
  echo "opcode_schema_failure=failed check_id=$opcode_check_id" | tee -a "$OUT"
  exit 1
fi

call_json "single_step_bus_error_schema_forced_failure" POST "/api/v2/debug/clock/step" '{"session_id":"ses_local","steps":1,"capture":["bus_error"],"force_bus_error_schema_invalid":true}' '^500$'
bus_check_id="$(extract_json 'd["error"]["details"]["check_id"]')"
if [[ "$bus_check_id" != "CAP-DIAG-03" ]]; then
  echo "bus_schema_failure=failed check_id=$bus_check_id" | tee -a "$OUT"
  exit 1
fi

call_json "clock_state" GET "/api/v2/debug/clock/state" "" '^200$'
clock_mode="$(extract_json 'd["data"]["mode"]')"
clock_monotonic="$(extract_json 'str(d["data"]["timestamp_emitter"]["monotonic"])')"
clock_tick_state="$(extract_json 'int(d["data"]["tick_counter"])')"
if [[ "$clock_mode" != "single_step" || "$clock_monotonic" != "True" || "$clock_tick_state" -lt "$tick_after" ]]; then
  echo "clock_state=failed mode=$clock_mode monotonic=$clock_monotonic tick_counter=$clock_tick_state tick_after=$tick_after" | tee -a "$OUT"
  exit 1
fi

call_json "clock_mode_realtime" POST "/api/v2/debug/clock/mode" '{"session_id":"ses_local","mode":"realtime"}' '^200$'
realtime_mode="$(extract_json 'd["data"]["to_mode"]')"
if [[ "$realtime_mode" != "realtime" ]]; then
  echo "clock_realtime=failed to_mode=$realtime_mode" | tee -a "$OUT"
  exit 1
fi

call_json "post_checks_health" GET "/api/v2/engine/health" "" '^200$'
post_ok="$(extract_json 'str(d.get("ok") is True)')"
[[ "$post_ok" == "True" ]] || { echo "post_checks_health=failed" | tee -a "$OUT"; exit 1; }

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S6-007",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "debug_step_matrix": "${MATRIX}",
  "summary": {
    "clock_mode_after_validation": "${clock_mode}",
    "tick_counter_after_step": ${tick_after},
    "tick_counter_at_state": ${clock_tick_state},
    "opcode_payload_count": ${opcode_count},
    "bus_error_payload_count": ${bus_count},
    "register_delta_guard_id": "${reg_guard_id}",
    "opcode_schema_check_id": "${opcode_check_id}",
    "bus_error_schema_check_id": "${bus_check_id}"
  }
}
EOF

echo "debug_step=pass clock_mode=$clock_mode tick_after=$tick_after tick_state=$clock_tick_state opcode_payloads=$opcode_count bus_error_payloads=$bus_count" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
