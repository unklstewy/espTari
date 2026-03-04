#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s8_mega_ste_active_extension_delta_004_smoke_postfix_${TS}.txt"
BUNDLE="captures/s8_mega_ste_active_extension_delta_bundle_004_${TS}.json"
MATRIX="TRACKING/evidence/s8_mega_ste_active_extension_delta_matrix_004.json"
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
call_json "start_ste" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"ste_pal","rom_id":"rom.atari.st.01"}' '^200$'
ste_profile="$(extract_json 'd["data"]["profile"]')"
call_json "stop_ste" POST "/api/v2/engine/session/stop" "{}" '^200$'

call_json "start_mega_ste" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"mega_ste_pal","rom_id":"rom.atari.st.01"}' '^200$'
mega_ste_profile="$(extract_json 'd["data"]["profile"]')"
[[ "$ste_profile" == "ste_pal" && "$mega_ste_profile" == "mega_ste_pal" ]] || { echo "profile_delta=failed ste=$ste_profile mega_ste=$mega_ste_profile" | tee -a "$OUT"; exit 1; }

call_json "unsupported_extension_fallback" POST "/api/v2/stream/control" '{"type":"set_rate_limit","stream":"video","pacing_mode":"__unsupported__"}' '^400$'
fallback_code="$(extract_json 'd["error"]["code"]')"
[[ "$fallback_code" == "BAD_REQUEST" ]] || { echo "fallback=failed code=$fallback_code" | tee -a "$OUT"; exit 1; }

call_json "stop" POST "/api/v2/engine/session/stop" "{}" '^200$'

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S8-004",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "matrix": "${MATRIX}",
  "summary": {
    "ste_profile": "${ste_profile}",
    "mega_ste_profile": "${mega_ste_profile}",
    "fallback_code": "${fallback_code}"
  }
}
EOF

echo "s8_004=pass" | tee -a "$OUT"
