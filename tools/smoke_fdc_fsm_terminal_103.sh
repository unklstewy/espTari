#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/fdc_fsm_terminal_103_smoke_postfix_${TS}.txt"
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

call_json "fdc_fsm_ok" GET "/api/v2/inspect/chipset/fdc/fsm?session_id=ses_local" "" "^200$"
FSM_STATE="$(extract_json 'd["data"]["fsm_state"]')"
HAS_FSM_FIELDS="$(extract_json 'str(all(k in d["data"] for k in ["active_command","command_seq","status_register","busy","drq","intrq","last_transition_tick","last_transition_us"]))')"
if [[ "$FSM_STATE" != "executing" || "$HAS_FSM_FIELDS" != "True" ]]; then
  echo "fdc_fsm_contract=failed state=$FSM_STATE has_fields=$HAS_FSM_FIELDS" | tee -a "$OUT"
  exit 1
fi
echo "fdc_fsm_contract=pass state=$FSM_STATE has_fields=$HAS_FSM_FIELDS" | tee -a "$OUT"

call_json "fdc_terminal_ok" GET "/api/v2/inspect/chipset/fdc/terminal?session_id=ses_local&limit=2" "" "^200$"
EVENT_COUNT="$(extract_json 'len(d["data"]["events"])')"
SEQ_DELTA="$(extract_json 'd["data"]["events"][1]["event_seq"] - d["data"]["events"][0]["event_seq"]')"
TS_MONO="$(extract_json 'str(d["data"]["events"][1]["timestamp_us"] >= d["data"]["events"][0]["timestamp_us"])')"
TICK_MONO="$(extract_json 'str(d["data"]["events"][1]["tick_counter"] >= d["data"]["events"][0]["tick_counter"])')"
TERMINAL_SIGNAL_RULES="$(extract_json 'str(all((e["busy"] is False and e["intrq"] is True) for e in d["data"]["events"]))')"

if [[ "$EVENT_COUNT" != "2" || "$SEQ_DELTA" != "1" || "$TS_MONO" != "True" || "$TICK_MONO" != "True" || "$TERMINAL_SIGNAL_RULES" != "True" ]]; then
  echo "fdc_terminal_contract=failed count=$EVENT_COUNT seq_delta=$SEQ_DELTA ts_mono=$TS_MONO tick_mono=$TICK_MONO terminal_signals=$TERMINAL_SIGNAL_RULES" | tee -a "$OUT"
  exit 1
fi
echo "fdc_terminal_contract=pass count=$EVENT_COUNT seq_delta=$SEQ_DELTA ts_mono=$TS_MONO tick_mono=$TICK_MONO terminal_signals=$TERMINAL_SIGNAL_RULES" | tee -a "$OUT"

call_json "fdc_missing_limit" GET "/api/v2/inspect/chipset/fdc/terminal?session_id=ses_local" "" "^400$"
BAD_REQUEST_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_REQUEST_CODE" != "BAD_REQUEST" ]]; then
  echo "fdc_bad_request_mapping=failed code=$BAD_REQUEST_CODE" | tee -a "$OUT"
  exit 1
fi
echo "fdc_bad_request_mapping=pass code=$BAD_REQUEST_CODE" | tee -a "$OUT"

call_json "fdc_unknown_session" GET "/api/v2/inspect/chipset/fdc/fsm?session_id=ses_unknown" "" "^409$"
ENGINE_NOT_RUNNING_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ENGINE_NOT_RUNNING_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "fdc_engine_not_running_mapping=failed code=$ENGINE_NOT_RUNNING_CODE" | tee -a "$OUT"
  exit 1
fi
echo "fdc_engine_not_running_mapping=pass code=$ENGINE_NOT_RUNNING_CODE" | tee -a "$OUT"

call_json "fdc_internal_error" GET "/api/v2/inspect/chipset/fdc/fsm?session_id=ses_local&force_fdc_unavailable=1" "" "^500$"
INTERNAL_ERROR_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$INTERNAL_ERROR_CODE" != "INTERNAL_ERROR" ]]; then
  echo "fdc_internal_error_mapping=failed code=$INTERNAL_ERROR_CODE" | tee -a "$OUT"
  exit 1
fi
echo "fdc_internal_error_mapping=pass code=$INTERNAL_ERROR_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"