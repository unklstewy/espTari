#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"

for _ in $(seq 1 30); do
  if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

echo "== download-entry invalid bool type -> BAD_REQUEST =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"disk.automation.a_093","overwrite":"no"}'
echo

echo "== download-entry dead link without allow_dead_retry -> CATALOG_LINK_DEAD =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"disk.demos.dead_entry"}'
echo

echo "== download-entry dead link with allow_dead_retry + high priority =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"disk.demos.dead_entry","allow_dead_retry":true,"priority":"high"}'
echo

echo "== download-entry same entry again -> already_queued =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/download-entry" \
  -H "Content-Type: application/json" \
  -d '{"entry_id":"disk.demos.dead_entry","allow_dead_retry":true,"priority":"high"}'
echo
