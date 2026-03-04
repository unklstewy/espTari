#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/mfp_irq_099_smoke_postfix_${TS}.txt"
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

call_json "mfp_irq_trace_a" GET "/api/v2/inspect/chipset/mfp/interrupts?session_id=ses_local&group=mfp&timer_id=A" "" "^200$"
CHECKS_PASS="$(extract_json 'str(all(v=="pass" for v in d["data"]["checks"].values()))')"
SRC_OK="$(extract_json 'str(d["data"]["last_interrupt"]["source"] == "mfp")')"
LINE_OK="$(extract_json 'str(d["data"]["last_interrupt"]["interrupt_line"] in {"irq2","irq6"})')"
VECTOR_OK="$(extract_json 'str(d["data"]["last_interrupt"]["vector"] in {18,20,24,26})')"
TS_1="$(extract_json 'int(d["data"]["last_interrupt"]["event_timestamp_us"])')"
if [[ "$CHECKS_PASS" != "True" || "$SRC_OK" != "True" || "$LINE_OK" != "True" || "$VECTOR_OK" != "True" ]]; then
  echo "mfp_irq_contract=failed checks=$CHECKS_PASS src=$SRC_OK line=$LINE_OK vector=$VECTOR_OK" | tee -a "$OUT"
  exit 1
fi

echo "mfp_irq_contract=pass checks=$CHECKS_PASS src=$SRC_OK line=$LINE_OK vector=$VECTOR_OK" | tee -a "$OUT"

call_json "mfp_irq_trace_b" GET "/api/v2/inspect/chipset/mfp/interrupts?session_id=ses_local&group=mfp&timer_id=B" "" "^200$"
TS_2="$(extract_json 'int(d["data"]["last_interrupt"]["event_timestamp_us"])')"
if (( TS_2 < TS_1 )); then
  echo "mfp_irq_monotonicity=failed ts1=$TS_1 ts2=$TS_2" | tee -a "$OUT"
  exit 1
fi

echo "mfp_irq_monotonicity=pass ts1=$TS_1 ts2=$TS_2" | tee -a "$OUT"

call_json "mfp_irq_invalid_group" GET "/api/v2/inspect/chipset/mfp/interrupts?session_id=ses_local&group=glue" "" "^400$"
BAD_GROUP_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_GROUP_CODE" != "BAD_REQUEST" ]]; then
  echo "mfp_irq_group_guard=failed code=$BAD_GROUP_CODE" | tee -a "$OUT"
  exit 1
fi

echo "mfp_irq_group_guard=pass code=$BAD_GROUP_CODE" | tee -a "$OUT"

call_json "mfp_irq_invalid_timer" GET "/api/v2/inspect/chipset/mfp/interrupts?session_id=ses_local&group=mfp&timer_id=Z" "" "^400$"
BAD_TIMER_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_TIMER_CODE" != "INSPECT_FILTER_INVALID" ]]; then
  echo "mfp_irq_timer_guard=failed code=$BAD_TIMER_CODE" | tee -a "$OUT"
  exit 1
fi

echo "mfp_irq_timer_guard=pass code=$BAD_TIMER_CODE" | tee -a "$OUT"

call_json "mfp_irq_unknown_session" GET "/api/v2/inspect/chipset/mfp/interrupts?session_id=ses_unknown&group=mfp" "" "^409$"
ENGINE_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ENGINE_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "mfp_irq_session_guard=failed code=$ENGINE_CODE" | tee -a "$OUT"
  exit 1
fi

echo "mfp_irq_session_guard=pass code=$ENGINE_CODE" | tee -a "$OUT"

call_json "mfp_irq_unresolved_vector_failfast" GET "/api/v2/inspect/chipset/mfp/interrupts?session_id=ses_local&group=mfp&force_unresolved_vector=1" "" "^500$"
VECTOR_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$VECTOR_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "mfp_irq_vector_failfast=failed code=$VECTOR_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "mfp_irq_vector_failfast=pass code=$VECTOR_FAIL_CODE" | tee -a "$OUT"

call_json "mfp_irq_timing_failfast" GET "/api/v2/inspect/chipset/mfp/interrupts?session_id=ses_local&group=mfp&force_timestamp_regression=1" "" "^500$"
TIMING_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$TIMING_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "mfp_irq_timing_failfast=failed code=$TIMING_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "mfp_irq_timing_failfast=pass code=$TIMING_FAIL_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
