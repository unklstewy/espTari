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

echo "== rescan-local floppies (base) =="
base_resp=$(curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/rescan-local" \
  -H "Content-Type: application/json" \
  -d '{"hash_mode":"metadata_only","scan_roots":["/sdcard/disks"]}')
echo "$base_resp"

base_scan_id=$(printf '%s' "$base_resp" | python -c 'import json,sys; print(json.load(sys.stdin)["data"]["scan_id"])')

echo "== rescan-local floppies (head) =="
head_resp=$(curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/rescan-local" \
  -H "Content-Type: application/json" \
  -d '{"hash_mode":"metadata_only","scan_roots":["/sdcard/disks"]}')
echo "$head_resp"

echo "== missing-report with since_scan_id=${base_scan_id} =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalogs/floppies/missing-report?since_scan_id=${base_scan_id}"
echo

echo "== missing-report invalid since_scan_id -> CONFLICT =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalogs/floppies/missing-report?since_scan_id=scan_999999"
echo
