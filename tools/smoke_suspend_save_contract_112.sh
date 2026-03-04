#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/suspend_save_contract_112_smoke_postfix_${TS}.txt"
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

call_json "suspend_save_missing_session" POST "/api/v2/engine/session/suspend-save" '{"name":"missing_session"}' "^400$"
MISSING_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MISSING_CODE" != "BAD_REQUEST" ]]; then
  echo "missing_session_guard=failed code=$MISSING_CODE" | tee -a "$OUT"
  exit 1
fi

echo "missing_session_guard=pass code=$MISSING_CODE" | tee -a "$OUT"

call_json "suspend_save_unknown_session" POST "/api/v2/engine/session/suspend-save" '{"session_id":"ses_unknown","name":"unknown_session"}' "^409$"
UNKNOWN_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$UNKNOWN_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "unknown_session_guard=failed code=$UNKNOWN_CODE" | tee -a "$OUT"
  exit 1
fi

echo "unknown_session_guard=pass code=$UNKNOWN_CODE" | tee -a "$OUT"

call_json "suspend_save_success" POST "/api/v2/engine/session/suspend-save" '{"session_id":"ses_local","name":"contract112","reason":"smoke","auto_resume":false,"include_stream_state":true}' "^200$"
SUCCESS_SESSION_ID="$(extract_json 'd["data"]["session_id"]')"
SUCCESS_STATE="$(extract_json 'd["data"]["state"]')"
SUCCESS_TRANSITION="$(extract_json 'd["data"]["lifecycle_transition"]')"
SUCCESS_SAVED_US="$(extract_json 'int(d["data"]["saved_at_us"])')"
SUCCESS_SNAPSHOT_ID="$(extract_json 'd["data"]["snapshot_id"]')"

if [[ "$SUCCESS_SESSION_ID" != "ses_local" || "$SUCCESS_STATE" != "suspended" || "$SUCCESS_TRANSITION" != "running->suspended" || "$SUCCESS_SAVED_US" -le 0 || -z "$SUCCESS_SNAPSHOT_ID" ]]; then
  echo "success_contract_fields=failed session_id=$SUCCESS_SESSION_ID state=$SUCCESS_STATE transition=$SUCCESS_TRANSITION saved_at_us=$SUCCESS_SAVED_US snapshot_id=$SUCCESS_SNAPSHOT_ID" | tee -a "$OUT"
  exit 1
fi

echo "success_contract_fields=pass session_id=$SUCCESS_SESSION_ID state=$SUCCESS_STATE transition=$SUCCESS_TRANSITION saved_at_us=$SUCCESS_SAVED_US snapshot_id=$SUCCESS_SNAPSHOT_ID" | tee -a "$OUT"

call_json "suspend_save_invalid_state" POST "/api/v2/engine/session/suspend-save" '{"session_id":"ses_local","name":"repeat_while_suspended"}' "^409$"
INVALID_STATE_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$INVALID_STATE_CODE" != "INVALID_SESSION_STATE" ]]; then
  echo "invalid_state_guard=failed code=$INVALID_STATE_CODE" | tee -a "$OUT"
  exit 1
fi

echo "invalid_state_guard=pass code=$INVALID_STATE_CODE" | tee -a "$OUT"

call_json "restore_resume_running" POST "/api/v2/engine/session/restore-resume" "{\"snapshot_id\":\"${SUCCESS_SNAPSHOT_ID}\",\"resume_mode\":\"running\"}" "^200$"

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"
