#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s7_cross_profile_abi_regression_006_smoke_postfix_${TS}.txt"
BUNDLE="captures/s7_cross_profile_abi_regression_bundle_006_${TS}.json"
MATRIX="TRACKING/evidence/s7_cross_profile_abi_regression_matrix_006.json"
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

BASELINE='{"machine":"atari_st","profile":"st_520_pal","rom_id":"rom.atari.st.01","tos_id":"tos.eu.1.04","disk_ids":["disk.automation.a_093"]}'
call_json "baseline_start_atari_st" POST "/api/v2/engine/session" "$BASELINE" '^200$'
base_machine="$(extract_json 'd["data"]["machine"]')"
base_profile="$(extract_json 'd["data"]["profile"]')"
base_state="$(extract_json 'd["data"]["state"]')"
if [[ "$base_machine" != "atari_st" || "$base_profile" != "st_520_pal" || "$base_state" != "running" ]]; then
  echo "baseline_start=failed machine=$base_machine profile=$base_profile state=$base_state" | tee -a "$OUT"
  exit 1
fi

call_json "guard_ste_profile_not_found" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"ste_pal","rom_id":"rom.atari.st.01"}' '^404$'
ste_code="$(extract_json 'd["error"]["code"]')"
if [[ "$ste_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "guard_ste_profile_not_found=failed code=$ste_code" | tee -a "$OUT"
  exit 1
fi

call_json "guard_mega_st_profile_not_found" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"mega_st_pal","rom_id":"rom.atari.st.01"}' '^404$'
mega_st_code="$(extract_json 'd["error"]["code"]')"
if [[ "$mega_st_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "guard_mega_st_profile_not_found=failed code=$mega_st_code" | tee -a "$OUT"
  exit 1
fi

call_json "guard_mega_ste_profile_not_found" POST "/api/v2/engine/session" '{"machine":"atari_st","profile":"mega_ste_pal","rom_id":"rom.atari.st.01"}' '^404$'
mega_ste_code="$(extract_json 'd["error"]["code"]')"
if [[ "$mega_ste_code" != "MACHINE_PROFILE_NOT_FOUND" ]]; then
  echo "guard_mega_ste_profile_not_found=failed code=$mega_ste_code" | tee -a "$OUT"
  exit 1
fi

echo "profile_guard_matrix=pass ste=$ste_code mega_st=$mega_st_code mega_ste=$mega_ste_code" | tee -a "$OUT"

call_json "guard_machine_profile_mismatch" POST "/api/v2/engine/session" '{"machine":"mega_st","profile":"st_520_pal","rom_id":"rom.atari.st.01"}' '^400$'
mismatch_code="$(extract_json 'd["error"]["code"]')"
mismatch_reason="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$mismatch_code" != "BAD_REQUEST" || "$mismatch_reason" != "machine/profile mismatch" ]]; then
  echo "guard_machine_profile_mismatch=failed code=$mismatch_code reason=$mismatch_reason" | tee -a "$OUT"
  exit 1
fi

call_json "guard_resolver_unsupported_machine" POST "/api/v2/ebins/resolve" '{"machine":"mega_st","components":["cpu"],"version_policy":"latest_compatible"}' '^404$'
resolve_code="$(extract_json 'd["error"]["code"]')"
resolve_reason="$(extract_json 'd["error"]["details"]["reason"]')"
if [[ "$resolve_code" != "EBIN_NOT_FOUND" || "$resolve_reason" != "resolver_machine_not_indexed" ]]; then
  echo "guard_resolver_unsupported_machine=failed code=$resolve_code reason=$resolve_reason" | tee -a "$OUT"
  exit 1
fi

ABI_MISMATCH='{"module_id":"st.cpu.m68k","module_type":"cpu","machine_targets":["atari_st"],"abi_version":"2.0.0","api_contract_version":"1.0.0","exports":["init"],"dependencies":[],"build_fingerprint":"build_s7_006","payload_sha256":"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"}'
call_json "guard_abi_mismatch_regression" POST "/api/v2/ebins/validate" "$ABI_MISMATCH" '^409$'
abi_code="$(extract_json 'd["error"]["code"]')"
if [[ "$abi_code" != "EBIN_ABI_MISMATCH" ]]; then
  echo "guard_abi_mismatch_regression=failed code=$abi_code" | tee -a "$OUT"
  exit 1
fi

call_json "session_envelope_regression" GET "/api/v2/engine/session" "" '^200$'
session_fields_ok="$(extract_json 'str(all(k in d["data"] for k in ["session_id","state","run_mode","machine","profile","tick_counter","cycle_counter"]))')"
if [[ "$session_fields_ok" != "True" ]]; then
  echo "session_envelope_regression=failed" | tee -a "$OUT"
  exit 1
fi

call_json "stream_video_envelope_regression" GET "/api/v2/stream/video?session_id=ses_local" "" '^200$'
video_fields_ok="$(extract_json 'str(all(k in d["data"] for k in ["stream","event_seq","event_timestamp_us","delivery","backpressure","video_metadata_contract","video_frame_meta_sample"]))')"
if [[ "$video_fields_ok" != "True" ]]; then
  echo "stream_video_envelope_regression=failed" | tee -a "$OUT"
  exit 1
fi

call_json "stream_audio_envelope_regression" GET "/api/v2/stream/audio?session_id=ses_local" "" '^200$'
audio_fields_ok="$(extract_json 'str(all(k in d["data"] for k in ["stream","event_seq","event_timestamp_us","delivery","backpressure","audio_metadata_contract","audio_chunk_meta_sample"]))')"
if [[ "$audio_fields_ok" != "True" ]]; then
  echo "stream_audio_envelope_regression=failed" | tee -a "$OUT"
  exit 1
fi

call_json "video_schema_guard_regression" GET "/api/v2/stream/video?session_id=ses_local&metadata_schema_version=2" "" '^400$'
video_schema_code="$(extract_json 'd["error"]["code"]')"
if [[ "$video_schema_code" != "UNSUPPORTED_VERSION" ]]; then
  echo "video_schema_guard_regression=failed code=$video_schema_code" | tee -a "$OUT"
  exit 1
fi

call_json "audio_schema_guard_regression" GET "/api/v2/stream/audio?session_id=ses_local&metadata_schema_version=2" "" '^400$'
audio_schema_code="$(extract_json 'd["error"]["code"]')"
if [[ "$audio_schema_code" != "UNSUPPORTED_VERSION" ]]; then
  echo "audio_schema_guard_regression=failed code=$audio_schema_code" | tee -a "$OUT"
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
  "task_id": "S7-006",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "cross_profile_abi_regression_matrix": "${MATRIX}",
  "summary": {
    "baseline_machine": "${base_machine}",
    "baseline_profile": "${base_profile}",
    "ste_guard_code": "${ste_code}",
    "mega_st_guard_code": "${mega_st_code}",
    "mega_ste_guard_code": "${mega_ste_code}",
    "mismatch_guard_code": "${mismatch_code}",
    "resolver_guard_code": "${resolve_code}",
    "abi_guard_code": "${abi_code}",
    "video_schema_guard_code": "${video_schema_code}",
    "audio_schema_guard_code": "${audio_schema_code}"
  }
}
EOF

echo "s7_cross_profile_abi_regression=pass baseline=${base_machine}/${base_profile} profile_guards=${ste_code},${mega_st_code},${mega_ste_code} abi_guard=${abi_code}" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
