#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_017_release_bundle_integration_${TS}.txt"
mkdir -p captures
: > "$OUT"
LAST_BODY=""

AUTH_BEARER="${AUTH_BEARER:-}"
AUTH_HEADER="${AUTH_HEADER:-}"
AUTH_CLIENT_ID="${AUTH_CLIENT_ID:-esptari-smoke}"
AUTH_CLIENT_SECRET="${AUTH_CLIENT_SECRET:-esptari-smoke-secret}"
AUTH_SCOPE="${AUTH_SCOPE:-engine:control inspect:read}"
AUTH_ARGS=()

mint_auth_if_needed() {
  if [[ -n "$AUTH_HEADER" ]]; then
    return 0
  fi
  if [[ -n "$AUTH_BEARER" ]]; then
    AUTH_HEADER="Authorization: Bearer ${AUTH_BEARER}"
    return 0
  fi

  local mint_resp minted_token
  mint_resp="$(curl --max-time 15 -sS -X POST "${BASE_URL}/api/v2/auth/token" \
    -H "Content-Type: application/json" \
    -d "{\"grant_type\":\"client_credentials\",\"client_id\":\"${AUTH_CLIENT_ID}\",\"client_secret\":\"${AUTH_CLIENT_SECRET}\",\"scope\":\"${AUTH_SCOPE}\"}" || true)"

  minted_token="$(printf '%s' "$mint_resp" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys
raw=sys.stdin.read().strip()
token=""
try:
    d=json.loads(raw) if raw else {}
    token=d.get("data",{}).get("access_token","") if d.get("ok") else ""
except Exception:
    token=""
print(token)')"

  if [[ -n "$minted_token" ]]; then
    AUTH_BEARER="$minted_token"
    AUTH_HEADER="Authorization: Bearer ${AUTH_BEARER}"
  fi
}

append_auth_args() {
  AUTH_ARGS=()
  if [[ -n "$AUTH_BEARER" ]]; then
    AUTH_ARGS+=( -H "Authorization: Bearer ${AUTH_BEARER}" )
  fi
  if [[ -n "$AUTH_HEADER" ]]; then
    AUTH_ARGS+=( -H "$AUTH_HEADER" )
  fi
}

wait_for_health() {
  local attempts="${1:-60}"
  local interval="${2:-1}"
  local i
  for ((i=1; i<=attempts; i++)); do
    if curl --max-time 2 -sS "${AUTH_ARGS[@]}" "${BASE_URL}/api/v2/engine/health" >/dev/null 2>&1; then
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
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path" "${AUTH_ARGS[@]}" -H "Content-Type: application/json" -d "$data")
  else
    code=$(curl --max-time 20 -sS -o "$body_file" -w "%{http_code}" -X "$method" "$BASE_URL$path" "${AUTH_ARGS[@]}")
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
    echo "step_failed=$name http=$code" | tee -a "$OUT"
    echo "$body" | tee -a "$OUT"
    exit 1
  fi

  LAST_BODY="$body"
}

extract_json() {
  local expr="$1"
  printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c "import json,sys; d=json.load(sys.stdin); print($expr)"
}

