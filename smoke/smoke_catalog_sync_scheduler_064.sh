#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://esptari.local}"

for _ in $(seq 1 40); do
  if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

echo "== create schedule =="
create_resp="$(curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalog-sync/schedules" \
  -H "Content-Type: application/json" \
  -d '{"job_type":"rom_catalog_sync","mode":"catalog_and_probe_links","cron":"0 */6 * * *","enabled":true,"catch_up":false}')"
echo "${create_resp}"

echo "== list schedules =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalog-sync/schedules"
echo

schedule_id="$(python3 -c 'import json,sys; print(json.loads(sys.stdin.read())["data"]["schedule_id"])' <<<"${create_resp}")"

echo "== get schedule =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalog-sync/schedules/${schedule_id}"
echo

echo "== run manual catalog-sync job =="
run_resp="$(curl --max-time 10 -sS -X POST "${BASE_URL}/api/v2/catalog-sync/jobs/run" \
  -H "Content-Type: application/json" \
  -d '{"job_type":"floppy_catalog_sync","mode":"catalog_and_probe_links","limit":25}')"
echo "${run_resp}"

job_id="$(python3 -c 'import json,sys; print(json.loads(sys.stdin.read())["data"]["job_id"])' <<<"${run_resp}")"

echo "== get manual job =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalog-sync/jobs/${job_id}"
echo

echo "== list jobs =="
curl --max-time 10 -sS "${BASE_URL}/api/v2/catalog-sync/jobs"
echo
