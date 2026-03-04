#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s7_mega_ste_extension_compatibility_005_smoke_postfix_${TS}.txt"
BUNDLE="captures/s7_mega_ste_extension_compatibility_bundle_005_${TS}.json"
MATRIX="TRACKING/evidence/s7_mega_ste_extension_compatibility_matrix_005.json"
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
  echo "baseline_start=failed machine=$start_machine profile=$start_profile state=$start_state" | tee -a "$OUT"
  exit 1
fi

VIDEO_CONTROL='{"type":"set_rate_limit","stream":"video","pacing_mode":"fixed_fps","target_fps":50,"max_burst_frames":2}'
call_json "video_control_baseline" POST "/api/v2/stream/control" "$VIDEO_CONTROL" '^200$'
video_mode="$(extract_json 'd["data"]["pacing"]["pacing_mode"]')"
video_fps="$(extract_json 'str(d["data"]["pacing"]["target_fps"])')"
if [[ "$video_mode" != "fixed_fps" || "$video_fps" != "50" ]]; then
  echo "video_control_baseline=failed mode=$video_mode target_fps=$video_fps" | tee -a "$OUT"
  exit 1
fi

AUDIO_CONTROL='{"type":"set_rate_limit","stream":"audio","pacing_mode":"fixed_hz","target_hz":240,"max_burst_chunks":2}'
call_json "audio_control_baseline" POST "/api/v2/stream/control" "$AUDIO_CONTROL" '^200$'
audio_mode="$(extract_json 'd["data"]["pacing"]["pacing_mode"]')"
audio_hz="$(extract_json 'str(d["data"]["pacing"]["target_hz"])')"
if [[ "$audio_mode" != "fixed_hz" || "$audio_hz" != "240" ]]; then
  echo "audio_control_baseline=failed mode=$audio_mode target_hz=$audio_hz" | tee -a "$OUT"
  exit 1
fi

call_json "video_backward_envelope" GET "/api/v2/stream/video?session_id=ses_local" "" '^200$'
video_ste_absent="$(extract_json 'str("ste_extensions" not in d["data"])')"
if [[ "$video_ste_absent" != "True" ]]; then
  echo "video_backward_envelope=failed ste_extensions_present=true" | tee -a "$OUT"
  exit 1
fi

call_json "audio_backward_envelope" GET "/api/v2/stream/audio?session_id=ses_local" "" '^200$'
audio_ste_absent="$(extract_json 'str("ste_extensions" not in d["data"])')"
if [[ "$audio_ste_absent" != "True" ]]; then
  echo "audio_backward_envelope=failed ste_extensions_present=true" | tee -a "$OUT"
  exit 1
fi

call_json "ste_profile_guard" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"ste_pal","rom_id":"rom.atari.st.01"}' '^404$'
ste_guard_code="$(extract_json 'd["error"]["code"]')"
ste_guard_path="$(extract_json 'd["error"]["details"]["path"]')"
if [[ "$ste_guard_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "ste_profile_guard=failed code=$ste_guard_code" | tee -a "$OUT"
  exit 1
fi

call_json "mega_ste_profile_guard" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"mega_ste_pal","rom_id":"rom.atari.st.01"}' '^404$'
mega_ste_guard_code="$(extract_json 'd["error"]["code"]')"
mega_ste_guard_path="$(extract_json 'd["error"]["details"]["path"]')"
if [[ "$mega_ste_guard_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "mega_ste_profile_guard=failed code=$mega_ste_guard_code" | tee -a "$OUT"
  exit 1
fi

if [[ "$ste_guard_path" == "$mega_ste_guard_path" || "$ste_guard_path" != */ste_pal.json || "$mega_ste_guard_path" != */mega_ste_pal.json ]]; then
  echo "compatibility_delta=failed ste_path=$ste_guard_path mega_ste_path=$mega_ste_guard_path" | tee -a "$OUT"
  exit 1
fi

echo "compatibility_delta=pass code=$ste_guard_code ste_path_suffix=ste_pal.json mega_ste_path_suffix=mega_ste_pal.json" | tee -a "$OUT"

INVALID_CONTROL='{"type":"set_rate_limit","stream":"video","pacing_mode":"mega_ste_delta"}'
call_json "unsupported_extension_control_guard" POST "/api/v2/stream/control" "$INVALID_CONTROL" '^400$'
invalid_control_code="$(extract_json 'd["error"]["code"]')"
if [[ "$invalid_control_code" != "BAD_REQUEST" ]]; then
  echo "unsupported_extension_control_guard=failed code=$invalid_control_code" | tee -a "$OUT"
  exit 1
fi

call_json "unsupported_extension_audio_schema_guard" GET "/api/v2/stream/audio?session_id=ses_local&metadata_schema_version=2" "" '^400$'
invalid_schema_code="$(extract_json 'd["error"]["code"]')"
if [[ "$invalid_schema_code" != "UNSUPPORTED_VERSION" ]]; then
  echo "unsupported_extension_audio_schema_guard=failed code=$invalid_schema_code" | tee -a "$OUT"
  exit 1
fi

call_json "session_stop_for_fallback_guard" POST "/api/v2/engine/session/stop" "{}" '^200$'
call_json "stopped_control_fallback_guard" POST "/api/v2/stream/control" "$VIDEO_CONTROL" '^409$'
stopped_control_code="$(extract_json 'd["error"]["code"]')"
if [[ "$stopped_control_code" != "ENGINE_NOT_RUNNING" ]]; then
  echo "stopped_control_fallback_guard=failed code=$stopped_control_code" | tee -a "$OUT"
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
  "task_id": "S7-005",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "mega_ste_extension_compatibility_matrix": "${MATRIX}",
  "summary": {
    "baseline_machine": "${start_machine}",
    "baseline_profile": "${start_profile}",
    "video_pacing_mode": "${video_mode}",
    "audio_pacing_mode": "${audio_mode}",
    "ste_guard_code": "${ste_guard_code}",
    "mega_ste_guard_code": "${mega_ste_guard_code}",
    "ste_guard_path": "${ste_guard_path}",
    "mega_ste_guard_path": "${mega_ste_guard_path}",
    "unsupported_control_code": "${invalid_control_code}",
    "unsupported_audio_schema_code": "${invalid_schema_code}",
    "stopped_control_guard_code": "${stopped_control_code}"
  }
}
EOF

echo "s7_mega_ste_extension_compatibility=pass baseline=${start_machine}/${start_profile} delta_guard=${mega_ste_guard_code} fallback=${invalid_control_code}" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
