#!/usr/bin/env bash
set -euo pipefail

BASE="http://esptari.local"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE}" "$0"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/input_capture_068_smoke_postflash_${TS}.txt"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

wait_for_health() {
  local attempts="${1:-60}"
  local interval="${2:-1}"
  local i
  for ((i=1; i<=attempts; i++)); do
    if curl --max-time 2 -sS "${BASE}/api/v2/engine/health" >/dev/null 2>&1; then
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
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE$path" -H "Content-Type: application/json" -d "$data")
  else
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE$path")
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
    echo "Step failed: $name HTTP=$code"
    echo "$body"
    exit 1
  fi
  LAST_BODY="$body"
}

wait_for_health

# mouse_over mode transitions
CFG_MO='{"session_id":"ses_local","browser_session_id":"browser_local","input_enabled":true,"capture_mode":"mouse_over"}'
call_json "config_mouse_over" POST "/api/v2/input/capture/config" "$CFG_MO" "^200$"

ENTER_MO='{"session_id":"ses_local","browser_session_id":"browser_local","input_enabled":true,"capture_mode":"mouse_over","request_action":"pointer_enter_hook"}'
call_json "mouse_over_enter" POST "/api/v2/input/capture/config" "$ENTER_MO" "^200$"

call_json "state_after_enter" GET "/api/v2/input/capture/state?session_id=ses_local&browser_session_id=browser_local" "" "^200$"

LEAVE_MO='{"session_id":"ses_local","browser_session_id":"browser_local","input_enabled":true,"capture_mode":"mouse_over","request_action":"pointer_leave_hook"}'
call_json "mouse_over_leave" POST "/api/v2/input/capture/config" "$LEAVE_MO" "^200$"

REL_MO='{"session_id":"ses_local","browser_session_id":"browser_local","reason":"explicit_release"}'
call_json "mouse_over_release_noop" POST "/api/v2/input/capture/release" "$REL_MO" "^200$"

# click_to_capture transitions
CFG_CT='{"session_id":"ses_local","browser_session_id":"browser_local","input_enabled":true,"capture_mode":"click_to_capture"}'
call_json "config_click_to_capture" POST "/api/v2/input/capture/config" "$CFG_CT" "^200$"

ACQ_CT='{"session_id":"ses_local","browser_session_id":"browser_local","input_enabled":true,"capture_mode":"click_to_capture","request_action":"click_acquire"}'
call_json "click_acquire_applied" POST "/api/v2/input/capture/config" "$ACQ_CT" "^200$"
call_json "click_acquire_noop" POST "/api/v2/input/capture/config" "$ACQ_CT" "^200$"

INJECT_EVT='{"session_id":"ses_local","browser_session_id":"browser_local","events":[{"event_id":"evt_1","type":"key_down","key":"A"}]}'
call_json "inject_when_captured" POST "/api/v2/input/events/inject" "$INJECT_EVT" "^200$"

REL_CT='{"session_id":"ses_local","browser_session_id":"browser_local","reason":"user_escape_sequence"}'
call_json "click_release" POST "/api/v2/input/capture/release" "$REL_CT" "^200$"

call_json "inject_not_active" POST "/api/v2/input/events/inject" "$INJECT_EVT" "^409$"

echo "Smoke PASS"
echo "Evidence: $OUT"
