#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s8_active_pairwise_abi_regression_006_smoke_postfix_${TS}.txt"
BUNDLE="captures/s8_active_pairwise_abi_regression_bundle_006_${TS}.json"
MATRIX="TRACKING/evidence/s8_active_pairwise_abi_regression_matrix_006.json"
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
  local profile="$1"
  call_json "stop_before_${profile}" POST "/api/v2/engine/session/stop" "{}" "^(200|409)$"
  call_json "start_${profile}" POST "/api/v2/engine/session" "{\"machine\":\"atari_st\",\"profile\":\"${profile}\",\"rom_id\":\"rom.atari.st.01\"}" '^200$'
  got_profile="$(extract_json 'd["data"]["profile"]')"
  [[ "$got_profile" == "$profile" ]] || { echo "start_${profile}=failed profile=$got_profile" | tee -a "$OUT"; exit 1; }
}

[[ -f "$MATRIX" ]] || { echo "missing_matrix=$MATRIX" | tee -a "$OUT"; exit 1; }

for p in st_520_pal mega_st_pal ste_pal mega_ste_pal; do
  start_profile "$p"
done

call_json "mismatch_guard" POST "/api/v2/engine/session" '{"machine":"mega_st","profile":"st_520_pal","rom_id":"rom.atari.st.01"}' '^400$'
mismatch_code="$(extract_json 'd["error"]["code"]')"

call_json "unsupported_version_guard" GET "/api/v2/stream/video?session_id=ses_local&metadata_schema_version=2" "" '^400$'
unsupported_code="$(extract_json 'd["error"]["code"]')"

call_json "stop_for_engine_not_running" POST "/api/v2/engine/session/stop" "{}" '^200$'
call_json "engine_not_running_guard" POST "/api/v2/stream/control" '{"type":"set_rate_limit","stream":"video","pacing_mode":"fixed_fps","target_fps":50}' '^409$'
stopped_code="$(extract_json 'd["error"]["code"]')"

[[ "$mismatch_code" == "BAD_REQUEST" && "$unsupported_code" == "UNSUPPORTED_VERSION" && "$stopped_code" == "ENGINE_NOT_RUNNING" ]] || {
  echo "guard_assertions_failed mismatch=$mismatch_code unsupported=$unsupported_code stopped=$stopped_code" | tee -a "$OUT"
  exit 1
}

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S8-006",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "matrix": "${MATRIX}",
  "summary": {
    "pairwise_profiles": ["st_520_pal", "mega_st_pal", "ste_pal", "mega_ste_pal"],
    "mismatch_code": "${mismatch_code}",
    "unsupported_version_code": "${unsupported_code}",
    "engine_not_running_code": "${stopped_code}"
  }
}
EOF

echo "s8_006=pass" | tee -a "$OUT"
