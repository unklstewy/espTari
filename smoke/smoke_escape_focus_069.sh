#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"
SESSION_ID="ses_local"
BROWSER_SESSION_ID="browser_local"

wait_health() {
  for _ in $(seq 1 30); do
    if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
      return 0
    fi
    sleep 1
  done
  return 1
}

wait_health

echo "== set click_to_capture mode =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"click_to_capture","request_action":"set_enabled"}'
echo

echo "== acquire capture =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"click_to_capture","request_action":"click_acquire"}'
echo

echo "== focus lost (release applied) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"click_to_capture","request_action":"focus_lost"}'
echo

echo "== focus regained (no auto-acquire) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"click_to_capture","request_action":"focus_regained"}'
echo

echo "== acquire again =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"click_to_capture","request_action":"click_acquire"}'
echo

echo "== escape release valid sequence (released) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/release" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","reason":"user_escape_sequence","sequence":["Escape","Escape"],"elapsed_ms":120}'
echo

echo "== acquire again =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"click_to_capture","request_action":"click_acquire"}'
echo

echo "== escape release invalid sequence (no-op) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/release" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","reason":"user_escape_sequence","sequence":["Escape","X"],"elapsed_ms":120}'
echo

echo "== final state snapshot =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/input/capture/state?session_id=${SESSION_ID}&browser_session_id=${BROWSER_SESSION_ID}"
echo
