#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s7_mega_st_media_catalog_003_smoke_postfix_${TS}.txt"
BUNDLE="captures/s7_mega_st_media_catalog_bundle_003_${TS}.json"
MATRIX="TRACKING/evidence/s7_mega_st_media_catalog_matrix_003.json"
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

call_json "catalog_lookup_baseline" GET "/api/v2/catalogs/floppies/entries/disk.automation.a_093" "" '^200$'
catalog_entry_id="$(extract_json 'd["data"]["id"]')"
if [[ "$catalog_entry_id" != "disk.automation.a_093" ]]; then
  echo "catalog_lookup_baseline=failed entry_id=$catalog_entry_id" | tee -a "$OUT"
  exit 1
fi

call_json "media_rom_attach_baseline" POST "/api/v2/media/rom/attach" '{"session_id":"ses_local","rom_id":"rom.atari.st.01"}' '^200$'
rom_result="$(extract_json 'd["data"]["result"]')"
rom_id="$(extract_json 'd["data"]["rom_id"]')"
if [[ "$rom_result" != "applied" || "$rom_id" != "rom.atari.st.01" ]]; then
  echo "media_rom_attach_baseline=failed result=$rom_result rom_id=$rom_id" | tee -a "$OUT"
  exit 1
fi

call_json "media_disk_attach_baseline" POST "/api/v2/media/disk/attach" '{"session_id":"ses_local","drive":"A","disk_id":"disk.automation.a_093","write_protect":true}' '^200$'
disk_result="$(extract_json 'd["data"]["result"]')"
disk_id="$(extract_json 'd["data"]["disk_id"]')"
if [[ "$disk_result" != "active" || "$disk_id" != "disk.automation.a_093" ]]; then
  echo "media_disk_attach_baseline=failed result=$disk_result disk_id=$disk_id" | tee -a "$OUT"
  exit 1
fi

call_json "session_continuity_after_attach" GET "/api/v2/engine/session" "" '^200$'
cont_state="$(extract_json 'd["data"]["state"]')"
cont_machine="$(extract_json 'd["data"]["machine"]')"
cont_profile="$(extract_json 'd["data"]["profile"]')"
if [[ "$cont_state" != "running" || "$cont_machine" != "atari_st" || "$cont_profile" != "st_520_pal" ]]; then
  echo "session_continuity_after_attach=failed state=$cont_state machine=$cont_machine profile=$cont_profile" | tee -a "$OUT"
  exit 1
fi

call_json "media_disk_eject_baseline" POST "/api/v2/media/disk/eject" '{"session_id":"ses_local","drive":"A"}' '^200$'
eject_result="$(extract_json 'd["data"]["result"]')"
if [[ "$eject_result" != "ejected" ]]; then
  echo "media_disk_eject_baseline=failed result=$eject_result" | tee -a "$OUT"
  exit 1
fi

call_json "media_missing_disk_guard" POST "/api/v2/media/disk/attach" '{"session_id":"ses_local","drive":"A","disk_id":"disk.missing.s7_003"}' '^404$'
missing_guard_code="$(extract_json 'd["error"]["code"]')"
if [[ "$missing_guard_code" != "CATALOG_ENTRY_NOT_FOUND" ]]; then
  echo "media_missing_disk_guard=failed code=$missing_guard_code" | tee -a "$OUT"
  exit 1
fi

call_json "media_force_active_fail_guard" POST "/api/v2/media/disk/attach" '{"session_id":"ses_local","drive":"B","disk_id":"disk.automation.a_093","force_active_fail":true}' '^409$'
force_fail_code="$(extract_json 'd["error"]["code"]')"
if [[ "$force_fail_code" != "MEDIA_ATTACH_FAILED" ]]; then
  echo "media_force_active_fail_guard=failed code=$force_fail_code" | tee -a "$OUT"
  exit 1
fi

call_json "mega_st_bootstrap_guard" POST "/api/v2/engine/session" '{"machine":"mega_st","profile":"mega_st_pal","rom_id":"rom.atari.st.01"}' '^404$'
mega_st_code="$(extract_json 'd["error"]["code"]')"
if [[ "$mega_st_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "mega_st_bootstrap_guard=failed code=$mega_st_code" | tee -a "$OUT"
  exit 1
fi

call_json "session_stop_for_media_guard" POST "/api/v2/engine/session/stop" "{}" '^200$'
call_json "media_attach_stopped_guard" POST "/api/v2/media/rom/attach" '{"session_id":"ses_local","rom_id":"rom.atari.st.01"}' '^409$'
stopped_media_code="$(extract_json 'd["error"]["code"]')"
if [[ "$stopped_media_code" != "INVALID_SESSION_STATE" ]]; then
  echo "media_attach_stopped_guard=failed code=$stopped_media_code" | tee -a "$OUT"
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
  "task_id": "S7-003",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "mega_st_media_catalog_matrix": "${MATRIX}",
  "summary": {
    "baseline_machine": "${start_machine}",
    "baseline_profile": "${start_profile}",
    "catalog_entry_id": "${catalog_entry_id}",
    "rom_attach_result": "${rom_result}",
    "disk_attach_result": "${disk_result}",
    "disk_eject_result": "${eject_result}",
    "missing_disk_guard_code": "${missing_guard_code}",
    "force_active_fail_code": "${force_fail_code}",
    "mega_st_bootstrap_code": "${mega_st_code}",
    "stopped_media_guard_code": "${stopped_media_code}"
  }
}
EOF

echo "s7_mega_st_media_catalog=pass baseline=${start_machine}/${start_profile} missing_guard=${missing_guard_code} mega_guard=${mega_st_code}" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"