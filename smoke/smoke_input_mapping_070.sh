#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"

for _ in $(seq 1 30); do
  if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

echo "== mappings/load =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/mappings/load" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"ses_local","mapping_profile_id":"atari_st_custom_gamepad_v1","replace":true}'
echo

echo "== mappings/update applied =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/mappings/update" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"ses_local","entries":[{"entry_id":"pad.south","host":{"device_type":"game_controller","code":"BTN_SOUTH"},"virtual":{"target":"joystick.port0.button","value":1}}]}'
echo

echo "== mappings/update no_op =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/input/mappings/update" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"ses_local","entries":[]}'
echo
