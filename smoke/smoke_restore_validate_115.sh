#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/restore_validate_115_smoke_postfix_${TS}.txt"
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

call_json "session_reset" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
call_json "session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"
call_json "suspend_save_baseline" POST "/api/v2/engine/session/suspend-save" '{"session_id":"ses_local","name":"rval115"}' "^200$"
SNAPSHOT_ID="$(extract_json 'd["data"]["snapshot_id"]')"

call_json "validate_missing_session" POST "/api/v2/engine/state/restore/validate" "{\"snapshot_id\":\"${SNAPSHOT_ID}\",\"strict\":true}" "^400$"
MISSING_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$MISSING_CODE" != "BAD_REQUEST" ]]; then
  echo "missing_session_guard=failed code=$MISSING_CODE" | tee -a "$OUT"
  exit 1
fi

echo "missing_session_guard=pass code=$MISSING_CODE" | tee -a "$OUT"

call_json "validate_unknown_session" POST "/api/v2/engine/state/restore/validate" "{\"session_id\":\"ses_unknown\",\"snapshot_id\":\"${SNAPSHOT_ID}\",\"strict\":true}" "^409$"
UNKNOWN_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$UNKNOWN_CODE" != "ENGINE_NOT_RUNNING" ]]; then
  echo "unknown_session_guard=failed code=$UNKNOWN_CODE" | tee -a "$OUT"
  exit 1
fi

echo "unknown_session_guard=pass code=$UNKNOWN_CODE" | tee -a "$OUT"

call_json "validate_bad_strict_type" POST "/api/v2/engine/state/restore/validate" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${SNAPSHOT_ID}\",\"strict\":\"true\"}" "^400$"
STRICT_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$STRICT_CODE" != "BAD_REQUEST" ]]; then
  echo "strict_type_guard=failed code=$STRICT_CODE" | tee -a "$OUT"
  exit 1
fi

echo "strict_type_guard=pass code=$STRICT_CODE" | tee -a "$OUT"

call_json "validate_snapshot_missing" POST "/api/v2/engine/state/restore/validate" '{"session_id":"ses_local","snapshot_id":"snap_missing_115","strict":true}' "^404$"
NOT_FOUND_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$NOT_FOUND_CODE" != "SNAPSHOT_NOT_FOUND" ]]; then
  echo "snapshot_not_found_guard=failed code=$NOT_FOUND_CODE" | tee -a "$OUT"
  exit 1
fi

echo "snapshot_not_found_guard=pass code=$NOT_FOUND_CODE" | tee -a "$OUT"

call_json "validate_success_shape" POST "/api/v2/engine/state/restore/validate" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${SNAPSHOT_ID}\",\"strict\":true}" "^200$"
COMPATIBLE="$(extract_json 'str(d["data"]["compatible"]).lower()')"
FAILED_RULE="$(extract_json '"null" if d["data"]["failed_rule_id"] is None else d["data"]["failed_rule_id"]')"
ERROR_CODE="$(extract_json '"null" if d["data"]["error_code"] is None else d["data"]["error_code"]')"
RULE_COUNT="$(extract_json 'len(d["data"]["evaluated_rules"])')"
RULE_FMT_OK="$(extract_json 'all(isinstance(r, dict) and "rule_id" in r and "result" in r for r in d["data"]["evaluated_rules"])')"
VALIDATED_AT="$(extract_json 'int(d["data"]["validated_at_us"])')"

if [[ "$COMPATIBLE" != "true" || "$FAILED_RULE" != "null" || "$ERROR_CODE" != "null" || "$RULE_COUNT" != "4" || "$RULE_FMT_OK" != "True" || "$VALIDATED_AT" -le 0 ]]; then
  echo "validate_shape=failed compatible=$COMPATIBLE failed_rule=$FAILED_RULE error_code=$ERROR_CODE rule_count=$RULE_COUNT rule_fmt_ok=$RULE_FMT_OK validated_at_us=$VALIDATED_AT" | tee -a "$OUT"
  exit 1
fi

echo "validate_shape=pass compatible=$COMPATIBLE failed_rule=$FAILED_RULE error_code=$ERROR_CODE rule_count=$RULE_COUNT rule_fmt_ok=$RULE_FMT_OK validated_at_us=$VALIDATED_AT" | tee -a "$OUT"

call_json "restore_resume_running" POST "/api/v2/engine/session/restore-resume" "{\"session_id\":\"ses_local\",\"snapshot_id\":\"${SNAPSHOT_ID}\",\"resume_mode\":\"running\"}" "^200$"

echo "Smoke PASS" | tee -a "$OUT"
echo "Evidence: $OUT"
