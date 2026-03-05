#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_013_conformance_integration_${TS}.txt"
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

  manifest_id="mf_s10_013_run_${run}"
  manifest_payload="{\"manifest_id\":\"${manifest_id}\",\"manifest\":{\"manifest_version\":1,\"target_machine\":\"atari_st\",\"profile\":\"st_520_pal\",\"suite\":\"s10_013\",\"cases\":[{\"case_id\":\"mfp_irq_contract\",\"kind\":\"api\",\"target\":\"/api/v2/conformance/subsystems/suites/report\",\"assertions\":[\"status_200\"]}]}}"
  call_json "manifest_load_run_${run}" POST "/api/v2/conformance/manifests/load" "$manifest_payload" "^200$"
  checks_total="$(extract_json 'd["data"]["checks_total"]')"
  checks_enabled="$(extract_json 'd["data"]["checks_enabled"]')"

  harness_payload="{\"session_id\":\"ses_local\",\"manifest_id\":\"${manifest_id}\",\"run_mode\":\"execute\",\"evidence_level\":\"summary\"}"
  call_json "harness_session_run_${run}" POST "/api/v2/conformance/harness/session" "$harness_payload" "^200$"
  harness_id="$(extract_json 'd["data"]["harness_session_id"]')"

  collect_payload="{\"harness_session_id\":\"${harness_id}\",\"artifact_types\":[\"logs\",\"metrics\"],\"window\":{\"start_event_seq\":1,\"end_event_seq\":2}}"
  call_json "evidence_collect_run_${run}" POST "/api/v2/conformance/harness/evidence/collect" "$collect_payload" "^200$"
  artifact_types="$(printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys
d=json.load(sys.stdin)
types=sorted(a.get("type", "") for a in d.get("data", {}).get("artifacts", []))
print(",".join(types))')"
  collection_state="$(extract_json 'd["data"]["state"]')"
  collection_id="$(extract_json 'd["data"]["collection_id"]')"

  report_payload="{\"harness_session_id\":\"${harness_id}\",\"collection_id\":\"${collection_id}\",\"format\":\"json\"}"
  call_json "report_package_run_${run}" POST "/api/v2/conformance/harness/report/package" "$report_payload" "^200$"
  package_state="$(extract_json 'd["data"]["state"]')"

  scaffold_payload="{\"harness_session_id\":\"${harness_id}\",\"subsystem\":\"mfp\",\"fixture_profile\":\"mfp_timer_irq_smoke\",\"seed_mode\":\"baseline\"}"
  call_json "subsystem_scaffold_run_${run}" POST "/api/v2/conformance/subsystems/scaffold" "$scaffold_payload" "^200$"
  scaffold_id="$(extract_json 'd["data"]["scaffold_id"]')"

  call_json "fixture_model_run_${run}" GET "/api/v2/conformance/subsystems/fixtures/model?scaffold_id=${scaffold_id}" "" "^200$"

  suite_payload="{\"harness_session_id\":\"${harness_id}\",\"scaffold_id\":\"${scaffold_id}\",\"suite_id\":\"mfp_acceptance_v1\",\"selection\":{\"include_case_ids\":[\"timer_a_irq\",\"vector_routing\"],\"exclude_case_ids\":[]},\"stop_on_failure\":true}"
  call_json "subsystem_suite_run_${run}" POST "/api/v2/conformance/subsystems/suites/run" "$suite_payload" "^200$"
  suite_run_id="$(extract_json 'd["data"]["suite_run_id"]')"
  suite_state="$(extract_json 'd["data"]["state"]')"
  suite_total="$(extract_json 'd["data"]["cases_total"]')"
  suite_passed="$(extract_json 'd["data"]["cases_passed"]')"
  suite_failed="$(extract_json 'd["data"]["cases_failed"]')"

  call_json "subsystem_suite_report_${run}" GET "/api/v2/conformance/subsystems/suites/report?suite_run_id=${suite_run_id}" "" "^200$"
  report_total="$(extract_json 'd["data"]["summary"]["cases_total"]')"
  report_passed="$(extract_json 'd["data"]["summary"]["cases_passed"]')"
  report_failed="$(extract_json 'd["data"]["summary"]["cases_failed"]')"
  case_results_sig="$(printf '%s' "$LAST_BODY" | /home/sannis/electronics/esp32p4/projects/espTari/.venv/bin/python -c 'import json,sys
d=json.load(sys.stdin)
rows=[]
for r in d.get("data", {}).get("case_results", []):
  rows.append("{}:{}".format(r.get("case_id", ""), r.get("result", "")))
print(",".join(sorted(rows)))')"

  if [[ "$collection_state" != "completed" || "$package_state" != "ready" || "$suite_state" != "completed" || "$suite_total" != "$suite_passed" || "$suite_failed" != "0" || "$report_total" != "$report_passed" || "$report_failed" != "0" ]]; then
    echo "integration_contract=failed run=${run} collection_state=${collection_state} package_state=${package_state} suite_state=${suite_state} suite=${suite_total}/${suite_passed}/${suite_failed} report=${report_total}/${report_passed}/${report_failed}" | tee -a "$OUT"
    exit 1
  fi

  run_signature="manifest=${checks_total}:${checks_enabled};artifacts=${artifact_types};suite=${suite_total}:${suite_passed}:${suite_failed};report=${report_total}:${report_passed}:${report_failed};cases=${case_results_sig}"
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
