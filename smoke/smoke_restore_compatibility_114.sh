#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/_lib/smoke_auth.sh"
smoke_auth_init "${BASE_URL}" "$0"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/restore_compatibility_114_smoke_postfix_${TS}.txt"
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

SNAPSHOT_ID="snap_restore_compat_114"
call_json "suspend_save_baseline" POST "/api/v2/engine/session/suspend-save" "{\"snapshot_id\":\"${SNAPSHOT_ID}\"}" "^200$"

call_json "validate_restore_compatible" POST "/api/v2/engine/state/restore/validate" "{\"snapshot_id\":\"${SNAPSHOT_ID}\",\"strict\":true}" "^200$"
COMPATIBLE="$(extract_json 'str(d["data"]["compatible"]).lower()')"
RULE_COUNT="$(extract_json 'len(d["data"]["evaluated_rules"])')"
if [[ "$COMPATIBLE" != "true" || "$RULE_COUNT" != "4" ]]; then
  echo "compatibility_matrix=failed compatible=$COMPATIBLE rule_count=$RULE_COUNT" | tee -a "$OUT"
  exit 1
fi

echo "compatibility_matrix=pass compatible=$COMPATIBLE rule_count=$RULE_COUNT" | tee -a "$OUT"

call_json "validate_missing_snapshot" POST "/api/v2/engine/state/restore/validate" '{"snapshot_id":"snap_missing_114","strict":true}' "^404$"
ERROR_CODE="$(extract_json 'd["error"]["code"]')"
if [[ "$ERROR_CODE" != "SNAPSHOT_NOT_FOUND" ]]; then
  echo "missing_snapshot_mapping=failed code=$ERROR_CODE" | tee -a "$OUT"
  exit 1
fi

echo "missing_snapshot_mapping=pass code=$ERROR_CODE" | tee -a "$OUT"

echo "Smoke PASS"
echo "Evidence: $OUT"
