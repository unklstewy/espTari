#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s8_ste_active_extension_controls_003_smoke_postfix_${TS}.txt"
BUNDLE="captures/s8_ste_active_extension_controls_bundle_003_${TS}.json"
MATRIX="TRACKING/evidence/s8_ste_active_extension_controls_matrix_003.json"
PYTHON_BIN="/home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

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
  [[ "$code" =~ $expected ]] || { echo "step_failed=$name http=$code" | tee -a "$OUT"; exit 1; }
  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | "$PYTHON_BIN" -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

[[ -f "$MATRIX" ]] || { echo "missing_matrix=$MATRIX" | tee -a "$OUT"; exit 1; }

call_json "pre_stop" POST "/api/v2/engine/session/stop" "{}" "^(200|409)$"
call_json "start_ste" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"ste_pal","rom_id":"rom.atari.st.01","tos_id":"tos.eu.1.04"}' '^200$'
ste_profile="$(extract_json 'd["data"]["profile"]')"
[[ "$ste_profile" == "ste_pal" ]] || { echo "start_ste=failed profile=$ste_profile" | tee -a "$OUT"; exit 1; }

call_json "stream_control_valid" POST "/api/v2/stream/control" '{"type":"set_rate_limit","stream":"video","pacing_mode":"fixed_fps","target_fps":50,"max_burst_frames":2}' '^200$'
call_json "stream_control_invalid" POST "/api/v2/stream/control" '{"type":"set_rate_limit","stream":"video","pacing_mode":"invalid_mode"}' '^400$'
invalid_code="$(extract_json 'd["error"]["code"]')"
[[ "$invalid_code" == "BAD_REQUEST" ]] || { echo "invalid_control=failed code=$invalid_code" | tee -a "$OUT"; exit 1; }

call_json "stream_video" GET "/api/v2/stream/video?session_id=ses_local" "" '^200$'
call_json "stream_audio" GET "/api/v2/stream/audio?session_id=ses_local" "" '^200$'
call_json "stop" POST "/api/v2/engine/session/stop" "{}" '^200$'

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S8-003",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "matrix": "${MATRIX}",
  "summary": {
    "ste_profile": "${ste_profile}",
    "invalid_control_code": "${invalid_code}"
  }
}
EOF

echo "s8_003=pass" | tee -a "$OUT"
