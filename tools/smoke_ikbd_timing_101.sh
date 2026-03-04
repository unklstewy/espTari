#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ikbd_timing_101_smoke_postfix_${TS}.txt"
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

call_json "ikbd_bridge_state" GET "/api/v2/inspect/chipset/ikbd/bridge?session_id=ses_local" "" "^200$"
BRIDGE_STATE_OK="$(extract_json 'str(d["data"]["bridge_state"] in {"attached","detached","error"})')"
PARSER_MODE_OK="$(extract_json 'str(d["data"]["parser_mode"] == "atari_st_ikbd")')"
ACIA_MODE_OK="$(extract_json 'str(d["data"]["acia_channel_mode"] in {"rx","tx","duplex"})')"
if [[ "$BRIDGE_STATE_OK" != "True" || "$PARSER_MODE_OK" != "True" || "$ACIA_MODE_OK" != "True" ]]; then
  echo "ikbd_bridge_contract=failed bridge=$BRIDGE_STATE_OK parser=$PARSER_MODE_OK acia=$ACIA_MODE_OK" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_bridge_contract=pass bridge=$BRIDGE_STATE_OK parser=$PARSER_MODE_OK acia=$ACIA_MODE_OK" | tee -a "$OUT"

call_json "ikbd_packets_keyboard" GET "/api/v2/inspect/chipset/ikbd/packets?session_id=ses_local&limit=4&packet_type=keyboard_scancode" "" "^200$"
CHECKS_PASS_KB="$(extract_json 'str(all(v=="pass" for v in d["data"]["checks"].values()))')"
TYPE_OK_KB="$(extract_json 'str(all(p["packet_type"]=="keyboard_scancode" for p in d["data"]["packets"]))')"
SEQ_OK_KB="$(extract_json 'str(all(curr["packet_seq"]==prev["packet_seq"]+1 for prev,curr in zip(d["data"]["packets"], d["data"]["packets"][1:])))')"
TS_OK_KB="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["packets"], d["data"]["packets"][1:])))')"
GAP_OK_KB="$(extract_json 'str(all(curr["inter_packet_gap_us"]==(curr["timestamp_us"]-prev["timestamp_us"]) for prev,curr in zip(d["data"]["packets"], d["data"]["packets"][1:])))')"
ACIA_SEQ_OK_KB="$(extract_json 'str(all(curr["acia_frame_seq"]>prev["acia_frame_seq"] for prev,curr in zip(d["data"]["packets"], d["data"]["packets"][1:])))')"
if [[ "$CHECKS_PASS_KB" != "True" || "$TYPE_OK_KB" != "True" || "$SEQ_OK_KB" != "True" || "$TS_OK_KB" != "True" || "$GAP_OK_KB" != "True" || "$ACIA_SEQ_OK_KB" != "True" ]]; then
  echo "ikbd_keyboard_contract=failed checks=$CHECKS_PASS_KB type=$TYPE_OK_KB seq=$SEQ_OK_KB ts=$TS_OK_KB gap=$GAP_OK_KB acia=$ACIA_SEQ_OK_KB" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_keyboard_contract=pass checks=$CHECKS_PASS_KB type=$TYPE_OK_KB seq=$SEQ_OK_KB ts=$TS_OK_KB gap=$GAP_OK_KB acia=$ACIA_SEQ_OK_KB" | tee -a "$OUT"

