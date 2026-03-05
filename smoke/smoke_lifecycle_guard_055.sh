#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"

for _ in $(seq 1 30); do
  if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

echo "== stop (pre) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session/stop" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"ses_local"}'
echo

echo "== pause from stopped -> INVALID_SESSION_STATE/G-LIFECYCLE-PAUSE =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session/pause" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"ses_local"}'
echo

echo "== suspend-save from stopped -> INVALID_SESSION_STATE/G-SUSPEND-01 =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session/suspend-save" \
  -H "Content-Type: application/json" \
  -d '{"snapshot_id":"snap.test.055"}'
echo

echo "== resume invalid mode -> BAD_REQUEST/G-RESUME-02 =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session/resume" \
  -H "Content-Type: application/json" \
  -d '{"resume_mode":"fast_forward"}'
echo

echo "== reset invalid mode -> BAD_REQUEST/G-RESET-01 =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session/reset" \
  -H "Content-Type: application/json" \
  -d '{"mode":"hard"}'
echo

echo "== restore-resume invalid mode -> BAD_REQUEST/G-RESUME-02 =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session/restore-resume" \
  -H "Content-Type: application/json" \
  -d '{"snapshot_id":"snap.any","resume_mode":"fast_forward"}'
echo
