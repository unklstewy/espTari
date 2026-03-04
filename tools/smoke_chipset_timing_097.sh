#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/chipset_timing_097_smoke_postfix_${TS}.txt"
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

call_json "integration_trace_first" GET "/api/v2/inspect/chipset/windows/integration?session_id=ses_local" "" "^200$"
CHECKS_PASS="$(extract_json 'str(all(v=="pass" for v in d["data"]["checks"].values()))')"
ORDER_OK="$(extract_json 'str(d["data"]["last_integration"]["chipset_order"] == ["glue","mmu","shifter"])')"
BUS_OWNER_OK="$(extract_json 'str(d["data"]["last_integration"]["bus_owner"] in {"glue","mmu","shifter","cpu","dma"})')"
TICK_1="$(extract_json 'int(d["data"]["last_integration"]["tick_counter"])')"
CYCLE_1="$(extract_json 'int(d["data"]["last_integration"]["cycle_counter"])')"
TS_1="$(extract_json 'int(d["data"]["last_integration"]["event_timestamp_us"])')"
if [[ "$CHECKS_PASS" != "True" || "$ORDER_OK" != "True" || "$BUS_OWNER_OK" != "True" ]]; then
  echo "chip_timing_contract=failed checks=$CHECKS_PASS order=$ORDER_OK bus_owner=$BUS_OWNER_OK" | tee -a "$OUT"
  exit 1
fi

echo "chip_timing_contract=pass checks=$CHECKS_PASS order=$ORDER_OK bus_owner=$BUS_OWNER_OK" | tee -a "$OUT"

call_json "integration_trace_second" GET "/api/v2/inspect/chipset/windows/integration?session_id=ses_local" "" "^200$"
TICK_2="$(extract_json 'int(d["data"]["last_integration"]["tick_counter"])')"
CYCLE_2="$(extract_json 'int(d["data"]["last_integration"]["cycle_counter"])')"
TS_2="$(extract_json 'int(d["data"]["last_integration"]["event_timestamp_us"])')"
if (( TICK_2 < TICK_1 || CYCLE_2 < CYCLE_1 || TS_2 < TS_1 )); then
  echo "chip_timing_monotonicity=failed tick1=$TICK_1 tick2=$TICK_2 cycle1=$CYCLE_1 cycle2=$CYCLE_2 ts1=$TS_1 ts2=$TS_2" | tee -a "$OUT"
  exit 1
fi

echo "chip_timing_monotonicity=pass tick1=$TICK_1 tick2=$TICK_2 cycle1=$CYCLE_1 cycle2=$CYCLE_2 ts1=$TS_1 ts2=$TS_2" | tee -a "$OUT"

call_json "integration_unknown_session" GET "/api/v2/inspect/chipset/windows/integration?session_id=ses_unknown" "" "^409$"
ENGINE_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ENGINE_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "chip_timing_engine_guard=failed code=$ENGINE_CODE" | tee -a "$OUT"
  exit 1
fi

echo "chip_timing_engine_guard=pass code=$ENGINE_CODE" | tee -a "$OUT"

call_json "integration_order_mismatch_failfast" GET "/api/v2/inspect/chipset/windows/integration?session_id=ses_local&force_order_mismatch=1" "" "^500$"
ORDER_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ORDER_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "chip_timing_order_failfast=failed code=$ORDER_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "chip_timing_order_failfast=pass code=$ORDER_FAIL_CODE" | tee -a "$OUT"

call_json "integration_timing_regression_failfast" GET "/api/v2/inspect/chipset/windows/integration?session_id=ses_local&force_timing_regression=1" "" "^500$"
TIMING_FAIL_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$TIMING_FAIL_CODE" != "INTERNAL_ERROR" ]]; then
  echo "chip_timing_regression_failfast=failed code=$TIMING_FAIL_CODE" | tee -a "$OUT"
  exit 1
fi

echo "chip_timing_regression_failfast=pass code=$TIMING_FAIL_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
