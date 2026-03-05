#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/restore_resume_113_smoke_postfix_${TS}.txt"
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

call_json "session_reset_initial" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
call_json "session_start_initial" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "suspend_save_baseline" POST "/api/v2/engine/session/suspend-save" '{"session_id":"ses_local","name":"restore113_a"}' "^200$"
SNAPSHOT_A="$(extract_json 'd["data"]["snapshot_id"]')"

call_json "restore_missing_session" POST "/api/v2/engine/session/restore-resume" "{\"snapshot_id\":\"${SNAPSHOT_A}\",\"resume_mode\":\"running\"}" "^400$"
MISSING_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MISSING_CODE" != "BAD_REQUEST" ]]; then
  echo "missing_session_guard=failed code=$MISSING_CODE" | tee -a "$OUT"
  exit 1
fi

echo "missing_session_guard=pass code=$MISSING_CODE" | tee -a "$OUT"

call_json "restore_unknown_session" POST "/api/v2/engine/session/restore-resume" "{\"session_id\":\"ses_unknown\",\"snapshot_id\":\"${SNAPSHOT_A}\",\"resume_mode\":\"running\"}" "^409$"
UNKNOWN_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$UNKNOWN_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "unknown_session_guard=failed code=$UNKNOWN_CODE" | tee -a "$OUT"
  exit 1
fi

echo "unknown_session_guard=pass code=$UNKNOWN_CODE" | tee -a "$OUT"

call_json "restore_invalid_resume_mode" POST "/api/v2/engine/session/restore-resume" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${SNAPSHOT_A}\",\"resume_mode\":\"fast_forward\"}" "^400$"
MODE_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MODE_CODE" != "BAD_REQUEST" ]]; then
  echo "resume_mode_guard=failed code=$MODE_CODE" | tee -a "$OUT"
  exit 1
fi

echo "resume_mode_guard=pass code=$MODE_CODE" | tee -a "$OUT"

call_json "restore_success_running" POST "/api/v2/engine/session/restore-resume" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${SNAPSHOT_A}\",\"resume_mode\":\"running\",\"reason\":\"contract_113\"}" "^200$"
RESTORE_SESSION_ID="$(extract_json 'd["data"]["session_id"]')"
RESTORE_STATE="$(extract_json 'd["data"]["state"]')"
RESTORE_TRANSITION="$(extract_json 'd["data"]["lifecycle_transition"]')"
RESTORE_SAVED_US="$(extract_json 'int(d["data"]["restored_at_us"])')"
RESTORE_SNAPSHOT_ID="$(extract_json 'd["data"]["snapshot_id"]')"
if [[ "$RESTORE_SESSION_ID" != "ses_local" || "$RESTORE_STATE" != "running" || "$RESTORE_TRANSITION" != "suspended->running" || "$RESTORE_SAVED_US" -le 0 || "$RESTORE_SNAPSHOT_ID" != "$SNAPSHOT_A" ]]; then
  echo "restore_contract_fields=failed session_id=$RESTORE_SESSION_ID state=$RESTORE_STATE transition=$RESTORE_TRANSITION restored_at_us=$RESTORE_SAVED_US snapshot_id=$RESTORE_SNAPSHOT_ID" | tee -a "$OUT"
  exit 1
fi

echo "restore_contract_fields=pass session_id=$RESTORE_SESSION_ID state=$RESTORE_STATE transition=$RESTORE_TRANSITION restored_at_us=$RESTORE_SAVED_US snapshot_id=$RESTORE_SNAPSHOT_ID" | tee -a "$OUT"

call_json "restore_not_suspended" POST "/api/v2/engine/session/restore-resume" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${SNAPSHOT_A}\",\"resume_mode\":\"paused\"}" "^409$"
NOT_SUSPENDED_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$NOT_SUSPENDED_CODE" != "ENGINE_NOT_SUSPENDED" ]]; then
  echo "not_suspended_guard=failed code=$NOT_SUSPENDED_CODE" | tee -a "$OUT"
  exit 1
fi

echo "not_suspended_guard=pass code=$NOT_SUSPENDED_CODE" | tee -a "$OUT"

call_json "session_reset_second" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
call_json "session_start_second" POST "/api/v2/engine/session/start" "" "^(200|409)$"
call_json "suspend_save_second" POST "/api/v2/engine/session/suspend-save" '{"session_id":"ses_local","name":"restore113_b"}' "^200$"
SNAPSHOT_B="$(extract_json 'd["data"]["snapshot_id"]')"

call_json "restore_snapshot_not_found" POST "/api/v2/engine/session/restore-resume" '{"session_id":"ses_local","snapshot_id":"snap_missing_113","resume_mode":"running"}' "^404$"
NOT_FOUND_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$NOT_FOUND_CODE" != "SNAPSHOT_NOT_FOUND" ]]; then
  echo "snapshot_not_found_guard=failed code=$NOT_FOUND_CODE" | tee -a "$OUT"
  exit 1
fi

echo "snapshot_not_found_guard=pass code=$NOT_FOUND_CODE" | tee -a "$OUT"

call_json "restore_success_paused" POST "/api/v2/engine/session/restore-resume" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${SNAPSHOT_B}\",\"resume_mode\":\"paused\"}" "^200$"
PAUSED_STATE="$(extract_json 'd["data"]["state"]')"
PAUSED_TRANSITION="$(extract_json 'd["data"]["lifecycle_transition"]')"
if [[ "$PAUSED_STATE" != "paused" || "$PAUSED_TRANSITION" != "suspended->paused" ]]; then
  echo "paused_transition=failed state=$PAUSED_STATE transition=$PAUSED_TRANSITION" | tee -a "$OUT"
  exit 1
fi

echo "paused_transition=pass state=$PAUSED_STATE transition=$PAUSED_TRANSITION" | tee -a "$OUT"

call_json "session_reset_final" POST "/api/v2/engine/session/reset" "" "^(200|409)$"

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"
