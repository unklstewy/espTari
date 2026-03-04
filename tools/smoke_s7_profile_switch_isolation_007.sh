#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s7_profile_switch_isolation_007_smoke_postfix_${TS}.txt"
BUNDLE="captures/s7_profile_switch_isolation_bundle_007_${TS}.json"
MATRIX="TRACKING/evidence/s7_profile_switch_isolation_matrix_007.json"
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

call_json "session_stop_pre" POST "/api/v2/engine/session/stop" "{}" "^(200|409)$"

BASELINE='{"machine":"atari_st","profile":"st_520_pal","rom_id":"rom.atari.st.01","tos_id":"tos.eu.1.04","disk_ids":["disk.automation.a_093"]}'
call_json "baseline_start_atari_st" POST "/api/v2/engine/session" "$BASELINE" '^200$'
base_machine="$(extract_json 'd["data"]["machine"]')"
base_profile="$(extract_json 'd["data"]["profile"]')"
base_state="$(extract_json 'd["data"]["state"]')"
if [[ "$base_machine" != "atari_st" || "$base_profile" != "st_520_pal" || "$base_state" != "running" ]]; then
  echo "baseline_start=failed machine=$base_machine profile=$base_profile state=$base_state" | tee -a "$OUT"
  exit 1
fi

call_json "switch_guard_ste" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"ste_pal","rom_id":"rom.atari.st.01"}' '^404$'
ste_guard_code="$(extract_json 'd["error"]["code"]')"
if [[ "$ste_guard_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "switch_guard_ste=failed code=$ste_guard_code" | tee -a "$OUT"
  exit 1
fi

call_json "switch_guard_mega_ste" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"mega_ste_pal","rom_id":"rom.atari.st.01"}' '^404$'
mega_ste_guard_code="$(extract_json 'd["error"]["code"]')"
if [[ "$mega_ste_guard_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "switch_guard_mega_ste=failed code=$mega_ste_guard_code" | tee -a "$OUT"
  exit 1
fi

call_json "switch_fallback_mismatch" POST "/api/v2/engine/session" '{"machine":"mega_st","profile":"st_520_pal","rom_id":"rom.atari.st.01"}' '^400$'
mismatch_code="$(extract_json 'd["error"]["code"]')"
mismatch_reason="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$mismatch_code" != "BAD_REQUEST" || "$mismatch_reason" != "machine/profile mismatch" ]]; then
  echo "switch_fallback_mismatch=failed code=$mismatch_code reason=$mismatch_reason" | tee -a "$OUT"
  exit 1
fi

call_json "isolation_state_preserved" GET "/api/v2/engine/session" "" '^200$'
iso_state="$(extract_json 'd["data"]["state"]')"
iso_machine="$(extract_json 'd["data"]["machine"]')"
iso_profile="$(extract_json 'd["data"]["profile"]')"
iso_fields_ok="$(extract_json 'str(all(k in d["data"] for k in ["session_id","state","run_mode","machine","profile","tick_counter","cycle_counter"]))')"
if [[ "$iso_state" != "running" || "$iso_machine" != "atari_st" || "$iso_profile" != "st_520_pal" || "$iso_fields_ok" != "True" ]]; then
  echo "isolation_state_preserved=failed state=$iso_state machine=$iso_machine profile=$iso_profile fields_ok=$iso_fields_ok" | tee -a "$OUT"
  exit 1
fi

VIDEO_CONTROL='{"type":"set_rate_limit","stream":"video","pacing_mode":"fixed_fps","target_fps":50,"max_burst_frames":2}'
call_json "post_switch_stream_control" POST "/api/v2/stream/control" "$VIDEO_CONTROL" '^200$'
control_mode="$(extract_json 'd["data"]["pacing"]["pacing_mode"]')"
if [[ "$control_mode" != "fixed_fps" ]]; then
  echo "post_switch_stream_control=failed mode=$control_mode" | tee -a "$OUT"
  exit 1
fi

call_json "post_switch_stream_video" GET "/api/v2/stream/video?session_id=ses_local" "" '^200$'
video_fields_ok="$(extract_json 'str(all(k in d["data"] for k in ["stream","event_seq","event_timestamp_us","delivery","backpressure","video_metadata_contract","video_frame_meta_sample"]))')"
if [[ "$video_fields_ok" != "True" ]]; then
  echo "post_switch_stream_video=failed" | tee -a "$OUT"
  exit 1
fi

call_json "post_switch_stream_audio" GET "/api/v2/stream/audio?session_id=ses_local" "" '^200$'
audio_fields_ok="$(extract_json 'str(all(k in d["data"] for k in ["stream","event_seq","event_timestamp_us","delivery","backpressure","audio_metadata_contract","audio_chunk_meta_sample"]))')"
if [[ "$audio_fields_ok" != "True" ]]; then
  echo "post_switch_stream_audio=failed" | tee -a "$OUT"
  exit 1
fi

INVALID_CONTROL='{"type":"set_rate_limit","stream":"video","pacing_mode":"switch_delta"}'
call_json "fallback_invalid_control" POST "/api/v2/stream/control" "$INVALID_CONTROL" '^400$'
invalid_control_code="$(extract_json 'd["error"]["code"]')"
if [[ "$invalid_control_code" != "BAD_REQUEST" ]]; then
  echo "fallback_invalid_control=failed code=$invalid_control_code" | tee -a "$OUT"
  exit 1
fi

call_json "session_stop_for_stopped_guard" POST "/api/v2/engine/session/stop" "{}" '^200$'
call_json "fallback_stopped_control" POST "/api/v2/stream/control" "$VIDEO_CONTROL" '^409$'
stopped_control_code="$(extract_json 'd["error"]["code"]')"
if [[ "$stopped_control_code" != "ENGINE_NOT_RUNNING" ]]; then
  echo "fallback_stopped_control=failed code=$stopped_control_code" | tee -a "$OUT"
  exit 1
fi

call_json "post_switch_health" GET "/api/v2/engine/health" "" '^200$'
health_ok="$(extract_json 'str(d.get("ok") is True)')"
if [[ "$health_ok" != "True" ]]; then
  echo "post_switch_health=failed ok=$health_ok" | tee -a "$OUT"
  exit 1
fi

call_json "post_switch_session_status" GET "/api/v2/engine/session" "" '^200$'
post_profile="$(extract_json 'd["data"]["profile"]')"
if [[ "$post_profile" != "st_520_pal" ]]; then
  echo "post_switch_session_status=failed profile=$post_profile" | tee -a "$OUT"
  exit 1
fi

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S7-007",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "profile_switch_isolation_matrix": "${MATRIX}",
  "summary": {
    "baseline_machine": "${base_machine}",
    "baseline_profile": "${base_profile}",
    "ste_guard_code": "${ste_guard_code}",
    "mega_ste_guard_code": "${mega_ste_guard_code}",
    "mismatch_guard_code": "${mismatch_code}",
    "isolation_state": "${iso_state}",
    "invalid_control_code": "${invalid_control_code}",
    "stopped_control_code": "${stopped_control_code}",
    "post_profile": "${post_profile}"
  }
}
EOF

echo "s7_profile_switch_isolation=pass baseline=${base_machine}/${base_profile} guards=${ste_guard_code},${mega_ste_guard_code} fallback=${invalid_control_code}" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
