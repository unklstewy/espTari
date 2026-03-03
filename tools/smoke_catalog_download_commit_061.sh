#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"

for _ in $(seq 1 30); do
  if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

echo "== staged commit success (verify_sha256) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"disk.automation.a_093","overwrite":true,"verify_sha256":true}'
echo

echo "== staged blocker simulate_truncated -> UPLOAD_INCOMPLETE =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"disk.automation.a_093","overwrite":true,"simulate_truncated":true}'
echo

echo "== staged blocker simulate_hash_mismatch -> CATALOG_SYNC_FAILED =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"disk.automation.a_093","overwrite":true,"verify_sha256":true,"simulate_hash_mismatch":true}'
echo

echo "== staged blocker simulate_commit_failure -> CATALOG_SYNC_FAILED =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"disk.automation.a_093","overwrite":true,"simulate_commit_failure":true}'
echo
