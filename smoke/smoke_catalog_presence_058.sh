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

echo "== rescan-local floppies (metadata_only) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/rescan-local" \
  -H "Content-Type: application/json" \
  -d '{"hash_mode":"metadata_only","scan_roots":["/sdcard/disks"]}'
echo

echo "== rescan-local roms (metadata_only) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/roms/rescan-local" \
  -H "Content-Type: application/json" \
  -d '{"hash_mode":"metadata_only","scan_roots":["/sdcard/roms"]}'
echo

echo "== missing-report floppies after rescan =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalogs/floppies/missing-report"
echo

echo "== rescan-local path guard -> PATH_NOT_ALLOWED =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/rescan-local" \
  -H "Content-Type: application/json" \
  -d '{"hash_mode":"metadata_only","scan_roots":["/tmp"]}'
echo
