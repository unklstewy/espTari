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

echo "== set mouse_over mode enabled =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"mouse_over","request_action":"set_enabled"}'
echo

echo "== pointer enter hook (applied) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"mouse_over","request_action":"pointer_enter_hook"}'
echo

echo "== pointer enter hook again (no-op idempotent) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"mouse_over","request_action":"pointer_enter_hook"}'
echo

echo "== explicit release in mouse_over (deterministic no-op) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/release" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","reason":"explicit_release"}'
echo

echo "== pointer leave hook (implicit release applied) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/capture/config" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"'"${SESSION_ID}"'","browser_session_id":"'"${BROWSER_SESSION_ID}"'","enabled":true,"capture_mode":"mouse_over","request_action":"pointer_leave_hook"}'
echo

echo "== state snapshot =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/input/capture/state?session_id=${SESSION_ID}&browser_session_id=${BROWSER_SESSION_ID}"
echo
