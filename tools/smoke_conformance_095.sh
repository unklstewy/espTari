#!/usr/bin/env bash
set -euo pipefail

BASE="http://esptari.local"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/conformance_095_smoke_postflash_${TS}.txt"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

wait_for_health() {
  local attempts="${1:-60}"
  local interval="${2:-1}"
  local i
  for ((i=1; i<=attempts; i++)); do
    if curl --max-time 2 -sS "${BASE}/api/v2/engine/health" >/dev/null 2>&1; then
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
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE$path" -H "Content-Type: application/json" -d "$data")
  else
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE$path")
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
    echo "Step failed: $name HTTP=$code"
    echo "$body"
    exit 1
  fi
  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

wait_for_health
call_json "engine_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

MANIFEST='{"manifest_id":"mf_smoke_095","manifest":{"manifest_version":1,"target_machine":"atari_st","profile":"st_520_pal","suite":"smoke_095","cases":[{"case_id":"clock_step_contract","kind":"api","target":"/api/v2/debug/clock/step","assertions":["status_200"]},{"case_id":"stream_backpressure_contract","kind":"api","target":"/api/v2/stream/telemetry/backpressure","assertions":["status_200"]}]}}'
call_json "manifest_load" POST "/api/v2/conformance/manifests/load" "$MANIFEST" "^200$"

SESSION='{"session_id":"ses_local","manifest_id":"mf_smoke_095","run_mode":"execute","evidence_level":"summary"}'
call_json "harness_session" POST "/api/v2/conformance/harness/session" "$SESSION" "^200$"
HARNESS_ID="$(extract_json 'd["data"]["harness_session_id"]')"

COLLECT="{\"harness_session_id\":\"${HARNESS_ID}\",\"artifact_types\":[\"logs\",\"metrics\"],\"window\":{\"start_event_seq\":1,\"end_event_seq\":2}}"
call_json "evidence_collect" POST "/api/v2/conformance/harness/evidence/collect" "$COLLECT" "^200$"
COLLECTION_ID="$(extract_json 'd["data"]["collection_id"]')"

REPORT="{\"harness_session_id\":\"${HARNESS_ID}\",\"collection_id\":\"${COLLECTION_ID}\",\"format\":\"json\"}"
call_json "report_package" POST "/api/v2/conformance/harness/report/package" "$REPORT" "^200$"
PACKAGE_ID="$(extract_json 'd["data"]["package_id"]')"

RUNNER_REQ="{\"harness_session_id\":\"${HARNESS_ID}\",\"checklist_id\":\"st_acceptance_v1\",\"selection\":{\"include_case_ids\":[\"clock_step_contract\",\"stream_backpressure_contract\"],\"exclude_case_ids\":[]},\"stop_on_failure\":true}"
call_json "checklist_run" POST "/api/v2/conformance/harness/checklist/run" "$RUNNER_REQ" "^200$"
RUNNER_ID="$(extract_json 'd["data"]["runner_id"]')"

call_json "checklist_status" GET "/api/v2/conformance/harness/checklist/run/status?runner_id=${RUNNER_ID}" "" "^200$"

REVIEW_REQ="{\"harness_session_id\":\"${HARNESS_ID}\",\"runner_id\":\"${RUNNER_ID}\",\"package_id\":\"${PACKAGE_ID}\",\"include_sections\":[\"summary\",\"failures\",\"artifacts\",\"telemetry\",\"checklist\"]}"
call_json "review_pack_generate" POST "/api/v2/conformance/harness/review-pack/generate" "$REVIEW_REQ" "^200$"
REVIEW_PACK_ID="$(extract_json 'd["data"]["review_pack_id"]')"

SIGNOFF_REQ="{\"review_pack_id\":\"${REVIEW_PACK_ID}\",\"signoff\":{\"requested_by\":\"qa_lead\",\"approver\":\"product_owner\",\"label\":\"s4_acceptance\"}}"
call_json "signoff_bundle_assemble" POST "/api/v2/conformance/harness/signoff-bundle/assemble" "$SIGNOFF_REQ" "^200$"

echo "Smoke PASS"
echo "Evidence: $OUT"
echo "Harness: $HARNESS_ID"
echo "Runner: $RUNNER_ID"
echo "Package: $PACKAGE_ID"
echo "ReviewPack: $REVIEW_PACK_ID"
