#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/t112_t115_lifecycle_runtime_${TS}.txt"
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

  minted_token="$(printf '%s' "$mint_resp" | python3 -c 'import json,sys
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
  printf '%s' "$LAST_BODY" | python3 -c "import json,sys; d=json.load(sys.stdin); print($expr)"
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

  suspend_payload="{\"session_id\":\"ses_local\",\"name\":\"t112_t115_run_${run}\",\"reason\":\"runtime_smoke\",\"auto_resume\":false,\"include_stream_state\":true}"
  call_json "suspend_save_ok_run_${run}" POST "/api/v2/engine/session/suspend-save" "$suspend_payload" "^200$"
  snapshot_id="$(extract_json 'd["data"]["snapshot_id"]')"
  suspend_state="$(extract_json 'd["data"]["state"]')"
  suspend_transition="$(extract_json 'd["data"]["lifecycle_transition"]')"
  saved_at_us="$(extract_json 'int(d["data"]["saved_at_us"])')"

  call_json "engine_status_suspended_run_${run}" GET "/api/v2/engine/status" "" "^200$"
  status_state="$(extract_json 'd["data"]["session_state"]')"
  status_last_transition="$(extract_json 'int(d["data"]["last_transition_us"])')"
  save_transition_match="$(python3 -c "print('True' if ${saved_at_us} == ${status_last_transition} else 'False')")"

  call_json "suspend_save_invalid_state_run_${run}" POST "/api/v2/engine/session/suspend-save" "$suspend_payload" "^409$"
  suspend_invalid_code="$(extract_json 'd["error"]["code"]')"

  validate_ok_payload="{\"session_id\":\"ses_local\",\"snapshot_id\":\"${snapshot_id}\",\"strict\":true}"
  call_json "restore_validate_ok_run_${run}" POST "/api/v2/engine/state/restore/validate" "$validate_ok_payload" "^200$"
  validate_compatible="$(extract_json 'str(bool(d["data"]["compatible"]))')"

  validate_missing_payload="{\"session_id\":\"ses_local\",\"snapshot_id\":\"missing_t112_t115_${run}\",\"strict\":true}"
  call_json "restore_validate_missing_run_${run}" POST "/api/v2/engine/state/restore/validate" "$validate_missing_payload" "^404$"
  validate_missing_code="$(extract_json 'd["error"]["code"]')"

  validate_bad_payload="{\"session_id\":\"ses_local\",\"snapshot_id\":\"${snapshot_id}\",\"strict\":\"yes\"}"
  call_json "restore_validate_bad_request_run_${run}" POST "/api/v2/engine/state/restore/validate" "$validate_bad_payload" "^400$"
  validate_bad_code="$(extract_json 'd["error"]["code"]')"

  restore_payload="{\"session_id\":\"ses_local\",\"snapshot_id\":\"${snapshot_id}\",\"resume_mode\":\"running\",\"reason\":\"runtime_smoke\"}"
  call_json "restore_resume_ok_run_${run}" POST "/api/v2/engine/session/restore-resume" "$restore_payload" "^200$"
  restore_state="$(extract_json 'd["data"]["state"]')"
  restore_transition="$(extract_json 'd["data"]["lifecycle_transition"]')"
  restored_at_us="$(extract_json 'int(d["data"]["restored_at_us"])')"

  call_json "engine_status_running_run_${run}" GET "/api/v2/engine/status" "" "^200$"
  running_status_state="$(extract_json 'd["data"]["session_state"]')"
  running_last_transition="$(extract_json 'int(d["data"]["last_transition_us"])')"
  restore_transition_match="$(python3 -c "print('True' if ${restored_at_us} == ${running_last_transition} else 'False')")"

  call_json "restore_resume_invalid_state_run_${run}" POST "/api/v2/engine/session/restore-resume" "$restore_payload" "^409$"
  restore_invalid_code="$(extract_json 'd["error"]["code"]')"

  call_json "suspend_save_force_fail_run_${run}" POST "/api/v2/engine/session/suspend-save?force_save_fail=1" "$suspend_payload" "^500$"
  force_fail_code="$(extract_json 'd["error"]["code"]')"
  rollback_preserved="$(extract_json 'str(bool(d["error"]["details"]["rollback_state_preserved"]))')"

  if [[ "$suspend_state" != "suspended" || "$suspend_transition" != "running->suspended" || "$status_state" != "suspended" || "$save_transition_match" != "True" || "$suspend_invalid_code" != "INVALID_SESSION_STATE" || "$validate_compatible" != "True" || "$validate_missing_code" != "SNAPSHOT_NOT_FOUND" || "$validate_bad_code" != "BAD_REQUEST" || "$restore_state" != "running" || "$restore_transition" != "suspended->running" || "$running_status_state" != "running" || "$restore_transition_match" != "True" || "$restore_invalid_code" != "ENGINE_NOT_SUSPENDED" || "$force_fail_code" != "INTERNAL_ERROR" || "$rollback_preserved" != "True" ]]; then
    echo "lifecycle_contract=failed run=${run} suspend=${suspend_state}:${suspend_transition}:${status_state}:${save_transition_match}:${suspend_invalid_code} validate=${validate_compatible}:${validate_missing_code}:${validate_bad_code} restore=${restore_state}:${restore_transition}:${running_status_state}:${restore_transition_match}:${restore_invalid_code} force_fail=${force_fail_code}:${rollback_preserved}" | tee -a "$OUT"
    exit 1
  fi

  run_signature="suspend=${suspend_state}:${suspend_transition}:${status_state}:${save_transition_match}:${suspend_invalid_code};validate=${validate_compatible}:${validate_missing_code}:${validate_bad_code};restore=${restore_state}:${restore_transition}:${running_status_state}:${restore_transition_match}:${restore_invalid_code};force_fail=${force_fail_code}:${rollback_preserved}"
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
