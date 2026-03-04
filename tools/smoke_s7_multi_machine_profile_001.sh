#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s7_multi_machine_profile_001_smoke_postfix_${TS}.txt"
BUNDLE="captures/s7_multi_machine_profile_bundle_001_${TS}.json"
MATRIX="TRACKING/evidence/s7_multi_machine_profile_matrix_001.json"
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

call_json "session_stop_pre" POST "/api/v2/engine/session/stop" "" "^(200|409)$"

VALID_START_PAYLOAD='{"machine":"atari_st","profile":"st_520_pal","rom_id":"rom.atari.st.01","tos_id":"tos.eu.1.04","disk_ids":["disk.automation.a_093"]}'
call_json "baseline_start_atari_st" POST "/api/v2/engine/session" "$VALID_START_PAYLOAD" '^200$'
start_machine="$(extract_json 'd["data"]["machine"]')"
start_profile="$(extract_json 'd["data"]["profile"]')"
manifest_ok="$(extract_json 'str(d["data"]["profile_manifest_validation"]["schema_valid"])')"
wiring_ok="$(extract_json 'str(d["data"]["profile_wiring_validation"]["wiring_valid"])')"
validated_modules="$(extract_json 'int(d["data"]["profile_wiring_validation"]["validated_modules"])')"
if [[ "$start_machine" != "atari_st" || "$start_profile" != "st_520_pal" || "$manifest_ok" != "True" || "$wiring_ok" != "True" || "$validated_modules" -ne 5 ]]; then
  echo "baseline_start_atari_st=failed machine=$start_machine profile=$start_profile manifest_ok=$manifest_ok wiring_ok=$wiring_ok validated_modules=$validated_modules" | tee -a "$OUT"
  exit 1
fi
echo "baseline_start_atari_st=pass machine=$start_machine profile=$start_profile validated_modules=$validated_modules" | tee -a "$OUT"

call_json "session_state_parity" GET "/api/v2/engine/session" "" '^200$'
state_machine="$(extract_json 'd["data"]["machine"]')"
state_profile="$(extract_json 'd["data"]["profile"]')"
state_value="$(extract_json 'd["data"]["state"]')"
if [[ "$state_machine" != "atari_st" || "$state_profile" != "st_520_pal" || "$state_value" != "running" ]]; then
  echo "session_state_parity=failed machine=$state_machine profile=$state_profile state=$state_value" | tee -a "$OUT"
  exit 1
fi
echo "session_state_parity=pass machine=$state_machine profile=$state_profile state=$state_value" | tee -a "$OUT"

call_json "resolve_atari_st_components" POST "/api/v2/ebins/resolve" '{"machine":"atari_st","components":["cpu","video"],"version_policy":"latest_compatible"}' '^200$'
resolve_machine="$(extract_json 'd["data"]["machine"]')"
resolve_components="$(extract_json 'int(d["data"]["resolved_count"])')"
if [[ "$resolve_machine" != "atari_st" || "$resolve_components" -lt 2 ]]; then
  echo "resolve_atari_st_components=failed machine=$resolve_machine components=$resolve_components" | tee -a "$OUT"
  exit 1
fi
echo "resolve_atari_st_components=pass machine=$resolve_machine components=$resolve_components" | tee -a "$OUT"

call_json "mega_st_machine_guard" POST "/api/v2/engine/session" '{"machine":"mega_st","profile":"st_520_pal","rom_id":"rom.atari.st.01"}' '^400$'
mega_st_guard_code="$(extract_json 'd["error"]["code"]')"
mega_st_guard_reason="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$mega_st_guard_code" != "BAD_REQUEST" || "$mega_st_guard_reason" != "machine/profile mismatch" ]]; then
  echo "mega_st_machine_guard=failed code=$mega_st_guard_code reason=$mega_st_guard_reason" | tee -a "$OUT"
  exit 1
fi
echo "mega_st_machine_guard=pass code=$mega_st_guard_code reason=$mega_st_guard_reason" | tee -a "$OUT"

call_json "mega_st_profile_not_found" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"mega_st_pal","rom_id":"rom.atari.st.01"}' '^404$'
mega_st_profile_code="$(extract_json 'd["error"]["code"]')"
if [[ "$mega_st_profile_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "mega_st_profile_not_found=failed code=$mega_st_profile_code" | tee -a "$OUT"
  exit 1
fi
echo "mega_st_profile_not_found=pass code=$mega_st_profile_code" | tee -a "$OUT"

call_json "ste_profile_not_found" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"ste_pal","rom_id":"rom.atari.st.01"}' '^404$'
ste_profile_code="$(extract_json 'd["error"]["code"]')"
if [[ "$ste_profile_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "ste_profile_not_found=failed code=$ste_profile_code" | tee -a "$OUT"
  exit 1
fi
echo "ste_profile_not_found=pass code=$ste_profile_code" | tee -a "$OUT"

call_json "mega_ste_profile_not_found" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"mega_ste_pal","rom_id":"rom.atari.st.01"}' '^404$'
mega_ste_profile_code="$(extract_json 'd["error"]["code"]')"
if [[ "$mega_ste_profile_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "mega_ste_profile_not_found=failed code=$mega_ste_profile_code" | tee -a "$OUT"
  exit 1
fi
echo "mega_ste_profile_not_found=pass code=$mega_ste_profile_code" | tee -a "$OUT"

call_json "resolve_unsupported_machine" POST "/api/v2/ebins/resolve" '{"machine":"mega_st","components":["cpu"],"version_policy":"latest_compatible"}' '^404$'
resolve_guard_code="$(extract_json 'd["error"]["code"]')"
resolve_guard_reason="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$resolve_guard_code" != "EBIN_NOT_FOUND" || "$resolve_guard_reason" != "resolver_machine_not_indexed" ]]; then
  echo "resolve_unsupported_machine=failed code=$resolve_guard_code reason=$resolve_guard_reason" | tee -a "$OUT"
  exit 1
fi
echo "resolve_unsupported_machine=pass code=$resolve_guard_code reason=$resolve_guard_reason" | tee -a "$OUT"

call_json "post_checks_health" GET "/api/v2/engine/health" "" '^200$'
post_ok="$(extract_json 'str(d.get("ok") is True)')"
[[ "$post_ok" == "True" ]] || { echo "post_checks_health=failed" | tee -a "$OUT"; exit 1; }

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S7-001",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "multi_machine_matrix": "${MATRIX}",
  "summary": {
    "baseline_machine": "${start_machine}",
    "baseline_profile": "${start_profile}",
    "validated_modules": ${validated_modules},
    "session_state": "${state_value}",
    "resolve_components": ${resolve_components},
    "mega_st_machine_guard_code": "${mega_st_guard_code}",
    "mega_st_profile_code": "${mega_st_profile_code}",
    "ste_profile_code": "${ste_profile_code}",
    "mega_ste_profile_code": "${mega_ste_profile_code}",
    "resolve_unsupported_machine_code": "${resolve_guard_code}"
  }
}
EOF

echo "s7_multi_machine_profile=pass baseline_machine=$start_machine baseline_profile=$start_profile resolve_components=$resolve_components" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"