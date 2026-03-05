#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/interrupt_wiring_107_smoke_postfix_${TS}.txt"
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

call_json "interrupt_wiring_state" GET "/api/v2/inspect/chipset/interrupts/wiring?session_id=ses_local" "" "^200$"
SUBSYSTEMS_OK="$(extract_json 'str(set(s["subsystem_id"] for s in d["data"]["subsystems"]) == {"mfp","acia","fdc","blitter","vbl"})')"
LINE_ENUM_OK="$(extract_json 'str(all(s["cpu_interrupt_line"] in {"irq1","irq2","irq3","irq4","irq5","irq6","irq7"} for s in d["data"]["subsystems"]))')"
if [[ "$SUBSYSTEMS_OK" != "True" || "$LINE_ENUM_OK" != "True" ]]; then
  echo "int_wiring_contract=failed subsystems=$SUBSYSTEMS_OK lines=$LINE_ENUM_OK" | tee -a "$OUT"
  exit 1
fi

echo "int_wiring_contract=pass subsystems=$SUBSYSTEMS_OK lines=$LINE_ENUM_OK" | tee -a "$OUT"

call_json "interrupt_wiring_checks" GET "/api/v2/inspect/chipset/interrupts/wiring/checks?session_id=ses_local&limit=4" "" "^200$"
CONF_OK="$(extract_json 'str(all(v=="pass" for v in d["data"]["conformance"].values()))')"
SEQ_OK="$(extract_json 'str(all(curr["check_seq"]==prev["check_seq"]+1 for prev,curr in zip(d["data"]["checks"], d["data"]["checks"][1:])))')"
TS_OK="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["checks"], d["data"]["checks"][1:])))')"
TICK_OK="$(extract_json 'str(all(curr["tick_counter"]>=prev["tick_counter"] for prev,curr in zip(d["data"]["checks"], d["data"]["checks"][1:])))')"
PASS_MATCH_OK="$(extract_json 'str(all((c["result"]!="pass") or (c["observed_cpu_line"]==c["expected_cpu_line"] and c["observed_vector"]==c["expected_vector"]) for c in d["data"]["checks"]))')"
if [[ "$CONF_OK" != "True" || "$SEQ_OK" != "True" || "$TS_OK" != "True" || "$TICK_OK" != "True" || "$PASS_MATCH_OK" != "True" ]]; then
  echo "int_wiring_checks_contract=failed conf=$CONF_OK seq=$SEQ_OK ts=$TS_OK tick=$TICK_OK pass_match=$PASS_MATCH_OK" | tee -a "$OUT"
  exit 1
fi

echo "int_wiring_checks_contract=pass conf=$CONF_OK seq=$SEQ_OK ts=$TS_OK tick=$TICK_OK pass_match=$PASS_MATCH_OK" | tee -a "$OUT"

call_json "int_wiring_bad_limit" GET "/api/v2/inspect/chipset/interrupts/wiring/checks?session_id=ses_local&limit=0" "" "^400$"
BAD_LIMIT_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_LIMIT_CODE" != "BAD_REQUEST" ]]; then
  echo "int_wiring_limit_guard=failed code=$BAD_LIMIT_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_wiring_limit_guard=pass code=$BAD_LIMIT_CODE" | tee -a "$OUT"

call_json "int_wiring_unknown_session" GET "/api/v2/inspect/chipset/interrupts/wiring?session_id=ses_unknown" "" "^409$"
SESSION_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$SESSION_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "int_wiring_session_guard=failed code=$SESSION_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_wiring_session_guard=pass code=$SESSION_CODE" | tee -a "$OUT"

call_json "int_wiring_unavailable_failfast" GET "/api/v2/inspect/chipset/interrupts/wiring?session_id=ses_local&force_wiring_unavailable=1" "" "^500$"
UNAVAILABLE_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$UNAVAILABLE_CODE" != "INTERNAL_ERROR" ]]; then
  echo "int_wiring_unavailable_failfast=failed code=$UNAVAILABLE_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_wiring_unavailable_failfast=pass code=$UNAVAILABLE_CODE" | tee -a "$OUT"

call_json "int_wire01_failfast" GET "/api/v2/inspect/chipset/interrupts/wiring/checks?session_id=ses_local&limit=2&force_seq_regression=1" "" "^500$"
WIRE01_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$WIRE01_CODE" != "INTERNAL_ERROR" ]]; then
  echo "int_wire01_failfast=failed code=$WIRE01_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_wire01_failfast=pass code=$WIRE01_CODE" | tee -a "$OUT"

call_json "int_wire02_failfast" GET "/api/v2/inspect/chipset/interrupts/wiring/checks?session_id=ses_local&limit=2&force_time_regression=1" "" "^500$"
WIRE02_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$WIRE02_CODE" != "INTERNAL_ERROR" ]]; then
  echo "int_wire02_failfast=failed code=$WIRE02_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_wire02_failfast=pass code=$WIRE02_CODE" | tee -a "$OUT"

call_json "int_wire03_failfast" GET "/api/v2/inspect/chipset/interrupts/wiring/checks?session_id=ses_local&limit=2&force_observed_mismatch=1" "" "^500$"
WIRE03_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$WIRE03_CODE" != "INTERNAL_ERROR" ]]; then
  echo "int_wire03_failfast=failed code=$WIRE03_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_wire03_failfast=pass code=$WIRE03_CODE" | tee -a "$OUT"

call_json "int_wire04_failfast" GET "/api/v2/inspect/chipset/interrupts/wiring/checks?session_id=ses_local&limit=2&force_map_mutation=1" "" "^500$"
WIRE04_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$WIRE04_CODE" != "INTERNAL_ERROR" ]]; then
  echo "int_wire04_failfast=failed code=$WIRE04_CODE" | tee -a "$OUT"
  exit 1
fi

echo "int_wire04_failfast=pass code=$WIRE04_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
