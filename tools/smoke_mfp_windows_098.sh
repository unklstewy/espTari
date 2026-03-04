#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/mfp_windows_098_smoke_postfix_${TS}.txt"
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

call_json "mfp_register_window_ok" GET "/api/v2/inspect/chipset/windows/registers?session_id=ses_local&group=mfp" "" "^200$"
GROUP_VALUE="$(extract_json 'd["data"]["window"]["group"]')"
HAS_REG_FIELDS="$(extract_json 'str(all(k in d["data"]["window"]["registers"][0] for k in ["name","offset","address","width_bits","access"]))')"
if [[ "$GROUP_VALUE" != "mfp" || "$HAS_REG_FIELDS" != "True" ]]; then
  echo "mfp_register_contract=failed group=$GROUP_VALUE reg_fields=$HAS_REG_FIELDS" | tee -a "$OUT"
  exit 1
fi
echo "mfp_register_contract=pass group=$GROUP_VALUE reg_fields=$HAS_REG_FIELDS" | tee -a "$OUT"

call_json "mfp_timers_ok" GET "/api/v2/inspect/chipset/windows/timers?session_id=ses_local&group=mfp" "" "^200$"
TIMER_COUNT="$(extract_json 'len(d["data"]["timers"])')"
HAS_TIMER_A="$(extract_json 'str(any(t["timer_id"]=="A" for t in d["data"]["timers"]))')"
HAS_TIMER_FIELDS="$(extract_json 'str(all(k in d["data"]["timers"][0] for k in ["timer_id","control_register","data_register","prescaler","counter_value","mode","enabled"]))')"
if [[ "$TIMER_COUNT" != "4" || "$HAS_TIMER_A" != "True" || "$HAS_TIMER_FIELDS" != "True" ]]; then
  echo "mfp_timer_contract=failed count=$TIMER_COUNT has_A=$HAS_TIMER_A fields=$HAS_TIMER_FIELDS" | tee -a "$OUT"
  exit 1
fi
echo "mfp_timer_contract=pass count=$TIMER_COUNT has_A=$HAS_TIMER_A fields=$HAS_TIMER_FIELDS" | tee -a "$OUT"

call_json "invalid_group_bad_request" GET "/api/v2/inspect/chipset/windows/timers?session_id=ses_local&group=shifter" "" "^400$"
BAD_REQUEST_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_REQUEST_CODE" != "BAD_REQUEST" ]]; then
  echo "mfp_bad_request_mapping=failed code=$BAD_REQUEST_CODE" | tee -a "$OUT"
  exit 1
fi
echo "mfp_bad_request_mapping=pass code=$BAD_REQUEST_CODE" | tee -a "$OUT"

call_json "unknown_session_engine_not_running" GET "/api/v2/inspect/chipset/windows/registers?session_id=ses_unknown&group=mfp" "" "^409$"
ENGINE_NOT_RUNNING_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ENGINE_NOT_RUNNING_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "mfp_engine_not_running_mapping=failed code=$ENGINE_NOT_RUNNING_CODE" | tee -a "$OUT"
  exit 1
fi
echo "mfp_engine_not_running_mapping=pass code=$ENGINE_NOT_RUNNING_CODE" | tee -a "$OUT"

call_json "invalid_timer_selector" GET "/api/v2/inspect/chipset/windows/timers?session_id=ses_local&group=mfp&timer_id=Z" "" "^400$"
FILTER_INVALID_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$FILTER_INVALID_CODE" != "INSPECT_FILTER_INVALID" ]]; then
  echo "mfp_filter_invalid_mapping=failed code=$FILTER_INVALID_CODE" | tee -a "$OUT"
  exit 1
fi
echo "mfp_filter_invalid_mapping=pass code=$FILTER_INVALID_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"