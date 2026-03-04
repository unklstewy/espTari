#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/acia_framing_100_smoke_postfix_${TS}.txt"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

AUTH_BEARER="${AUTH_BEARER:-}"
AUTH_HEADER="${AUTH_HEADER:-}"
AUTH_ARGS=()
if [[ -n "$AUTH_BEARER" ]]; then
  AUTH_ARGS+=( -H "Authorization: Bearer ${AUTH_BEARER}" )
fi
if [[ -n "$AUTH_HEADER" ]]; then
  AUTH_ARGS+=( -H "$AUTH_HEADER" )
fi

wait_for_health() {
  local attempts="${1:-60}"
  local interval="${2:-1}"
  local i
  for ((i=1; i<=attempts; i++)); do
    if curl --max-time 2 -sS "${AUTH_ARGS[@]}" "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
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
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path" "${AUTH_ARGS[@]}" -H "Content-Type: application/json" -d "$data")
  else
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path" "${AUTH_ARGS[@]}")
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

call_json "acia_bridge_state" GET "/api/v2/inspect/chipset/acia/bridge?session_id=ses_local" "" "^200$"
BRIDGE_STATE_OK="$(extract_json 'str(d["data"]["bridge_state"] in {"attached","detached","error"})')"
CHANNEL_MODE_OK="$(extract_json 'str(d["data"]["channel_mode"] in {"rx","tx","duplex"})')"
FRAMING_PROFILE_OK="$(extract_json 'str(len(d["data"]["framing_profile"]) > 0)')"
if [[ "$BRIDGE_STATE_OK" != "True" || "$CHANNEL_MODE_OK" != "True" || "$FRAMING_PROFILE_OK" != "True" ]]; then
  echo "acia_bridge_contract=failed bridge=$BRIDGE_STATE_OK mode=$CHANNEL_MODE_OK profile=$FRAMING_PROFILE_OK" | tee -a "$OUT"
  exit 1
fi

echo "acia_bridge_contract=pass bridge=$BRIDGE_STATE_OK mode=$CHANNEL_MODE_OK profile=$FRAMING_PROFILE_OK" | tee -a "$OUT"

call_json "acia_frames_trace_a" GET "/api/v2/inspect/chipset/acia/frames?session_id=ses_local&limit=4" "" "^200$"
CHECKS_PASS="$(extract_json 'str(all(v=="pass" for v in d["data"]["checks"].values()))')"
SHAPE_OK="$(extract_json 'str(all(f["encoding"]=="8N1" and f["start_bit"]==0 and f["stop_bits"]==1 and f["parity"]=="none" for f in d["data"]["frames"]))')"
if [[ "$CHECKS_PASS" != "True" || "$SHAPE_OK" != "True" ]]; then
  echo "acia_frames_contract=failed checks=$CHECKS_PASS shape=$SHAPE_OK" | tee -a "$OUT"
  exit 1
fi

HOST_SEQ_MONO_A="$(extract_json 'str(all(curr["frame_seq"]==prev["frame_seq"]+1 for prev,curr in zip([f for f in d["data"]["frames"] if f["direction"]=="host_to_st"],[f for f in d["data"]["frames"] if f["direction"]=="host_to_st"][1:])))')"
ST_SEQ_MONO_A="$(extract_json 'str(all(curr["frame_seq"]==prev["frame_seq"]+1 for prev,curr in zip([f for f in d["data"]["frames"] if f["direction"]=="st_to_host"],[f for f in d["data"]["frames"] if f["direction"]=="st_to_host"][1:])))')"
HOST_TS_MONO_A="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip([f for f in d["data"]["frames"] if f["direction"]=="host_to_st"],[f for f in d["data"]["frames"] if f["direction"]=="host_to_st"][1:])))')"
ST_TS_MONO_A="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip([f for f in d["data"]["frames"] if f["direction"]=="st_to_host"],[f for f in d["data"]["frames"] if f["direction"]=="st_to_host"][1:])))')"
if [[ "$HOST_SEQ_MONO_A" != "True" || "$ST_SEQ_MONO_A" != "True" || "$HOST_TS_MONO_A" != "True" || "$ST_TS_MONO_A" != "True" ]]; then
  echo "acia_frames_monotonicity_a=failed host_seq=$HOST_SEQ_MONO_A st_seq=$ST_SEQ_MONO_A host_ts=$HOST_TS_MONO_A st_ts=$ST_TS_MONO_A" | tee -a "$OUT"
  exit 1
fi

echo "acia_frames_monotonicity_a=pass host_seq=$HOST_SEQ_MONO_A st_seq=$ST_SEQ_MONO_A host_ts=$HOST_TS_MONO_A st_ts=$ST_TS_MONO_A" | tee -a "$OUT"

call_json "acia_frames_bad_limit" GET "/api/v2/inspect/chipset/acia/frames?session_id=ses_local&limit=0" "" "^400$"
BAD_LIMIT_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_LIMIT_CODE" != "BAD_REQUEST" ]]; then
  echo "acia_frames_limit_guard=failed code=$BAD_LIMIT_CODE" | tee -a "$OUT"
  exit 1
fi

echo "acia_frames_limit_guard=pass code=$BAD_LIMIT_CODE" | tee -a "$OUT"

call_json "acia_frames_unknown_session" GET "/api/v2/inspect/chipset/acia/frames?session_id=ses_unknown&limit=2" "" "^409$"
ENGINE_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ENGINE_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "acia_frames_session_guard=failed code=$ENGINE_CODE" | tee -a "$OUT"
  exit 1
fi

echo "acia_frames_session_guard=pass code=$ENGINE_CODE" | tee -a "$OUT"

call_json "acia_bridge_unavailable_failfast" GET "/api/v2/inspect/chipset/acia/bridge?session_id=ses_local&force_bridge_unavailable=1" "" "^500$"
BRIDGE_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BRIDGE_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "acia_bridge_failfast=failed code=$BRIDGE_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "acia_bridge_failfast=pass code=$BRIDGE_FAIL_CODE" | tee -a "$OUT"

call_json "acia_frames_seq_failfast" GET "/api/v2/inspect/chipset/acia/frames?session_id=ses_local&limit=2&force_frame_regression=1" "" "^500$"
SEQ_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$SEQ_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "acia_frm01_failfast=failed code=$SEQ_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "acia_frm01_failfast=pass code=$SEQ_FAIL_CODE" | tee -a "$OUT"

call_json "acia_frames_ts_failfast" GET "/api/v2/inspect/chipset/acia/frames?session_id=ses_local&limit=2&force_timestamp_regression=1" "" "^500$"
TS_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$TS_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "acia_frm02_failfast=failed code=$TS_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "acia_frm02_failfast=pass code=$TS_FAIL_CODE" | tee -a "$OUT"

call_json "acia_frames_shape_failfast" GET "/api/v2/inspect/chipset/acia/frames?session_id=ses_local&limit=2&force_framing_invalid=1" "" "^500$"
SHAPE_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$SHAPE_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "acia_frm03_failfast=failed code=$SHAPE_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "acia_frm03_failfast=pass code=$SHAPE_FAIL_CODE" | tee -a "$OUT"

call_json "acia_frames_rejected_failfast" GET "/api/v2/inspect/chipset/acia/frames?session_id=ses_local&limit=2&force_rejected_emitted=1" "" "^500$"
REJECT_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$REJECT_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "acia_frm04_failfast=failed code=$REJECT_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "acia_frm04_failfast=pass code=$REJECT_FAIL_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
