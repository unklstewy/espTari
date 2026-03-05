#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"

echo "== trigger system reset =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/system" \
  -H "Content-Type: application/json" \
  -d '{"action":"reset"}' >/dev/null || true

echo "== wait for health after startup =="
ok=0
for i in $(seq 1 40); do
  if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/tmp/esptari_health_122.json 2>/dev/null; then
    ok=1
    break
  fi
  sleep 1
done

if [[ "$ok" -ne 1 ]]; then
  echo "health_check=timeout"
  exit 1
fi

echo "health_check=ok"
cat /tmp/esptari_health_122.json
echo

echo "== web route still available post-ntp-bootstrap =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalogs/floppies/entries"
echo
