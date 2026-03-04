#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/psg_audio_104_smoke_postfix_${TS}.txt"
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
    echo "Step failed: $name HTTP=$code" | tee -a "$OUT"
    echo "$body" | tee -a "$OUT"
    exit 1
  fi

  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

wait_for_health
call_json "session_reset" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
call_json "session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "psg_registers" GET "/api/v2/inspect/chipset/psg/registers?session_id=ses_local" "" "^200$"
BASE_OK="$(extract_json 'str(d["data"]["base_address"]=="0x00FF8800")')"
WINDOW_OK="$(extract_json 'str(d["data"]["window_bytes"]==16)')"
REGS_OK="$(extract_json 'str(len(d["data"]["registers"])>=4 and all(set(["name","index","value","latched_tick"]).issubset(r.keys()) for r in d["data"]["registers"]))')"
if [[ "$BASE_OK" != "True" || "$WINDOW_OK" != "True" || "$REGS_OK" != "True" ]]; then
  echo "psg_register_window_contract=failed base=$BASE_OK window=$WINDOW_OK regs=$REGS_OK" | tee -a "$OUT"
  exit 1
fi

echo "psg_register_window_contract=pass base=$BASE_OK window=$WINDOW_OK regs=$REGS_OK" | tee -a "$OUT"

call_json "psg_audio" GET "/api/v2/inspect/chipset/psg/audio?session_id=ses_local&limit=3" "" "^200$"
CONF_OK="$(extract_json 'str(all(v=="pass" for v in d["data"]["conformance"].values()))')"
SEQ_OK="$(extract_json 'str(all(curr["frame_seq"]==prev["frame_seq"]+1 for prev,curr in zip(d["data"]["states"], d["data"]["states"][1:])))')"
TS_OK="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["states"], d["data"]["states"][1:])))')"
TICK_OK="$(extract_json 'str(all(curr["tick_counter"]>=prev["tick_counter"] for prev,curr in zip(d["data"]["states"], d["data"]["states"][1:])))')"
LEVEL_OK="$(extract_json 'str(all(0<=s["channel_a_level"]<=15 and 0<=s["channel_b_level"]<=15 and 0<=s["channel_c_level"]<=15 for s in d["data"]["states"]))')"
SHAPE_OK="$(extract_json 'str(all(0<=s["envelope_shape"]<=255 for s in d["data"]["states"]))')"
if [[ "$CONF_OK" != "True" || "$SEQ_OK" != "True" || "$TS_OK" != "True" || "$TICK_OK" != "True" || "$LEVEL_OK" != "True" || "$SHAPE_OK" != "True" ]]; then
  echo "psg_audio_contract=failed conf=$CONF_OK seq=$SEQ_OK ts=$TS_OK tick=$TICK_OK level=$LEVEL_OK shape=$SHAPE_OK" | tee -a "$OUT"
  exit 1
fi

echo "psg_audio_contract=pass conf=$CONF_OK seq=$SEQ_OK ts=$TS_OK tick=$TICK_OK level=$LEVEL_OK shape=$SHAPE_OK" | tee -a "$OUT"

call_json "psg_audio_bad_limit" GET "/api/v2/inspect/chipset/psg/audio?session_id=ses_local&limit=0" "" "^400$"
BAD_LIMIT_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_LIMIT_CODE" != "BAD_REQUEST" ]]; then
  echo "psg_audio_limit_guard=failed code=$BAD_LIMIT_CODE" | tee -a "$OUT"
  exit 1
fi

echo "psg_audio_limit_guard=pass code=$BAD_LIMIT_CODE" | tee -a "$OUT"

call_json "psg_registers_unknown_session" GET "/api/v2/inspect/chipset/psg/registers?session_id=ses_unknown" "" "^409$"
SESSION_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$SESSION_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "psg_registers_session_guard=failed code=$SESSION_CODE" | tee -a "$OUT"
  exit 1
fi

echo "psg_registers_session_guard=pass code=$SESSION_CODE" | tee -a "$OUT"

call_json "psg_aud01_failfast" GET "/api/v2/inspect/chipset/psg/audio?session_id=ses_local&limit=2&force_register_reflection_miss=1" "" "^500$"
AUD01_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$AUD01_CODE" != "INTERNAL_ERROR" ]]; then
  echo "psg_aud01_failfast=failed code=$AUD01_CODE" | tee -a "$OUT"
  exit 1
fi

echo "psg_aud01_failfast=pass code=$AUD01_CODE" | tee -a "$OUT"

call_json "psg_aud02_failfast" GET "/api/v2/inspect/chipset/psg/audio?session_id=ses_local&limit=2&force_frame_seq_gap=1" "" "^500$"
AUD02_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$AUD02_CODE" != "INTERNAL_ERROR" ]]; then
  echo "psg_aud02_failfast=failed code=$AUD02_CODE" | tee -a "$OUT"
  exit 1
fi

echo "psg_aud02_failfast=pass code=$AUD02_CODE" | tee -a "$OUT"

call_json "psg_aud03_failfast" GET "/api/v2/inspect/chipset/psg/audio?session_id=ses_local&limit=2&force_time_regression=1" "" "^500$"
AUD03_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$AUD03_CODE" != "INTERNAL_ERROR" ]]; then
  echo "psg_aud03_failfast=failed code=$AUD03_CODE" | tee -a "$OUT"
  exit 1
fi

echo "psg_aud03_failfast=pass code=$AUD03_CODE" | tee -a "$OUT"

call_json "psg_aud04_failfast" GET "/api/v2/inspect/chipset/psg/audio?session_id=ses_local&limit=2&force_level_overflow=1" "" "^500$"
AUD04_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$AUD04_CODE" != "INTERNAL_ERROR" ]]; then
  echo "psg_aud04_failfast=failed code=$AUD04_CODE" | tee -a "$OUT"
  exit 1
fi

echo "psg_aud04_failfast=pass code=$AUD04_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
