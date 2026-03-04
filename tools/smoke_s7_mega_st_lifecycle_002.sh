#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s7_mega_st_lifecycle_002_smoke_postfix_${TS}.txt"
BUNDLE="captures/s7_mega_st_lifecycle_bundle_002_${TS}.json"
MATRIX="TRACKING/evidence/s7_mega_st_lifecycle_matrix_002.json"
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

call_json "session_stop_pre" POST "/api/v2/engine/session/stop" "" "^(200|409)$"

ATARI_START='{"machine":"atari_st","profile":"st_520_pal","rom_id":"rom.atari.st.01","tos_id":"tos.eu.1.04","disk_ids":["disk.automation.a_093"]}'
call_json "baseline_start_atari_st" POST "/api/v2/engine/session" "$ATARI_START" '^200$'
start_machine="$(extract_json 'd["data"]["machine"]')"
start_profile="$(extract_json 'd["data"]["profile"]')"
start_state="$(extract_json 'd["data"]["state"]')"
if [[ "$start_machine" != "atari_st" || "$start_profile" != "st_520_pal" || "$start_state" != "running" ]]; then
  echo "baseline_start_atari_st=failed machine=$start_machine profile=$start_profile state=$start_state" | tee -a "$OUT"
  exit 1
fi

call_json "session_fields_running" GET "/api/v2/engine/session" "" '^200$'
session_fields_ok="$($PYTHON_BIN -c 'import json,sys
d=json.load(sys.stdin)["data"]
required=["session_id","state","run_mode","machine","profile","tick_counter","cycle_counter"]
print("true" if all(k in d for k in required) else "false")
' <<<"$LAST_BODY")"
if [[ "$session_fields_ok" != "true" ]]; then
  echo "session_fields_running=failed" | tee -a "$OUT"
  exit 1
fi

call_json "lifecycle_pause" POST "/api/v2/engine/session/pause" "{}" '^200$'
call_json "session_state_after_pause" GET "/api/v2/engine/session" "" '^200$'
paused_state="$(extract_json 'd["data"]["state"]')"
if [[ "$paused_state" != "paused" ]]; then
  echo "lifecycle_pause=failed state=$paused_state" | tee -a "$OUT"
  exit 1
fi

call_json "lifecycle_resume" POST "/api/v2/engine/session/resume" '{"resume_mode":"running"}' '^200$'
call_json "session_state_after_resume" GET "/api/v2/engine/session" "" '^200$'
resumed_state="$(extract_json 'd["data"]["state"]')"
if [[ "$resumed_state" != "running" ]]; then
  echo "lifecycle_resume=failed state=$resumed_state" | tee -a "$OUT"
  exit 1
fi

call_json "lifecycle_reset" POST "/api/v2/engine/session/reset" '{"mode":"warm","preserve_media":true}' '^200$'
call_json "session_state_after_reset" GET "/api/v2/engine/session" "" '^200$'
reset_state="$(extract_json 'd["data"]["state"]')"
if [[ "$reset_state" != "running" ]]; then
  echo "lifecycle_reset=failed state=$reset_state" | tee -a "$OUT"
  exit 1
fi

call_json "lifecycle_stop" POST "/api/v2/engine/session/stop" "{}" '^200$'
call_json "session_state_after_stop" GET "/api/v2/engine/session" "" '^200$'
stopped_state="$(extract_json 'd["data"]["state"]')"
if [[ "$stopped_state" != "stopped" ]]; then
  echo "lifecycle_stop=failed state=$stopped_state" | tee -a "$OUT"
  exit 1
fi

call_json "mega_st_bootstrap_guard" POST "/api/v2/engine/session" '{"machine":"mega_st","profile":"mega_st_pal","rom_id":"rom.atari.st.01"}' '^404$'
mega_bootstrap_code="$(extract_json 'd["error"]["code"]')"
if [[ "$mega_bootstrap_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "mega_st_bootstrap_guard=failed code=$mega_bootstrap_code" | tee -a "$OUT"
  exit 1
fi

call_json "mega_st_mismatch_guard" POST "/api/v2/engine/session" '{"machine":"mega_st","profile":"st_520_pal","rom_id":"rom.atari.st.01"}' '^400$'
mega_mismatch_code="$(extract_json 'd["error"]["code"]')"
mega_mismatch_reason="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$mega_mismatch_code" != "BAD_REQUEST" || "$mega_mismatch_reason" != "machine/profile mismatch" ]]; then
  echo "mega_st_mismatch_guard=failed code=$mega_mismatch_code reason=$mega_mismatch_reason" | tee -a "$OUT"
  exit 1