mint_auth_if_needed
append_auth_args
if [[ ${#AUTH_ARGS[@]} -eq 0 ]]; then
  echo "auth_header=none" | tee -a "$OUT"
else
  echo "auth_header=configured" | tee -a "$OUT"
fi

echo "determinism_runs=3" | tee -a "$OUT"
wait_for_health

run_signature_ref=""
for run in 1 2 3; do
  call_json "session_reset_run_${run}" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
  call_json "session_start_run_${run}" POST "/api/v2/engine/session/start" "" "^(200|409)$"

  manifest_id="mf_s10_017_run_${run}"
  manifest_payload="{\"manifest_id\":\"${manifest_id}\",\"manifest\":{\"manifest_version\":1,\"target_machine\":\"atari_st\",\"profile\":\"st_520_pal\",\"suite\":\"s10_017\",\"cases\":[{\"case_id\":\"clock_step_contract\",\"kind\":\"api\",\"target\":\"/api/v2/debug/clock/step\",\"assertions\":[\"status_200\"]},{\"case_id\":\"stream_backpressure_contract\",\"kind\":\"api\",\"target\":\"/api/v2/stream/telemetry/backpressure\",\"assertions\":[\"status_200\"]}]}}"
  call_json "manifest_load_run_${run}" POST "/api/v2/conformance/manifests/load" "$manifest_payload" "^200$"
  checks_total="$(extract_json 'd["data"]["checks_total"]')"
  checks_enabled="$(extract_json 'd["data"]["checks_enabled"]')"

  harness_payload="{\"session_id\":\"ses_local\",\"manifest_id\":\"${manifest_id}\",\"run_mode\":\"execute\",\"evidence_level\":\"summary\"}"
  call_json "harness_session_run_${run}" POST "/api/v2/conformance/harness/session" "$harness_payload" "^200$"
  harness_id="$(extract_json 'd["data"]["harness_session_id"]')"

  collect_payload="{\"harness_session_id\":\"${harness_id}\",\"artifact_types\":[\"logs\",\"metrics\"],\"window\":{\"start_event_seq\":1,\"end_event_seq\":2}}"
  call_json "evidence_collect_run_${run}" POST "/api/v2/conformance/harness/evidence/collect" "$collect_payload" "^200$"
  collection_id="$(extract_json 'd["data"]["collection_id"]')"

  report_payload="{\"harness_session_id\":\"${harness_id}\",\"collection_id\":\"${collection_id}\",\"format\":\"json\"}"
  call_json "report_package_run_${run}" POST "/api/v2/conformance/harness/report/package" "$report_payload" "^200$"
  package_id="$(extract_json 'd["data"]["package_id"]')"

  checklist_payload="{\"harness_session_id\":\"${harness_id}\",\"checklist_id\":\"st_acceptance_v1\",\"selection\":{\"include_case_ids\":[\"clock_step_contract\",\"stream_backpressure_contract\"],\"exclude_case_ids\":[]},\"stop_on_failure\":true}"
  call_json "checklist_run_${run}" POST "/api/v2/conformance/harness/checklist/run" "$checklist_payload" "^200$"
  runner_id="$(extract_json 'd["data"]["runner_id"]')"

  review_payload="{\"harness_session_id\":\"${harness_id}\",\"runner_id\":\"${runner_id}\",\"package_id\":\"${package_id}\",\"include_sections\":[\"summary\",\"failures\",\"artifacts\",\"telemetry\",\"checklist\"]}"
  call_json "review_pack_generate_${run}" POST "/api/v2/conformance/harness/review-pack/generate" "$review_payload" "^200$"
  review_pack_id="$(extract_json 'd["data"]["review_pack_id"]')"

  signoff_payload="{\"review_pack_id\":\"${review_pack_id}\",\"signoff\":{\"requested_by\":\"qa_lead\",\"approver\":\"product_owner\",\"label\":\"s10_017_release_final\"}}"
  call_json "signoff_bundle_assemble_${run}" POST "/api/v2/conformance/harness/signoff-bundle/assemble" "$signoff_payload" "^200$"
  bundle_state="$(extract_json 'd["data"]["state"]')"
  bundle_uri="$(extract_json 'd["data"]["bundle_uri"]')"
  bundle_sha="$(extract_json 'd["data"]["sha256"]')"

  uri_ok="$(/home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import sys
uri=sys.argv[1]
print(str("/signoff_bundle_" in uri and uri.endswith(".zip")))' "$bundle_uri")"
  sha_len="$(/home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import sys
print(len(sys.argv[1]))' "$bundle_sha")"
  if [[ "$bundle_state" != "ready" || "$uri_ok" != "True" || "$sha_len" != "64" ]]; then
    echo "release_bundle_contract=failed run=${run} state=${bundle_state} uri_ok=${uri_ok} sha_len=${sha_len}" | tee -a "$OUT"
    exit 1
  fi

  run_signature="manifest=${checks_total}:${checks_enabled};bundle=${bundle_state}:zip:sha64"
  echo "run_${run}_signature=${run_signature}" | tee -a "$OUT"

  if [[ -z "$run_signature_ref" ]]; then
    run_signature_ref="$run_signature"
  elif [[ "$run_signature" != "$run_signature_ref" ]]; then
    echo "determinism_check=failed run=${run}" | tee -a "$OUT"
    exit 1
  fi
done

echo "determinism_check=pass" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
