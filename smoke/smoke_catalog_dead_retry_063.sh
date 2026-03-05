#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"
CATALOG="floppies"
ENTRY="disk.demos.dead_entry"

for _ in $(seq 1 30); do
  if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

echo "== reset system =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/system" \
  -H "Content-Type: application/json" \
  -d '{"action":"reset"}' >/dev/null

echo "== mark-dead idempotent telemetry =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/${CATALOG}/mark-dead" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"'"${ENTRY}"'","reason":"HTTP 404 repeated 3 times"}'
echo

echo "== dead entry without allow_dead_retry -> CATALOG_LINK_DEAD =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/${CATALOG}/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"'"${ENTRY}"'","overwrite":true,"verify_sha256":false}'
echo

echo "== dead retry with simulated commit failure -> failure telemetry increments =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/${CATALOG}/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"'"${ENTRY}"'","overwrite":true,"allow_dead_retry":true,"simulate_commit_failure":true}'
echo

echo "== entry projection after retry failure =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalogs/${CATALOG}/entries/${ENTRY}"
echo

echo "== dead retry successful commit -> dead->online and fail_count reset =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/${CATALOG}/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"'"${ENTRY}"'","overwrite":true,"allow_dead_retry":true}'
echo

echo "== entry projection after retry success =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalogs/${CATALOG}/entries/${ENTRY}"
echo
