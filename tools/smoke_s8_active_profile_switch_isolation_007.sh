#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s8_active_profile_switch_isolation_007_smoke_postfix_${TS}.txt"
BUNDLE="captures/s8_active_profile_switch_isolation_bundle_007_${TS}.json"
MATRIX="TRACKING/evidence/s8_active_profile_switch_isolation_matrix_007.json"
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

switch_to_profile() {
  local profile="$1"
  call_json "stop_before_${profile}" POST "/api/v2/engine/session/stop" "{}" "^(200|409)$"
  call_json "start_${profile}" POST "/api/v2/engine/session" "{\"machine\":\"atari_st\",\"profile\":\"${profile}\",\"rom_id\":\"rom.atari.st.01\"}" '^200$'
  current_profile="$(extract_json 'd["data"]["profile"]')"
  [[ "$current_profile" == "$profile" ]] || { echo "switch_${profile}=failed profile=$current_profile" | tee -a "$OUT"; exit 1; }
  call_json "session_after_${profile}" GET "/api/v2/engine/session" "" '^200$'
  session_profile="$(extract_json 'd["data"]["profile"]')"
  [[ "$session_profile" == "$profile" ]] || { echo "session_after_${profile}=failed profile=$session_profile" | tee -a "$OUT"; exit 1; }
}

[[ -f "$MATRIX" ]] || { echo "missing_matrix=$MATRIX" | tee -a "$OUT"; exit 1; }

switch_to_profile "st_520_pal"
switch_to_profile "ste_pal"
switch_to_profile "mega_ste_pal"
switch_to_profile "mega_st_pal"

call_json "health" GET "/api/v2/engine/health" "" '^200$'
call_json "invalid_fallback_guard" POST "/api/v2/engine/session" '{"machine":"mega_st","profile":"st_520_pal","rom_id":"rom.atari.st.01"}' '^400$'
fallback_code="$(extract_json 'd["error"]["code"]')"
[[ "$fallback_code" == "BAD_REQUEST" ]] || { echo "fallback_guard=failed code=$fallback_code" | tee -a "$OUT"; exit 1; }

call_json "final_stop" POST "/api/v2/engine/session/stop" "{}" '^200$'

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S8-007",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "matrix": "${MATRIX}",
  "summary": {
    "switch_sequence": ["st_520_pal", "ste_pal", "mega_ste_pal", "mega_st_pal"],
    "fallback_code": "${fallback_code}"
  }
}
EOF

echo "s8_007=pass" | tee -a "$OUT"