call_json "ikbd_packets_mouse" GET "/api/v2/inspect/chipset/ikbd/packets?session_id=ses_local&limit=4&packet_type=mouse_packet" "" "^200$"
CHECKS_PASS_MS="$(extract_json 'str(all(v=="pass" for v in d["data"]["checks"].values()))')"
TYPE_OK_MS="$(extract_json 'str(all(p["packet_type"]=="mouse_packet" for p in d["data"]["packets"]))')"
SEQ_OK_MS="$(extract_json 'str(all(curr["packet_seq"]==prev["packet_seq"]+1 for prev,curr in zip(d["data"]["packets"], d["data"]["packets"][1:])))')"
TS_OK_MS="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["packets"], d["data"]["packets"][1:])))')"
GAP_OK_MS="$(extract_json 'str(all(curr["inter_packet_gap_us"]==(curr["timestamp_us"]-prev["timestamp_us"]) for prev,curr in zip(d["data"]["packets"], d["data"]["packets"][1:])))')"
ACIA_SEQ_OK_MS="$(extract_json 'str(all(curr["acia_frame_seq"]>prev["acia_frame_seq"] for prev,curr in zip(d["data"]["packets"], d["data"]["packets"][1:])))')"
if [[ "$CHECKS_PASS_MS" != "True" || "$TYPE_OK_MS" != "True" || "$SEQ_OK_MS" != "True" || "$TS_OK_MS" != "True" || "$GAP_OK_MS" != "True" || "$ACIA_SEQ_OK_MS" != "True" ]]; then
  echo "ikbd_mouse_contract=failed checks=$CHECKS_PASS_MS type=$TYPE_OK_MS seq=$SEQ_OK_MS ts=$TS_OK_MS gap=$GAP_OK_MS acia=$ACIA_SEQ_OK_MS" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_mouse_contract=pass checks=$CHECKS_PASS_MS type=$TYPE_OK_MS seq=$SEQ_OK_MS ts=$TS_OK_MS gap=$GAP_OK_MS acia=$ACIA_SEQ_OK_MS" | tee -a "$OUT"

call_json "ikbd_packets_missing_type" GET "/api/v2/inspect/chipset/ikbd/packets?session_id=ses_local&limit=2" "" "^400$"
MISSING_TYPE_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MISSING_TYPE_CODE" != "BAD_REQUEST" ]]; then
  echo "ikbd_packet_type_guard=failed code=$MISSING_TYPE_CODE" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_packet_type_guard=pass code=$MISSING_TYPE_CODE" | tee -a "$OUT"

call_json "ikbd_packets_unknown_session" GET "/api/v2/inspect/chipset/ikbd/packets?session_id=ses_unknown&limit=2&packet_type=keyboard_scancode" "" "^409$"
UNKNOWN_SESSION_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$UNKNOWN_SESSION_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "ikbd_session_guard=failed code=$UNKNOWN_SESSION_CODE" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_session_guard=pass code=$UNKNOWN_SESSION_CODE" | tee -a "$OUT"

call_json "ikbd_bridge_unavailable_failfast" GET "/api/v2/inspect/chipset/ikbd/bridge?session_id=ses_local&force_parser_unavailable=1" "" "^500$"
BRIDGE_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BRIDGE_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "ikbd_bridge_failfast=failed code=$BRIDGE_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_bridge_failfast=pass code=$BRIDGE_FAIL_CODE" | tee -a "$OUT"

call_json "ikbd_pkt01_failfast" GET "/api/v2/inspect/chipset/ikbd/packets?session_id=ses_local&limit=2&packet_type=keyboard_scancode&force_sequence_regression=1" "" "^500$"
PKT01_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$PKT01_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "ikbd_pkt01_failfast=failed code=$PKT01_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_pkt01_failfast=pass code=$PKT01_FAIL_CODE" | tee -a "$OUT"

call_json "ikbd_pkt02_failfast" GET "/api/v2/inspect/chipset/ikbd/packets?session_id=ses_local&limit=2&packet_type=keyboard_scancode&force_timestamp_regression=1" "" "^500$"
PKT02_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$PKT02_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "ikbd_pkt02_failfast=failed code=$PKT02_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_pkt02_failfast=pass code=$PKT02_FAIL_CODE" | tee -a "$OUT"

call_json "ikbd_pkt03_failfast" GET "/api/v2/inspect/chipset/ikbd/packets?session_id=ses_local&limit=2&packet_type=keyboard_scancode&force_gap_mismatch=1" "" "^500$"
PKT03_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$PKT03_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "ikbd_pkt03_failfast=failed code=$PKT03_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_pkt03_failfast=pass code=$PKT03_FAIL_CODE" | tee -a "$OUT"

call_json "ikbd_pkt04_failfast" GET "/api/v2/inspect/chipset/ikbd/packets?session_id=ses_local&limit=2&packet_type=keyboard_scancode&force_unresolved_frame=1" "" "^500$"
PKT04_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$PKT04_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "ikbd_pkt04_failfast=failed code=$PKT04_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "ikbd_pkt04_failfast=pass code=$PKT04_FAIL_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
