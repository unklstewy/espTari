#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/ebin_s10_012_suspend_restore_integration_${TS}.txt"
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

  call_json "suspend_save_run_${run}" POST "/api/v2/engine/session/suspend-save" "{\"session_id\":\"ses_local\",\"name\":\"s10_012_run_${run}\",\"reason\":\"integration\",\"auto_resume\":false,\"include_stream_state\":true}" "^200$"
  suspend_state="$(extract_json 'd["data"]["state"]')"
  suspend_transition="$(extract_json 'd["data"]["lifecycle_transition"]')"
  snapshot_id="$(extract_json 'd["data"]["snapshot_id"]')"
  suspend_reason="$(extract_json 'd["data"].get("reason", "integration")')"
  if [[ "$suspend_state" != "suspended" || "$suspend_transition" != "running->suspended" || -z "$snapshot_id" ]]; then
    echo "suspend_contract=failed run=${run} state=${suspend_state} transition=${suspend_transition} snapshot=${snapshot_id}" | tee -a "$OUT"
    exit 1
  fi

  call_json "restore_running_run_${run}" POST "/api/v2/engine/session/restore-resume" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${snapshot_id}\",\"resume_mode\":\"running\",\"reason\":\"integration_restore\"}" "^200$"
  restore_state="$(extract_json 'd["data"]["state"]')"
  restore_transition="$(extract_json 'd["data"]["lifecycle_transition"]')"
  restore_snapshot="$(extract_json 'd["data"]["snapshot_id"]')"
  if [[ "$restore_state" != "running" || "$restore_transition" != "suspended->running" || "$restore_snapshot" != "$snapshot_id" ]]; then
    echo "restore_running_contract=failed run=${run} state=${restore_state} transition=${restore_transition} snapshot=${restore_snapshot}" | tee -a "$OUT"
    exit 1
  fi

  call_json "restore_not_suspended_run_${run}" POST "/api/v2/engine/session/restore-resume" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${snapshot_id}\",\"resume_mode\":\"paused\"}" "^409$"
  not_suspended_code="$(extract_json 'd["error"]["code"]')"
  if [[ "$not_suspended_code" != "ENGINE_NOT_SUSPENDED" ]]; then
    echo "restore_not_suspended_guard=failed run=${run} code=${not_suspended_code}" | tee -a "$OUT"
    exit 1
  fi

  run_signature="suspend=${suspend_state}:${suspend_transition}:${suspend_reason};restore=${restore_state}:${restore_transition};guard=${not_suspended_code}"
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
