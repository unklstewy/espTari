#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/dma_arbitration_102_smoke_postfix_${TS}.txt"
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

call_json "dma_pacing_ok" GET "/api/v2/inspect/chipset/dma/pacing?session_id=ses_local" "" "^200$"
PACING_MODE="$(extract_json 'd["data"]["pacing_mode"]')"
WINDOW_TICKS="$(extract_json 'int(d["data"]["request_window_ticks"])')"
MAX_PER_WINDOW="$(extract_json 'int(d["data"]["max_requests_per_window"])')"
HAS_FIELDS="$(extract_json 'str(all(k in d["data"] for k in ["window_start_tick","window_end_tick","queued_requests","last_request_seq"]))')"
if [[ "$PACING_MODE" != "deterministic_tick" || "$WINDOW_TICKS" -le 0 || "$MAX_PER_WINDOW" -le 0 || "$HAS_FIELDS" != "True" ]]; then
  echo "dma_pacing_contract=failed mode=$PACING_MODE window_ticks=$WINDOW_TICKS max_per_window=$MAX_PER_WINDOW has_fields=$HAS_FIELDS" | tee -a "$OUT"
  exit 1
fi
echo "dma_pacing_contract=pass mode=$PACING_MODE window_ticks=$WINDOW_TICKS max_per_window=$MAX_PER_WINDOW has_fields=$HAS_FIELDS" | tee -a "$OUT"

call_json "dma_arbitration_ok" GET "/api/v2/inspect/chipset/dma/arbitration?session_id=ses_local&limit=3" "" "^200$"
EVENT_COUNT="$(extract_json 'len(d["data"]["events"])')"
SEQ_DELTA_1="$(extract_json 'd["data"]["events"][1]["request_seq"] - d["data"]["events"][0]["request_seq"]')"
SEQ_DELTA_2="$(extract_json 'd["data"]["events"][2]["request_seq"] - d["data"]["events"][1]["request_seq"]')"
SCHED_MONO="$(extract_json 'str(d["data"]["events"][1]["scheduled_tick"] >= d["data"]["events"][0]["scheduled_tick"] and d["data"]["events"][2]["scheduled_tick"] >= d["data"]["events"][1]["scheduled_tick"])')"
TS_MONO="$(extract_json 'str(d["data"]["events"][1]["timestamp_us"] >= d["data"]["events"][0]["timestamp_us"] and d["data"]["events"][2]["timestamp_us"] >= d["data"]["events"][1]["timestamp_us"])')"
GRANT_RULES="$(extract_json 'str(all((e["grant_state"]=="granted" and e["granted_tick"] is not None and e["granted_tick"]>=e["scheduled_tick"]) or (e["grant_state"]!="granted" and e["granted_tick"] is None) for e in d["data"]["events"]))')"
GRANTED_COUNT="$(extract_json 'sum(1 for e in d["data"]["events"] if e["grant_state"]=="granted")')"

if [[ "$EVENT_COUNT" != "3" || "$SEQ_DELTA_1" != "1" || "$SEQ_DELTA_2" != "1" || "$SCHED_MONO" != "True" || "$TS_MONO" != "True" || "$GRANT_RULES" != "True" ]]; then
  echo "dma_arbitration_contract=failed count=$EVENT_COUNT seq_deltas=$SEQ_DELTA_1,$SEQ_DELTA_2 sched_mono=$SCHED_MONO ts_mono=$TS_MONO grant_rules=$GRANT_RULES" | tee -a "$OUT"
  exit 1
fi

if [[ "$GRANTED_COUNT" -gt "$MAX_PER_WINDOW" ]]; then
  echo "dma_window_cap=failed granted_count=$GRANTED_COUNT max_per_window=$MAX_PER_WINDOW" | tee -a "$OUT"
  exit 1
fi

echo "dma_arbitration_contract=pass count=$EVENT_COUNT seq_deltas=$SEQ_DELTA_1,$SEQ_DELTA_2 sched_mono=$SCHED_MONO ts_mono=$TS_MONO grant_rules=$GRANT_RULES" | tee -a "$OUT"
echo "dma_window_cap=pass granted_count=$GRANTED_COUNT max_per_window=$MAX_PER_WINDOW" | tee -a "$OUT"

call_json "dma_arbitration_missing_limit" GET "/api/v2/inspect/chipset/dma/arbitration?session_id=ses_local" "" "^400$"
BAD_REQUEST_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_REQUEST_CODE" != "BAD_REQUEST" ]]; then
  echo "dma_bad_request_mapping=failed code=$BAD_REQUEST_CODE" | tee -a "$OUT"
  exit 1
fi
echo "dma_bad_request_mapping=pass code=$BAD_REQUEST_CODE" | tee -a "$OUT"

call_json "dma_unknown_session" GET "/api/v2/inspect/chipset/dma/pacing?session_id=ses_unknown" "" "^409$"
ENGINE_NOT_RUNNING_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ENGINE_NOT_RUNNING_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "dma_engine_not_running_mapping=failed code=$ENGINE_NOT_RUNNING_CODE" | tee -a "$OUT"
  exit 1
fi
echo "dma_engine_not_running_mapping=pass code=$ENGINE_NOT_RUNNING_CODE" | tee -a "$OUT"

call_json "dma_internal_error" GET "/api/v2/inspect/chipset/dma/arbitration?session_id=ses_local&limit=3&force_dma_unavailable=1" "" "^500$"
INTERNAL_ERROR_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$INTERNAL_ERROR_CODE" != "INTERNAL_ERROR" ]]; then
  echo "dma_internal_error_mapping=failed code=$INTERNAL_ERROR_CODE" | tee -a "$OUT"
  exit 1
fi
echo "dma_internal_error_mapping=pass code=$INTERNAL_ERROR_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"