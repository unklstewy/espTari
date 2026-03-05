#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/startup_sequence_109_smoke_postfix_${TS}.txt"
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

call_json "startup_sequence" GET "/api/v2/inspect/chipset/startup/sequence?session_id=ses_local" "" "^200$"
PHASE_OK="$(extract_json 'str(d["data"]["phase"] in {"assert_reset","clock_stabilize","register_seed","interrupt_enable","ready"})')"
STATUS_OK="$(extract_json 'str(d["data"]["verification_status"] in {"pending","pass","fail"})')"
CONF_01_OK="$(extract_json 'str(d["data"]["conformance"]["RST-SEQ-01"]=="pass")')"
if [[ "$PHASE_OK" != "True" || "$STATUS_OK" != "True" || "$CONF_01_OK" != "True" ]]; then
  echo "startup_sequence_contract=failed phase=$PHASE_OK status=$STATUS_OK conf01=$CONF_01_OK" | tee -a "$OUT"
  exit 1
fi

echo "startup_sequence_contract=pass phase=$PHASE_OK status=$STATUS_OK conf01=$CONF_01_OK" | tee -a "$OUT"

call_json "startup_verification" GET "/api/v2/inspect/chipset/startup/verification?session_id=ses_local&limit=4" "" "^200$"
CONF_OK="$(extract_json 'str(all(v=="pass" for v in d["data"]["conformance"].values()))')"
EVSEQ_OK="$(extract_json 'str(all(curr["event_seq"]==prev["event_seq"]+1 for prev,curr in zip(d["data"]["events"], d["data"]["events"][1:])))')"
STEPSEQ_OK="$(extract_json 'str(all(curr["step_seq"]==prev["step_seq"]+1 for prev,curr in zip(d["data"]["events"], d["data"]["events"][1:])))')"
TS_OK="$(extract_json 'str(all(curr["timestamp_us"]>=prev["timestamp_us"] for prev,curr in zip(d["data"]["events"], d["data"]["events"][1:])))')"
TICK_OK="$(extract_json 'str(all(curr["tick_counter"]>=prev["tick_counter"] for prev,curr in zip(d["data"]["events"], d["data"]["events"][1:])))')"
PASS_MATCH_OK="$(extract_json 'str(all((e["result"]!="pass") or (e["expected"]==e["observed"]) for e in d["data"]["events"]))')"
if [[ "$CONF_OK" != "True" || "$EVSEQ_OK" != "True" || "$STEPSEQ_OK" != "True" || "$TS_OK" != "True" || "$TICK_OK" != "True" || "$PASS_MATCH_OK" != "True" ]]; then
  echo "startup_verification_contract=failed conf=$CONF_OK evseq=$EVSEQ_OK stepseq=$STEPSEQ_OK ts=$TS_OK tick=$TICK_OK passmatch=$PASS_MATCH_OK" | tee -a "$OUT"
  exit 1
fi

echo "startup_verification_contract=pass conf=$CONF_OK evseq=$EVSEQ_OK stepseq=$STEPSEQ_OK ts=$TS_OK tick=$TICK_OK passmatch=$PASS_MATCH_OK" | tee -a "$OUT"

call_json "startup_verification_bad_limit" GET "/api/v2/inspect/chipset/startup/verification?session_id=ses_local&limit=0" "" "^400$"
BAD_LIMIT_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$BAD_LIMIT_CODE" != "BAD_REQUEST" ]]; then
  echo "startup_verification_limit_guard=failed code=$BAD_LIMIT_CODE" | tee -a "$OUT"
  exit 1
fi

echo "startup_verification_limit_guard=pass code=$BAD_LIMIT_CODE" | tee -a "$OUT"

call_json "startup_sequence_unknown_session" GET "/api/v2/inspect/chipset/startup/sequence?session_id=ses_unknown" "" "^409$"
SESSION_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$SESSION_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "startup_sequence_session_guard=failed code=$SESSION_CODE" | tee -a "$OUT"
  exit 1
fi

echo "startup_sequence_session_guard=pass code=$SESSION_CODE" | tee -a "$OUT"

call_json "rst_seq01_failfast" GET "/api/v2/inspect/chipset/startup/sequence?session_id=ses_local&force_phase_order_violation=1" "" "^500$"
RST01_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$RST01_CODE" != "INTERNAL_ERROR" ]]; then
  echo "rst_seq01_failfast=failed code=$RST01_CODE" | tee -a "$OUT"
  exit 1
fi

echo "rst_seq01_failfast=pass code=$RST01_CODE" | tee -a "$OUT"

call_json "rst_seq02_failfast" GET "/api/v2/inspect/chipset/startup/verification?session_id=ses_local&limit=2&force_step_seq_regression=1" "" "^500$"
RST02_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$RST02_CODE" != "INTERNAL_ERROR" ]]; then
  echo "rst_seq02_failfast=failed code=$RST02_CODE" | tee -a "$OUT"
  exit 1
fi

echo "rst_seq02_failfast=pass code=$RST02_CODE" | tee -a "$OUT"

call_json "rst_seq03_failfast" GET "/api/v2/inspect/chipset/startup/verification?session_id=ses_local&limit=2&force_time_regression=1" "" "^500$"
RST03_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$RST03_CODE" != "INTERNAL_ERROR" ]]; then
  echo "rst_seq03_failfast=failed code=$RST03_CODE" | tee -a "$OUT"
  exit 1
fi

echo "rst_seq03_failfast=pass code=$RST03_CODE" | tee -a "$OUT"

call_json "rst_seq04_failfast_seq" GET "/api/v2/inspect/chipset/startup/sequence?session_id=ses_local&force_ready_without_pass=1" "" "^500$"
RST04A_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$RST04A_CODE" != "INTERNAL_ERROR" ]]; then
  echo "rst_seq04a_failfast=failed code=$RST04A_CODE" | tee -a "$OUT"
  exit 1
fi

echo "rst_seq04a_failfast=pass code=$RST04A_CODE" | tee -a "$OUT"

call_json "rst_seq04_failfast_verify" GET "/api/v2/inspect/chipset/startup/verification?session_id=ses_local&limit=2&force_ready_without_pass=1" "" "^500$"
RST04B_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$RST04B_CODE" != "INTERNAL_ERROR" ]]; then
  echo "rst_seq04b_failfast=failed code=$RST04B_CODE" | tee -a "$OUT"
  exit 1
fi

echo "rst_seq04b_failfast=pass code=$RST04B_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
