#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s7_ste_extension_controls_004_smoke_postfix_${TS}.txt"
BUNDLE="captures/s7_ste_extension_controls_bundle_004_${TS}.json"
MATRIX="TRACKING/evidence/s7_ste_extension_controls_matrix_004.json"
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

START_PAYLOAD='{"machine":"atari_st","profile":"st_520_pal","rom_id":"rom.atari.st.01","tos_id":"tos.eu.1.04","disk_ids":["disk.automation.a_093"]}'
call_json "baseline_start_atari_st" POST "/api/v2/engine/session" "$START_PAYLOAD" '^200$'
start_machine="$(extract_json 'd["data"]["machine"]')"
start_profile="$(extract_json 'd["data"]["profile"]')"
start_state="$(extract_json 'd["data"]["state"]')"
if [[ "$start_machine" != "atari_st" || "$start_profile" != "st_520_pal" || "$start_state" != "running" ]]; then
  echo "baseline_start_atari_st=failed machine=$start_machine profile=$start_profile state=$start_state" | tee -a "$OUT"
  exit 1
fi

VIDEO_CONTROL_PAYLOAD='{"type":"set_rate_limit","stream":"video","pacing_mode":"fixed_fps","target_fps":50,"max_burst_frames":2}'
call_json "video_extension_control_baseline" POST "/api/v2/stream/control" "$VIDEO_CONTROL_PAYLOAD" '^200$'
video_stream="$(extract_json 'd["data"]["stream"]')"
video_type="$(extract_json 'd["data"]["type"]')"
video_mode="$(extract_json 'd["data"]["pacing"]["pacing_mode"]')"
video_target_fps="$(extract_json 'str(d["data"]["pacing"]["target_fps"])')"
video_burst="$(extract_json 'str(d["data"]["pacing"]["max_burst_frames"])')"
if [[ "$video_stream" != "video" || "$video_type" != "set_rate_limit" || "$video_mode" != "fixed_fps" || "$video_target_fps" != "50" || "$video_burst" != "2" ]]; then
  echo "video_extension_control_baseline=failed stream=$video_stream type=$video_type mode=$video_mode target_fps=$video_target_fps burst=$video_burst" | tee -a "$OUT"
  exit 1
fi

AUDIO_CONTROL_PAYLOAD='{"type":"set_rate_limit","stream":"audio","pacing_mode":"fixed_hz","target_hz":240,"max_burst_chunks":2}'
call_json "audio_extension_control_baseline" POST "/api/v2/stream/control" "$AUDIO_CONTROL_PAYLOAD" '^200$'
audio_stream="$(extract_json 'd["data"]["stream"]')"
audio_type="$(extract_json 'd["data"]["type"]')"
audio_mode="$(extract_json 'd["data"]["pacing"]["pacing_mode"]')"
audio_target_hz="$(extract_json 'str(d["data"]["pacing"]["target_hz"])')"
audio_burst="$(extract_json 'str(d["data"]["pacing"]["max_burst_chunks"])')"
if [[ "$audio_stream" != "audio" || "$audio_type" != "set_rate_limit" || "$audio_mode" != "fixed_hz" || "$audio_target_hz" != "240" || "$audio_burst" != "2" ]]; then
  echo "audio_extension_control_baseline=failed stream=$audio_stream type=$audio_type mode=$audio_mode target_hz=$audio_target_hz burst=$audio_burst" | tee -a "$OUT"
  exit 1
fi

call_json "video_backward_envelope" GET "/api/v2/stream/video?session_id=ses_local" "" '^200$'
video_required_fields_ok="$(extract_json 'str(all(k in d["data"] for k in ["stream","event_seq","event_timestamp_us","delivery","backpressure","video_metadata_contract","video_frame_meta_sample"]))')"
video_ste_absent="$(extract_json 'str("ste_extensions" not in d["data"])')"
if [[ "$video_required_fields_ok" != "True" || "$video_ste_absent" != "True" ]]; then
  echo "video_backward_envelope=failed required=$video_required_fields_ok ste_absent=$video_ste_absent" | tee -a "$OUT"
  exit 1
fi

call_json "audio_backward_envelope" GET "/api/v2/stream/audio?session_id=ses_local" "" '^200$'
audio_required_fields_ok="$(extract_json 'str(all(k in d["data"] for k in ["stream","event_seq","event_timestamp_us","delivery","backpressure","audio_metadata_contract","audio_chunk_meta_sample"]))')"
audio_ste_absent="$(extract_json 'str("ste_extensions" not in d["data"])')"
if [[ "$audio_required_fields_ok" != "True" || "$audio_ste_absent" != "True" ]]; then
  echo "audio_backward_envelope=failed required=$audio_required_fields_ok ste_absent=$audio_ste_absent" | tee -a "$OUT"
  exit 1
fi

INVALID_VIDEO_CONTROL='{"type":"set_rate_limit","stream":"video","pacing_mode":"ste_delta"}'
call_json "invalid_extension_usage_control_guard" POST "/api/v2/stream/control" "$INVALID_VIDEO_CONTROL" '^400$'
invalid_control_code="$(extract_json 'd["error"]["code"]')"
if [[ "$invalid_control_code" != "BAD_REQUEST" ]]; then
  echo "invalid_extension_usage_control_guard=failed code=$invalid_control_code" | tee -a "$OUT"
  exit 1
fi

call_json "invalid_extension_usage_video_schema_guard" GET "/api/v2/stream/video?session_id=ses_local&metadata_schema_version=2" "" '^400$'
invalid_video_schema_code="$(extract_json 'd["error"]["code"]')"
if [[ "$invalid_video_schema_code" != "UNSUPPORTED_VERSION" ]]; then
  echo "invalid_extension_usage_video_schema_guard=failed code=$invalid_video_schema_code" | tee -a "$OUT"
  exit 1
fi

call_json "ste_profile_guard" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"ste_pal","rom_id":"rom.atari.st.01"}' '^404$'
ste_guard_code="$(extract_json 'd["error"]["code"]')"
if [[ "$ste_guard_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "ste_profile_guard=failed code=$ste_guard_code" | tee -a "$OUT"
  exit 1
fi

call_json "session_stop_for_control_guard" POST "/api/v2/engine/session/stop" "{}" '^200$'
call_json "stopped_control_guard" POST "/api/v2/stream/control" "$VIDEO_CONTROL_PAYLOAD" '^409$'
stopped_control_code="$(extract_json 'd["error"]["code"]')"
if [[ "$stopped_control_code" != "ENGINE_NOT_RUNNING" ]]; then
  echo "stopped_control_guard=failed code=$stopped_control_code" | tee -a "$OUT"
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
  "task_id": "S7-004",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "ste_extension_controls_matrix": "${MATRIX}",
  "summary": {
    "baseline_machine": "${start_machine}",
    "baseline_profile": "${start_profile}",
    "video_control_mode": "${video_mode}",
    "audio_control_mode": "${audio_mode}",
    "invalid_control_code": "${invalid_control_code}",
    "invalid_video_schema_code": "${invalid_video_schema_code}",
    "ste_profile_guard_code": "${ste_guard_code}",
    "stopped_control_guard_code": "${stopped_control_code}",
    "video_backward_envelope": ${video_required_fields_ok},
    "audio_backward_envelope": ${audio_required_fields_ok}
  }
}
EOF

echo "s7_ste_extension_controls=pass baseline=${start_machine}/${start_profile} ste_guard=${ste_guard_code} invalid_control=${invalid_control_code}" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"