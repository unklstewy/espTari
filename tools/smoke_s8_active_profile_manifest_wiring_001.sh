#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s8_active_profile_manifest_wiring_001_smoke_postfix_${TS}.txt"
BUNDLE="captures/s8_active_profile_manifest_wiring_bundle_001_${TS}.json"
MATRIX="TRACKING/evidence/s8_active_profile_manifest_wiring_matrix_001.json"
PYTHON_BIN="/home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

wait_for_health() {
  for i in $(seq 1 60); do
    if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
      echo "health_check=ok attempts=${i}" | tee -a "$OUT"
      return 0
    fi
    sleep 1
  done
  echo "health_check=timeout" | tee -a "$OUT"
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
  local got_profile got_state
  got_profile="$(extract_json 'd["data"]["profile"]')"
  got_state="$(extract_json 'd["data"]["state"]')"
  [[ "$got_profile" == "$profile" && "$got_state" == "running" ]] || { echo "start_${label}=failed profile=$got_profile state=$got_state" | tee -a "$OUT"; exit 1; }
}

wait_for_health
[[ -f "$MATRIX" ]] || { echo "missing_matrix=$MATRIX" | tee -a "$OUT"; exit 1; }

start_profile "mega_st" "mega_st_pal"
start_profile "ste" "ste_pal"
start_profile "mega_ste" "mega_ste_pal"

call_json "mismatch_guard" POST "/api/v2/engine/session" '{"machine":"mega_st","profile":"st_520_pal","rom_id":"rom.atari.st.01"}' '^400$'
mismatch_code="$(extract_json 'd["error"]["code"]')"
[[ "$mismatch_code" == "BAD_REQUEST" ]] || { echo "mismatch_guard=failed code=$mismatch_code" | tee -a "$OUT"; exit 1; }

call_json "final_stop" POST "/api/v2/engine/session/stop" "{}" "^200$"

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S8-001",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "matrix": "${MATRIX}",
  "summary": {
    "mega_st_active": true,
    "ste_active": true,
    "mega_ste_active": true,
    "mismatch_guard_code": "${mismatch_code}"
  }
}
EOF

echo "s8_001=pass" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
