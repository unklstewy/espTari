#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/interrupt_hierarchy_106_smoke_postfix_${TS}.txt"
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

call_json "interrupt_hierarchy" GET "/api/v2/inspect/chipset/interrupts/hierarchy?session_id=ses_local" "" "^200$"
LEVEL_ORDER_OK="$(extract_json 'str(d["data"]["cpu_level_order"] == [7,6,5,4,3,2,1])')"
SOURCES_OK="$(extract_json 'str(set(s["source_id"] for s in d["data"]["sources"]) == {"mfp","acia","fdc","blitter","vbl"})')"
CHECK01_OK="$(extract_json 'str(d["data"]["checks"]["INT-MAP-01"] == "pass")')"
if [[ "$LEVEL_ORDER_OK" != "True" || "$SOURCES_OK" != "True" || "$CHECK01_OK" != "True" ]]; then
  echo "int_hierarchy_contract=failed levels=$LEVEL_ORDER_OK sources=$SOURCES_OK check01=$CHECK01_OK" | tee -a "$OUT"
  exit 1
fi

echo "int_hierarchy_contract=pass levels=$LEVEL_ORDER_OK sources=$SOURCES_OK check01=$CHECK01_OK" | tee -a "$OUT"

call_json "interrupt_routes" GET "/api/v2/inspect/chipset/interrupts/routes?session_id=ses_local&limit=4" "" "^200$"
CHECKS_OK="$(extract_json 'str(all(v=="pass" for v in d["data"]["checks"].values()))')"
SEQ_OK="$(extract_json 'str(all(curr["route_seq"]==prev["route_seq"]+1 for prev,curr in zip(d["data"]["routes"], d["data"]["routes"][1:])))')"
TS_OK="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["routes"], d["data"]["routes"][1:])))')"
TICK_OK="$(extract_json 'str(all(curr["tick_counter"]>=prev["tick_counter"] for prev,curr in zip(d["data"]["routes"], d["data"]["routes"][1:])))')"
ORDER_OK="$(extract_json 'str([r["source_id"] for r in d["data"]["routes"]] == ["vbl","mfp","acia","fdc"])')"
if [[ "$CHECKS_OK" != "True" || "$SEQ_OK" != "True" || "$TS_OK" != "True" || "$TICK_OK" != "True" || "$ORDER_OK" != "True" ]]; then
  echo "int_routes_contract=failed checks=$CHECKS_OK seq=$SEQ_OK ts=$TS_OK tick=$TICK_OK order=$ORDER_OK" | tee -a "$OUT"
  exit 1
fi

echo "int_routes_contract=pass checks=$CHECKS_OK seq=$SEQ_OK ts=$TS_OK tick=$TICK_OK order=$ORDER_OK" | tee -a "$OUT"

call_json "interrupt_routes_bad_limit" GET "/api/v2/inspect/chipset/interrupts/routes?session_id=ses_local&limit=0" "" "^400$"
BAD_LIMIT_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_LIMIT_CODE" != "BAD_REQUEST" ]]; then
  echo "int_limit_guard=failed code=$BAD_LIMIT_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_limit_guard=pass code=$BAD_LIMIT_CODE" | tee -a "$OUT"

call_json "interrupt_unknown_session" GET "/api/v2/inspect/chipset/interrupts/hierarchy?session_id=ses_unknown" "" "^409$"
SESSION_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$SESSION_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "int_session_guard=failed code=$SESSION_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_session_guard=pass code=$SESSION_CODE" | tee -a "$OUT"

call_json "int_map01_failfast" GET "/api/v2/inspect/chipset/interrupts/hierarchy?session_id=ses_local&force_duplicate_source=1" "" "^500$"
MAP01_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MAP01_CODE" != "INTERNAL_ERROR" ]]; then
  echo "int_map01_failfast=failed code=$MAP01_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_map01_failfast=pass code=$MAP01_CODE" | tee -a "$OUT"

call_json "int_map02_failfast" GET "/api/v2/inspect/chipset/interrupts/routes?session_id=ses_local&limit=2&force_route_seq_regression=1" "" "^500$"
MAP02_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MAP02_CODE" != "INTERNAL_ERROR" ]]; then
  echo "int_map02_failfast=failed code=$MAP02_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_map02_failfast=pass code=$MAP02_CODE" | tee -a "$OUT"

call_json "int_map03_failfast" GET "/api/v2/inspect/chipset/interrupts/routes?session_id=ses_local&limit=2&force_route_time_regression=1" "" "^500$"
MAP03_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MAP03_CODE" != "INTERNAL_ERROR" ]]; then
  echo "int_map03_failfast=failed code=$MAP03_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_map03_failfast=pass code=$MAP03_CODE" | tee -a "$OUT"

call_json "int_map04_failfast" GET "/api/v2/inspect/chipset/interrupts/routes?session_id=ses_local&limit=2&force_route_order_violation=1" "" "^500$"
MAP04_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MAP04_CODE" != "INTERNAL_ERROR" ]]; then
  echo "int_map04_failfast=failed code=$MAP04_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_map04_failfast=pass code=$MAP04_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
