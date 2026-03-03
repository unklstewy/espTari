#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"

for _ in $(seq 1 30); do
  if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

echo "== probe-links floppies with timeout policy (mark dead after 1 failure) =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/probe-links" \
  -H "Content-Type: application/json" \
  -d '{"limit":2,"timeout_ms":1,"mark_dead_after_failures":1}'
echo

echo "== probe-links floppies second pass confirms dead persistence =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/probe-links" \
  -H "Content-Type: application/json" \
  -d '{"limit":2,"timeout_ms":1,"mark_dead_after_failures":1}'
echo

echo "== probe-links invalid timeout -> BAD_REQUEST =="
curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalogs/floppies/probe-links" \
  -H "Content-Type: application/json" \
  -d '{"timeout_ms":0,"mark_dead_after_failures":1}'
echo
