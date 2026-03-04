#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s8_mega_st_active_lifecycle_002_smoke_postfix_${TS}.txt"
BUNDLE="captures/s8_mega_st_active_lifecycle_bundle_002_${TS}.json"
MATRIX="TRACKING/evidence/s8_mega_st_active_lifecycle_matrix_002.json"
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
call_json "start_mega_st" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"mega_st_pal","rom_id":"rom.atari.st.01","tos_id":"tos.eu.1.04","disk_ids":["disk.automation.a_093"]}' '^200$'
start_profile="$(extract_json 'd["data"]["profile"]')"
[[ "$start_profile" == "mega_st_pal" ]] || { echo "start_profile=failed profile=$start_profile" | tee -a "$OUT"; exit 1; }

call_json "pause" POST "/api/v2/engine/session/pause" "{}" '^200$'
call_json "resume" POST "/api/v2/engine/session/resume" '{"resume_mode":"running"}' '^200$'
call_json "reset" POST "/api/v2/engine/session/reset" '{"mode":"warm","preserve_media":true}' '^200$'
reset_state="$(extract_json 'd["data"]["state"]')"
[[ "$reset_state" == "running" ]] || { echo "reset_state=failed state=$reset_state" | tee -a "$OUT"; exit 1; }

call_json "session_state" GET "/api/v2/engine/session" "" '^200$'
session_profile="$(extract_json 'd["data"]["profile"]')"
[[ "$session_profile" == "mega_st_pal" ]] || { echo "session_profile=failed profile=$session_profile" | tee -a "$OUT"; exit 1; }

call_json "stop" POST "/api/v2/engine/session/stop" "{}" '^200$'

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S8-002",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "matrix": "${MATRIX}",
  "summary": {
    "start_profile": "${start_profile}",
    "reset_state": "${reset_state}",
    "session_profile": "${session_profile}"
  }
}
EOF

echo "s8_002=pass" | tee -a "$OUT"
