#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
CATALOG="${CATALOG:-floppies}"
ENTRY="${ENTRY:-disk.demos.dead_entry}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/s6_catalog_reliability_004_smoke_postfix_${TS}.txt"
BUNDLE="captures/s6_catalog_reliability_bundle_004_${TS}.json"
MATRIX="TRACKING/evidence/s6_catalog_reliability_matrix_004.json"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

wait_for_health() {
  local attempts="${1:-60}"
  local interval="${2:-1}"
  local i
  for ((i=1; i<=attempts; i++)); do
    if curl --max-time 2 -sS "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
      echo "health_check=ok attempts=${i}" | tee -a "$OUT"
      return 0
    fi
    sleep "$interval"
  done
  echo "health_check=timeout attempts=${attempts}" | tee -a "$OUT"
  exit 1
}

call_json() {
  local name="$1" method="$2" path="$3" data="$4" expected="$5"
  local body_file code body
  body_file="$(mktemp)"
  if [[ -n "$data" ]]; then
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path" -H "Content-Type: application/json" -d "$data")
  else
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path")
  fi
  body="$(cat "$body_file")"
  rm -f "$body_file"

  {
    echo "### $name"
    echo "$method $path"
    echo "HTTP $code"
    echo "$body"
    echo
  } >> "$OUT"

  if [[ ! "$code" =~ $expected ]]; then
    echo "Step failed: $name HTTP=$code" | tee -a "$OUT"
    echo "$body" | tee -a "$OUT"
    exit 1
  fi

  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

wait_for_health
if [[ ! -f "$MATRIX" ]]; then
  echo "missing_matrix=$MATRIX" | tee -a "$OUT"
  exit 1
fi

call_json "engine_session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "mark_dead_seed" POST "/api/v2/catalogs/${CATALOG}/mark-dead" "{\"entry_id\":\"${ENTRY}\",\"reason\":\"S6-004 reliability seed\"}" "^200$"
seed_after="$(extract_json 'd["data"]["state_after"]')"
if [[ "$seed_after" != "dead" ]]; then
  echo "mark_dead_seed=failed state_after=$seed_after" | tee -a "$OUT"
  exit 1
fi
echo "mark_dead_seed=pass state_after=$seed_after" | tee -a "$OUT"

call_json "probe_timeout_policy" POST "/api/v2/catalogs/${CATALOG}/probe-links" '{"limit":2,"timeout_ms":1,"mark_dead_after_failures":1}' "^200$"
probe_worker="$(extract_json 'd["data"]["worker_id"]')"
probe_timed_out="$(extract_json 'int(d["data"]["summary"].get("timed_out",0))')"
if [[ -z "$probe_worker" || "$probe_timed_out" -lt 0 ]]; then
  echo "probe_timeout_policy=failed worker=$probe_worker timed_out=$probe_timed_out" | tee -a "$OUT"
  exit 1
fi
echo "probe_timeout_policy=pass worker_id=$probe_worker timed_out=$probe_timed_out" | tee -a "$OUT"

call_json "probe_invalid_timeout" POST "/api/v2/catalogs/${CATALOG}/probe-links" '{"timeout_ms":0,"mark_dead_after_failures":1}' "^400$"
bad_timeout_code="$(extract_json 'd["error"]["code"]')"
if [[ "$bad_timeout_code" != "BAD_REQUEST" ]]; then
  echo "probe_invalid_timeout=failed code=$bad_timeout_code" | tee -a "$OUT"
  exit 1
fi
echo "probe_invalid_timeout=pass code=$bad_timeout_code" | tee -a "$OUT"

call_json "download_dead_without_retry" POST "/api/v2/catalogs/${CATALOG}/download-entry" "{\"entry_id\":\"${ENTRY}\"}" "^409$"
no_retry_code="$(extract_json 'd["error"]["code"]')"
if [[ "$no_retry_code" != "CATALOG_LINK_DEAD" ]]; then
  echo "download_dead_without_retry=failed code=$no_retry_code" | tee -a "$OUT"
  exit 1
fi
echo "download_dead_without_retry=pass code=$no_retry_code" | tee -a "$OUT"

call_json "download_dead_retry_commit_failure" POST "/api/v2/catalogs/${CATALOG}/download-entry" "{\"entry_id\":\"${ENTRY}\",\"overwrite\":true,\"allow_dead_retry\":true,\"simulate_commit_failure\":true}" "^409$"
retry_fail_code="$(extract_json 'd["error"]["code"]')"
if [[ "$retry_fail_code" != "CATALOG_SYNC_FAILED" ]]; then
  echo "download_dead_retry_commit_failure=failed code=$retry_fail_code" | tee -a "$OUT"
  exit 1
fi
echo "download_dead_retry_commit_failure=pass code=$retry_fail_code" | tee -a "$OUT"

call_json "entry_projection_after_retry_failure" GET "/api/v2/catalogs/${CATALOG}/entries/${ENTRY}" "" "^200$"
entry_state_fail="$(extract_json 'd["data"]["availability_state"]')"
entry_retry_failures="$(extract_json 'int(d["data"]["dead_retry_failures"])')"
if [[ "$entry_state_fail" != "dead" || "$entry_retry_failures" -lt 1 ]]; then
  echo "entry_projection_after_retry_failure=failed state=$entry_state_fail dead_retry_failures=$entry_retry_failures" | tee -a "$OUT"
  exit 1
fi
echo "entry_projection_after_retry_failure=pass state=$entry_state_fail dead_retry_failures=$entry_retry_failures" | tee -a "$OUT"

call_json "download_dead_retry_success" POST "/api/v2/catalogs/${CATALOG}/download-entry" "{\"entry_id\":\"${ENTRY}\",\"overwrite\":true,\"allow_dead_retry\":true}" "^200$"
retry_success_state="$(extract_json 'd["data"]["availability_state"]')"
if [[ "$retry_success_state" != "online" ]]; then
  echo "download_dead_retry_success=failed availability_state=$retry_success_state" | tee -a "$OUT"
  exit 1
fi
echo "download_dead_retry_success=pass availability_state=$retry_success_state" | tee -a "$OUT"

call_json "entry_projection_after_retry_success" GET "/api/v2/catalogs/${CATALOG}/entries/${ENTRY}" "" "^200$"
entry_state_success="$(extract_json 'd["data"]["availability_state"]')"
entry_retry_successes="$(extract_json 'int(d["data"]["dead_retry_successes"])')"
if [[ "$entry_state_success" != "online" || "$entry_retry_successes" -lt 1 ]]; then
  echo "entry_projection_after_retry_success=failed state=$entry_state_success dead_retry_successes=$entry_retry_successes" | tee -a "$OUT"
  exit 1
fi
echo "entry_projection_after_retry_success=pass state=$entry_state_success dead_retry_successes=$entry_retry_successes" | tee -a "$OUT"

call_json "post_checks_health" GET "/api/v2/engine/health" "" "^200$"
post_ok="$(extract_json 'str(d.get("ok") is True)')"
if [[ "$post_ok" != "True" ]]; then
  echo "post_checks_health=failed ok=$post_ok" | tee -a "$OUT"
  exit 1
fi

cat > "$BUNDLE" <<EOF
{
  "bundle_version": 1,
  "task_id": "S6-004",
  "generated_at": "${TS}",
  "harness_capture": "${OUT}",
  "reliability_matrix": "${MATRIX}",
  "summary": {
    "probe_worker_id": "${probe_worker}",
    "probe_timed_out": ${probe_timed_out},
    "dead_retry_failures": ${entry_retry_failures},
    "dead_retry_successes": ${entry_retry_successes}
  }
}
EOF

echo "catalog_reliability=pass probe_worker=$probe_worker dead_retry_failures=$entry_retry_failures dead_retry_successes=$entry_retry_successes" | tee -a "$OUT"
echo "bundle_path=${BUNDLE}" | tee -a "$OUT"
echo "Smoke PASS"
echo "Evidence: $OUT"
