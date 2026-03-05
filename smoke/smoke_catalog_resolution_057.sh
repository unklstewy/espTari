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

echo "== start session valid catalog-backed ids =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/engine/session" \
  -H "Content-Type: application/json" \
  -d '{"machine":"atari_st","profile":"st_520_pal","rom_id":"rom.atari.st.01","tos_id":"tos.eu.1.04","disk_ids":["disk.automation.a_093"]}'
echo

echo "== media/rom/attach valid rom_id =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/media/rom/attach" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"ses_local","rom_id":"rom.atari.st.01"}'
echo

echo "== media/disk/attach missing disk_id -> CATALOG_ENTRY_NOT_FOUND =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/media/disk/attach" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"ses_local","disk_id":"disk.missing.057"}'
echo

echo "== media/cartridge/attach catalog missing -> CATALOG_NOT_FOUND =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/media/cartridge/attach" \
  -H "Content-Type: application/json" \
  -d '{"session_id":"ses_local","cartridge_id":"cart.demo.057"}'
echo
