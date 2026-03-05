#!/usr/bin/env bash
set -u

BASE="http://esptari.local"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/conformance_151_smoke_postflash_${TS}.txt"
mkdir -p captures
: > "$OUT"

fail=0
LAST_BODY=""

call_json() {
  local name="$1" method="$2" path="$3" data="$4" expected_regex="$5"
  local body_file code body curl_rc
  body_file="$(mktemp)"
  if [[ -n "$data" ]]; then
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE$path" -H "Content-Type: application/json" -d "$data")
  else
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE$path")
  fi
  curl_rc=$?
  body="$(cat "$body_file")"
  rm -f "$body_file"

  {
    echo "### $name"
    echo "$method $path"
    echo "CURL_RC $curl_rc"
    echo "HTTP $code"
    echo "$body"
    echo
  } >> "$OUT"

  if [[ $curl_rc -ne 0 ]]; then
    echo "Smoke step failed (transport): $name rc=$curl_rc"
    fail=1
    return
  fi

  if [[ ! "$code" =~ $expected_regex ]]; then
    echo "Smoke step failed (HTTP): $name expected=$expected_regex got=$code"
    echo "Body: $body"
    fail=1
    return
  fi

  LAST_BODY="$body"
  echo "Smoke step ok: $name HTTP $code"
}

sleep 3
call_json "engine_health" GET "/api/v2/engine/health" "" "^200$"
call_json "engine_status" GET "/api/v2/engine/status" "" "^200$"
call_json "engine_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

MANIFEST_PAYLOAD='{"manifest_id":"mf_smoke_001","manifest":{"manifest_version":1,"target_machine":"atari_st","profile":"st_520_pal","suite":"smoke","cases":[{"case_id":"c_api_health","kind":"api","target":"/api/v2/engine/health","assertions":["status_200"]}]}}'
call_json "conformance_manifest_load" POST "/api/v2/conformance/manifests/load" "$MANIFEST_PAYLOAD" "^200$"

if [[ $fail -eq 0 ]]; then
  HARNESS_PAYLOAD='{"session_id":"ses_local","manifest_id":"mf_smoke_001","run_mode":"execute","evidence_level":"summary"}'
  call_json "conformance_harness_session" POST "/api/v2/conformance/harness/session" "$HARNESS_PAYLOAD" "^200$"
fi

HARNESS_ID=""
if [[ $fail -eq 0 ]]; then
  HARNESS_ID="$(printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys; print(json.load(sys.stdin)["data"]["harness_session_id"])' 2>/dev/null)"
  if [[ -z "$HARNESS_ID" ]]; then
    echo "Failed parsing harness_session_id"
    fail=1
  fi
fi

if [[ $fail -eq 0 ]]; then
  COLLECT_PAYLOAD="{\"harness_session_id\":\"${HARNESS_ID}\",\"artifact_types\":[\"logs\",\"metrics\"],\"window\":{\"start_event_seq\":1,\"end_event_seq\":2}}"
  call_json "conformance_evidence_collect" POST "/api/v2/conformance/harness/evidence/collect" "$COLLECT_PAYLOAD" "^200$"
fi

COLLECTION_ID=""
if [[ $fail -eq 0 ]]; then
  COLLECTION_ID="$(printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys; print(json.load(sys.stdin)["data"]["collection_id"])' 2>/dev/null)"
  if [[ -z "$COLLECTION_ID" ]]; then
    echo "Failed parsing collection_id"
    fail=1
  fi
fi

if [[ $fail -eq 0 ]]; then
  REPORT_PAYLOAD="{\"harness_session_id\":\"${HARNESS_ID}\",\"collection_id\":\"${COLLECTION_ID}\",\"format\":\"json\"}"
  call_json "conformance_report_package" POST "/api/v2/conformance/harness/report/package" "$REPORT_PAYLOAD" "^200$"
fi

if [[ $fail -eq 0 ]]; then
  echo "Smoke PASS"
  echo "Evidence: $OUT"
  echo "Harness: $HARNESS_ID"
  echo "Collection: $COLLECTION_ID"
  exit 0
fi

echo "Smoke FAIL"
echo "Evidence: $OUT"
exit 1
