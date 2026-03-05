#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/suspend_save_rollback_043_smoke_postfix_${TS}.txt"
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

mint_auth_if_needed
append_auth_args
if [[ ${#AUTH_ARGS[@]} -eq 0 ]]; then
  echo "auth_header=none" | tee -a "$OUT"
else
  echo "auth_header=configured" | tee -a "$OUT"
fi

wait_for_health

call_json "session_reset" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
call_json "session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "resume_preflight" POST "/api/v2/engine/session/resume" '{"session_id":"ses_local","resume_mode":"running"}' "^(200|409)$"

call_json "status_pre_suspend" GET "/api/v2/engine/status" "" "^200$"
STATE_PRE_SUSPEND="$(extract_json 'd["data"]["session_state"]')"
if [[ "$STATE_PRE_SUSPEND" != "running" ]]; then
  echo "Precondition failed: lifecycle_state=$STATE_PRE_SUSPEND"
  exit 1
fi

call_json "suspend_save_success" POST "/api/v2/engine/session/suspend-save" '{"session_id":"ses_local","name":"suspend_043_ok"}' "^200$"
SUCCESS_SNAPSHOT_ID="$(extract_json 'd["data"]["snapshot_id"]')"

call_json "restore_resume_running" POST "/api/v2/engine/session/restore-resume" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${SUCCESS_SNAPSHOT_ID}\",\"resume_mode\":\"running\"}" "^200$"

call_json "suspend_save_forced_fail" POST "/api/v2/engine/session/suspend-save?force_save_fail=1" '{"session_id":"ses_local","snapshot_id":"snap_043_fail"}' "^500$"

call_json "status_after_forced_fail" GET "/api/v2/engine/status" "" "^200$"
STATE_AFTER_FAIL="$(extract_json 'd["data"]["session_state"]')"
if [[ "$STATE_AFTER_FAIL" != "running" ]]; then
  echo "Rollback validation failed: lifecycle_state=$STATE_AFTER_FAIL"
  exit 1
fi

call_json "suspend_save_invalid_state" POST "/api/v2/engine/session/suspend-save" '{"session_id":"ses_local","snapshot_id":"snap_043_state_guard"}' "^200$"
call_json "suspend_save_invalid_state_repeat" POST "/api/v2/engine/session/suspend-save" '{"session_id":"ses_local","snapshot_id":"snap_043_state_guard_2"}' "^409$"

echo "Smoke PASS"
echo "Evidence: $OUT"
