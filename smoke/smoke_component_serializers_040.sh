#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${BASE_URL:-http://esptari.local}"
TS="$(date +%Y%m%d_%H%M%S)"
OUT="captures/component_serializers_040_smoke_postfix_${TS}.txt"
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
call_json "session_reset" POST "/api/v2/engine/session/reset" "" "^(200|409)$"
call_json "session_start" POST "/api/v2/engine/session/start" "" "^(200|409)$"

call_json "save_serializers_run1" POST "/api/v2/engine/state/save" '{"session_id":"ses_local","name":"serializers_040_a"}' "^200$"
FP1="$(extract_json 'd["data"]["serializer_fingerprint"]')"

call_json "save_serializers_run2" POST "/api/v2/engine/state/save" '{"session_id":"ses_local","name":"serializers_040_b"}' "^200$"
FP2="$(extract_json 'd["data"]["serializer_fingerprint"]')"

if [[ "$FP1" != "$FP2" ]]; then
  echo "determinism_check=failed fp1=$FP1 fp2=$FP2" | tee -a "$OUT"
  exit 1
fi
echo "determinism_check=pass fingerprint=$FP1" | tee -a "$OUT"

call_json "save_force_invalid" POST "/api/v2/engine/state/save?force_serializer_invalid=1" '{"session_id":"ses_local","name":"serializers_040_fail"}' "^500$"

echo "Smoke PASS"
echo "Evidence: $OUT"
