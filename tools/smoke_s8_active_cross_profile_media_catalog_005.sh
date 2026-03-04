#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s8_active_cross_profile_media_catalog_005_smoke_postfix_${TS}.txt"
BUNDLE="captures/s8_active_cross_profile_media_catalog_bundle_005_${TS}.json"
MATRIX="TRACKING/evidence/s8_active_cross_profile_media_catalog_matrix_005.json"
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

start_profile() {
  local label="$1" profile="$2"
  call_json "stop_before_${label}" POST "/api/v2/engine/session/stop" "{}" "^(200|409)$"
  call_json "start_${label}" POST "/api/v2/engine/session" "{\"machine\":\"atari_st\",\"profile\":\"${profile}\",\"rom_id\":\"rom.atari.st.01\",\"tos_id\":\"tos.eu.1.04\",\"disk_ids\":[\"disk.automation.a_093\"]}" '^200$'
  got_profile="$(extract_json 'd["data"]["profile"]')"
  [[ "$got_profile" == "$profile" ]] || { echo "start_${label}=failed profile=$got_profile" | tee -a "$OUT"; exit 1; }
  call_json "stream_video_${label}" GET "/api/v2/stream/video?session_id=ses_local" "" '^200$'
  call_json "stream_audio_${label}" GET "/api/v2/stream/audio?session_id=ses_local" "" '^200$'
}

[[ -f "$MATRIX" ]] || { echo "missing_matrix=$MATRIX" | tee -a "$OUT"; exit 1; }

start_profile "atari_st" "st_520_pal"
start_profile "mega_st" "mega_st_pal"
start_profile "ste" "ste_pal"
start_profile "mega_ste" "mega_ste_pal"

call_json "media_blocker_missing_rom" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"st_520_pal","rom_id":"rom.missing.entry"}' '^404$'
media_blocker_code="$(extract_json 'd["error"]["code"]')"
[[ "$media_blocker_code" == "CATALOG_ENTRY_NOT_FOUND" ]] || { echo "media_blocker=failed code=$media_blocker_code" | tee -a "$OUT"; exit 1; }

call_json "stop" POST "/api/v2/engine/session/stop" "{}" "^(200|409)$"

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S8-005",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "matrix": "${MATRIX}",
  "summary": {
    "profiles_active": ["st_520_pal", "mega_st_pal", "ste_pal", "mega_ste_pal"],
    "media_blocker_code": "${media_blocker_code}"
  }
}
EOF

echo "s8_005=pass" | tee -a "$OUT"
