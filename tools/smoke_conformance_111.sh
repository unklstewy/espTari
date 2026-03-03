#!/usr/bin/env bash
set -euo pipefail

BASE="http://esptari.local"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/conformance_111_smoke_postflash_${TS}.txt"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

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

sleep 3
call_json "engine_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

MANIFEST='{"manifest_id":"mf_smoke_111","manifest":{"manifest_version":1,"target_machine":"atari_st","profile":"st_520_pal","suite":"smoke_111","cases":[{"case_id":"mfp_irq_contract","kind":"api","target":"/api/v2/conformance/subsystems/suites/report","assertions":["status_200"]}]}}'
call_json "manifest_load" POST "/api/v2/conformance/manifests/load" "$MANIFEST" "^200$"

SESSION='{"session_id":"ses_local","manifest_id":"mf_smoke_111","run_mode":"execute","evidence_level":"summary"}'
call_json "harness_session" POST "/api/v2/conformance/harness/session" "$SESSION" "^200$"
HARNESS_ID="$(extract_json 'd["data"]["harness_session_id"]')"

COLLECT="{\"harness_session_id\":\"${HARNESS_ID}\",\"artifact_types\":[\"logs\",\"metrics\"],\"window\":{\"start_event_seq\":1,\"end_event_seq\":2}}"
call_json "evidence_collect" POST "/api/v2/conformance/harness/evidence/collect" "$COLLECT" "^200$"
COLLECTION_ID="$(extract_json 'd["data"]["collection_id"]')"

REPORT="{\"harness_session_id\":\"${HARNESS_ID}\",\"collection_id\":\"${COLLECTION_ID}\",\"format\":\"json\"}"
call_json "report_package" POST "/api/v2/conformance/harness/report/package" "$REPORT" "^200$"

SCAFFOLD_REQ="{\"harness_session_id\":\"${HARNESS_ID}\",\"subsystem\":\"mfp\",\"fixture_profile\":\"mfp_timer_irq_smoke\",\"seed_mode\":\"baseline\"}"
call_json "subsystem_scaffold" POST "/api/v2/conformance/subsystems/scaffold" "$SCAFFOLD_REQ" "^200$"
SCAFFOLD_ID="$(extract_json 'd["data"]["scaffold_id"]')"

call_json "fixture_model" GET "/api/v2/conformance/subsystems/fixtures/model?scaffold_id=${SCAFFOLD_ID}" "" "^200$"

SUITE_REQ="{\"harness_session_id\":\"${HARNESS_ID}\",\"scaffold_id\":\"${SCAFFOLD_ID}\",\"suite_id\":\"mfp_acceptance_v1\",\"selection\":{\"include_case_ids\":[\"timer_a_irq\",\"vector_routing\"],\"exclude_case_ids\":[]},\"stop_on_failure\":true}"
call_json "subsystem_suite_run" POST "/api/v2/conformance/subsystems/suites/run" "$SUITE_REQ" "^200$"
SUITE_RUN_ID="$(extract_json 'd["data"]["suite_run_id"]')"

call_json "subsystem_suite_report" GET "/api/v2/conformance/subsystems/suites/report?suite_run_id=${SUITE_RUN_ID}" "" "^200$"

echo "Smoke PASS"
echo "Evidence: $OUT"
echo "Harness: $HARNESS_ID"
echo "Scaffold: $SCAFFOLD_ID"
echo "SuiteRun: $SUITE_RUN_ID"
