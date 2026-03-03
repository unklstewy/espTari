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

echo "== start with valid rom_id/disk_ids/tos_id =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session" \
  -H "Content-Type: application/json" \
  -d '{"machine":"atari_st","profile":"st_520_pal","rom_id":"rom.atari.st.01","tos_id":"tos.eu.1.04","disk_ids":["disk.automation.a_093"]}'
echo

echo "== stop (mid) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session/stop" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"ses_local"}'
echo

echo "== start with missing rom_id entry =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session" \
  -H "Content-Type: application/json" \
  -d '{"machine":"atari_st","profile":"st_520_pal","rom_id":"rom.missing"}'
echo