fi

call_json "session_stop_pre_guard_checks" POST "/api/v2/engine/session/stop" "{}" '^(200|409)$'
call_json "session_state_pre_guard_checks" GET "/api/v2/engine/session" "" '^200$'
state_pre_guards="$(extract_json 'd["data"]["state"]')"
if [[ "$state_pre_guards" != "stopped" ]]; then
  echo "session_state_pre_guard_checks=failed state=$state_pre_guards" | tee -a "$OUT"
  exit 1
fi

call_json "stopped_pause_guard" POST "/api/v2/engine/session/pause" "{}" '^409$'
pause_guard_code="$(extract_json 'd["error"]["code"]')"
if [[ "$pause_guard_code" != "INVALID_SESSION_STATE" ]]; then
  echo "stopped_pause_guard=failed code=$pause_guard_code" | tee -a "$OUT"
  exit 1
fi

call_json "stopped_resume_guard" POST "/api/v2/engine/session/resume" '{"resume_mode":"running"}' '^409$'
resume_guard_code="$(extract_json 'd["error"]["code"]')"
if [[ "$resume_guard_code" != "INVALID_SESSION_STATE" ]]; then
  echo "stopped_resume_guard=failed code=$resume_guard_code" | tee -a "$OUT"
  exit 1
fi

call_json "stopped_reset_guard" POST "/api/v2/engine/session/reset" '{"mode":"warm","preserve_media":true}' '^409$'
reset_guard_code="$(extract_json 'd["error"]["code"]')"
if [[ "$reset_guard_code" != "INVALID_SESSION_STATE" ]]; then
  echo "stopped_reset_guard=failed code=$reset_guard_code" | tee -a "$OUT"
  exit 1
fi

call_json "session_fields_after_guards" GET "/api/v2/engine/session" "" '^200$'
post_state="$(extract_json 'd["data"]["state"]')"
post_fields_ok="$($PYTHON_BIN -c 'import json,sys
d=json.load(sys.stdin)["data"]
required=["session_id","state","run_mode","machine","profile","tick_counter","cycle_counter"]
print("true" if all(k in d for k in required) else "false")
' <<<"$LAST_BODY")"
if [[ "$post_state" != "stopped" || "$post_fields_ok" != "true" ]]; then
  echo "session_fields_after_guards=failed state=$post_state fields_ok=$post_fields_ok" | tee -a "$OUT"
  exit 1
fi

call_json "post_checks_health" GET "/api/v2/engine/health" "" '^200$'
health_ok="$(extract_json 'str(d.get("ok") is True)')"
if [[ "$health_ok" != "True" ]]; then
  echo "post_checks_health=failed ok=$health_ok" | tee -a "$OUT"
  exit 1
fi

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S7-002",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "mega_st_lifecycle_matrix": "${MATRIX}",
  "summary": {
    "baseline_machine": "${start_machine}",
    "baseline_profile": "${start_profile}",
    "paused_state": "${paused_state}",
    "resumed_state": "${resumed_state}",
    "reset_state": "${reset_state}",
    "stopped_state": "${stopped_state}",
    "mega_st_bootstrap_code": "${mega_bootstrap_code}",
    "mega_st_mismatch_code": "${mega_mismatch_code}",
    "pause_guard_code": "${pause_guard_code}",
    "resume_guard_code": "${resume_guard_code}",
    "reset_guard_code": "${reset_guard_code}"
  }
}
EOF

echo "s7_mega_st_lifecycle=pass baseline=${start_machine}/${start_profile} mega_guard=${mega_bootstrap_code} lifecycle_guards=${pause_guard_code},${resume_guard_code},${reset_guard_code}" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"